#include "uci.h"
#include "igd_lib.h"
#include "igd_dnsmasq.h"
#include "igd_wifi.h"
#include "igd_url_safe.h"
#include "igd_interface.h"
#include "igd_upnp.h"
#include "igd_nat.h"
#include "igd_qos.h"
#include "igd_ali.h"
#include "igd_plug.h"
#include "igd_log.h"
#include "igd_system.h"
#include "igd_tunnel.h"

static int apple_dns;
static int wanpid = 4;
static int urlid;
static int dnsid[2];
static int ifstats;
static int ispstat;
static int detect;
static int online;
static char domain[IGD_DNS_LEN];
static struct account_info account;
static struct schedule_entry *lan_event;
static struct schedule_entry *net_event;
static struct schedule_entry *pppoe_event;
static struct iface_hander static_hander;
static struct iface_hander dhcp_hander;
static struct iface_hander pppoe_hander;
static struct iface igd_iface[NF_PORT_NUM_MX];

static int interface_param_save(struct iface_conf *config);
static int interface_mac_set(struct iface *ifc, unsigned char *mac);
extern int get_sysflag(unsigned char bit);

//static 
struct iface *interface_get(int uid)
{
	return &igd_iface[uid];
}

static int set_kernel_bridge(int action)
{
	struct nlk_msg_comm nlk;
	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_BRIDGE_MODE;
	nlk.action = action;
	return nlk_head_send(NLK_MSG_SET_BRIDGE, &nlk, sizeof(nlk));
}

static int add_drop_dnsgrp(void)
{
	if (apple_dns > 0)
		return 0;
	char drop_dns[IGD_DNS_GRP_PER_MX][IGD_DNS_LEN];
	memset(drop_dns, 0x0, sizeof(drop_dns));
	arr_strcpy(drop_dns[0], "apple.com");
	arr_strcpy(drop_dns[1], "itools.com");
	arr_strcpy(drop_dns[2], "itools.info");
	arr_strcpy(drop_dns[3], "appleiphonecell.com");
	arr_strcpy(drop_dns[4], "thinkdifferent.us");
	arr_strcpy(drop_dns[5], "airport.us");
	arr_strcpy(drop_dns[6], "ibook.info");
	apple_dns = add_dns_grp("apple", 7, drop_dns);
	if (apple_dns > 0)
		return 0;
	return -1;
}

static int add_url_redirect(void)
{
	struct inet_host host;
	struct http_rule_api rule;
	
	memset(&host, 0, sizeof(host));
	host.type = INET_HOST_NONE;
	memset(&rule, 0, sizeof(rule));
	rule.l3.type = INET_L3_NONE;
	rule.mid = URL_MID_REDIRECT;
	arr_strcpy(rule.rd.url, "http://");
	snprintf(rule.rd.url + 7, sizeof(rule.rd.url) - 7, "%s", domain);
	return register_http_rule(&host, &rule);
}

/*sometimes do not need redirect*/
static int add_dns_drop(void)
{
	struct inet_host host;
	struct dns_rule_api arg;

	if (add_drop_dnsgrp())
		return -1;
	memset(&host, 0, sizeof(host));
	host.type = INET_HOST_NONE;
	memset(&arg, 0, sizeof(arg));
	arg.target = NF_TARGET_DENY;
	arg.l3.type = INET_L3_DNSID;
	arg.l3.gid = apple_dns;
	return register_dns_rule(&host, &arg);
}

static int add_dns_redirect(void)
{
	struct inet_host host;
	struct dns_rule_api arg;
	
	memset(&host, 0, sizeof(host));
	host.type = INET_HOST_NONE;
	memset(&arg, 0, sizeof(arg));
	arg.target = NF_TARGET_REDIRECT;
	arg.l3.type = INET_L3_NONE;
	arg.type = 0x1;
	arg.ip = (unsigned int)inet_addr("1.127.127.254");
	return register_dns_rule(&host, &arg);
}

#if 0
static int add_ap2_web_deny(void)
{
	struct inet_host host;
	struct net_rule_api arg;
	
	memset(&host, 0, sizeof(host));
	host.type = INET_HOST_NONE;
	memset(&arg, 0, sizeof(arg));
	arg.target = NF_TARGET_DENY;
	arg.l3.type = INET_L3_NONE;
	arg.l4.type = INET_L4_DST;
	arg.l4.dst.start = 80;
	arg.l4.dst.start = 81;
	return register_local_rule(&host, &arg);
}
#endif

static int apple_dns_action(int action)
{
	switch (action) {
	case NLK_ACTION_ADD:
		if (dnsid[1] <= 0)
			dnsid[1] = add_dns_drop();
		break;
	case NLK_ACTION_DEL:
		if (dnsid[1] > 0) {
			if (!unregister_dns_rule(dnsid[1]))
				dnsid[1] = 0;
		}
		break;
	default:
		break;
	}
	return 0;
}

static int redirect_rule_action(int action)
{
	switch(action) {
	case NLK_ACTION_ADD:
		if (urlid <= 0)
			urlid = add_url_redirect();
		if (dnsid[0] <= 0)
			dnsid[0] = add_dns_redirect();
		break;
	case NLK_ACTION_DEL:
		if (urlid > 0) {
			if (!unregister_http_rule(URL_MID_REDIRECT, urlid))
				urlid = 0;
		}
		if (dnsid[0] > 0) {
			if (!unregister_dns_rule(dnsid[0]))
				dnsid[0] = 0;
		}
		break;
	default:
		break;
	}
	return 0;
}

static void ifstats_cb(void *data)
{	
	int cur = 0;
	struct iface_conf config;
	unsigned long bitmap;
	struct iface *ifc = NULL;
	struct timeval tv;
	int reset = 0;
	
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (!schedule(tv, ifstats_cb, NULL)) {
		console_printf("schedule ifstats event error\n");
		return;
	}
	if (get_link_status(&bitmap))
		return;
	if ((bitmap & (0x1 << wanpid)))
		cur = 1;
	ifc = interface_get(1);
	/*only last 0 cur is 1 need restart*/
	if (ifc->config.mode != MODE_DHCP)
		goto out;
	if (!cur || ifstats)
		goto out;
	ifc->hander->if_down(ifc);
	memcpy(&config, &ifc->config, sizeof(config));
	ifc->hander->if_up(ifc, &config);
out:
	if (ifstats && !cur)
		ADD_SYS_LOG("WAN口未插入网线");
	else if (!ifstats && cur)
		ADD_SYS_LOG("WAN口已启用");
	ifstats = cur;
#ifdef ALI_REQUIRE_SWITCH
	if (mu_call(ALI_MOD_GET_RESET, NULL, 0, NULL, 0)) {
		redirect_rule_action(NLK_ACTION_ADD);
		reset = 1;
	} else {
		redirect_rule_action(NLK_ACTION_DEL);
		reset = 0;
	}
#endif	
	if (!igd_test_bit(IF_STATUS_IP_UP, &ifc->status)
		|| (!ifstats && ifc->config.mode != MODE_WISP
		&& ifc->config.mode != MODE_3G)
		|| (online == NET_OFFLINE) || reset)
		apple_dns_action(NLK_ACTION_ADD);
	else
		apple_dns_action(NLK_ACTION_DEL);
	return;	
}

static void net_status_cb(void *data)
{
	net_event = NULL;
	online = NET_OFFLINE;
}

