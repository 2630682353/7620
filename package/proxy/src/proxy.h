#ifndef ___PROXY_H___
#define ___PROXY_H___

#define PROXY_DNS_HMX   499
#define PROXY_DNS_ADDR_MX   6
#define PROXY_DNS_ENTRY_MX   300
#define PROXY_DNS_ENTRY_ADDR_MX   (PROXY_DNS_ENTRY_MX * PROXY_DNS_ADDR_MX)

struct proxy_dns_node {
	struct dns_node_comm comm;
	struct list_head list;
	struct dns_addr *addr[PROXY_DNS_ADDR_MX];
};

#endif //___PROXY_H___
