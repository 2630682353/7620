#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include "igd_lib.h"
#include "igd_plug.h"
#include "nlk_ipc.h"
#include "igd_interface.h"
#include "igd_cloud.h"
#include "igd_system.h"
#include "igd_lanxun.h"

#define IGD_PLUG_ERR(fmt,args...) \
	igd_log("/tmp/igd_plug_err", "%s[%d]:"fmt, __FUNCTION__, __LINE__, ##args);

#ifdef FLASH_4_RAM_32
#define IGD_PLUG_DBG(fmt,args...) do {}while(0)
#else
#define IGD_PLUG_DBG(fmt,args...) \
	igd_log("/tmp/igd_plug_dbg", "%s[%d]:"fmt, __FUNCTION__, __LINE__, ##args);
#endif

static unsigned long igd_switch_flag[BITS_TO_LONGS(CSS_MAX)];
static unsigned long igd_switch_up_flag[BITS_TO_LONGS(CSS_MAX)];

/*SF: switch flag*/
#define PLUG_TEST_SF(flag)  igd_test_bit(flag, igd_switch_flag)
#define PLUG_SET_SF(flag)  igd_set_bit(flag, igd_switch_flag)
#define PLUG_CLEAR_SF(flag)  igd_clear_bit(flag, igd_switch_flag)

/*SUF: switch upload flag*/
#define PLUG_TEST_SUF(flag)  igd_test_bit(flag, igd_switch_up_flag)
#define PLUG_SET_SUF(flag)  igd_set_bit(flag, igd_switch_up_flag)
#define PLUG_CLEAR_SUF(flag)  igd_clear_bit(flag, igd_switch_up_flag)

#define PLUG_DIR "/tmp/app"

static int get_plug_switch(int flag, char *sw, int len)
{
	int num;
	char **val;
	char str[256];

	memset(sw, 0, len);
	snprintf(str, sizeof(str), "qos_rule.switch.%d", flag);
	if (uuci_get(str, &val, &num))
		return 0;
	if (val[0])
		strncpy(sw, val[0], len - 1);
	uuci_get_free(val, num);
	return 0;
}

static int set_plug_switch(int flag, char *sw)
{
	int l;
	char str[128];

	uuci_set("qos_rule.switch=cloud");

	l = snprintf(str, sizeof(str), "qos_rule.switch.%d=", flag);
	if (sw[0])
		snprintf(&str[l], sizeof(str) - l, "%s", sw);
	uuci_set(str);
	return 1;
}

static int igd_plug_switch_get(int *data, int len)
{
	char str[2];
	memset(str, 0, 2);
	if (len != sizeof(*data)) {
		return -1;	
	}
	get_plug_switch(*data, str, sizeof(str));
	if ((str[0] == '0') || !str[0]) {
		return 0;
	} else {
		return 1;
	}
}

#if 0
static void igd_wwlh_switch(char *sw)
{
	if ((sw[0] == '0') || !sw[0]) {
		system("killall wWpt");
	} else {
		system("killall wWpt");
		system("wWpt -i br-lan -c cyir -m $DIR/tmp/wWplugin.md5 &");
	}
}
#endif

#define PULG_FUNCTION_________________________________________

static unsigned char switch_timer_flag = 0;
static void igd_switch_timer(void *data)
{
	int i, sw_len;
	struct timeval tv;
	unsigned char buf[128];
	char sw[32];

	switch_timer_flag = 0;
	if (!data && (sys_uptime() > 180)) {
		for (i = CSS_PROXY; i < CSS_MAX; i++) {
			if (!PLUG_TEST_SF(i)) {
				switch_timer_flag = 1;
				IGD_PLUG_DBG("plug switch %d\n", i);
				CC_PUSH2(buf, 0, 9);
				CC_PUSH2(buf, 2, CSO_REQ_SWITCH_STATUS);
				CC_PUSH1(buf, 8, i);
				CC_CALL_ADD(buf, 9);
			}
			if (!PLUG_TEST_SUF(i)) {
				switch_timer_flag = 1;
				IGD_PLUG_DBG("plug switch up [%d]\n", i);

				get_plug_switch(i, sw, sizeof(sw));
				sw_len = strlen(sw);

				CC_PUSH2(buf, 0, 10 + sw_len);
				CC_PUSH2(buf, 2, CSO_UP_SWITCH_STATUS);
				CC_PUSH1(buf, 8, i);
				CC_PUSH1(buf, 9, sw_len);
				if (sw_len > 0)
					CC_PUSH_LEN(buf, 10, sw, sw_len);
				CC_CALL_ADD(buf, 10 + sw_len);

				PLUG_SET_SUF(i);
			}
		}

		if (!switch_timer_flag)
			return;
	}

	tv.tv_sec = 60;
	tv.tv_usec = 0;
	if (!schedule(tv, igd_switch_timer, NULL))
		IGD_PLUG_ERR("schedule err\n");
}

static int igd_plug_switch(struct nlk_switch_config *sw, int len)
{
	if ((len != sizeof(*sw))
		|| (sw->flag >= CSS_MAX)
		|| (sw->flag < CSS_PROXY)) {
		IGD_PLUG_ERR("input err, %d\n", len);
		return 0;
	}
	IGD_PLUG_DBG("[%d], [%s]\n", sw->flag, sw->status);
	set_plug_switch(sw->flag, sw->status);

	PLUG_SET_SF(sw->flag);
	PLUG_CLEAR_SUF(sw->flag);

	switch (sw->flag) {
	case CSS_WWLH:
//		igd_wwlh_switch(sw->status);
		break;
		
	default:
		break;
	}

	if (!switch_timer_flag)
		igd_switch_timer(NULL);
	return 0;
}

#define NEW_PLUG_FUNCTION_______________________________

static unsigned char igd_plug_request_flag = 0;
static struct list_head igd_plug_cache_list = LIST_HEAD_INIT(igd_plug_cache_list);

struct plug_cache_list {
	struct list_head list;
	struct plug_cache_info info;
};

static void plug_stat_pro(void *data)
{
	int fd, len;
	char tmp[256], *plug_name = data + 1;
	char state = ((char *)data)[0];

	snprintf(tmp, sizeof(tmp),
		"%s/%s/state", PLUG_DIR, plug_name);
	fd = open(tmp, O_RDONLY);
	if (fd < 0) {
		IGD_PLUG_ERR("open fail, %s\n", tmp);
		goto release;
	}

	memset(tmp, 0, sizeof(tmp));
	if (read(fd, tmp, sizeof(tmp) - 1) < 0) {
		IGD_PLUG_ERR("read fail, %s\n", plug_name);
		goto release;
	}

	if (!!atoi(tmp) != state) {
		IGD_PLUG_ERR("state err, [%s], [%s], [%d]\n", plug_name, tmp, state);
		goto release;
	}

	len = strlen(plug_name);
	if ((10 + len) > sizeof(tmp)) {
		IGD_PLUG_ERR("plug_name too long [%s]\n", plug_name);
		goto release;
	}

	CC_PUSH2(tmp, 0, 10 + len);
	CC_PUSH2(tmp, 2, CSO_UP_PLUGIN_INFO);
	CC_PUSH1(tmp, 8, state);
	CC_PUSH1(tmp, 9, len);
	CC_PUSH_LEN(tmp, 10, plug_name, len);
	CC_CALL_ADD(tmp, 10 + len);

	IGD_PLUG_DBG("upload state, %s, %d\n", plug_name, state);
release:
	if (!state) {
		snprintf(tmp, sizeof(tmp), "rm -rf %s/%s", PLUG_DIR, plug_name);
		if (system(tmp)) {
			IGD_PLUG_ERR("rm %s err\n", tmp);
			return;
		}
	}
	free(data);
	if (fd > 0)
		close(fd);
}

static void igd_plug_stat_timer(char *plug_name, int start)
{
	char *data = NULL;
	struct timeval tv;
	int len = strlen(plug_name);

	data = malloc(len + 2);
	if (!data) {
		IGD_PLUG_ERR("malloc err, %s\n", plug_name);
		return;
	}
	memset(data, 0, len + 2);
	data[0] = start;
	memcpy(data + 1, plug_name, len);

	tv.tv_sec = 3;
	tv.tv_usec = 0;
	if (!schedule(tv, plug_stat_pro, data))
		IGD_PLUG_ERR("schedule err\n");
}

static int unzip(struct plug_info *pinfo, char *path)
{
	char cmd[256] = {0};

	if (access(PLUG_DIR, F_OK))
		mkdir(PLUG_DIR, S_IRWXU);
	if (access(path, F_OK))
		mkdir(path, S_IRWXU);

	snprintf(cmd, sizeof(cmd), "tar -zxvf %s -C %s",
			pinfo->dir, path);
	if (system(cmd)) {
		IGD_PLUG_ERR("unzip err\n");
		return -1;
	}
	return 0;
}

static int igd_plug_kill(char *plug_name, int len, int timer_flag)
{
	char cmd[256] = {0};
	char path[128] = {0};

	if (!plug_name[0] || len >= PLUG_NAME_MAX_LEN) {
		IGD_PLUG_ERR("input err, %d\n", len);
		return 0;
	}

	snprintf(path, sizeof(path), "%s/%s", PLUG_DIR, plug_name);
	if (access(path, F_OK))
		return 0;

	snprintf(cmd, sizeof(cmd), "%s/init.sh %s stop &", path, path);
	if (system(cmd)) {
		IGD_PLUG_ERR("stop %s err\n", plug_name);
		return -1;
	}

	if (!timer_flag) {
		snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
		if (system(cmd)) {
			IGD_PLUG_ERR("rm %s err\n", path);
			return -1;
		}
	} else {
		igd_plug_stat_timer(plug_name, 0);
	}
	return 0;
}

static int igd_plug_kill_all(void)
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
		igd_plug_kill(d->d_name, strlen(d->d_name), 1);
	}
	closedir(dir);
	return 0;
}