static int detect_net_status_action(int status)
{
	if (net_event) {
		dschedule(net_event);
		net_event = NULL;
	}
	switch(detect) {
	case DETECT_OFF:
		break;
	case DETECT_ON:
		if (online == NET_OFFLINE && status == NET_ONLINE)
			ADD_SYS_LOG("检测上网成功");
		else if (online == NET_ONLINE && status == NET_OFFLINE)
			ADD_SYS_LOG("检测上网失败");
		online = status;
		if (online == NET_ONLINE)
			detect = DETECT_OFF;
		break;
	default:
		break;
	}
	return 0;
}

static int nlk_net_status_action(int action)
{
	struct timeval tv;
	
	switch (action) {
	case NLK_ACTION_DEL:
		if (online == NET_OFFLINE || detect == DETECT_ON)
			break;
		detect = DETECT_ON;
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		net_event = schedule(tv, net_status_cb, NULL);
		exec_cmd("killall -kill %s", NETDETECT);
		exec_cmd("%s %s %u %s &", NETDETECT, IPC_PATH_MU, IF_MOD_DETECT_REPLY, TSPEED_CONFIG_FILE);
		break;
	default:
		return -1;
	}
	return 0;
}

static void pppoe_cb(void *data)
{
	pppoe_event = NULL;
	exec_cmd("killall -kill pppoe-server");
}

static int interface_get_account(int option, struct account_info *info)
{
	char cmd[128];
	struct iface *ifc;
	
	switch (option) {
	case 0:
		memset(&account, 0x0, sizeof(account));
		if (pppoe_event) {
			dschedule(pppoe_event);
			pppoe_cb(NULL);
		}
		struct timeval tv;
		
		tv.tv_sec = 300;
		tv.tv_usec = 0;
		pppoe_event = schedule(tv, pppoe_cb, NULL);
		if (NULL == pppoe_event)
			return -1;
		return exec_cmd(PPPOE_SERVER_CMD);
	case 1:
		if (!strlen(account.user) || !strlen(account.passwd))
			return -1;
		ifc = interface_get(1);
		interface_mac_set(ifc, account.peermac);
		memcpy(ifc->config.mac, account.peermac, ETH_ALEN);
		snprintf(cmd, sizeof(cmd), "%s.%s.macaddr=%02x:%02x:%02x:%02x:%02x:%02x",
					IF_CONFIG_FILE, ifc->uiname, MAC_SPLIT(account.peermac));
		uuci_set(cmd);
		memcpy(info, &account, sizeof(*info));
		break;
	default:
		break;
	}
	return 0;
}

static int interface_mode_detect(struct detect_info *info)
{
	FILE *fp = NULL;
	char line[128] = {0};
	char key[16] = {0};
	char value[16] = {0};
	unsigned long bitmap;
	
	if (get_link_status(&bitmap) < 0)
		return -1;
	memset(info, 0x0, sizeof(*info));
	if (bitmap & (0x1 << wanpid))
		info->link = 1;
	else
		return 0;
	if (online == NET_ONLINE 
#ifdef ALI_REQUIRE_SWITCH
		&& !mu_call(ALI_MOD_GET_RESET, NULL, 0, NULL, 0)
#endif
		) {
		info->net = NET_ONLINE;
		return 0;
	}
	if (access(PPPOE_DETECT_PID, F_OK))
		exec_cmd(PPPOE_DETECT_CMD);
	fp = fopen(PPPOE_MODE_FILE, "r");
	if (NULL == fp)
		return 0;
	if (!fgets(line, 127, fp))
		goto out;
	if (sscanf(line, "%s %s", key, value) != 2)
		goto out;
	info->mode = atoi(value);
out:
	fclose(fp);
	remove(PPPOE_DETECT_PID);
	remove(PPPOE_MODE_FILE);
	return 0;
}

static void interface_lan_cb(void *data)
{
	int i;

	for (i = 0; i < PORT_MX; i++) {
		if ((i == wanpid) && ifstats)
			continue;
		set_port_link(i, NLK_ACTION_ADD);
	}
	lan_event = NULL;
}

static void interface_lan_restart(void)
{
	int i;
	struct timeval tv;
	struct iface *ifc;

	if (lan_event)
		return;
	tv.tv_sec = 6;
	tv.tv_usec = 0;
	lan_event = schedule(tv, interface_lan_cb, NULL);
	if (!lan_event)
		return;
	ifc = interface_get(1);
	for (i = 0; i < PORT_MX; i++) {
		if ((i == wanpid) && (ifc->config.mode != MODE_WISP)
			&& (ifc->config.mode != MODE_3G))
			continue;
		set_port_link(i, NLK_ACTION_DEL);
	}
}

static int interface_lan_check(struct sys_msg_ipcp *wan)
{
	char start[16];
	char end[16];
	char lanip[16];
	struct iface *ifc = NULL;
	struct iface_conf if_conf;
	struct dhcp_conf config;

	ifc = interface_get(0);
	if ((ntohl(ifc->ipcp.ip.s_addr) & ntohl(wan->mask.s_addr)) 
		== (ntohl(wan->ip.s_addr) & ntohl(wan->mask.s_addr))) {
		if (!strncmp(inet_ntoa(ifc->ipcp.ip), "10.9.9.1", 7)) {
			arr_strcpy(lanip, "10.10.10.1");
			arr_strcpy(start, "10.10.10.100");
			arr_strcpy(end, "10.10.10.150");
		} else {
			arr_strcpy(lanip, "10.9.9.1");
			arr_strcpy(start, "10.9.9.100");
			arr_strcpy(end, "10.9.9.150");
		}
	} else if (((ntohl(ifc->ipcp.ip.s_addr) & ntohl(wan->mask.s_addr)) 
			== (ntohl(wan->dns[0].s_addr) & ntohl(wan->mask.s_addr))) ||
			((ntohl(ifc->ipcp.ip.s_addr) & ntohl(wan->mask.s_addr)) 
			== (ntohl(wan->dns[1].s_addr) & ntohl(wan->mask.s_addr)))) {
		if (!strncmp(inet_ntoa(ifc->ipcp.ip), "10.8.8.1", 7)) {
			arr_strcpy(lanip, "10.10.10.1");
			arr_strcpy(start, "10.10.10.100");
			arr_strcpy(end, "10.10.10.150");
		} else {
			arr_strcpy(lanip, "10.8.8.1");
			arr_strcpy(start, "10.8.8.100");
			arr_strcpy(end, "10.8.8.150");
		}
	} else
		return 0;
	memcpy(&if_conf, &ifc->config, sizeof(if_conf));
	inet_aton(lanip, &if_conf.statip.ip);
	inet_aton("255.255.255.0", &if_conf.statip.mask);
	ifc->hander->if_down(ifc);
	ifc->hander->if_up(ifc, &if_conf);
	mu_call(DNSMASQ_DHCP_SHOW, NULL, 0, &config, sizeof(config));
	inet_aton(lanip, &config.ip);
	inet_aton(start, &config.start);
	inet_aton(end, &config.end);
	inet_aton("255.255.255.0", &config.mask);
	mu_call(DNSMASQ_DHCP_SET, &config, sizeof(config), NULL, 0);
	return 0;
}

static int  interface_broadcast(int mid, int action, struct sys_msg_ipcp *ipcp)
{
	struct nlk_sys_msg nlk;
	
	memset(&nlk, 0x0, sizeof(nlk));
	memcpy(&nlk.msg.ipcp, ipcp, sizeof(struct sys_msg_ipcp));
	nlk.comm.gid = NLKMSG_GRP_IF;
	nlk.comm.mid = mid;
	nlk.comm.action = action;
	nlk_event_send(NLKMSG_GRP_IF, &nlk, sizeof(nlk));
	return 0;
}

