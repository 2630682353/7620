#ifndef __SAAS_LIB_H__
#define __SAAS_LIB_H__
#include <dirent.h>
#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

#define  SAAS_MODE_VENDER "telcom"
#define  SAAS_PRODUCT "gateway"

#define  SAAS_VER  "1.0.0"
#define  SAAS_SVN   6789


#define SAAS_DBG printf


typedef struct {
	int up_speed;
	int down_speed;
	int up_bytes;
	int down_bytes;
}network_stat_t;

typedef struct {
	int cpu_percent;
	int mem_percent;
	int connect_counts;
	int wifi_user_count;
	int total_mem_k;
	int mem_used;
	int mem_free;
	int network_type;
	char pppoe_account[64];
	network_stat_t ns;
}saas_system_info_t;






















#endif
