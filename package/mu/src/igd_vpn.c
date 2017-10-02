#include "igd_lib.h"
#include "igd_vpn.h"
#include "igd_cloud.h"
#include "uci.h"

static struct nlk_vpn vcfg;
static struct vpn_info vinfo;
static char vpnserver[IGD_DNS_LEN];

#define VPN_LOG_PATH  "/tmp/vpn.log"
#ifdef FLASH_4_RAM_32
#define VPN_DEBUG(fmt,args...) do{}while(0)
#else
#define VPN_DEBUG(fmt,args...) do{igd_log(VPN_LOG_PATH, fmt, ##args);}while(0)
#endif

static inline int check_dns_char(char data)
{
	if (data > ' ' && data <= '~')
		return 1;
	else
		return 0;
}

static void vpn_dns_cb(void *data)
{
	struct timeval tv;
	
	tv.tv_sec = 120;
	tv.tv_usec = 0;
	if (!schedule(tv, vpn_dns_cb, NULL)) {
		VPN_DEBUG("schedule vpn event error\n");
		return;
	}
	if (!vcfg.devid)
		vcfg.devid = get_devid();
	exec_cmd("igd_dns %s %s %u &", vpnserver, IPC_PATH_MU, VPN_MOD_DNS_REPLEY);
}

static int vpn_set_param(void)
{
	if (!vcfg.devid || !vcfg.sip[0].s_addr)
		return -1;
	vcfg.comm.mid = VPN_SET_PARAM;
	return nlk_head_send(NLK_MSG_VPN, &vcfg, sizeof(vcfg));
}

static int vpn_set_dns(char (*dns)[IGD_DNS_LEN], int nr)
{
	struct nlk_msg_comm nlk;
	memset(&nlk, 0x0, sizeof(nlk));
	nlk.mid = VPN_SET_DNS_LIST;
	nlk.obj_nr = nr;
	nlk.obj_len = IGD_DNS_LEN;
	return nlk_data_send(NLK_MSG_VPN, &nlk, sizeof(nlk), dns, nr * IGD_DNS_LEN);
}

static int vpn_dns_parser(void)
{
	int i = 0, j;
	char *host = NULL;
	char *tmp = NULL;
	char *data = NULL;
	char buf[IGD_DNS_LEN];
	char dns[VPN_DNS_PER_MX][IGD_DNS_LEN] = {{0},};
	unsigned char pwd[16] = {0xcc, 0xfc, 0x78, 0x66, 0x35, 0x32, 0x97, 0xfc, 0x78, 0x99};
	
	if (!vinfo.vpn_enable || access(VPN_FILE, 0))
		return -1;
	data = malloc(VPN_DNS_DATA_MX);
	if (!data) {
		VPN_DEBUG("no mem\n");
		return -1;
	}
	if (igd_aes_dencrypt(VPN_FILE, (unsigned char *)data, VPN_DNS_DATA_MX, pwd) <= 0) {
		VPN_DEBUG("dencrypt vpn dns error\n");
		goto out;
	}
	host = data;
	while ((tmp = strchr(host, '\n')) != NULL) {
		if (tmp - host > sizeof(buf) - 1)
			goto next;
		__arr_strcpy_end((unsigned char *)buf, (unsigned char *)host, tmp - host, '\n');
		if (!strlen(buf))
			goto next;
		for (j = 0; j < strlen(buf); j++) {
			if (check_dns_char(buf[j])) {
				dns[i][j] = buf[j];
			} else {
				memset(dns[i], 0x0, IGD_DNS_LEN);
				goto next;
			}
		}
		i++;
		if (i >= VPN_DNS_PER_MX)
			break;
		memset(buf, 0x0, sizeof(buf));
	next:
		host = tmp + 1;
		continue;
	}
	if (i > 0)
		vpn_set_dns(dns, i);
out:
	free(data);
	remove(VPN_FILE);
	return 0;
}

static int vpn_set_server(void)
{
	if (vinfo.vpn_enable && CC_CALL_RCONF_INFO(RCONF_FLAG_VPNSERVER,
		vpnserver, sizeof(vpnserver))) {
		VPN_DEBUG("can not get vpn server\n");
		return -1;
	}
	return 0;
}

