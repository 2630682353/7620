#ifndef __NETFILTER_NF_L7_H__
#define __NETFILTER_NF_L7_H__

#define L7_DIR "/tmp/l7_tmp/"
enum {
	L7_APP_QQ = 1 + L7_APP_MX * L7_MID_IM,
	L7_APP_WEIXIN,
	L7_APP_TAOBAO,
};
enum {
	L7_APP_WOW = 1 + L7_APP_MX * L7_MID_GAME,
	L7_APP_LOL,
	L7_APP_QQ_GAME,
	L7_APP_CF,
	L7_APP_DNF,
};

enum {
	L7_APP_MOVIE = L7_APP_MX * L7_MID_MOVIE,
	L7_APP_YOUKU,
	L7_APP_IQIYI,
	L7_APP_SOHU,
	L7_APP_LETV,
};

enum {
	L7_NLK_SET,
	L7_NLK_RULE,
	L7_NLK_HTTP,
	L7_NLK_DNS,
	L7_NLK_EXCLUDE,
	L7_NLK_GATHER,
	L7_NLK_UA,
};

#define L7_CONN_VALUE_MX	8

#define MATCH_TYPE_NONE		0
#define MATCH_TYPE_MEM		1
#define MATCH_TYPE_STR		2
#define MATCH_TYPE_FIX		3
#define MATCH_TYPE_RANGE	4
#define MATCH_TYPE_SUM		5
#define MATCH_TYPE_MX		6

#define L7_DEPS_MX	100

struct l7_skb_info {
	struct nf_conn *ct;
	struct sk_buff *skb;
	unsigned char *data;
	union {
		struct tcphdr *tcph;
		struct udphdr *udph;
	};
	int proto;
	int port;
	int dir;
	int len;
	int step;
	unsigned char *value;
};

struct l7_key {
	uint8_t type;
	uint8_t len;
	int16_t offset; 
	char value[8];
};

struct l7_pkt_len {
	uint16_t len;
	int16_t offset;
	uint16_t order; 
	int16_t adjust;
};

#define L7_L4_NONE	0 /*  1-65535 */
#define L7_L4_RANGE	1 
#define L7_L4_ENUM	2 /* such as:80,433,8080 */
#define L7_L4_TYPE5	5
#define L7_L4_PORT_MX	20

struct l7_l4 {
	int type;
	uint16_t port[L7_L4_PORT_MX];
};

#define HTTP_RULE_URL_MX 	10
struct nlk_l7_rule {
	struct nlk_msg_comm comm;
	struct inet_l3 l3;
	struct l7_l4 l4_src; /* only use type 5 */
	struct l7_l4 l4;
	short proto;
	short deps_type;
	int deps_id;
	int fdt_type; /* fundamental match type */
	int url_nr;
	struct l7_key fdt_key[2]; /* mx support 2 fdt key match */
	struct l7_pkt_len fdt_len;
	char url[HTTP_RULE_URL_MX][IGD_DNS_LEN];
	char data[];
};

struct nlk_l7_key {
	uint8_t dir; 
	uint8_t pkt_id;
	uint8_t type;
	uint8_t len; /*len  <= 8*/
	uint8_t end;
	uint8_t ext_type;
	uint8_t ext_len;
	int16_t offset; 
	int16_t ext_offset;
	uint16_t order;
	uint16_t len_min;
	uint16_t len_max;
	union {
		char value[8];
		uint32_t range[2]; /*  0 is range_min, 1 is range mx */
	};
};

extern int l7_register_base(int mid, int id, void *fn);
extern void l7_may_done(struct nf_conn *ct);
#endif

