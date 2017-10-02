#ifndef _IGD_ADVERT_
#define _IGD_ADVERT_

#define ADVERT_LOG_ZIP "/tmp/adlog.gz"
#define ADVERT_LOG_FILE "/tmp/adlog"
#define ADVERT_MOD_INIT DEFINE_MSG(MODULE_ADVERT, 0)

enum {
	ADVERT_MOD_UPDATE = DEFINE_MSG(MODULE_ADVERT, 1),
	ADVERT_MOD_ACTION,
	ADVERT_MOD_DUMP,
	ADVERT_MOD_UPLOAD_SUCCESS,
	ADVERT_MOD_SET_DNS,
	ADVERT_MOD_SET_SERVER,
	ADVERT_MOD_LOG_DUMP,
	ADVERT_MOD_SYSTEM,
};

struct advert_rule {
	unsigned char type;
	unsigned char rid;
	unsigned char time_on;
	unsigned short hgid;/*host grp*/
	unsigned short ugid;/*url grp*/
	struct time_comm tm;
	struct list_head list;
	struct inet_host host;
	struct http_rule_api args;
};

#define ADVERT_CONFIG "qos_rule"
#define ADVERT_FILE "/tmp/rconf/advert.txt"

extern int advert_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
#endif

