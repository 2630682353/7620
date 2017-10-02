#include "uci.h"
#include "igd_lib.h"
#include "igd_dnsmasq.h"
#include "igd_interface.h"
#include "igd_system.h"
#include "igd_wifi.h"

static int rule_id;
static struct dhcp_conf config;
static char domain[IGD_DNS_LEN];
static struct ip_mac static_host[IP_MAC_MAX];
static struct ip_mac ipmac_bind[IP_MAC_MAX];
static struct schedule_entry *dnsmasq = NULL;
static struct ddns_info ddnsif;
static struct ddns_status ddnsst;

static int add_redirect_rule(struct dhcp_conf *config)
{
	struct inet_host host;
	struct dns_rule_api arg;
	
	memset(&host, 0, sizeof(host));
	host.type = INET_HOST_NONE;
	memset(&arg, 0, sizeof(arg));
	arg.target = NF_TARGET_REDIRECT;
	arg.l3.type = INET_L3_DNS;
	arr_strcpy(arg.l3.dns, domain);
	arg.type = 0x1;
	arg.ip = (unsigned int)config->ip.s_addr;
	if (rule_id > 0)
		unregister_dns_rule(rule_id);
	rule_id = register_dns_rule(&host, &arg);
	return 0;
}

static int dnsmasq_write_conf(struct dhcp_conf *config)
{
	int len = 0;
	char buf[64] = {0};
	FILE *fp = NULL;
	
	//`tmp/dnsmasq.d` is for lanxun dns configs
	exec_cmd("mkdir /tmp/dnsmasq.d");
	
	fp = fopen(DNSMASQ_CONFIG_FILE, "w");
	if (NULL == fp)
		return -1;
	fprintf(fp,
		"# auto-generated config "
		"file from /etc/config/dhcp\n"
		"conf-file=/etc/dnsmasq.conf\n"
		"dhcp-authoritative\n"
		"domain-needed\n"
		"localise-queries\n"
//		"read-ethers\n"
		"bogus-priv\n"
		"expand-hosts\n"
		"domain=lan\n"
		"server=/lan/\n"
		"dhcp-leasefile=/tmp/dhcp.leases\n"
		"resolv-file=/tmp/resolv.conf.auto\n"
//		"addn-hosts=/tmp/hosts\n"
		"stop-dns-rebind\n"
		"rebind-localhost-ok\n"
		"dhcp-broadcast=tag:needs-broadcast\n"
		"conf-dir=/tmp/dnsmasq.d\n");
	
	if (!config->enable)
		goto out;
	if (config->dns1.s_addr)
		len = sprintf(buf, "%u.%u.%u.%u,",
		NIPQUAD(config->dns1.s_addr));
	if (config->dns2.s_addr)
		len += sprintf(&buf[len], "%u.%u.%u.%u,",
		NIPQUAD(config->dns2.s_addr));
	if (len) {
		buf[len - 1] = 0;
		fprintf(fp, "dhcp-option=6,%s\n", buf);
	}
	fprintf(fp,
		"dhcp-range=lan,%u.%u.%u.%u,"
		"%u.%u.%u.%u,%u.%u.%u.%u,%dh\n",
		NIPQUAD(config->start.s_addr),
		NIPQUAD(config->end.s_addr),
		NIPQUAD(config->mask.s_addr),
		config->time);

	loop_for_each(0, IP_MAC_MAX) {
		if (!static_host[i].ip.s_addr)
			continue;
			fprintf(fp,
				"dhcp-host=%02x:%02x:%02x:%02x:%02x:%02x,"
				"%u.%u.%u.%u\n",
				MAC_SPLIT(static_host[i].mac),
				NIPQUAD(static_host[i].ip.s_addr));
	}loop_for_each_end();
	
	loop_for_each_2(j, 0, IP_MAC_MAX) {
		if (!ipmac_bind[j].ip.s_addr)
			continue;
			fprintf(fp, "dhcp-host=%02x:%02x:%02x:%02x:%02x:%02x,"
				"%u.%u.%u.%u\n",
				MAC_SPLIT(ipmac_bind[j].mac),
				NIPQUAD(ipmac_bind[j].ip.s_addr));
	}loop_for_each_end();
out:
	fclose(fp);
	return 0;
}

