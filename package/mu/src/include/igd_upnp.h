#ifndef _IGD_UPNPD_
#define _IGD_UPNPD_

#define UPNPD_MOD_INIT DEFINE_MSG(MODULE_UPNPD, 0)

enum {
	UPNPD_SERVER_SET = DEFINE_MSG(MODULE_UPNPD, 1),
	UPNPD_RULE_ADD,
	UPNPD_RULE_DEL,
	UPNPD_RULE_DUMP,
	UPNPD_HOSTOFFLINE,
	UPNPD_STATE_SET,
	UPNPD_STATE_GET,
};

struct rule_state {
	u_int64_t packets;
	u_int64_t bytes;
};

struct upnpd {
	struct {
		struct in_addr ip;
		unsigned short port;
	}in, out;
	unsigned char proto;
	char desc[64];
};

struct upnpd_info{
	struct list_head list;
	struct upnpd upnp;
	struct rule_state state;
};

struct mu_upnpd {
	int action;
	int index;
	int flag;/*if flag is set get upnpd info by index*/
	struct upnpd upnp;
};

#define IP_NAT_RANGE_MAP_IPS 1
#define IP_NAT_RANGE_PROTO_SPECIFIED 2
#define IP_NAT_RANGE_PROTO_RANDOM 4
#define IP_NAT_RANGE_PERSISTENT 8

union nf_conntrack_man_proto {
	__be16 all;
	struct { __be16 port; } tcp;
	struct { __be16 port; } udp;
	struct { __be16 id;   } icmp;
	struct { __be16 port; } dccp;
	struct { __be16 port; } sctp;
	struct { __be16 key;  } gre;
};

struct nf_nat_range {
	unsigned int flags;
	__be32 min_ip, max_ip;
	union nf_conntrack_man_proto min, max;
};

struct nf_nat_multi_range_compat {
	unsigned int rangesize;
	struct nf_nat_range range[1];
};

#define ip_nat_multi_range	nf_nat_multi_range_compat
#define ip_nat_range		nf_nat_range
#define IPTC_HANDLE		struct iptc_handle *

#define UPNNPD_CHAIN "MINIUPNPD"
#define UPNPD_CONFIG_FILE "/tmp/miniupnpd.conf"
#define UPNPD_PID_FILE "/var/run/miniupnpd.pid"
extern int upnpd_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
#endif
