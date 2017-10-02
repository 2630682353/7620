#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "libcom.h"
#include "cloud_client.h"

#define PLUG_DIR "/tmp/app"
#define PLUG_FILE_DIR "/tmp/app_gz"

static struct list_head plug_run_list = LIST_HEAD_INIT(plug_run_list);
static struct list_head plug_kill_list = LIST_HEAD_INIT(plug_kill_list);
unsigned char plug_request_flag = 1;

int cc_plug_start(struct plug_cache_info *pci)
{
	struct plug_cache_info *p = NULL;

	list_for_each_entry(p, &plug_run_list, list) {
		if (!strcmp(p->plug_name, pci->plug_name)) {
			return 0;
		}
	}
	p = malloc(sizeof(*p));
	if (!p) {
		CC_ERR("mallco fail, %s\n", pci->plug_name);
		return -1;
	}
	memset(p, 0, sizeof(*p));
	strncpy(p->url, pci->url, sizeof(p->url));
	strncpy(p->md5, pci->md5, sizeof(p->md5));
	strncpy(p->plug_name, pci->plug_name, sizeof(p->plug_name));
	list_add_tail(&p->list, &plug_run_list);
	return 0;
}

static int cc_plug_stop_one(char *plug_name)
{
	struct plug_cache_info *p = NULL;

	if (!plug_name || !plug_name[0]) {
		CC_ERR("input err, %s\n", plug_name ? : "null");
		return -1;
	}

	p = malloc(sizeof(*p));
	if (!p) {
		CC_ERR("mallco fail, %s\n", plug_name);
		return -1;
	}
	memset(p, 0, sizeof(*p));
	strncpy(p->plug_name, plug_name, sizeof(p->plug_name));
	list_add_tail(&p->list, &plug_kill_list);
	return 0;
}

static int cc_plug_stop_all(void)
{
	DIR *dir;
	struct dirent *d;

	dir = opendir(PLUG_DIR);
	if (!dir)
		return -1;

	while (1) {
		d = readdir(dir);
		if (!d)
			break;
		if (!strcmp(".", d->d_name))
			continue;
		if (!strcmp("..", d->d_name))
			continue;
		cc_plug_stop_one(d->d_name);
	}
	closedir(dir);
	return 0;
}

int cc_plug_stop(struct plug_cache_info *pci)
{
	if (!pci)
		return cc_plug_stop_all();
	return cc_plug_stop_one(pci->plug_name);
}

static int cc_plug_state(char *plug_name)
{
	int fd, state = -1;
	char file[128], str[32] = {0};

	snprintf(file, sizeof(file),
		"%s/%s/state", PLUG_DIR, plug_name);

	fd = open(file, O_RDONLY);
	if (fd < 0)
		return -1;

	if (read(fd, str, sizeof(str) - 1) < 0) {
		CC_ERR("read fail, %s\n", plug_name);
		goto err;
	}
	if (!str[0])
		goto err;
	state = !!atoi(str);
	//CC_DBG("%s, %d\n", plug_name, state);

err:
	close(fd);
	return state;
}

static int cc_plug_kill(struct plug_cache_info *pci)
{
	int time = 3;
	char cmd[256] = {0};
	char path[128] = {0};

	snprintf(path, sizeof(path), "%s/%s", PLUG_DIR, pci->plug_name);
	if (access(path, F_OK))
		return -1;

	CC_DBG("kill [%s]\n", pci->plug_name);

	snprintf(cmd, sizeof(cmd), "%s/init.sh %s stop &", path, path);
	if (system(cmd)) {
		CC_ERR("stop %s err\n", pci->plug_name);
		return -1;
	}

	while (time > 0) {
		if (cc_plug_state(pci->plug_name) == 0) {
			CC_DBG("stop succ, %s\n", pci->plug_name);
			break;
		}
		time--;
		sleep(1);
	}

	snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
	if (system(cmd)) {
		CC_ERR("rm %s err\n", path);
		return -1;
	}
	return (time > 0) ? 0 : -1;
}

static int cc_plug_run(struct plug_cache_info *pci)
{
	int time = 3;
	char cmd[512], path[128] = {0};

	cc_plug_kill(pci);

	CC_DBG("run [%s]\n", pci->plug_name);

	if (access(PLUG_DIR, F_OK))
		mkdir(PLUG_DIR, 0777);
	snprintf(path, sizeof(path),
		"%s/%s", PLUG_DIR, pci->plug_name);
	if (access(path, F_OK))
		mkdir(path, 0777);

	snprintf(cmd, sizeof(cmd), "tar -zxvf %s/%s -C %s",
			PLUG_FILE_DIR, pci->plug_name, path);
	if (system(cmd)) {
		CC_ERR("unzip err, cmd:%s\n", cmd);
		return -1;
	}
	snprintf(cmd, sizeof(cmd), "chmod 777 %s/init.sh", path);
	if (system(cmd)) {
		CC_ERR("%s fail\n", cmd);
		return -1;
	}
	snprintf(cmd, sizeof(cmd), "%s/init.sh %s start &", path, path);
	if (system(cmd)) {
		CC_ERR("start %s err\n", cmd);
		return -1;
	}
	while (time > 0) {
		if (cc_plug_state(pci->plug_name) == 1) {
			CC_DBG("run succ, %s\n", pci->plug_name);
			return 0;
		}
		time--;
		sleep(1);
	}
	return -1;
}