static void dnsmasp_cb(void *data)
{
	char cmd[128];

	dnsmasq = NULL;
	loop_for_each(0, IP_MAC_MAX) {
		if (!ipmac_bind[i].ip.s_addr)
			continue;
		snprintf(cmd, sizeof(cmd) - 1, "arp -s %u.%u.%u.%u "
					"%02x:%02x:%02x:%02x:%02x:%02x",
					NIPQUAD(ipmac_bind[i].ip.s_addr),
					MAC_SPLIT(ipmac_bind[i].mac));
		exec_cmd(cmd);
	} loop_for_each_end();
	if (dnsmasq_write_conf(&config))
		return;
	add_redirect_rule(&config);
	mu_call(IF_MOD_LAN_RESTART, NULL, 0, NULL, 0);
	mu_call(WIFI_MOD_DISCONNCT_ALL, NULL, 0, NULL, 0);
	mu_call(SYSTEM_MOD_DAEMON_DEL, DNSMASQ_CMD, strlen(DNSMASQ_CMD) + 1, NULL, 0);
	mu_call(SYSTEM_MOD_DAEMON_ADD, DNSMASQ_CMD, strlen(DNSMASQ_CMD) + 1, NULL, 0);
}

static int dnsmasp_restart(void)
{
	struct timeval tv;

	if (dnsmasq) {
		dschedule(dnsmasq);
		dnsmasq = NULL;
	}
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	dnsmasq = schedule(tv, dnsmasp_cb, NULL);
	if (!dnsmasq)
		return -1;
	return 0;
}

static int dnsmasq_set_static_hosts(struct ip_mac *hosts)
{
	memset(static_host, 0x0, sizeof(static_host));
	memcpy(static_host, hosts, sizeof(struct ip_mac) * IP_MAC_MAX);
	return dnsmasp_restart();
}

static int ddns_get_status(void)
{
	int fd;
	
	if (ddnsif.enable == 0) {
	 	memcpy(&(ddnsst.dif), &ddnsif, sizeof(ddnsif));
		return 0;
	}
	memcpy(&(ddnsst.dif), &ddnsif, sizeof(ddnsif));
	fd = open(DDNS_TEMP_FILE, O_RDWR);		
	read(fd, ddnsst.basic, 4);
	lseek(fd, 4, SEEK_SET);	
	read(fd, ddnsst.domains, 149);	
	close(fd);
	return 0;
}

static int ddns_write_conf(void) 
{
	FILE *fp = NULL;
	
	fp = fopen(DDNS_CONFIG_FILE, "w");
	if (NULL == fp)
		return -1;
	fprintf(fp,
		"# auto-generated config \n"
		"szHost = phddns60.oray.net \n"
		"szUserID = %s \n"
		"szUserPWD = %s \n"
		"nicName = %s \n"
		"szLog = /dev/null \n", ddnsif.userid
		, ddnsif.pwd, WAN1_DEVNAME);
	fclose(fp);
	return 0;
}

static int ddns_init(void)
{
	char *val;
	val = uci_getval(DHCP_CONFIG, "wan", "ddns_enable");
	if (val) {
		ddnsif.enable = atoi(val);
		free(val);
	}
	val = uci_getval(DHCP_CONFIG, "wan", "ddns_userid");
	if (val) {
		strcpy(ddnsif.userid, val);
		free(val);
	}
	val = uci_getval(DHCP_CONFIG, "wan", "ddns_pwd");
	if (val) {
		strcpy(ddnsif.pwd, val);
		free(val);
	}	
	return 0;	
}

static int ddns_restart(void)
{
	exec_cmd("killall -kill %s", PHDDNS);
	unlink(DDNS_TEMP_FILE);
	if (ddnsif.enable) {
		if (ddns_write_conf())
			return -1;
		exec_cmd("%s &", PHDDNS);
	}
	return 0;
}

static int ddns_save_config(struct ddns_info *dif)
{
	char cmd[128] = {0};
   	snprintf(cmd, 128, "%s.wan.ddns_enable=%d", DHCP_CONFIG, dif->enable);
    	uuci_set(cmd);
	if ((dif->userid[0] == 0) || (dif->pwd[0] == 0))	 
		 return 0;
	memset(cmd, 0, sizeof(cmd));
   	snprintf(cmd, 128, "%s.wan.ddns_userid=%s", DHCP_CONFIG, dif->userid);
    	uuci_set(cmd);
	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, 128, "%s.wan.ddns_pwd=%s", DHCP_CONFIG, dif->pwd);
    	uuci_set(cmd);
	return 0;
}

static int ddns_set(struct ddns_info *dif)
{
	ddnsif.enable = dif->enable;
	if ((dif->userid[0] == 0) || (dif->pwd[0] == 0)) {	 
		if (ddns_restart())
			return -1;
		ddns_save_config(dif);
		return 0;
	}
	strcpy(ddnsif.userid, dif->userid);
	strcpy(ddnsif.pwd, dif->pwd);
	if (ddns_restart())
		return -1;
	ddns_save_config(dif);
	return 0;
}

