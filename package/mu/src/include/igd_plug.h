#ifndef __IGD_PROXY_H__
#define __IGD_PROXY_H__

#include "module.h"
#include "igd_cloud.h"

enum {
	IGD_PLUG_INIT = DEFINE_MSG(MODULE_PLUG, 0),
	IGD_PLUG_SWITCH,
	IGD_PLUG_SWITCH_GET,

	IGD_PLUG_KILL,
	IGD_PLUG_KILL_ALL,
	IGD_PLUG_START,
	IGD_PLUG_CACHE,
	IGD_PLUG_STOP_REQUEST,
};

#define PROXY_PORT  80
#define PROXY_MARK  3000
#define PROXY_DNAT_CHAIN   "PROXY_DNS"

#define PROXY_DNS_FILE_MAX  (500*1024)
#define PROXY_DNS_FILE "/tmp/rconf/proxy_dns.txt"
#define PROXY_INIT_FILE "/etc/init.d/proxy.init"

extern int igd_plug_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);

#define DIR_MAX_LEN 128
#define PLUG_NAME_MAX_LEN 64
struct plug_info {
	char dir[DIR_MAX_LEN];
	char plug_name[PLUG_NAME_MAX_LEN];
};

struct plug_cache_info {
	char plug_name[PLUG_NAME_MAX_LEN];
	char url[128];
	char md5[33];
};

#define PLUG_PROC "cloud_exchange p"
#define PLUG_FILE_DIR "/tmp/app_gz"

#endif

