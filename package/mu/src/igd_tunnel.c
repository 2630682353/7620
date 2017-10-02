#include "igd_lib.h"
#include "igd_tunnel.h"
#include "uci.h"

struct tunnel tcfg[TUNNEL_MX];
struct schedule_entry *l2tp[TUNNEL_MX];

static int write_pptp_opfile(int index, struct tunnel_conf *cfg)
{
	FILE *fp = NULL;
	char fpath[32];
	
	snprintf(fpath, sizeof(fpath) - 1, "/tmp/options.pptp.%d", index);
	fp = fopen(fpath, "w");
	if (NULL == fp)
		return -1;
	fprintf(fp,
			"noipdefault\n"
			"refuse-pap\n"
			"noauth\n"
			"nobsdcomp\n"
			"nodeflate\n"
			"idle 0\n"
			"holdoff 30\n"
			"mtu 1400\n"
			"mru 1400\n"
			"nodetach\n"
			"persist\n"
			"usepeerdns\n"
			"lcp-echo-failure 15\n"
			"lcp-echo-interval 8\n"
			"plugin pptp.so\n"
			"pppd_type %d\n"
			"tunnel_id %d\n",
			PPPD_TYPE_TUNNEL_PPTP,
			index);
	if (cfg->mppe)
		fprintf(fp, "mppe required,no40,no56,stateless\n");
	fclose(fp);
	return 0;
}

static int write_l2tp_opfile(int index, struct tunnel_conf *cfg)
{
	FILE *fp = NULL;
	char fpath[32];

	snprintf(fpath, sizeof(fpath) - 1, "/tmp/options.l2tp.%d", index);
	fp = fopen(fpath, "w");
	if (!fp)
		return -1;
	fprintf(fp,
			"name %s\n"
			"user %s\n"
			"password %s\n"
			"noipdefault\n"
			"refuse-pap\n"
			"noauth\n"
			"nobsdcomp\n"
			"nodeflate\n"
			"idle 0\n"
			"holdoff 30\n"
			"mtu 1400\n"
			"mru 1400\n"
			"nodetach\n"
			"maxfail 5\n"
			"usepeerdns\n"
			"lcp-echo-failure 15\n"
			"lcp-echo-interval 8\n"
			"pppd_type %d\n"
			"tunnel_id %d\n",
			cfg->user, cfg->user,
			cfg->password,
			PPPD_TYPE_TUNNEL_L2TP,
			index);
	if (cfg->mppe)
		fprintf(fp, "mppe required,no40,no56,stateless\n");
	fclose(fp);
	return 0;
}

static int tunnel_generic_ip_up(struct sys_msg_ipcp *ipcp)
{
	struct in_addr tdns;
	struct tunnel *cfg = &tcfg[ipcp->uid];
	
	if (igd_test_bit(TUNNEL_STATUS_IP_UP, &cfg->status))
		return 0;
	if (ipcp->dns[0].s_addr)
		tdns.s_addr = ipcp->dns[0].s_addr;
	else if (ipcp->dns[1].s_addr)
		tdns.s_addr = ipcp->dns[1].s_addr;
	else
		return -1;
	exec_cmd("iptables -t nat -A PREROUTING -i %s -j %s", LAN_DEVNAME, TUNNEL_CHAIN);
	exec_cmd("iptables -t nat -A %s -p udp --dport 53 -j DNAT --to %u.%u.%u.%u:53",
			TUNNEL_CHAIN, NIPQUAD(tdns.s_addr));
	exec_cmd("iptables -t nat -A POSTROUTING -o %s -j MASQUERADE", ipcp->devname);
	exec_cmd("ip route add default via %u.%u.%u.%u dev %s table %d",
			NIPQUAD(ipcp->gw.s_addr), ipcp->devname, TUNNEL_RTABLE);
	cfg->ipcp = *ipcp;
	igd_clear_bit(TUNNEL_STATUS_IP_DOWN, &cfg->status);
	igd_set_bit(TUNNEL_STATUS_IP_UP, &cfg->status);
	return 0;
}

