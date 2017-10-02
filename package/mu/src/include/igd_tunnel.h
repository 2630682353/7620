#ifndef _IGD_TUNNEL_
#define _IGD_TUNNEL_

#define TUNNEL_RTABLE 99

#define TUNNEL_CFG_FILE "tunnel"
#define TUNNEL_CFG_PATH "/etc/config/tunnel"

#define L2TP_EXE "xl2tpd"
#define PPTP_PPPD "pppd-pptp"
#define L2TP_PPPD "pppd-l2tp"
#define L2TP_PPPD_OPFILE "/tmp/options.l2tp"
#define PPTP_PPPD_OPFILE "/tmp/options.pptp"
#define L2TP_CTR_PIPO "/var/run/xl2tpd/l2tp-control"
#define L2TP_NAME "wiairl2tp"

#define TUNNEL_MX 2
#define TUNNEL_CHAIN "TUNNEL"

#define TUNNEL_MOD_INIT DEFINE_MSG(MODULE_TUNNEL, 0)

enum {
	TUNNEL_MOD_SET = DEFINE_MSG(MODULE_TUNNEL, 1),
	TUNNEL_MOD_SHOW,
	TUNNEL_MOD_INFO,
	TUNNEL_MOD_RESET,
	TUNNEL_MOD_IP_UP,
	TUNNEL_MOD_IP_DOWN,
};

enum {
	TUNNEL_STATUS_IP_INIT = 0,
	TUNNEL_STATUS_IP_UP,
	TUNNEL_STATUS_IP_DOWN,
};

struct tunnel_conf;
struct tunnel_hander {
	int (*tunnel_up)(int index, struct tunnel_conf *cfg);
	int (*tunnel_down)(int index);
	int (*tunnel_ip_up)(struct sys_msg_ipcp *ipcp);
	int (*tunnel_ip_down)(struct sys_msg_ipcp *ipcp);
};

struct tunnel_conf {
	int index;
	unsigned short enable;
	unsigned short type;
	unsigned short mppe;
	unsigned short port;
	struct in_addr serverip;
	char sdomain[IGD_NAME_LEN];
	char user[IGD_NAME_LEN];
	char password[IGD_NAME_LEN];
};

struct tunnel {
	unsigned int invalid;
	unsigned long status;
	struct tunnel_conf config;
	struct tunnel_hander *hander;
	struct sys_msg_ipcp ipcp;
};

extern int tunnel_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
#endif