static int dnsmasq_set_ipmac_bind(struct ip_mac *hosts)
{
	char cmd[128];
	
	loop_for_each(0, IP_MAC_MAX) {
		if (!ipmac_bind[i].ip.s_addr)
			continue;
		snprintf(cmd, sizeof(cmd) - 1, "arp -d %u.%u.%u.%u",
					NIPQUAD(ipmac_bind[i].ip.s_addr));
		exec_cmd(cmd);
	}loop_for_each_end();
	memcpy(ipmac_bind, hosts, sizeof(ipmac_bind));
	return dnsmasp_restart();
}

static int dnsmasq_save_config(struct dhcp_conf *config)
{
	char cmd[64] = {0};
	unsigned int start, end;

	start = ntohl(config->start.s_addr & (~config->mask.s_addr));
	end = ntohl(config->end.s_addr) - ntohl(config->start.s_addr) + 1;
	snprintf(cmd, 63, "%s.lan.start=%u", DHCP_CONFIG, start);
	uuci_set(cmd);
	snprintf(cmd, 63, "%s.lan.limit=%u", DHCP_CONFIG, end);
	uuci_set(cmd);
	snprintf(cmd, 63, "%s.lan.enable=%d", DHCP_CONFIG, config->enable);
	uuci_set(cmd);
	snprintf(cmd, 63, "%s.lan.dns1=%s", DHCP_CONFIG, config->dns1.s_addr?inet_ntoa(config->dns1):"");
	uuci_set(cmd);
	snprintf(cmd, 63, "%s.lan.dns2=%s", DHCP_CONFIG, config->dns2.s_addr?inet_ntoa(config->dns2):"");
	uuci_set(cmd);
	snprintf(cmd, 63, "%s.lan.time=%d", DHCP_CONFIG, config->time);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.lan.shost", DHCP_CONFIG);
	uuci_delete(cmd);
	snprintf(cmd, sizeof(cmd), "%s.lan.ipmac", DHCP_CONFIG);
	uuci_delete(cmd);
	loop_for_each(0, IP_MAC_MAX) {
		if (!static_host[i].ip.s_addr)
			continue;
		snprintf(cmd, sizeof(cmd), "%s.lan.shost=%02x:%02x:%02x:%02x:%02x:%02x %u.%u.%u.%u",
				DHCP_CONFIG, MAC_SPLIT(static_host[i].mac),
				NIPQUAD(static_host[i].ip.s_addr));
		uuci_add_list(cmd);
	} loop_for_each_end();
	loop_for_each_2(j, 0, IP_MAC_MAX) {
		if (!ipmac_bind[j].ip.s_addr)
			continue;
		snprintf(cmd, sizeof(cmd), "%s.lan.ipmac=%02x:%02x:%02x:%02x:%02x:%02x %u.%u.%u.%u",
				DHCP_CONFIG, MAC_SPLIT(ipmac_bind[j].mac),
				NIPQUAD(ipmac_bind[j].ip.s_addr));
		uuci_add_list(cmd);
	} loop_for_each_end();
	return 0;
}