static int tunnel_generic_ip_down(struct sys_msg_ipcp *ipcp)
{
	struct tunnel *cfg = &tcfg[ipcp->uid];
	
	if (igd_test_bit(TUNNEL_STATUS_IP_DOWN, &cfg->status))
		return 0;
	exec_cmd("iptables -t nat -F %s", TUNNEL_CHAIN);
	exec_cmd("iptables -t nat -D PREROUTING -i %s -j %s", LAN_DEVNAME, TUNNEL_CHAIN);
	exec_cmd("iptables -t nat -D POSTROUTING -o %s -j MASQUERADE", cfg->ipcp.devname);
	exec_cmd("ip route del default via %u.%u.%u.%u dev %s table %d",
			NIPQUAD(cfg->ipcp.gw.s_addr), cfg->ipcp.devname,
			TUNNEL_RTABLE);
	exec_cmd("iptables -t nat -D %s -p udp --dport 53 -j DNAT --to %u.%u.%u.%u:53",
			TUNNEL_CHAIN, NIPQUAD(ipcp->dns[0].s_addr));
	exec_cmd("iptables -t nat -D %s -p udp --dport 53 -j DNAT --to %u.%u.%u.%u:53",
			TUNNEL_CHAIN, NIPQUAD(ipcp->dns[1].s_addr));
	memset(&cfg->ipcp, 0x0, sizeof(cfg->ipcp));
	igd_clear_bit(TUNNEL_STATUS_IP_UP, &cfg->status);
	igd_set_bit(TUNNEL_STATUS_IP_DOWN, &cfg->status);
	return 0;
}

static int pptp_tunnel_up(int index, struct tunnel_conf *cfg)
{
	char fpath[32];

	if (write_pptp_opfile(index, cfg))
		return -1;
	snprintf(fpath, sizeof(fpath) - 1, "/tmp/options.pptp.%d", index);
	return exec_cmd("%s file %s user %s password %s pptp_server %s &",
				PPTP_PPPD, fpath, cfg->user, cfg->password,
				cfg->sdomain[0]?cfg->sdomain:inet_ntoa(cfg->serverip));
}

static void l2tp_cb(void *data)
{
	char cmd[512];
	char fpath[32];
	struct tunnel_conf *cfg = (struct tunnel_conf *)data;
	
	l2tp[cfg->index] = NULL;
	snprintf(fpath, sizeof(fpath) - 1, "/tmp/options.l2tp.%d", cfg->index);
	snprintf(cmd, sizeof(cmd) - 1,
			"echo \"a %s lns = %s:%d;"
			"ppp debug = yes;"
			"require authentication = no;"
			"autodial = yes;redial = yes;"
			"redial timeout = 30;"
			"pppoptfile = %s\" > %s",
			L2TP_NAME,
			cfg->sdomain[0]?cfg->sdomain:inet_ntoa(cfg->serverip),
			cfg->port, fpath, L2TP_CTR_PIPO);
	system(cmd);
}

static int l2tp_tunnel_up(int index, struct tunnel_conf *cfg)
{
	struct timeval tv;
	
	if (write_l2tp_opfile(index, cfg))
		return 0;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	l2tp[index] = schedule(tv, l2tp_cb, &tcfg[index].config);
	if (!l2tp[index])
		return -1;
	return exec_cmd("%s &", L2TP_EXE);
}

static int pptp_tunnel_down(int index)
{
	struct sys_msg_ipcp ipcp;
	
	memcpy(&ipcp, &tcfg[index].ipcp, sizeof(ipcp));
	ipcp.uid = index;
	exec_cmd("killall %s", PPTP_PPPD);
	return tcfg[index].hander->tunnel_ip_down(&ipcp);
}

static int l2tp_tunnel_down(int index)
{
	char cmd[32];
	struct sys_msg_ipcp ipcp;
	
	if (l2tp[index]) {
		dschedule(l2tp[index]);
		l2tp[index] = NULL;
	}
	memcpy(&ipcp, &tcfg[index].ipcp, sizeof(ipcp));
	ipcp.uid = index;
	snprintf(cmd, sizeof(cmd) - 1, "echo \"r %s\" > %s", L2TP_NAME, L2TP_CTR_PIPO);
	system(cmd);
	exec_cmd("killall %s", L2TP_PPPD);
	exec_cmd("killall %s", L2TP_EXE);
	return tcfg[index].hander->tunnel_ip_down(&ipcp);
}

static struct tunnel_hander pptp_hander = {
	.tunnel_up = pptp_tunnel_up,
	.tunnel_down = pptp_tunnel_down,
	.tunnel_ip_up = tunnel_generic_ip_up,
	.tunnel_ip_down = tunnel_generic_ip_down,
};

static struct tunnel_hander l2tp_hander = {
	.tunnel_up = l2tp_tunnel_up,
	.tunnel_down = l2tp_tunnel_down,
	.tunnel_ip_up = tunnel_generic_ip_up,
	.tunnel_ip_down = tunnel_generic_ip_down,
};