static int interface_get_error(struct iface_info *info)
{
	if (!info->ip.s_addr)
		info->err = ispstat;
	else
		info->err = ISP_STAT_IPCP_FINISH;
	ispstat = ISP_STAT_INIT;
	return 0;
}

static int interface_speed_set(struct iface *ifc, unsigned short speed)
{
	if (!ifc->uid)
		return 0;
	if (ifc->config.speed == speed)
		return 0;
	return set_port_speed(wanpid, speed);
}

static int interface_mac_set(struct iface *ifc, unsigned char *mac)
{
	char buf[64] = {0};

	if (ifc->config.isbridge)
		return 0;
	sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(mac));
	return exec_cmd("ifconfig %s hw ether %s", ifc->devname, buf);
}

static int interface_mtu_set(struct iface *ifc, int mtu)
{
	if (ifc->config.isbridge)
		return 0;
	return exec_cmd("ifconfig %s mtu %d", ifc->devname, mtu);
}

static int interface_ip_set(struct sys_msg_ipcp *ipcp)
{
	return exec_cmd("ifconfig %s %u.%u.%u.%u netmask %u.%u.%u.%u", ipcp->devname,
					NIPQUAD(ipcp->ip.s_addr), NIPQUAD(ipcp->mask.s_addr));
}

static int interface_route_set(struct sys_msg_ipcp *ipcp)
{	
	exec_cmd("ip route del default");
	return exec_cmd("ip route add default via %u.%u.%u.%u dev %s",
					NIPQUAD(ipcp->gw.s_addr), ipcp->devname);
}

static int interface_dns_set(struct sys_msg_ipcp *ipcp)
{
	FILE *fp = NULL;

	fp = fopen(WAN_DNS_FILE, "w");
	if (NULL == fp)
		return -1;
	if (!ipcp->dns[0].s_addr)
		inet_aton("223.5.5.5", &ipcp->dns[0]);
	if (!ipcp->dns[1].s_addr)
		inet_aton("223.6.6.6", &ipcp->dns[1]);
	fprintf(fp, "nameserver %u.%u.%u.%u\n"
				"nameserver %u.%u.%u.%u\n",
				NIPQUAD(ipcp->dns[0].s_addr),
				NIPQUAD(ipcp->dns[1].s_addr));
	fclose(fp);
	return 0;
}

static int interface_sec_set(struct sys_msg_ipcp *ipcp)
{
	exec_cmd("iptables -t filter -F");
	exec_cmd("iptables -A INPUT -i %s -p udp --dport 68 -j ACCEPT", ipcp->devname);
	//exec_cmd("iptables -A INPUT -i %s -p tcp --dport 2323 -j ACCEPT", ipcp->devname);
	exec_cmd("iptables -A INPUT -i %s -p icmp -j ACCEPT", ipcp->devname);
	exec_cmd("iptables -A INPUT -i %s -m state --state NEW -j DROP", ipcp->devname);
	return 0;
}

static int interface_nat_set(struct sys_msg_ipcp *ipcp)
{
	exec_cmd("iptables -t nat -F");
	return exec_cmd("iptables -t nat -A POSTROUTING -o %s -j MASQUERADE", ipcp->devname);
}

static int interface_qos_set(void)
{
	return exec_cmd("qos-start.sh restart");
}

static int interface_bridge_set(struct sys_msg_ipcp *ipcp)
{
	struct dhcp_conf dhcp;
	
	mu_call(DNSMASQ_DHCP_SHOW, NULL, 0, &dhcp, sizeof(dhcp));
	dhcp.ip.s_addr = ipcp->ip.s_addr;
	mu_call(DNSMASQ_DHCP_SET, &dhcp, sizeof(dhcp), NULL, 0);
	return 0;
}

static int interface_dump(struct iface_info *info)
{
	unsigned long bitmap;
	struct iface *ifc = NULL;

	if (info->uid >= NF_PORT_NUM_MX)
		return 0;
	ifc = interface_get(info->uid);
	info->type = ifc->type;
	info->uid = ifc->uid;
	info->mode = ifc->config.mode;
	info->ip = ifc->ipcp.ip;
	info->mask = ifc->ipcp.mask;
	info->gw = ifc->ipcp.gw;
	info->dns[0] = ifc->ipcp.dns[0];
	info->dns[1] = ifc->ipcp.dns[1];
	memcpy(info->mac, ifc->config.mac, ETH_ALEN);
	arr_strcpy(info->uiname, ifc->uiname);
	arr_strcpy(info->devname, ifc->ipcp.devname);
	if (get_link_status(&bitmap) < 0)
		return -1;
	if (bitmap & (0x1 << wanpid))
		info->link = 1;
	if (!igd_test_bit(IF_STATUS_IP_UP, &ifc->status))
		info->net = NET_OFFLINE;
	else if ((info->mode != MODE_WISP) && (info->mode != MODE_3G)
			&& (info->link == 0)) {
		info->net = NET_OFFLINE;
#ifdef ALI_REQUIRE_SWITCH
	} else if (mu_call(ALI_MOD_GET_RESET, NULL, 0, NULL, 0)) {
		info->net = NET_OFFLINE;
#endif
	} else {
		info->net = online;
	}
	interface_get_error(info);
	return 0;
}

static int interface_generic_ip_up(struct iface *ifc, struct sys_msg_ipcp *ipcp)
{
	if (igd_test_bit(IF_STATUS_IP_UP, &ifc->status))
		return 0;
	if (interface_ip_set(ipcp))
		return -1;
	if (ifc->type == IF_TYPE_LAN)
		goto out;
	if (interface_dns_set(ipcp))
		return -1;
	if (!ifc->config.isbridge) {
		if (interface_nat_set(ipcp))
			return -1;
	}
	if (interface_route_set(ipcp))
		return -1;
	if (ifc->config.isbridge)
		goto out;
	if (interface_lan_check(ipcp))
		return -1;
	if (interface_sec_set(ipcp))
		return -1;
	if (interface_qos_set())
		return -1;
out:
	ifc->ipcp = *ipcp;
	if (ifc->type == IF_TYPE_WAN) {
		if (!ifc->config.isbridge) {
			mu_call(NAT_MOD_RESTART, NULL, 0, NULL, 0);
			mu_call(UPNPD_SERVER_SET, NULL, 0, NULL, 0);
			mu_call(TUNNEL_MOD_RESET, NULL, 0, NULL, 0);
			mu_call(DNSMASQ_DDNS_RESTART, NULL, 0, NULL, 0);
		}
		exec_cmd("killall -kill %s", NETDETECT);
		exec_cmd("%s %s %u %s &", NETDETECT, IPC_PATH_MU, IF_MOD_DETECT_REPLY, TSPEED_CONFIG_FILE);
		detect = DETECT_ON;
		switch (ifc->config.mode) {
		case MODE_DHCP:
			ADD_SYS_LOG("DHCP获取IP成功");
			break;
		case MODE_PPPOE:
			ADD_SYS_LOG("pppoe拨号成功");
			mu_call(SYSTEM_MOD_PPPOE_SUCC, NULL, 0, NULL, 0);
			break;
		case MODE_STATIC:
			ADD_SYS_LOG("设置上网IP地址成功");
			break;
		case MODE_WISP:
			ADD_SYS_LOG("中继成功");
			break;
		case MODE_3G:
			ADD_SYS_LOG("3G拨号成功");
			break;
		default:
			break;
		}
		if (ifc->config.isbridge) {
			interface_bridge_set(ipcp);
			interface_lan_restart();
			mu_call(URL_SAFE_MOD_SET_LANIP, ipcp, sizeof(struct sys_msg_ipcp), NULL, 0);
		}
	} else {
		interface_lan_restart();
		mu_call(URL_SAFE_MOD_SET_LANIP, ipcp, sizeof(struct sys_msg_ipcp), NULL, 0);
	}
	igd_clear_bit(IF_STATUS_IP_DOWN, &ifc->status);
	igd_set_bit(IF_STATUS_IP_UP, &ifc->status);
	return interface_broadcast(IF_GRP_MID_IPCGE, NLK_ACTION_ADD, ipcp);
}

