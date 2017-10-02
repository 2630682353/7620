#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include "linux_list.h"
#include "saas_lib.h"
#include "nlk_ipc.h"

int saas_sysinfo(struct saas_sys_info *sys)
{
	int l;
	char *ptr;
	struct sysinfo info;

	sys->devid = get_devid();

	ptr = read_firmware("Revision");
	sys->Revision = ptr ? atoi(ptr) : -1;

	sysinfo(&info);
	sys->toltal_mem = info.totalram/1024;

	read_mac(sys->mac);

	ptr = read_firmware("VENDOR");
	l = snprintf(sys->model, sizeof(sys->model), 
		"%s_", ptr ? : "ERR");

	ptr = read_firmware("PRODUCT");
	snprintf(sys->model + l, sizeof(sys->model) - l, 
		"%s", ptr ? : "ERR");

	ptr = read_firmware("CURVER");
	snprintf(sys->ver, sizeof(sys->ver), 
		"%s", ptr ? : "ERR");

	ptr = read_firmware("DATE");
	snprintf(sys->date, sizeof(sys->date), 
		"%s", ptr ? : "ERR");
	return 0;
}

int saas_wifi_num(void)
{
	int i = 0, nr = 0, num = 0;
	struct host_info *host = NULL;

	host = dump_host_alive(&nr);
	if (!host)
		return 0;
	for (i = 0; i < nr; i++) {
		if (host[i].is_wifi)
			num++;
	}
	free(host);
	return num;
}

int saas_connect_num(void)
{
	int num = 0;
	FILE *fp;

	fp = popen("cat /proc/igd/conn | grep ^[^i] | wc -l", "r");
	if (!fp)
		return 0;
	if (fscanf(fp, "%d", &num) != 1) {
		num = 0;
	}
	pclose(fp);
	return num;
}

static int saas_add_proc(
	struct saas_proc_info *info, struct saas_proc_list *proc)
{
	int len = 0;
	char *ptr, *name = proc->name;
	struct saas_proc_list *l;

	//printf("-----name:%s\n", name);
	l = malloc(sizeof(*l));
	if (!l)
		return -1;
	memcpy(l, proc, sizeof(*l));
	if (name[0] == '[') {
		name++;
		len = strlen(name);
		if ((len < 1) || (len > (sizeof(proc->name) - 2))) {
			free(l);
			SAAS_DBG("name err\n");
			return -1;
		}
		if (name[len - 1] == ']')
			name[len - 1] = '\0';
	} else {
		ptr = strrchr(name, '/');
		if (ptr && strlen(ptr + 1))
			name = ptr + 1;
	}
	strncpy(l->name, name, sizeof(l->name) - 1);
	l->name[sizeof(l->name) - 1] = 0;
	//printf("name:%s\n", l->name);
	list_add(&l->list, &info->proc);
	return 0;
}

static int saas_free_proc(struct saas_proc_info *info)
{
	struct saas_proc_list *l, *_l;

	list_for_each_entry_safe(l, _l, &info->proc, list) {
		list_del(&l->list);
		free(l);
	}
	return 0;
}

static int getmembypid(pid_t pid)
{
	char file[128];
	FILE *fp = NULL;
	int mem = 0;

	snprintf(file, sizeof(file), "/proc/%d/statm", pid);
	fp = fopen(file, "r");
	if (!fp)
		return -1;

	if (fscanf(fp, "%*d %d ", &mem) != 1)
		mem = -1;
	fclose(fp);
	//SAAS_DBG("pid:%d, mem:%d\n", pid, mem);
	return mem*4;
}

int saas_check_proc(
	int mem, int cpu, 
	int (*cb)(struct saas_proc_info *))
{
	FILE *fp = NULL;
	int ret, flag = 0;
	char buf[4096], *ptr;
	struct saas_proc_info info;
	struct saas_proc_list list;

	fp = popen("top -b -n 1", "r");
	if (!fp) {
		SAAS_DBG("popen fail, %s\n", strerror(errno));
		goto err;
	}
	memset(&info, 0, sizeof(info));
	INIT_LIST_HEAD(&info.proc);

	ret = fscanf(fp, "Mem: %dK used, %dK free, %*[^\n]\n", 
		&info.used_mem, &info.free_mem);
	if (ret != 2) {
		SAAS_DBG("Mem fail, %d,\n", ret);
		goto err;
	}
	ret = fscanf(fp, "CPU:   %*d%% usr   %*d%% sys"
		"   %*d%% nic %d%% idle   %*[^\n]\n", 
		&info.cpuused);
	if (ret != 1) {
		SAAS_DBG("CPU fail, %d,\n", ret);
		goto err;
	}
	info.cpuused = (info.cpuused > 100) ? 0 : (100 - info.cpuused);
	//printf("mem:%d,%d, cpu:%d\n", info.used_mem, info.free_mem, info.cpuused);

	while (1) {
		//1243  1075 root     S     4740   8%   0% /tmp/app/tesla/bin/tesla
		//153     1 root     S <    884   1%   0% ubusd
		memset(buf, 0, sizeof(buf));
		if (!fgets(buf, sizeof(buf) - 1, fp))
			break;
		ptr = buf;
		while (*ptr == ' ')
			ptr++;
		if (!memcmp(ptr, "PID", 3)) {
			flag = 1;
			continue;
		} else if (!flag) {
			continue;
		}
		//printf("-----:%s", ptr);
		memset(&list, 0, sizeof(list));
		ret = sscanf(ptr, "%d %*d %*[^0-9]%d %*d%% %d%% %511[^ \n]\n",
			&list.pid, &list.mem, &list.cpu, list.name);
		//printf("%d, pid:[%d], mem:[%d], cpu:[%d], name:[%s]\n",
		//	ret, list.pid, list.mem, list.cpu, list.name);
		list.mem = getmembypid(list.pid);
		if (ret == 4)
			saas_add_proc(&info, &list);
	}
	cb(&info);
	saas_free_proc(&info);
err:
	pclose(fp);
	return 0;
}

