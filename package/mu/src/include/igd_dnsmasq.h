#ifndef _DNS_MASQUE_
#define _DNS_MASQUE_

#define DHCP_CONFIG "dhcp"
#define DNSMASQ_CONFIG_FILE "/tmp/dnsmasq.conf"
#define DDNS_CONFIG_FILE "/tmp/phlinux.conf"
#define DDNS_TEMP_FILE "/tmp/ddns_temp"
#define PHDDNS "phddns"
#define DNSMASQ_CMD "dnsmasq -C /tmp/dnsmasq.conf -8 /tmp/dnsmasq_log -q -k"

struct dhcp_conf {
	int enable;
	int time;
	struct in_addr ip;
	struct in_addr mask;
	struct in_addr start;
	struct in_addr end;
	struct in_addr dns1;
	struct in_addr dns2;
};

struct ddns_info {
	int enable;
	char userid[30];
	char pwd[30];
};

struct ddns_status{
	struct ddns_info dif;
	char basic[5];
	char domains[150];
};

struct ip_mac {
	struct in_addr ip;
	unsigned char mac[ETH_ALEN];
};

#define IP_MAC_MAX 10

#define DNSMASQ_MOD_INIT DEFINE_MSG(MODULE_DNSMASQ, 0)

enum {
	DNSMASQ_DHCP_SET = DEFINE_MSG(MODULE_DNSMASQ, 1),
	DNSMASQ_DHCP_SHOW,
	DNSMASQ_SHOST_SET,
	DNSMASQ_SHOST_SHOW,
	DNSMASQ_IPMAC_SET,
	DNSMASQ_IPMAC_SHOW,
	DNSMASQ_DDNS_SET,
	DNSMASQ_DDNS_SHOW,	
	DNSMASQ_DDNS_RESTART
};

extern int dnsmasq_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
#endif