static int interface_generic_ip_down(struct iface *ifc, struct sys_msg_ipcp *ipcp)
{
	if (igd_test_bit(IF_STATUS_IP_DOWN, &ifc->status))
		return 0;
	if (ifc->type == IF_TYPE_WAN) {
		online = NET_OFFLINE;
		detect = DETECT_OFF;
		exec_cmd("killall -kill %s", NETDETECT);
	}
	interface_ip_set(ipcp);
	ifc->ipcp = *ipcp;
	igd_clear_bit(IF_STATUS_IP_UP, &ifc->status);
	igd_set_bit(IF_STATUS_IP_DOWN, &ifc->status);
	return interface_broadcast(IF_GRP_MID_IPCGE, NLK_ACTION_DEL, ipcp);
}

static int interface_dhcp_up(struct iface *ifc, struct iface_conf *config)
{	
	if (interface_mac_set(ifc, config->mac))
		return -1;
	if (interface_mtu_set(ifc, config->dhcp.mtu))
		return -1;
	ADD_SYS_LOG("DHCP开始");
	set_dev_uid(WAN1_DEVNAME, 1);
	return exec_cmd("udhcpc -p /var/run/udhcpc.pid -f -t 0 -i %s &", ifc->devname);
}

static int interface_dhcp_down(struct iface *ifc)
{
	struct sys_msg_ipcp ipcp;

	exec_cmd("killall -kill udhcpc");
	memset(&ipcp, 0x0, sizeof(ipcp));
	ipcp.type = ifc->type;
	ipcp.uid = ifc->uid;
	arr_strcpy(ipcp.devname, ifc->devname);
	return ifc->hander->ip_down(ifc, &ipcp);
}

static struct iface_hander dhcp_hander = {
	.if_up = interface_dhcp_up,
	.if_down = interface_dhcp_down,
	.ip_up = interface_generic_ip_up,
	.ip_down = interface_generic_ip_down,
};

static int interface_pppoe_up(struct iface *ifc, struct iface_conf *config)
{
	char cmd[1024];
	char pppoetag[64];
	
	if (interface_mac_set(ifc, config->mac))
		return -1;
	if (interface_mtu_set(ifc, 1500))
		return -1;
	set_dev_uid(WAN1_DEVNAME, 1);
	memset(cmd, 0x0, sizeof(cmd));
	memset(pppoetag, 0x0, sizeof(pppoetag));
	if (strlen(config->pppoe.pppoe_server) > 0)
		snprintf(pppoetag, sizeof(pppoetag), "rp_pppoe_service %s ", config->pppoe.pppoe_server);
	sprintf(cmd,
		"pppd file "
		"/etc/ppp/options.pppoe "
		"nodetach "
		"ipparam wan "
		"ifname pppoe-wan "
		"nodefaultroute "
		"usepeerdns persist "
		"user %s "
		"password %s "
		"mtu %d "
		"mru %d "
		"plugin rp-pppoe.so "
		"%s"
		"nic-%s "
		"pppd_type %d "
		"holdoff 20 &",
		config->pppoe.user,
		config->pppoe.passwd,
		config->pppoe.comm.mtu,
		config->pppoe.comm.mtu,
		pppoetag,
		ifc->devname,
		PPPD_TYPE_PPPOE_CLIENT);
	ADD_SYS_LOG("pppoe拨号开始");
	return exec_cmd(cmd); 
}

static int interface_pppoe_down(struct iface *ifc)
{
	struct sys_msg_ipcp ipcp;
	exec_cmd("killall pppd");
	sleep(2);
	memset(&ipcp, 0x0, sizeof(ipcp));
	ipcp.type = ifc->type;
	ipcp.uid = ifc->uid;
	arr_strcpy(ipcp.devname, ifc->devname);
	return ifc->hander->ip_down(ifc, &ipcp);
}

static struct iface_hander pppoe_hander = {
	.if_up = interface_pppoe_up,
	.if_down = interface_pppoe_down,
	.ip_up = interface_generic_ip_up,
	.ip_down = interface_generic_ip_down,
};

static int interface_static_up(struct iface *ifc, struct iface_conf *config)
{
	struct sys_msg_ipcp ipcp;
	
	if (interface_mac_set(ifc, config->mac))
		return -1;
	if (interface_mtu_set(ifc, config->statip.comm.mtu))
		return -1;
	set_dev_uid(WAN1_DEVNAME, 1);
	memset(&ipcp, 0x0, sizeof(ipcp));
	ipcp.type = ifc->type;
	ipcp.uid = ifc->uid;
	ipcp.ip.s_addr = config->statip.ip.s_addr;
	ipcp.mask.s_addr = config->statip.mask.s_addr;
	ipcp.gw.s_addr = config->statip.gw.s_addr;
	ipcp.dns[0].s_addr = config->statip.comm.dns[0].s_addr;
	ipcp.dns[1].s_addr = config->statip.comm.dns[1].s_addr;
	if (!ipcp.dns[0].s_addr)
		ipcp.dns[0].s_addr = config->statip.gw.s_addr;
	arr_strcpy(ipcp.devname, ifc->devname);
	if (ifc->type == IF_TYPE_WAN)
		ADD_SYS_LOG("静态IP开始");
	ifc->config.mode = MODE_STATIC;
	return ifc->hander->ip_up(ifc, &ipcp);
}

static int interface_static_down(struct iface *ifc)
{
	struct sys_msg_ipcp ipcp;
	memset(&ipcp, 0x0, sizeof(ipcp));
	ipcp.type = ifc->type;
	ipcp.uid = ifc->uid;
	arr_strcpy(ipcp.devname, ifc->devname);
	return ifc->hander->ip_down(ifc, &ipcp);
}

static struct iface_hander static_hander = {
	.if_up = interface_static_up,
	.if_down = interface_static_down,
	.ip_up = interface_generic_ip_up,
	.ip_down = interface_generic_ip_down,
};

static int interface_wisp_up(struct iface *ifc, struct iface_conf *config)
{
	char cmd[512];
	struct iface *lanif = NULL;
	
	exec_cmd("killall -kill wisp");
	set_dev_uid(WAN1_DEVNAME, 0);
	lanif = interface_get(0);
	interface_mac_set(ifc, lanif->config.mac);
	exec_cmd("brctl addif %s %s", LAN_DEVNAME, WAN1_DEVNAME);
	ADD_SYS_LOG("无线中继开始 ssid %s", config->wisp.ssid);
	snprintf(cmd, sizeof(cmd) -1, "wisp -s \"%s\" -e %s -k \"%s\" -c %d &",
					config->wisp.ssid, config->wisp.security,
					config->wisp.key, config->wisp.channel);
	return system(cmd);
}