static int tunnel_ipcp(int action, struct sys_msg_ipcp *ipcp)
{
	struct tunnel *cfg;

	if (ipcp->uid >= TUNNEL_MX || ipcp->uid < 0)
		return -1;
	cfg = &tcfg[ipcp->uid];
	if (!cfg->hander || cfg->config.type != ipcp->pppd_type)
		return -1;
	switch (action) {
	case NLK_ACTION_ADD:
		return cfg->hander->tunnel_ip_up(ipcp);
	case NLK_ACTION_DEL:
		return cfg->hander->tunnel_ip_down(ipcp);
	default:
		break;
	}
	return 0;
}

static int tunnel_info(int index, struct sys_msg_ipcp *ipcp)
{
	if (index > TUNNEL_MX || index < 0)
		return -1;
	if (!tcfg[index].invalid)
		return -1;
	memcpy(ipcp, &tcfg[index].ipcp, sizeof(*ipcp));
	return 0;
}

static int tunnel_show(int index, struct tunnel_conf *cfg)
{
	if (index > TUNNEL_MX || index < 0)
		return -1;
	if (!tcfg[index].invalid)
		return -1;
	memcpy(cfg, &tcfg[index].config, sizeof(*cfg));
	return 0;
}

static int tunnel_set(struct tunnel_conf *cfg)
{
	int i;
	int index = cfg->index;
	
	if (index >= 0) {
		if (tcfg[index].hander)
			tcfg[index].hander->tunnel_down(index);
		if (!cfg->user[0] && !cfg->password[0]) {
			memset(&tcfg[index], 0x0, sizeof(struct tunnel));
			return 0;
		}
	} else {
		for (i = 0; i < TUNNEL_MX; i++) {
			if (tcfg[i].invalid)
				continue;
			tcfg[i].invalid = 1;
			cfg->index = i;
			index = i;
			break;
		} 
	}
	if (index < 0 || !tcfg[index].invalid)
		return -1;
	switch(cfg->type) {
	case PPPD_TYPE_TUNNEL_PPTP:
		tcfg[index].hander = &pptp_hander;
		break;
	case PPPD_TYPE_TUNNEL_L2TP:
		tcfg[index].hander = &l2tp_hander;
		break;
	default:
		return -1;
	}
	if (cfg->enable)
		return tcfg[index].hander->tunnel_up(index, cfg);
	return 0;
}

static int tunnel_restart(void)
{
	int i;

	for (i = 0; i < TUNNEL_MX; i++) {
		if (tcfg[i].invalid)
			tunnel_set(&tcfg[i].config);	
	}
	return 0;
}

static int tunnel_save_config(struct tunnel_conf *cfg)
{
	char cmd[128];
	int index = cfg->index;

	tcfg[index].config = *cfg;
	if (tcfg[index].invalid)
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].enable=%d",
				TUNNEL_CFG_FILE, index, cfg->enable);
	else
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].enable=",
				TUNNEL_CFG_FILE, index);
	uuci_set(cmd);
	
	if (tcfg[index].invalid)
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].type=%d",
				TUNNEL_CFG_FILE, index, cfg->type);
	else
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].type=",
				TUNNEL_CFG_FILE, index);
	uuci_set(cmd);
	
	if (tcfg[index].invalid)
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].mppe=%d",
				TUNNEL_CFG_FILE, index, cfg->mppe);
	else
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].mppe=",
				TUNNEL_CFG_FILE, index);
	uuci_set(cmd);

	if (tcfg[index].invalid)
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].port=%d",
				TUNNEL_CFG_FILE, index, cfg->port);
	else
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].port=",
				TUNNEL_CFG_FILE, index);
	uuci_set(cmd);
	
	if (tcfg[index].invalid && cfg->serverip.s_addr)
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].ipaddr=%s",
				TUNNEL_CFG_FILE, index, inet_ntoa(cfg->serverip));
	else
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].ipaddr=",
				TUNNEL_CFG_FILE, index);
	uuci_set(cmd);
	
	if (tcfg[index].invalid)
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].domain=%s",
				TUNNEL_CFG_FILE, index, cfg->sdomain);
	else
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].domain=",
				TUNNEL_CFG_FILE, index);
	uuci_set(cmd);
	
	if (tcfg[index].invalid)
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].user=%s",
				TUNNEL_CFG_FILE, index, cfg->user);
	else
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].user=",
				TUNNEL_CFG_FILE, index);
	uuci_set(cmd);
	
	if (tcfg[index].invalid)	
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].password=%s",
				TUNNEL_CFG_FILE, index, cfg->password);
	else
		snprintf(cmd, sizeof(cmd) - 1, "%s.@tunnel[%d].password=",
				TUNNEL_CFG_FILE, index);	
	uuci_set(cmd);
	return 0;
}