static int vpn_ipcp(int action, struct sys_msg_ipcp *ipcp)
{
	struct in_addr dnsip;
	
	if (!vcfg.enable || ipcp->pppd_type != PPPD_TYPE_PPTP_CLIENT)
		return 0;
	remove("/tmp/pptp_pppd.log");
	exec_cmd("ip route del default table %d", VPN_RTABLE);
	exec_cmd("iptables -t nat -F %s", VPN_CHAIN);
	exec_cmd("iptables -t nat -D PREROUTING -i %s -j %s", LAN_DEVNAME, VPN_CHAIN);
	exec_cmd("iptables -t nat -D POSTROUTING -o %s -j MASQUERADE", vinfo.ipcp.devname);
	if (action == NLK_ACTION_DEL) {
		vcfg.enable = 0;
		goto out;
	}
	
	if (ipcp->dns[0].s_addr)
		dnsip = ipcp->dns[0];
	else if (ipcp->dns[1].s_addr)
		dnsip = ipcp->dns[1];
	else
		goto out;
	vcfg.enable = 1;
	exec_cmd("iptables -t nat -A "
				"PREROUTING -i %s -j %s", 
				LAN_DEVNAME, VPN_CHAIN);
	exec_cmd("iptables -t nat -A "
				"POSTROUTING -o %s -j "
				"MASQUERADE", ipcp->devname);
	exec_cmd("iptables -t nat -A %s "
				"-p udp --dport 53 "
				"-m mark --mark %d "
				"-j DNAT --to "
				"%u.%u.%u.%u:53",
				VPN_CHAIN, VPN_MARK,
				NIPQUAD(dnsip.s_addr));
	exec_cmd("ip route add default "
				"via %u.%u.%u.%u "
				"dev %s table %d",
				NIPQUAD(ipcp->gw.s_addr),
				ipcp->devname, VPN_RTABLE);
out:
	vinfo.ipcp = *ipcp;
	vpn_set_param();
	return 0;
}

static int vpn_param_set(struct vpn_info *info)
{
	char cmd[128];

	if (!vinfo.vpn_enable)
		return 0;
	arr_strcpy(vinfo.user, info->user);
	arr_strcpy(vinfo.password, info->password);
	snprintf(cmd, sizeof(cmd), "%s.login.user=%s", VPN_CFG_FILE, vinfo.user);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.login.password=%s", VPN_CFG_FILE, vinfo.password);
	uuci_set(cmd);
	return 0;
}

static int vpn_callid_set(struct vpn_info *info)
{
	arr_strcpy(vinfo.callid, info->callid);
	return 0;
}

static int vpn_init_config(void)
{	
	struct uci_option *option;
	struct uci_element *se, *oe;
	struct uci_section *section;
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;

	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, VPN_CFG_FILE, &pkg)) {
		uci_free_context(ctx);
		return -1;
	}
	uci_foreach_element(&pkg->sections, se) {
		section = uci_to_section(se);
		if (section == NULL)
			continue;
		if (strcmp(section->type, "pptp") || strcmp(section->e.name, "login"))
			continue;
		uci_foreach_element(&section->options, oe) {
			option = uci_to_option(oe);
			if (option == NULL || option->type != UCI_TYPE_STRING)
				continue;
			if (!strcmp(oe->name, "user"))
				arr_strcpy(vinfo.user, option->v.string);
			else if (!strcmp(oe->name, "password"))
				arr_strcpy(vinfo.password, option->v.string);
		}		
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx);
	return 0;
}

static int vpn_init(void)
{
	if (access(VPN_INIT_FILE, F_OK))
		return 0;
	vinfo.vpn_enable = 1;
	if (vpn_set_server() < 0)
		return -1;
	vpn_init_config();
	vpn_dns_parser();
	exec_cmd("iptables -t nat -N %s", VPN_CHAIN);
	exec_cmd("ip rule add prio 32800 table %d", VPN_RTABLE);
	exec_cmd("ip rule add prio 100 fwmark %d table %d", VPN_MARK, VPN_RTABLE);
	exec_cmd("iptables -t mangle -A OUTPUT -d 26.0.0.0/32 -j MARK --set-mark %d", VPN_MARK);
	vpn_dns_cb(NULL);
	return 0;
}

int vpn_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;
	struct igd_dns *dns;

	switch (msgid) {
	case VPN_MOD_INIT:
		ret = vpn_init();
		break;
	case VPN_MOD_SET_SERVER:
		ret = vpn_set_server();
		break;
	case VPN_MOD_SET_DNSLIST:
		ret = vpn_dns_parser();
		break;
	case VPN_MOD_DNS_REPLEY:
		if (!data || len != sizeof(struct igd_dns))
			return -1;
		dns = (struct igd_dns *)data;
		if (dns->nr <= 0 || dns->nr > DNS_IP_MX)
			return -1;
		memcpy(vcfg.sip, dns->addr, sizeof(vcfg.sip));
		ret = vpn_set_param();
		break;
	case VPN_MOD_PPPD_IPUP:
		if (!data || len != sizeof(struct sys_msg_ipcp))
			return -1;
		ret = vpn_ipcp(NLK_ACTION_ADD, (struct sys_msg_ipcp *)data);
		break;
	case VPN_MOD_PPPD_IPDOWN:
		if (!data || len != sizeof(struct sys_msg_ipcp))
			return -1;
		ret = vpn_ipcp(NLK_ACTION_DEL, (struct sys_msg_ipcp *)data);
		break;
	case VPN_MOD_PARAM_SET:
		if (!data || len != sizeof(struct vpn_info))
			return -1;
		ret = vpn_param_set((struct vpn_info *)data);
		break;
	case VPN_MOD_STATUS_DUMP:
		if (!rbuf || rlen != sizeof(struct vpn_info))
			return -1;
		*(struct vpn_info *)rbuf = vinfo;
		ret = 0;
		break;
	case VPN_MOD_SET_CALLID:
		if (!data || len != sizeof(struct vpn_info))
			return -1;
		ret = vpn_callid_set((struct vpn_info *)data);
		break; 
	default:
		break;
	}
	return ret;
}
