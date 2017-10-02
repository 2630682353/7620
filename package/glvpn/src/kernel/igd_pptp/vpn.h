#ifndef _VPN_KERNEL_H_
#define _VPN_KERNEL_H_

struct vpn_check_head {
	uint16_t tot_len;
	uint16_t cmd;
	uint32_t devid;
};

struct vpn_dns_head {
	int nr;
	int mx;
	struct list_head list;
	struct nf_trie_root root;
};

struct vpn_dns_cache {
	struct list_head list;
	struct nf_trie_node node;
	struct nf_trie_root *root;
};

struct vpn_dns_list {
	uint32_t check_id;
	struct list_head list;
	const struct net_device *in;
	const struct net_device *out;
	struct sk_buff_head dns_queue;
	unsigned long jiffies;
	unsigned char dns[32];
	int nlen;
	int iscn;
};

struct vpn_head {
	uint32_t enable;
	uint32_t devid;
	struct timer_list vtimer;
	struct vpn_dns_head dns_h;
	struct list_head dns_queue;
	uint32_t vpn_dns[DNS_IP_MX];
	uint32_t vpn_sip[DNS_IP_MX];
};

#define VPN_TIMER_TIMEOUT HZ / 100
#define VPN_TIMER_DNS_TIMEOUT HZ / 5

#define VPN_DNS_CACHE_MX 512

#define VPN_CHECK_CMD 2100
#define VPN_RECV_CMD 2101

#define VPN_CHECK_DST_PORT 8622
#define VPN_CHECK_SRC_PORT 1200
#endif