static int igd_plug_cmd(char *cmd)
{
	pid_t pid;
	char str[128];

	pid = fork();
	if (pid < 0)
		return -1;
	else if (pid > 0)
		return 0;
	snprintf(str, sizeof(str), \
		"echo 500 > /proc/%d/oom_score_adj", getpid());
	system(str);
	exit(system(cmd));
	return 0;
}

static int igd_plug_start(struct plug_info *pinfo, int len)
{
	char cmd[256] = {0};
	char path[128] = {0};

	if ((len != sizeof(struct plug_info)) ||
		!pinfo->dir[0] || !pinfo->plug_name[0]) {
		IGD_PLUG_ERR("input err, %d\n", len);
		return 0;
	}
	igd_plug_kill(pinfo->plug_name, strlen(pinfo->plug_name), 0);

	snprintf(path, sizeof(path), "%s/%s", PLUG_DIR, pinfo->plug_name);
	if (unzip(pinfo, path))
		return 0;

	snprintf(cmd, sizeof(cmd), "%s/init.sh", path);
	if (access(cmd, F_OK)) {
		IGD_PLUG_ERR("%s no find\n", cmd);
		return -1;
	}

	snprintf(cmd, sizeof(cmd), "chmod 777 %s/init.sh", path);
	if (system(cmd)) {
		IGD_PLUG_ERR("%s fail\n", cmd);
		return -1;
	}

	snprintf(cmd, sizeof(cmd), "%s/init.sh %s start &", path, path);
	if (igd_plug_cmd(cmd)) {
		IGD_PLUG_ERR("start %s err\n", pinfo->plug_name);
		return -1;
	}
	igd_plug_stat_timer(pinfo->plug_name, 1);
	return 0;
}

