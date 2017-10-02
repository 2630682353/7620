#ifndef _IGD_MU_INTERFACE_
#define _IGD_MU_INTERFACE_

enum {
	IF_TYPE_LAN = 1,
	IF_TYPE_WAN,	
};

enum {
	MODE_DHCP = 1,
	MODE_PPPOE,
	MODE_STATIC,
	MODE_WISP,
	MODE_3G,
};

enum {
	IF_STATUS_INIT = 1,
	IF_STATUS_IP_UP,
	IF_STATUS_IP_DOWN,
};

enum {
	NET_OFFLINE = 0,
	NET_ONLINE,
};

enum {
	DETECT_OFF = 0,
	DETECT_ON,
};

#define WAN_BAK "wanbak"
#define DNS_DOMAIN "www.baidu.com"
#define IF_CONFIG_FILE "network"
#define WAN_DNS_FILE "/var/resolv.conf.auto"
#define WAN_DHCP_PID "/var/run/udhcpc.pid"

#define PPPOE_SERVER_CMD "pppoe-server -I "LAN_DEVNAME" -k -F -L 10.254.254.1 &"

#define PPPOE_MODE_FILE "/tmp/pppoe_dhcp"
#define PPPOE_DETECT_PID "/var/run/pppoe_dhcp.pid"
#define PPPOE_DETECT_CMD "/usr/bin/pppoe_dhcp -I "WAN1_DEVNAME" -t 1 -F 1 -p /var/run/pppoe_dhcp.pid &"

#define NETDETECT "netdetect"

#define IF_MOD_INIT DEFINE_MSG(MODULE_INTERFACE, 0)
enum {
	IF_MOD_PARAM_SET = DEFINE_MSG(MODULE_INTERFACE, 1),
	IF_MOD_PARAM_SHOW,
	IF_MOD_IFACE_INFO,
	IF_MOD_IFACE_IP_UP,
	IF_MOD_IFACE_IP_DOWN,
	IF_MOD_WAN_DETECT,
	IF_MOD_GET_ACCOUNT,
	IF_MOD_SEND_ACCOUNT,
	IF_MOD_NET_STATUS,
	IF_MOD_DETECT_REPLY,
	IF_MOD_ISP_STATUS,
	IF_MOD_LAN_RESTART,
	IF_MOD_FIREWALL_GET,
	IF_MOD_FIREWALL_SET,
	IF_MOD_WAN_BRIDAGE,
};

struct detect_info {
	int link;
	int net;
	int mode;
};

struct account_info {	
	char user[IGD_NAME_LEN];
	char passwd[IGD_NAME_LEN];
	uint8_t peermac[ETH_ALEN];
};

struct iconf_comm {
	int mtu;
	struct in_addr dns[DNS_IP_MX];
};

struct pppoe_conf {
	struct iconf_comm comm;
	char user[IGD_NAME_LEN];
	char passwd[IGD_NAME_LEN];
	char pppoe_server[IGD_NAME_LEN];
};

struct static_conf {
	struct iconf_comm comm;
	struct in_addr ip;	
	struct in_addr mask;	
	struct in_addr gw;
};

struct wisp_conf {
	int channel;
	char ssid[IGD_NAME_LEN];
	char key[IGD_NAME_LEN];
	char security[IGD_NAME_LEN];
};

struct iface_conf {
	unsigned char uid;
	unsigned char mode;
	unsigned char mac[ETH_ALEN];
	unsigned short speed;
	unsigned short isbridge;
	struct iconf_comm dhcp;
	struct pppoe_conf pppoe;
	struct static_conf statip;
	struct wisp_conf wisp;
};

struct iface_info {
	int type;
	int uid;
	int mode;
	int link; //link up or link down
	int net; //is connected to internet
	int err;
	char devname[IGD_NAME_LEN];
	char uiname[IGD_NAME_LEN];
	uint8_t mac[ETH_ALEN];
	int mtu;
	struct in_addr ip;
	struct in_addr mask;
	struct in_addr gw;
	struct in_addr dns[DNS_IP_MX];
};

struct iface;
struct iface_hander {
	int (*if_up)(struct iface *ifc, struct iface_conf *config);
	int (*if_down)(struct iface *ifc);
	int (*ip_up)(struct iface *ifc, struct sys_msg_ipcp *ipcp);
	int (*ip_down)(struct iface *ifc, struct sys_msg_ipcp *ipcp);
};

struct iface {
	int type;
	int uid;
	char devname[IGD_NAME_LEN];
	char uiname[IGD_NAME_LEN];
	unsigned long status;
	struct iface_hander *hander;
	struct iface_conf config;
	struct sys_msg_ipcp ipcp;
};

extern int interface_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);

struct firewall {
	int is_dos_open;
	int is_flood_open;
};
#endif

