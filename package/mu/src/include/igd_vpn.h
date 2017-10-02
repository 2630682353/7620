#ifndef _IGD_VPN_
#define _IGD_VPN_

#define VPN_FILE "/tmp/rconf/vpndns.txt"
#define VPN_INIT_FILE "/etc/init.d/gl.init"

#define VPN_DNS_DATA_MX IGD_DNS_LEN * VPN_DNS_PER_MX

#define VPN_RTABLE 100
#define VPN_MARK 1000
#define VPN_CHAIN "VPNCHAIN"

#define VPN_CFG_FILE "vpnconfig"

#define VPN_MOD_INIT DEFINE_MSG(MODULE_VPN, 0)
enum {
	VPN_MOD_SET_SERVER = DEFINE_MSG(MODULE_VPN, 1),
	VPN_MOD_SET_DNSLIST,
	VPN_MOD_DNS_REPLEY,
	VPN_MOD_PPPD_IPUP,
	VPN_MOD_PPPD_IPDOWN,
	VPN_MOD_PARAM_SET,
	VPN_MOD_STATUS_DUMP,
	VPN_MOD_SET_CALLID,
};

struct vpn_info {
	int vpn_enable;
	char user[IGD_NAME_LEN];
	char password[IGD_NAME_LEN];
	char callid[IGD_NAME_LEN * 2];
	struct sys_msg_ipcp ipcp;
};
extern int vpn_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
#endif