static int igd_plug_cache(
	struct plug_cache_info *pci, int len,
	struct plug_cache_info *dump, int dlen)
{
	int nr = 0, num;
	struct plug_cache_list *pcl, *_pcl;
	static unsigned char start_down = 0;

	if (dump) {
		num = dlen/sizeof(*dump);
		if (num*sizeof(*dump) != dlen) {
			start_down = 0;
			IGD_PLUG_DBG("input err\n");
			return -1;
		}

		list_for_each_entry_safe_reverse(pcl, _pcl, &igd_plug_cache_list, list) {
			if (num <= 0)
				break;
			memcpy(&dump[nr], &pcl->info, sizeof(*dump));
			nr++;
			num--;
			list_del(&pcl->list);
			free(pcl);
		}

		if (nr == 0)
			start_down = 0;
		return nr;
	} else if (!pci || (len != sizeof(*pci))) {
		IGD_PLUG_DBG("input err\n");
		return -1;
	}

	list_for_each_entry(pcl, &igd_plug_cache_list, list) {
		if (strcmp(pcl->info.plug_name, pci->plug_name))
			continue;
		memcpy(&pcl->info, pci, sizeof(pcl->info));
		return 0;
	}

	pcl = malloc(sizeof(*pcl));
	if (!pcl) {
		IGD_PLUG_ERR("malloc fail\n");
		return -1;
	}
	memset(pcl, 0, sizeof(*pcl));
	memcpy(&pcl->info, pci, sizeof(pcl->info));
	list_add(&pcl->list, &igd_plug_cache_list);