static int dnsmasq_init(void)
{
	char *res;
	int i = 0, j = 0;
	unsigned int start, limit;
	struct uci_element *se, *oe, *e;
	struct uci_option *option;
	struct uci_section *section;
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;
	
	ddns_init();
	res = read_firmware("DOMAIN");
	if (res)
		arr_strcpy(domain, res);
	else
		arr_strcpy(domain, "wiair.cc");
	exec_cmd("touch /etc/config/%s", IF_CONFIG_FILE);

	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, IF_CONFIG_FILE, &pkg)) {
		uci_free_context(ctx); 
		return -1;
	}
	uci_foreach_element(&pkg->sections, se) {
		section = uci_to_section(se);
		if (section == NULL)
			continue;
		if (strcmp(section->type, "interface") || strcmp(se->name, LAN_UINAME))
			continue;
		uci_foreach_element(&section->options, oe) {
			option = uci_to_option(oe);
			if (option == NULL)
				continue;
			if (option->type != UCI_TYPE_STRING)
				continue;
			if (!strcmp(oe->name, "ipaddr"))
				inet_aton(option->v.string, &config.ip);
			else if (!strcmp(oe->name, "netmask"))
				inet_aton(option->v.string, &config.mask);
		}
		break;
	}
	uci_unload(ctx, pkg);
	uci_free_context(ctx);

	if (!config.ip.s_addr) {
		res = read_firmware("LANIP");
		if (res)
			inet_aton(res, &config.ip);
		else
			inet_aton("192.168.1.1", &config.ip);
	}
	if (!config.mask.s_addr)
		inet_aton("255.255.255.0", &config.mask);

	start = -1;
	limit = -1;
	config.enable = -1;
	exec_cmd("touch /etc/config/%s", DHCP_CONFIG);
	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, DHCP_CONFIG, &pkg)) {
		uci_free_context(ctx); 
		return -1;
	}
	uci_foreach_element(&pkg->sections, se) {
		section = uci_to_section(se);
		if (section == NULL)
			continue;
		if (strcmp(section->type, "dhcp") || strcmp(se->name, LAN_UINAME))
			continue;
		uci_foreach_element(&section->options, oe) {
			option = uci_to_option(oe);
			if (option == NULL)
				continue;
			if (!strcmp(oe->name, "start"))
				start = atoi(option->v.string);
			else if (!strcmp(oe->name, "limit"))
				limit = atoi(option->v.string);
			else if (!strcmp(oe->name, "enable"))
				config.enable = atoi(option->v.string);
			else if (!strcmp(oe->name, "dns1"))
				inet_aton(option->v.string, &config.dns1);
			else if (!strcmp(oe->name, "dns2"))
				inet_aton(option->v.string, &config.dns2);
			else if (!strcmp(oe->name, "time"))
				config.time = atoi(option->v.string);
			else if (!strcmp(oe->name, "shost") && (option->type == UCI_TYPE_LIST)) {
				uci_foreach_element(&option->v.list, e) {
					parse_mac(e->name, static_host[i].mac);
					res = strchr(e->name, ' ');
					if (!res)
						continue;
					res++;
					inet_aton(res, &static_host[i].ip);
					i++;
				}
			} else if (!strcmp(oe->name, "ipmac") && (option->type == UCI_TYPE_LIST)) {
				uci_foreach_element(&option->v.list, e) {
					parse_mac(e->name, ipmac_bind[j].mac);
					res = strchr(e->name, ' ');
					if (!res)
						continue;
					res++;
					inet_aton(res, &ipmac_bind[j].ip);
					j++;
				}
			}
		}
		break;
	}
	uci_unload(ctx, pkg);
	uci_free_context(ctx);
	if (start == -1)
		start = 50;
	if (!limit == -1)
		limit = 200;
	if (config.enable == -1)
		config.enable = 1;
	if (config.time == 0)
		config.time = 12;
	config.start.s_addr = htonl((ntohl(config.ip.s_addr) & ntohl(config.mask.s_addr)) | start);
	config.end.s_addr = htonl((ntohl(config.ip.s_addr) & ntohl(config.mask.s_addr)) | (start + limit -1));
	return dnsmasq_set_ipmac_bind(ipmac_bind);
}

int dnsmasq_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;

	switch (msgid) {
	case DNSMASQ_MOD_INIT:
		ret = dnsmasq_init();
		break;
	case DNSMASQ_DHCP_SET:
		if (!data || len != sizeof(struct dhcp_conf))
			return -1;
		ret = dnsmasq_set_ipmac_bind(ipmac_bind);
		if (!ret) {
			memcpy(&config, data, sizeof(config));
			dnsmasq_save_config(&config);
		}
		break;
	case DNSMASQ_DDNS_SHOW:
		if (!rbuf || rlen != sizeof(struct ddns_status))
			return -1;
		ret = ddns_get_status();
		memcpy(rbuf, &ddnsst, sizeof(ddnsst));
		break;
	case DNSMASQ_DDNS_SET:
		ret = ddns_set((struct ddns_info*)data);
		
		break;
	case DNSMASQ_DHCP_SHOW:
		if (!rbuf || rlen != sizeof(struct dhcp_conf))
			return -1;
		memcpy(rbuf, &config, sizeof(config));
		ret = 0;
		break;	
	case DNSMASQ_DDNS_RESTART:
		ret = ddns_restart();
		break;
	case DNSMASQ_SHOST_SET:
		if (!data || len != sizeof(static_host))
			return -1;
		ret = dnsmasq_set_static_hosts((struct ip_mac *) data);
		if (!ret)
			dnsmasq_save_config(&config);
		break;
	case DNSMASQ_SHOST_SHOW:
		if (!rbuf || rlen != sizeof(static_host))
			return -1;
		memcpy(rbuf, static_host, sizeof(static_host));
		ret = 0;
		break;
	case DNSMASQ_IPMAC_SET:
		if (!data || len != sizeof(ipmac_bind))
			return -1;
		ret = dnsmasq_set_ipmac_bind((struct ip_mac *) data);
		if (!ret)
			dnsmasq_save_config(&config);
		break;
	case DNSMASQ_IPMAC_SHOW:
		if (!rbuf || rlen != sizeof(ipmac_bind))
			return -1;
		memcpy(rbuf, ipmac_bind, sizeof(ipmac_bind));
		ret = 0;
		break;
	default:
		break;
	}
	
	return ret;
}