static int interface_wisp_down(struct iface *ifc)
{
	struct sys_msg_ipcp ipcp;

	exec_cmd("killall -kill wisp");
	exec_cmd("killall -kill udhcpc");
	exec_cmd("wisp down &");
	exec_cmd("brctl delif %s %s", LAN_DEVNAME, WAN1_DEVNAME);
	memset(&ipcp, 0x0, sizeof(ipcp));
	ipcp.type = ifc->type;
	ipcp.uid = ifc->uid;
	arr_strcpy(ipcp.devname, ifc->ipcp.devname);
	return ifc->hander->ip_down(ifc, &ipcp);
}

static struct iface_hander wisp_hander = {
	.if_up = interface_wisp_up,
	.if_down = interface_wisp_down,
	.ip_up = interface_generic_ip_up,
	.ip_down = interface_generic_ip_down,
};

static int interface_3g_up(struct iface *ifc, struct iface_conf *config)
{
	struct iface *lanif = NULL;
	
	ADD_SYS_LOG("3G拨号开始");
	set_dev_uid(WAN1_DEVNAME, 0);
	lanif = interface_get(0);
	interface_mac_set(ifc, lanif->config.mac);
	exec_cmd("brctl addif %s %s", LAN_DEVNAME, WAN1_DEVNAME);
	exec_cmd("w3g &");
	return 0;
}

static int interface_3g_down(struct iface *ifc)
{
	struct sys_msg_ipcp ipcp;
	
	exec_cmd("killall -kill w3g");
	exec_cmd("killall pppd");
	exec_cmd("killall chat");
	sleep(2);
	exec_cmd("killall -kill pppd");
	exec_cmd("killall -kill chat");
	memset(&ipcp, 0x0, sizeof(ipcp));
	ipcp.type = ifc->type;
	ipcp.uid = ifc->uid;
	arr_strcpy(ipcp.devname, ifc->devname);
	exec_cmd("brctl delif %s %s", LAN_DEVNAME, WAN1_DEVNAME);
	return ifc->hander->ip_down(ifc, &ipcp);
}

static struct iface_hander g3_hander = {
	.if_up = interface_3g_up,
	.if_down = interface_3g_down,
	.ip_up = interface_generic_ip_up,
	.ip_down = interface_generic_ip_down,
};

static int interface_wan_bridge(void)
{
	char cmd[128];
	struct iface *ifc, *lanif;
	struct iface_conf *config;
	
	ifc = interface_get(1);
	config = &ifc->config;
	if (config->mode == MODE_PPPOE || config->mode == MODE_3G
		|| !igd_test_bit(IF_STATUS_IP_UP, &ifc->status)
		|| config->isbridge)
		return 0;
	if (ifc->hander->if_down(ifc))
		return -1;
	lanif = interface_get(0);
	interface_mac_set(ifc, lanif->config.mac);
	ispstat = ISP_STAT_INIT;
	exec_cmd("rmmod fp.ko");
	set_kernel_bridge(NLK_ACTION_ADD);
	exec_cmd("killall -kill miniupnpd");
	exec_cmd("iptables -F");
	exec_cmd("iptables -F");
	exec_cmd("iptables -t nat -F");
	exec_cmd("iptables -t mangle -F");
	exec_cmd("qos-start.sh stop");
	exec_cmd("brctl addif %s %s", LAN_DEVNAME, WAN1_DEVNAME);
	exec_cmd("brctl addif %s %s", LAN_DEVNAME, APCLI_DEVNAME);
	snprintf(cmd, sizeof(cmd), "echo \"1\" >/sys/class/net/%s/bridge/nf_call_iptables", LAN_DEVNAME);
	system(cmd);
	snprintf(cmd, sizeof(cmd), "echo \"1\" >/sys/class/net/%s/bridge/nf_call_arptables", LAN_DEVNAME);
	system(cmd);
	config->isbridge = 1;
	arr_strcpy(ifc->devname, LAN_DEVNAME);
	return ifc->hander->if_up(ifc, config);
}

static int interface_ipchange(struct sys_msg_ipcp *ipcp, int action)
{
	struct iface *ifc = NULL;
	struct iconf_comm *comm = NULL;

	if (ipcp->uid >= NF_PORT_NUM_MX)
		return -1;
	ifc = interface_get(ipcp->uid);
	switch (ifc->config.mode) {
	case MODE_DHCP:
		comm = &ifc->config.dhcp;
		break;
	case MODE_PPPOE:
		comm = &ifc->config.pppoe.comm;
		break;
	case MODE_WISP:
		break;
	case MODE_3G:
		break;
	default:
		return -1;
	}
	switch(action) {
	case NLK_ACTION_ADD:
		if (comm) {
			if (comm->dns[0].s_addr)
				ipcp->dns[0].s_addr = comm->dns[0].s_addr;
			if (comm->dns[1].s_addr)
				ipcp->dns[1].s_addr = comm->dns[1].s_addr;
		}
		return ifc->hander->ip_up(ifc, ipcp);
	case NLK_ACTION_DEL:
		return ifc->hander->ip_down(ifc, ipcp);
	default:
		break;
	}
	return 0;
}

