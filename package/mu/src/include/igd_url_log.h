#ifndef _IGD_MODULE_URL_LOG_
#define _IGD_MODULE_URL_LOG_

#define URL_LOG_ZIP "/tmp/url_log.gz"
#define URL_LOG_FILE "/tmp/url_log"
#define URL_LOG_DOMAIN_FILE "/tmp/rconf/domain.txt"
#define URL_LOG_LEN_MX URL_LOG_LIST_MX * IGD_URL_LEN

#define URL_LOG_MOD_INIT DEFINE_MSG(MODULE_URLLOG, 0)
enum {
	URL_LOG_MOD_DUMP_URL = DEFINE_MSG(MODULE_URLLOG, 1),
	URL_LOG_MOD_SET_URL,
	URL_LOG_MOD_GET_DNS,
	URL_LOG_MOD_SET_SERVER,
};

extern int urllog_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
#endif