static int cc_plug_wget(struct plug_cache_info *pci)
{
	int i = 0;
	char cmd[512], file[128];
	unsigned char md5[32];

	CC_DBG("wget [%s]\n", pci->plug_name);

	pci->pid = fork();
	if (pci->pid < 0) {
		CC_ERR("fork fail\n");
		return -1;
	} else if (pci->pid > 0) {
		return 0;
	}

	if (access(PLUG_FILE_DIR, F_OK))
		mkdir(PLUG_FILE_DIR, 0777);

	snprintf(file, sizeof(file),
		"%s/%s", PLUG_FILE_DIR, pci->plug_name);

	for (i = 0; i < 3; i++) {
		snprintf(cmd, sizeof(cmd),
			"wget -q %s -O %s -T 10", pci->url, file);
		if (!system(cmd))
			break;
		CC_DBG("wget fail, [%s]\n", cmd);
		sleep(3);
	}
	if (i >= 3)
		goto err;

	if (igd_md5sum(file, md5)) {
		CC_ERR("%s calc md5 fail\n", file);
		goto err;
	}

	for (i = 0; i < 16; i++)
		sprintf(&cmd[i*2], "%02X", md5[i]);
	if (strncasecmp(cmd, pci->md5, 32)) {
		CC_ERR("MD5ERR:\n%s\n%s\n", cmd, pci->md5);
		goto err;
	}
	if (cc_plug_run(pci))
		goto err;

	snprintf(cmd, sizeof(cmd), "rm -rf %s", PLUG_FILE_DIR);
	system(cmd);
	exit(0);

err:
	snprintf(cmd, sizeof(cmd), "rm -rf %s", PLUG_FILE_DIR);
	system(cmd);
	exit(-1);
}

static int cc_plug_request(struct cloud_client_info *cci)
{
	char *ptr;
	unsigned char buf[512];
	char mode[128];
	int l, i;

	CC_DBG("plug request\n");

	CC_PUSH2(buf, 2, CSO_REQ_PLUGIN_INFO);
	CC_PUSH4(buf, 4, cci->key);
	i = 8;

	ptr = read_firmware("CURVER");
	l = ptr ? strlen(ptr) : -1;
	if ((l <= 0) || ((l + i + 1) > sizeof(buf))) {
		CC_ERR("CURVER err, %d\n", l);
		return -1;
	}
	CC_DBG("CURVER:%s\n", ptr);
	CC_PUSH1(buf, i, l);
	i += 1;
	CC_PUSH_LEN(buf, i, ptr, l);
	i += l;

	ptr = read_firmware("VENDOR");
	if (!ptr) {
		CC_ERR("VENDOR err\n");
		return -1;
	}
	l = snprintf(mode, sizeof(mode), "%s_", ptr);

	ptr = read_firmware("PRODUCT");
	if (!ptr) {
		CC_ERR("PRODUCT err\n");
		return -1;
	}
	snprintf(&mode[l], sizeof(mode) - l, "%s", ptr);
	CC_DBG("MODE:%s\n", mode);

	l = strlen(mode);
	if ((l <= 0) || ((l + i + 1) > sizeof(buf))) {
		CC_ERR("MODE err, %d\n", l);
		return -1;
	}
	CC_PUSH1(buf, i, l);
	i += 1;
	CC_PUSH_LEN(buf, i, mode, l);
	i += l;

	CC_PUSH2(buf, 0, i);
	return cc_send(cci->sock, buf, i);
}

void cc_plug_loop(struct cloud_client_info *cci)
{
	struct plug_cache_info *p = NULL;

	if (plug_request_flag) {
		plug_request_flag++;
		if (plug_request_flag >= 12) {
			plug_request_flag = 1;
			cc_plug_request(cci);
		}
	}

	list_for_each_entry(p, &plug_run_list, list) {
		if (p->pid > 0)
			return;
		cc_plug_wget(p);
		if (p->pid < 0) {
			list_del(&p->list);
			free(p);
		}
		return;
	}

	list_for_each_entry(p, &plug_kill_list, list) {
		if (p->pid > 0)
			return;
		p->pid = fork();
		if (p->pid < 0) {
			CC_ERR("fork kill fail, %s\n", p->plug_name);
			list_del(&p->list);
			free(p);
			return;
		} else if (p->pid > 0) {
			return;
		} else {
			exit(cc_plug_kill(p));
		}
	}
}

static int cc_plug_up(
	char *plug_name, int state,
	struct cloud_client_info *cci)
{
	int len;
	unsigned char tmp[256];

	len = strlen(plug_name);
	CC_PUSH2(tmp, 0, 10 + len);
	CC_PUSH2(tmp, 2, CSO_UP_PLUGIN_INFO);
	CC_PUSH4(tmp, 4, cci->key);
	CC_PUSH1(tmp, 8, state);
	CC_PUSH1(tmp, 9, len);
	CC_PUSH_LEN(tmp, 10, plug_name, len);
	return cc_send(cci->sock, tmp, 10 + len);
}

int cc_plug_wait(
	pid_t pid, int state,
	struct cloud_client_info *cci)
{
	struct plug_cache_info *p = NULL;

	list_for_each_entry(p, &plug_run_list, list) {
		if (p->pid != pid)
			continue;
		if (!state)
			cc_plug_up(p->plug_name, 1, cci);
		CC_DBG("run:%s, %d, 0x%X\n", p->plug_name, p->pid, state);
		list_del(&p->list);
		free(p);
		return 0;
	}

	list_for_each_entry(p, &plug_kill_list, list) {
		if (p->pid != pid)
			continue;
		if (!state)
			cc_plug_up(p->plug_name, 0, cci);
		CC_DBG("kill:%s, %d, 0x%X\n", p->plug_name, p->pid, state);
		list_del(&p->list);
		free(p);
		return 0;
	}
	return 0;
}