static int interface_param_save(struct iface_conf *config)
{
	char cmd[128] = {0};
	struct iface *ifc = NULL;

	if (config->uid >= NF_PORT_NUM_MX)
		return -1;
	ifc = interface_get(config->uid);
	ifc->config = *config;
	snprintf(cmd, sizeof(cmd), "%s.%s", IF_CONFIG_FILE, ifc->uiname);
	uuci_delete(cmd);
	snprintf(cmd, sizeof(cmd), "%s.%s=interface", IF_CONFIG_FILE, ifc->uiname);
	uuci_set(cmd);
	switch(config->mode) {
	case MODE_DHCP:
		snprintf(cmd, sizeof(cmd), "%s.%s.proto=dhcp", IF_CONFIG_FILE, ifc->uiname);
		uuci_set(cmd);
		break;
	case MODE_PPPOE:
		snprintf(cmd, sizeof(cmd), "%s.%s.proto=pppoe", IF_CONFIG_FILE, ifc->uiname);
		uuci_set(cmd);
		break;
	case MODE_STATIC:
		snprintf(cmd, sizeof(cmd), "%s.%s.proto=static", IF_CONFIG_FILE, ifc->uiname);
		uuci_set(cmd);
		break;
	case MODE_WISP:
		snprintf(cmd, sizeof(cmd), "%s.%s.proto=wisp", IF_CONFIG_FILE, ifc->uiname);
		uuci_set(cmd);
		break;
	case MODE_3G:
		snprintf(cmd, sizeof(cmd), "%s.%s.proto=3G", IF_CONFIG_FILE, ifc->uiname);
		uuci_set(cmd);
		break;
	default:
		break;
	}
	snprintf(cmd, sizeof(cmd), "%s.%s.macaddr=%02x:%02x:%02x:%02x:%02x:%02x", IF_CONFIG_FILE, ifc->uiname, MAC_SPLIT(config->mac));
	uuci_set(cmd);
	
	/*save static*/
	if (config->statip.ip.s_addr) {
		snprintf(cmd, sizeof(cmd), "%s.%s.ipaddr=%u.%u.%u.%u", IF_CONFIG_FILE, ifc->uiname, NIPQUAD(config->statip.ip));
		uuci_set(cmd);
	}
	if (config->statip.mask.s_addr) {
		snprintf(cmd, sizeof(cmd), "%s.%s.netmask=%u.%u.%u.%u", IF_CONFIG_FILE, ifc->uiname, NIPQUAD(config->statip.mask));
		uuci_set(cmd);
	}
	if (ifc->type == IF_TYPE_LAN)
		return 0;
	if (config->statip.gw.s_addr) {
		snprintf(cmd, sizeof(cmd), "%s.%s.gateway=%u.%u.%u.%u", IF_CONFIG_FILE, ifc->uiname, NIPQUAD(config->statip.gw));
		uuci_set(cmd);
	}
	if (config->statip.comm.mtu > 0) {
		snprintf(cmd, sizeof(cmd), "%s.%s.mtu_static=%d", IF_CONFIG_FILE, ifc->uiname, config->statip.comm.mtu);
		uuci_set(cmd);
	}
	if (config->statip.comm.dns[0].s_addr) {
		snprintf(cmd, sizeof(cmd), "%s.%s.dns_static=%u.%u.%u.%u", IF_CONFIG_FILE, ifc->uiname, NIPQUAD(config->statip.comm.dns[0]));
		uuci_set(cmd);
	}
	if (config->statip.comm.dns[1].s_addr) {
		snprintf(cmd, sizeof(cmd), "%s.%s.dns1_static=%u.%u.%u.%u", IF_CONFIG_FILE, ifc->uiname, NIPQUAD(config->statip.comm.dns[1]));
		uuci_set(cmd);
	}
	
	/*save dhcp*/
	if (config->dhcp.mtu > 0) {
		snprintf(cmd, sizeof(cmd), "%s.%s.mtu_dhcp=%d", IF_CONFIG_FILE, ifc->uiname, config->dhcp.mtu);
		uuci_set(cmd);
	}
	if (config->dhcp.dns[0].s_addr) {
		snprintf(cmd, sizeof(cmd), "%s.%s.dns_dhcp=%u.%u.%u.%u", IF_CONFIG_FILE, ifc->uiname, NIPQUAD(config->dhcp.dns[0]));
		uuci_set(cmd);
	}
	if (config->dhcp.dns[1].s_addr) {
		snprintf(cmd, sizeof(cmd), "%s.%s.dns1_dhcp=%u.%u.%u.%u", IF_CONFIG_FILE, ifc->uiname, NIPQUAD(config->dhcp.dns[1]));
		uuci_set(cmd);
	}
	
	/*save pppoe*/
	if (config->pppoe.user[0]) {
		snprintf(cmd, sizeof(cmd), "%s.%s.username=%s", IF_CONFIG_FILE, ifc->uiname, config->pppoe.user);
		uuci_set(cmd);
	}
	if (config->pppoe.passwd[0]) {
		snprintf(cmd, sizeof(cmd), "%s.%s.password=%s", IF_CONFIG_FILE, ifc->uiname, config->pppoe.passwd);
		uuci_set(cmd);
	}
	if (config->pppoe.pppoe_server[0]) {
		snprintf(cmd, sizeof(cmd), "%s.%s.pppoe_server=%s", IF_CONFIG_FILE, ifc->uiname, config->pppoe.pppoe_server);
		uuci_set(cmd);
	}
	if (config->pppoe.comm.mtu > 0) {
		snprintf(cmd, sizeof(cmd), "%s.%s.mtu_pppoe=%d", IF_CONFIG_FILE, ifc->uiname, config->pppoe.comm.mtu);
		uuci_set(cmd);
	}
	if (config->pppoe.comm.dns[0].s_addr) {
		snprintf(cmd, sizeof(cmd), "%s.%s.dns_pppoe=%u.%u.%u.%u", IF_CONFIG_FILE, ifc->uiname, NIPQUAD(config->pppoe.comm.dns[0]));
		uuci_set(cmd);
	}
	if (config->pppoe.comm.dns[1].s_addr) {
		snprintf(cmd, sizeof(cmd), "%s.%s.dns1_pppoe=%u.%u.%u.%u", IF_CONFIG_FILE, ifc->uiname, NIPQUAD(config->pppoe.comm.dns[1]));
		uuci_set(cmd);
	}

	/*save wisp*/
	if (config->wisp.ssid[0]) {
		snprintf(cmd, sizeof(cmd), "%s.%s.ssid=%s", IF_CONFIG_FILE, ifc->uiname, config->wisp.ssid);
		uuci_set(cmd);
	}
	if (config->wisp.security[0]) {
		snprintf(cmd, sizeof(cmd), "%s.%s.security=%s", IF_CONFIG_FILE, ifc->uiname, config->wisp.security);
		uuci_set(cmd);
	}
	if (config->wisp.channel > 0) {
		snprintf(cmd, sizeof(cmd), "%s.%s.channel=%d", IF_CONFIG_FILE, ifc->uiname, config->wisp.channel);
		uuci_set(cmd);
	}
	if (config->wisp.key[0]) {
		snprintf(cmd, sizeof(cmd), "%s.%s.key=%s", IF_CONFIG_FILE, ifc->uiname, config->wisp.key);
		uuci_set(cmd);
	}
	snprintf(cmd, sizeof(cmd), "%s.%s.speed=%d", IF_CONFIG_FILE, ifc->uiname, config->speed);
	uuci_set(cmd);
	return 0;
}

static int interface_param_show(struct iface_conf *config)
{
	struct iface *ifc = NULL;

	if (config->uid >= NF_PORT_NUM_MX)
		return -1;
	ifc = interface_get(config->uid);
	*config = ifc->config;
	return 0;
}

void save_pppoe_user(char *username, char *password)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "pppoe_username=%s", username);
	mtd_set_val(buf);
	snprintf(buf, sizeof(buf), "pppoe_passwd=%s", password);
	mtd_set_val(buf);
}

static int interface_param_set(struct iface_conf *config)
{
	struct iface *ifc = NULL;

	if (config->uid >= NF_PORT_NUM_MX)
		return -1;
	ifc = interface_get(config->uid);
	if (ifc->hander->if_down(ifc))
		return -1;
	switch (config->mode) {
	case MODE_DHCP:
		ifc->hander = &dhcp_hander;
		break;
	case MODE_PPPOE:
		ifc->hander = &pppoe_hander;
		save_pppoe_user(config->pppoe.user, config->pppoe.passwd);
		break;
	case MODE_STATIC:
		ifc->hander = &static_hander;
		break;
	case MODE_WISP:
		ifc->hander = &wisp_hander;
		break;
	case MODE_3G:
		ifc->hander = &g3_hander;
		break;
	default:
		return -1;
	}
	ispstat = ISP_STAT_INIT;
	interface_speed_set(ifc, config->speed);
	return ifc->hander->if_up(ifc, config);
}