	if (!start_down) {
		mu_call(SYSTEM_MOD_DAEMON_ADD,
			PLUG_PROC, strlen(PLUG_PROC) + 1, NULL, 0);
	}
	return 0;
}

static int igd_plug_request(void)
{
	char *ptr;
	unsigned char buf[512];
	char mode[128];
	int l, i;

	CC_PUSH2(buf, 2, CSO_REQ_PLUGIN_INFO);
	i = 8;

	ptr = read_firmware("CURVER");
	l = ptr ? strlen(ptr) : -1;
	if ((l <= 0) || ((l + i + 1) > sizeof(buf))) {
		IGD_PLUG_ERR("CURVER err, %d\n", l);
		return -1;
	}
	IGD_PLUG_DBG("CURVER:%s\n", ptr);
	CC_PUSH1(buf, i, l);
	i += 1;
	CC_PUSH_LEN(buf, i, ptr, l);
	i += l;

	ptr = read_firmware("VENDOR");
	if (!ptr) {
		IGD_PLUG_ERR("VENDOR err\n");
		return -1;
	}
	l = snprintf(mode, sizeof(mode), "%s_", ptr);

	ptr = read_firmware("PRODUCT");
	if (!ptr) {
		IGD_PLUG_ERR("PRODUCT err\n");
		return -1;
	}
	snprintf(&mode[l], sizeof(mode) - l, "%s", ptr);
	IGD_PLUG_DBG("MODE:%s\n", mode);

	l = strlen(mode);
	if ((l <= 0) || ((l + i + 1) > sizeof(buf))) {
		IGD_PLUG_ERR("MODE err, %d\n", l);
		return -1;
	}
	CC_PUSH1(buf, i, l);
	i += 1;
	CC_PUSH_LEN(buf, i, mode, l);
	i += l;

	CC_PUSH2(buf, 0, i);
	CC_CALL_ADD(buf, i);
	return 0;
}

static void igd_plug_timer(void *data)
{
	struct timeval tv;

	if (igd_plug_request_flag)
		return;

	if (!data && (sys_uptime() > 180))
		igd_plug_request();

	tv.tv_sec = 22;
	tv.tv_usec = 0;
	if (!schedule(tv, igd_plug_timer, NULL))
		IGD_PLUG_ERR("schedule err\n");
}

static int igd_plug_init(void)
{
	memset(igd_switch_flag, 0xFF, sizeof(igd_switch_flag));
	memset(igd_switch_up_flag, 0xFF, sizeof(igd_switch_up_flag));
	PLUG_CLEAR_SF(CSS_DBGLOG);
//	PLUG_CLEAR_SF(CSS_WWLH);

	igd_switch_timer((void *)1);
	igd_plug_timer((void *)1);
	return 0;
}

int igd_plug_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	switch (msgid) {
	case IGD_PLUG_INIT:
		return igd_plug_init();
	case IGD_PLUG_SWITCH:
		return igd_plug_switch(data, len);
	case IGD_PLUG_SWITCH_GET:
		return igd_plug_switch_get((int *)data, len);
	case IGD_PLUG_KILL:
		return igd_plug_kill(data, len, 1);
	case IGD_PLUG_KILL_ALL:
		return igd_plug_kill_all();
	case IGD_PLUG_START:
		return igd_plug_start(data, len);
	case IGD_PLUG_CACHE:
		return igd_plug_cache(data, len, rbuf, rlen);
	case IGD_PLUG_STOP_REQUEST:
		igd_plug_request_flag = 1;
		return 0;
	default:
		break;
	}
	return 0;
}
