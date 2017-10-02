#ifndef _IGD_MODULE_URL_SAFE_
#define _IGD_MODULE_URL_SAFE_

#define URLSAFE_CONFIG_FILE "qos_rule"

#define URL_SAFE_DATA_MX IGD_DNS_LEN * 4096
#define URL_SAFE_KO "url.ko"
#define URL_SERVER_PORT 8621
#define URL_SERVER_DOMAIN "secure.wiair.com"
#define URL_SAFE_WHITE_LIST "/tmp/rconf/white.txt"

#define URL_SAFE_MOD_INIT DEFINE_MSG(MODULE_URLSAFE, 0)
enum {
	URL_SAFE_MOD_SET_ENABLE = DEFINE_MSG(MODULE_URLSAFE, 1),
	URL_SAFE_MOD_GET_DNS,
	URL_SAFE_MOD_SET_IP,
	URL_SAFE_MOD_SET_WLIST,
	URL_SAFE_MOD_SET_LANIP,
	URL_SAFE_MOD_SET_SERVER,
};

struct urlsafe_conf {
	int enable;
	unsigned int devid;
	struct in_addr local;
	struct in_addr remote[DNS_IP_MX];
	char rd_url[IGD_DNS_LEN];
};

struct urlsafe_trust {
	struct in_addr ip;
	unsigned char url[IGD_DNS_LEN];
};

extern int urlsafe_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
#endif

