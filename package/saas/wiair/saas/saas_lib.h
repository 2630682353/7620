#ifndef __SAAS_LIB_H__
#define __SAAS_LIB_H__

#define SAAS_DBG printf

struct saas_proc_list {
	struct list_head list;
	char name[512];
	pid_t pid;
	int mem;
	int cpu;
};

struct saas_proc_info {
	int used_mem;
	int free_mem;
	int cpuused;
	struct list_head proc;
};

struct saas_sys_info {
	unsigned int devid;
	int Revision;
	int toltal_mem;
	unsigned char mac[6];
	char model[64];
	char ver[64];
	char date[64];
};

extern int saas_sysinfo(struct saas_sys_info *sys);
extern int saas_wifi_num(void);
extern int saas_connect_num(void);
extern int saas_check_proc(int mem, int cpu, int (*cb)(struct saas_proc_info *));

#endif

