#ifndef _IGD_LOG_
#define _IGD_LOG_

#define SYS_LOG_MX_NR 50
#define LOG_DESC_LEN 64
#define LOG_FILE "/tmp/logfile"
#define LOG_FILE_ZIP "/tmp/logfile.gz"

#define LOG_MOD_INIT DEFINE_MSG(MODULE_LOG, 0)
enum {
	LOG_MOD_ADD = DEFINE_MSG(MODULE_LOG, 1),
	LOG_MOD_DUMP,
	LOG_MOD_DOWNLOAD,
};

struct log_head {
	unsigned short nr;
	unsigned short mx;
	struct list_head logh;
};

struct log_desc {
	unsigned long timer;
	char desc[LOG_DESC_LEN];
};

struct log_node {
	struct list_head list;
	struct log_desc desc;
};

#define ADD_SYS_LOG(fmt, args...) do { \
	char desc[LOG_DESC_LEN]; \
	memset(desc, 0x0, sizeof(desc)); \
	snprintf(desc, sizeof(desc), fmt, ##args); \
	mu_call(LOG_MOD_ADD, desc, sizeof(desc), NULL, 0); \
} while(0)

#define _ADD_SYS_LOG(fmt, args...) do { \
	char desc[LOG_DESC_LEN]; \
	memset(desc, 0x0, sizeof(desc)); \
	snprintf(desc, sizeof(desc), fmt, ##args); \
	mu_msg(LOG_MOD_ADD, desc, sizeof(desc), NULL, 0); \
} while(0)
extern int log_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
#endif