static int interface_cfg_check(struct iface *ifc)
{
	char *res = NULL;
	unsigned char mac[ETH_ALEN];
	unsigned char tmp[ETH_ALEN] = {0X99, 0X88, 0X03, 0X01, 0X00, 0X02};
	struct iface_conf *config = &ifc->config;

	memset(mac, 0x0, sizeof(mac));
	if (!memcmp(mac, config->mac, ETH_ALEN)) {
		if (read_mac(mac))
			memcpy(mac, tmp, ETH_ALEN);
		memcpy(config->mac, mac, ETH_ALEN);
		if (ifc->uid)
			config->mac[5]++;
	}
	switch (config->mode) {
	case MODE_DHCP:
		if (config->dhcp.mtu > 1500 || config->dhcp.mtu <= 0)
			config->dhcp.mtu = 1500;
		break;
	case MODE_STATIC:
		if (config->statip.comm.mtu > 1500 || config->statip.comm.mtu <= 0)
			config->statip.comm.mtu = 1500;
		if (ifc->uid)
			break;
		if (!config->statip.ip.s_addr) {
			res = read_firmware("LANIP");
			if (res)
				inet_aton(res, &config->statip.ip);
			else
				inet_aton("192.168.1.1", &config->statip.ip);
		}
		if (!config->statip.mask.s_addr)
			inet_aton("255.255.255.0", &config->statip.mask);
		break;
	case MODE_PPPOE:
		if (config->pppoe.comm.mtu > 1480 || config->pppoe.comm.mtu <= 0)
			config->pppoe.comm.mtu = 1480;
		break;
	case MODE_WISP:
		break;
	case MODE_3G:
		break;
	default:
		if (!ifc->uid) {
			ifc->hander = &static_hander;
			config->mode = MODE_STATIC;
			config->statip.comm.mtu = 1500;
			res = read_firmware("LANIP");
			if (res)
				inet_aton(res, &config->statip.ip);
			else
				inet_aton("192.168.1.1", &config->statip.ip);
			inet_aton("255.255.255.0", &config->statip.mask);
		} else {
			ifc->hander = &dhcp_hander;
			config->mode = MODE_DHCP;
			config->dhcp.mtu = 1500;
		}
	}
#ifdef LAN_WAN_SWITCH
	if (ifc->type == IF_TYPE_WAN) {
		if (get_sysflag(SYSFLAG_FACTORY) <= 0) {
			ifc->config.mode = MODE_3G;
			ifc->hander = &g3_hander;
		}
	}
#endif
	return 0;
}

static int interface_reset_bak(struct iface *ifc)
{
	char cmd[128];
	char *retstr;
	
	retstr = uci_getval(IF_CONFIG_FILE, WAN_BAK, "proto");
	if (!retstr)
		return 0;
	free(retstr);
	snprintf(cmd, sizeof(cmd), "%s.%s", IF_CONFIG_FILE, WAN_BAK);
	uuci_delete(cmd);
	interface_param_save(&ifc->config);
	return 0;
}

static int interface_load_bak(struct iface *ifc, struct uci_list *sections)
{
	struct uci_element *se, *oe;
	struct uci_option *option;
	struct uci_section *section;
	struct iconf_comm *bak = NULL;

	if (!sections)
		return -1;
	uci_foreach_element(sections, se) {
		section = uci_to_section(se);
		if (section == NULL)
			continue;
		if (strcmp(section->type, "interface") || strcmp(se->name, WAN_BAK)) 
			continue;
		uci_foreach_element(&section->options, oe) {
			option = uci_to_option(oe);
			if (option == NULL)
				continue;
			if (option->type != UCI_TYPE_STRING)
				continue;
			if (!strcmp(oe->name, "proto")) {
				if (!strcmp(option->v.string, "pppoe"))
					bak = &ifc->config.pppoe.comm;
				else if (!strcmp(option->v.string, "static"))
					bak = &ifc->config.statip.comm;
				else if (!strcmp(option->v.string, "dhcp"))
					bak = &ifc->config.dhcp;
			} else if (!strcmp(oe->name, "username"))
				arr_strcpy(ifc->config.pppoe.user, option->v.string);
			else if (!strcmp(oe->name, "password"))
				arr_strcpy(ifc->config.pppoe.passwd, option->v.string);
			else if (!strcmp(oe->name, "ipaddr"))
				inet_aton(option->v.string, &ifc->config.statip.ip);
			else if (!strcmp(oe->name, "netmask"))
				inet_aton(option->v.string, &ifc->config.statip.mask);
			else if (!strcmp(oe->name, "gateway"))
				inet_aton(option->v.string, &ifc->config.statip.gw);
			else if (!strcmp(oe->name, "dns") && bak)
				inet_aton(option->v.string, &bak->dns[0]);
			else if (!strcmp(oe->name, "dns1") && bak)
				inet_aton(option->v.string, &bak->dns[1]);
			else if (!strcmp(oe->name, "mtu") && bak)
				bak->mtu = atoi(option->v.string);
		}
		break;
	}
	return 0;	
}

static int interface_load_config(struct iface *ifc, struct uci_list *sections)
{
	struct uci_element *se, *oe;
	struct uci_option *option;
	struct uci_section *section;
	struct iface_conf *config = &ifc->config;
	struct iconf_comm *cur = NULL;
	unsigned short speed = SPEED_DUPLEX_AUTO;

	if (!sections)
		goto out;
	uci_foreach_element(sections, se) {
		section = uci_to_section(se);
		if (section == NULL)
			continue;
		if (strcmp(section->type, "interface") || strcmp(se->name, ifc->uiname)) 
			continue;
		uci_foreach_element(&section->options, oe) {
			option = uci_to_option(oe);
			if (option == NULL)
				continue;
			if (option->type != UCI_TYPE_STRING)
				continue;
			if (!strcmp(oe->name, "proto")) {
				if (!strncmp(option->v.string, "dhcp", 4)) {
					config->mode = MODE_DHCP;
					ifc->hander = &dhcp_hander;
					cur = &config->dhcp;
				} else if (!strncmp(option->v.string, "pppoe", 5)) {
					config->mode = MODE_PPPOE;
					ifc->hander = &pppoe_hander;
					cur = &config->pppoe.comm;
				} else if (!strncmp(option->v.string, "static", 6)) {
					config->mode = MODE_STATIC;
					ifc->hander = &static_hander;
					cur = &config->statip.comm;
				} else if (!strncmp(option->v.string, "wisp", 4)) {
					config->mode = MODE_WISP;
					ifc->hander = &wisp_hander;
				} else if (!strncmp(option->v.string, "3G", 2)) {
					config->mode = MODE_3G;
					ifc->hander = &g3_hander;
				}
			}else if (!strcmp(oe->name, "macaddr"))
				parse_mac(option->v.string, config->mac);
			else if (!strcmp(oe->name, "username"))
				arr_strcpy(config->pppoe.user, option->v.string);
			else if (!strcmp(oe->name, "password"))
				arr_strcpy(config->pppoe.passwd, option->v.string);
			else if (!strcmp(oe->name, "pppoe_server"))
				arr_strcpy(config->pppoe.pppoe_server, option->v.string);
			else if (!strcmp(oe->name, "ipaddr"))
				inet_aton(option->v.string, &config->statip.ip);
			else if (!strcmp(oe->name, "netmask"))
				inet_aton(option->v.string, &config->statip.mask);
			else if (!strcmp(oe->name, "gateway"))
				inet_aton(option->v.string, &config->statip.gw);
			else if (!strcmp(oe->name, "channel"))
				config->wisp.channel = atoi(option->v.string);
			else if (!strcmp(oe->name, "security"))
				arr_strcpy(config->wisp.security, option->v.string);
			else if (!strcmp(oe->name, "ssid"))
				arr_strcpy(config->wisp.ssid, option->v.string);
			else if (!strcmp(oe->name, "key"))
				arr_strcpy(config->wisp.key, option->v.string);
			else if (!strcmp(oe->name, "mtu") && cur)
				cur->mtu = atoi(option->v.string);
			else if (!strcmp(oe->name, "mtu_dhcp"))
				config->dhcp.mtu = atoi(option->v.string);
			else if (!strcmp(oe->name, "mtu_pppoe"))
				config->pppoe.comm.mtu = atoi(option->v.string);
			else if (!strcmp(oe->name, "mtu_static"))
				config->statip.comm.mtu = atoi(option->v.string);
			else if (!strcmp(oe->name, "dns") && cur)
				inet_aton(option->v.string, &cur->dns[0]);
			else if (!strcmp(oe->name, "dns1") && cur)
				inet_aton(option->v.string, &cur->dns[1]);
			else if (!strcmp(oe->name, "dns_dhcp"))
				inet_aton(option->v.string, &config->dhcp.dns[0]);
			else if (!strcmp(oe->name, "dns1_dhcp"))
				inet_aton(option->v.string, &config->dhcp.dns[1]);
			else if (!strcmp(oe->name, "dns_pppoe"))
				inet_aton(option->v.string, &config->pppoe.comm.dns[0]);
			else if (!strcmp(oe->name, "dns1_pppoe"))
				inet_aton(option->v.string, &config->pppoe.comm.dns[1]);
			else if (!strcmp(oe->name, "dns_static"))
				inet_aton(option->v.string, &config->statip.comm.dns[0]);
			else if (!strcmp(oe->name, "dns1_static"))
				inet_aton(option->v.string, &config->statip.comm.dns[1]);
			else if (!strcmp(oe->name, "speed"))
				speed = atoi(option->v.string);
		}
	}
out:
	config->uid = ifc->uid;
	interface_cfg_check(ifc);
	igd_set_bit(IF_STATUS_INIT, &ifc->status);
	interface_speed_set(ifc, speed);
	ifc->config.speed = speed;
	ifc->hander->if_up(ifc, config);
	return 0;
}