static int tunnel_config_check(struct tunnel_conf *cfg)
{
	struct tunnel *t;

	t = &tcfg[cfg->index];
	if (!cfg->user[0] || !cfg->password[0])
		goto error;
	if (!cfg->sdomain[0] && !cfg->serverip.s_addr)
		goto error;
	switch (cfg->type) {
	case PPPD_TYPE_TUNNEL_PPTP:
		t->invalid = 1;
		t->hander = &pptp_hander;
		return 0;
	case PPPD_TYPE_TUNNEL_L2TP:
		t->invalid = 1;
		t->hander = &l2tp_hander;
		return 0;
	default:
		goto error;
	}
error:
	memset(cfg, 0x0, sizeof(*cfg));
	return -1;
}

static int tunnel_init_config(void)
{	
	int i = 0;
	struct uci_option *option;
	struct uci_element *se, *oe;
	struct uci_section *section;
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;

	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, TUNNEL_CFG_FILE, &pkg)) {
		uci_free_context(ctx);
		return -1;
	}
	uci_foreach_element(&pkg->sections, se) {
		section = uci_to_section(se);
		if (section == NULL)
			continue;
		if (strcmp(section->type, "tunnel"))
			continue;
		tcfg[i].config.index = i;
		uci_foreach_element(&section->options, oe) {
			option = uci_to_option(oe);
			if (option == NULL || option->type != UCI_TYPE_STRING)
				continue;
			if (!strcmp(oe->name, "enable"))
				tcfg[i].config.enable = atoi(option->v.string);
			else if (!strcmp(oe->name, "type"))
				tcfg[i].config.type = atoi(option->v.string);
			else if (!strcmp(oe->name, "mppe"))
				tcfg[i].config.mppe = atoi(option->v.string);
			else if (!strcmp(oe->name, "port"))
				tcfg[i].config.port = atoi(option->v.string);
			else if (!strcmp(oe->name, "domain"))
				arr_strcpy(tcfg[i].config.sdomain, option->v.string);
			else if (!strcmp(oe->name, "ipaddr"))
				inet_aton(option->v.string, &tcfg[i].config.serverip);
			else if (!strcmp(oe->name, "user"))
				arr_strcpy(tcfg[i].config.user, option->v.string);
			else if (!strcmp(oe->name, "password"))
				arr_strcpy(tcfg[i].config.password, option->v.string);
		}
		if (tunnel_config_check(&tcfg[i].config))
			continue;
		i++;
		if (i >= TUNNEL_MX)
			break;
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx);
	return 0;
}

static int tunnel_init(void)
{
	if (access(TUNNEL_CFG_PATH, F_OK))
		return 0;
	tunnel_init_config();
	exec_cmd("iptables -t nat -N %s", TUNNEL_CHAIN);
	exec_cmd("ip rule add prio 32800 table %d", TUNNEL_RTABLE);
	exec_cmd("ip rule add prio 100 dev %s table %d", LAN_DEVNAME, TUNNEL_RTABLE);
	return tunnel_restart();
}

int tunnel_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;

	switch (msgid) {
	case TUNNEL_MOD_INIT:
		ret = tunnel_init();
		break;
	case TUNNEL_MOD_SET:
		if (!data || len != sizeof(struct tunnel_conf))
			return -1;
		ret = tunnel_set((struct tunnel_conf *)data);
		if (!ret)
			tunnel_save_config((struct tunnel_conf *)data);
		break;
	case TUNNEL_MOD_SHOW:
		if (!data || (len != sizeof(int)) || !rbuf || (rlen != sizeof(struct tunnel_conf)))
			return -1;
		ret = tunnel_show(*(int *)data, (struct tunnel_conf *)rbuf);
		break;
	case TUNNEL_MOD_INFO:
		if (!data || (len != sizeof(int)) || !rbuf || (rlen != sizeof(struct sys_msg_ipcp)))
			return -1;
		ret = tunnel_info(*(int *)data, (struct sys_msg_ipcp *)rbuf);
		break;
	case TUNNEL_MOD_RESET:
		ret = tunnel_restart();
		break;
	case TUNNEL_MOD_IP_UP:
		if (!data || len != sizeof(struct sys_msg_ipcp))
			return -1;
		ret = tunnel_ipcp(NLK_ACTION_ADD, (struct sys_msg_ipcp *)data);
		break;
	case TUNNEL_MOD_IP_DOWN:
		if (!data || len != sizeof(struct sys_msg_ipcp))
			return -1;
		ret = tunnel_ipcp(NLK_ACTION_DEL, (struct sys_msg_ipcp *)data);
		break;
	default:
		break;
	}
	return ret;
}
