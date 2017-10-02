#ifndef _IGD_MODULE_QOS_
#define _IGD_MODULE_QOS_

#define QOS_CONFIG_FILE  "qos_rule"
#define TSPEED_CONFIG_FILE  "/tmp/rconf/tspeed.txt"
#define TSPEED_STATUS_FILE  "/tmp/testspeed"

/*TSF:test speed flag*/
#define TSF_PID  "PID"
#define TSF_PING  "PING"
#define TSF_DOWN  "DOWN"
#define TSF_SPEED  "SPEED"
#define TSF_ERR  "ERR"

#define QOS_MOD_INIT DEFINE_MSG(MODULE_QOS, 0)
enum {
	QOS_PARAM_SET = DEFINE_MSG(MODULE_QOS, 1),
	QOS_PARAM_SHOW,
	QOS_TEST_SPEED,
	QOS_SPEED_SHOW,
	QOS_TEST_BREAK,
};

struct qos_conf {
	unsigned int enable;
	unsigned int up;
	unsigned int down;
};

struct tspeed_info {
	int flag;
	unsigned int ispeed;
	unsigned int up;
	unsigned int down;
	unsigned int delay;
};

extern int qos_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
#endif