static int interface_init(void)
{
	char *res;
	unsigned long bitmap;
	struct iface *ifc = NULL;
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;

	ctx = uci_alloc_context();
	uci_load(ctx, IF_CONFIG_FILE, &pkg);
	
	memset(igd_iface, 0x0, sizeof(igd_iface));
	res = read_firmware("WANPID");
	if (res)
		wanpid = atoi(res);
	res = read_firmware("DOMAIN");
	if (res)
		arr_strcpy(domain, res);
	else
		arr_strcpy(domain, "wiair.cc");
	//init lan
	ifc = interface_get(0);
	ifc->type = IF_TYPE_LAN;
	ifc->uid = 0;
	arr_strcpy(ifc->devname, LAN_DEVNAME);
	arr_strcpy(ifc->uiname, LAN_UINAME);
	interface_load_config(ifc, pkg?&pkg->sections:NULL);
	//init wan
	ifc = interface_get(1);
	ifc->type = IF_TYPE_WAN;
	ifc->uid = 1;
	arr_strcpy(ifc->devname, WAN1_DEVNAME);
	arr_strcpy(ifc->uiname, WAN1_UINAME);
	interface_load_config(ifc, pkg?&pkg->sections:NULL);
	interface_load_bak(ifc, pkg?&pkg->sections:NULL);
	if (pkg)
		uci_unload(ctx, pkg);
	uci_free_context(ctx);
	interface_reset_bak(ifc);
	if (!get_link_status(&bitmap) && (bitmap & (0x1 << wanpid)))
		ifstats = 1;
	ifstats_cb(NULL);
	#if 0
	add_ap2_web_deny();
	#endif
	exec_cmd("iptables -t mangle -I POSTROUTING -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu");
	return 0;
}

int get_firewall_parm(char *parm)
{
	char cmd[256] = {0};
	char **val;
	int num;
	int ret = 0;

	sprintf(cmd, "network.firewall.%s", parm);
	uuci_get(cmd, &val, &num);
	if (!val)
		ret = 0;
	else
		ret = atoi(val[0]);
	uuci_get_free(val, num);
	return ret;
}

int firewall_get(struct firewall *info, int rlen)
{
	if (rlen != sizeof(*info))
		return -1;
	info->is_dos_open = get_firewall_parm("is_dos_open");
	info->is_flood_open = get_firewall_parm("is_flood_open");
	return 0;
}

int firewall_set(struct firewall *info, int len)
{
	char cmd[256] = {0};

	if (len != sizeof(*info))
		return -1;
	uuci_set("network.firewall=interface");
	sprintf(cmd, "network.firewall.is_dos_open=%d", info->is_dos_open);
	uuci_set(cmd);
	sprintf(cmd, "network.firewall.is_flood_open=%d", info->is_flood_open);
	uuci_set(cmd);
	return 0;
}

int interface_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;
	struct iface_conf *config = NULL;
	struct iface_info *info = NULL;
	
	switch(msgid) {
	case IF_MOD_INIT:
		ret = interface_init();
		break;
	case IF_MOD_IFACE_IP_UP:
		if (len != sizeof(struct sys_msg_ipcp) || !data)
			return -1;
		ret = interface_ipchange((struct sys_msg_ipcp *)data, NLK_ACTION_ADD);
		break;
	case IF_MOD_IFACE_IP_DOWN:
		if (len != sizeof(struct sys_msg_ipcp) || !data)
			return -1;
		ret = interface_ipchange((struct sys_msg_ipcp *)data, NLK_ACTION_DEL);
		break;
	case IF_MOD_PARAM_SET:
		if (len != sizeof(struct iface_conf) || !data)
			return -1;
		ret = interface_param_set((struct iface_conf *)data);
		if (!ret)
			interface_param_save((struct iface_conf *)data);
		break;
	case IF_MOD_PARAM_SHOW:
		if (rlen != sizeof(struct iface_conf) || !rbuf || !data)
			return -1;
		config = (struct iface_conf *)rbuf;
		config->uid = *(int *)data;
		ret = interface_param_show(config);
		break;
	case IF_MOD_IFACE_INFO:
		if (rlen != sizeof(struct iface_info) || !rbuf || !data)
			return -1;
		info = (struct iface_info *)rbuf;
		info->uid = *(int *)data;
		ret = interface_dump(info);
		break;
	case IF_MOD_WAN_DETECT:
		if (rlen != sizeof(struct detect_info) || !rbuf)
			return -1;
		ret = interface_mode_detect((struct detect_info *)rbuf);
		break;
	case IF_MOD_GET_ACCOUNT:
		if (!data || len != sizeof(int) || !rbuf || rlen != sizeof(struct account_info))
			return -1;
		ret = interface_get_account(*(int *)data, (struct account_info *)rbuf);
		break;
	case IF_MOD_SEND_ACCOUNT:
		if (!data || len != sizeof(struct account_info))
			return -1;
		memcpy(&account, data, sizeof(account));
		ret = 0;
		break;
	case IF_MOD_NET_STATUS:
		if (!data || len != sizeof(int))
			return -1;
		ret = nlk_net_status_action(*(int *)data);
		break;
	case IF_MOD_DETECT_REPLY:
		if (!data || len != sizeof(int))
			return -1;
		ret = detect_net_status_action(*(int *)data);
		break;
	case IF_MOD_ISP_STATUS:
		if (!data || len != sizeof(int))
			return -1;
		ispstat = *(int *)data;
		if (ispstat == ISP_STAT_AUTH_FAILD)
			ADD_SYS_LOG("拨号失败，帐号密码错误");
		else if (ispstat == ISP_STAT_DNOT_REPLY)
			ADD_SYS_LOG("拨号失败，运营商无响应");
		ret = 0;
		break;
	case IF_MOD_LAN_RESTART:
		interface_lan_restart();
		ret = 0;
		break;
	case IF_MOD_FIREWALL_GET:
		ret = firewall_get(rbuf, rlen);
		break;
	case IF_MOD_FIREWALL_SET:
		ret = firewall_set(data, len);
		break;
	case IF_MOD_WAN_BRIDAGE:
		ret = interface_wan_bridge();
		break;
	default:
		break;
	}
	return ret;
}
