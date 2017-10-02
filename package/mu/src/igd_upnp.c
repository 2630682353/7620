#include "igd_lib.h"
#include "igd_interface.h"
#include "igd_dnsmasq.h"
#include "igd_upnp.h"
#include <linux/types.h>
#include <libiptc/libiptc.h>
#include <iptables.h>

static struct list_head upnpd_h;
static struct schedule_entry *upnp_cb;
static int config_status = 1;
static int server_status = 1;

#define UPNPD_LOG_PATH  "/tmp/upnp.log"
#ifdef FLASH_4_RAM_32
#define UPNPD_DEBUG(fmt,args...) do{}while(0)
#else
#define UPNPD_DEBUG(fmt,args...) do{igd_log(UPNPD_LOG_PATH, fmt, ##args);}while(0)
#endif

static int get_ipt_rule_state(struct upnpd *upnp, struct rule_state *state)
{
	int ret = -1;
	IPTC_HANDLE h;
	const struct ipt_entry * e;
	const struct ipt_entry_target * target;
	const struct ip_nat_multi_range * mr;
	const struct ipt_entry_match *match;

	h = iptc_init("nat");
	if (!h) {
		UPNPD_DEBUG("get_redirect_rule() :iptc_init() failed");
		return -1;
	}
	
	if (!iptc_is_chain(UPNNPD_CHAIN, h)) {
		UPNPD_DEBUG("chain %s not found", UPNNPD_CHAIN);
		return -1;
	}

	for (e = iptc_first_rule(UPNNPD_CHAIN, h); e; e = iptc_next_rule(e, h)) {
		if (upnp->proto != e->ip.proto) {
			UPNPD_DEBUG("upnp protocol is not match, proto is %d , %d\n", upnp->proto, e->ip.proto);
			continue;
		}

		match = (const struct ipt_entry_match *)&e->elems;
		if (0 == strncmp(match->u.user.name, "tcp", IPT_FUNCTION_MAXNAMELEN)) {
			const struct ipt_tcp * info;
			info = (const struct ipt_tcp *)match->data;
			if (upnp->out.port != info->dpts[0]) {
				UPNPD_DEBUG("upnp tcp port is not match, eport is %d, %d\n", upnp->out.port, info->dpts[0]);
				continue;
			}
		} else {
			const struct ipt_udp * info;
			info = (const struct ipt_udp *)match->data;
			if(upnp->out.port != info->dpts[0]) {
				UPNPD_DEBUG("upnp udp port is not match, eport is %d, %d\n", upnp->out.port, info->dpts[0]);
				continue;
			}
		}
		target = (void *)e + e->target_offset;
		//target = ipt_get_target(e);
		mr = (const struct ip_nat_multi_range *)&target->data[0];
		if (ntohl(upnp->in.ip.s_addr) != ntohl(mr->range[0].min_ip)) {
			UPNPD_DEBUG("upnp iaddr is not match, iaddr is %u.%u.%u.%u, %u.%u.%u.%u\n",
							NIPQUAD(upnp->in.ip.s_addr), NIPQUAD(mr->range[0].min_ip));
			continue;
		}
		if (upnp->in.port != ntohs(mr->range[0].min.all)) {
			UPNPD_DEBUG("upnp iport is not match, iport is %d, %d\n", upnp->in.port, ntohs(mr->range[0].min.all));
			continue;
		}
		state->packets = e->counters.pcnt;
		state->bytes = e->counters.bcnt;
		ret = 0;
	}
	if(h)
		iptc_free(h);
	return ret;
}

static int ipt_rule_action(struct upnpd *upnp, int action)
{
	char proto[16];
	if (upnp->proto == IPPROTO_TCP)
		arr_strcpy(proto, "tcp");
	else if (upnp->proto == IPPROTO_UDP)
		arr_strcpy(proto, "udp");
	switch (action) {
	case NLK_ACTION_ADD:
		exec_cmd("iptables -t nat -A %s -p %s --dport %d -j DNAT --to %s:%d", UPNNPD_CHAIN,
					proto, upnp->out.port, inet_ntoa(upnp->in.ip), upnp->in.port);
		break;
	case NLK_ACTION_DEL:
		exec_cmd("iptables -t nat -D %s -p %s --dport %d -j DNAT --to %s:%d",UPNNPD_CHAIN,
					proto, upnp->out.port, inet_ntoa(upnp->in.ip), upnp->in.port);
		break;
	default:
		return -1;
	}
	return 0;
}

static int del_host_upnpd_rule(struct nlk_host *host)
{
	struct upnpd_info *tmp, *upnp;
	
	list_for_each_entry_safe(upnp, tmp, &upnpd_h, list) {
		if (upnp->upnp.in.ip.s_addr == host->addr.s_addr) {
			ipt_rule_action(&upnp->upnp, NLK_ACTION_DEL);
			list_del(&upnp->list);
			free(upnp);
		}
	}
	return 0;
}

static int upnpd_rule_action(struct mu_upnpd *info, struct upnpd *rinfo)
{
	int nr = 0;
	struct upnpd_info *tmp, *cur;
	struct upnpd *upnp = &info->upnp;

	switch (info->action) {
	case NLK_ACTION_ADD:
		list_for_each_entry(tmp, &upnpd_h, list) {
			if (memcmp(&tmp->upnp, upnp, sizeof(struct upnpd)))
				continue;
			return 0;
		}
		tmp = malloc(sizeof(struct upnpd_info));
		memset(tmp, 0x0, sizeof(struct upnpd_info));
		tmp->upnp = *upnp;
		list_add(&tmp->list, &upnpd_h);
		ipt_rule_action(&tmp->upnp, NLK_ACTION_ADD);
		UPNPD_DEBUG("add upnpd proto is %d, iaddr is %s, iport is %d, eport is %d, dsec is %s\n", tmp->upnp.proto,
							inet_ntoa(tmp->upnp.in.ip), tmp->upnp.in.port, tmp->upnp.out.port, tmp->upnp.desc);
		break;
	case NLK_ACTION_DEL:
		list_for_each_entry_safe(cur, tmp, &upnpd_h, list) {
			if ((cur->upnp.out.port != upnp->out.port) || (cur->upnp.proto != upnp->proto))
				continue;
			ipt_rule_action(&cur->upnp, NLK_ACTION_DEL);
			list_del(&cur->list);
			UPNPD_DEBUG("del upnpd proto is %d, iaddr is %s, iport is %d, eport is %d, dsec is %s\n", cur->upnp.proto,
							inet_ntoa(cur->upnp.in.ip), cur->upnp.in.port, cur->upnp.out.port, cur->upnp.desc);
			free(cur);
		}
		break;
	case NLK_ACTION_DUMP:
		list_for_each_entry(cur, &upnpd_h, list) {
			if (info->flag) {
				if (nr == info->index)
					goto match;
			} else {
				if ((cur->upnp.out.port == upnp->out.port) && (cur->upnp.proto == upnp->proto))
					goto match;
			}
			nr++;
			continue;
	match:
			memcpy(rinfo, &cur->upnp, sizeof(struct upnpd));
			return 0;
		}
		return -1;
	default:
		break;
	}
	return 0;
}

static void upnpd_get_uuid(char *uuid, int len, unsigned char *mac)
{
	unsigned int U8, U41, U42, U21, U22;
	struct timeval now;

	gettimeofday(&now, NULL);
	U8 = now.tv_sec;
	U41 = now.tv_usec;
	gettimeofday(&now, NULL);
	U42 = now.tv_usec;
	gettimeofday(&now, NULL);
	U21 = now.tv_usec;
	gettimeofday(&now, NULL);
	U22 = now.tv_usec;

	snprintf(uuid, len, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		U8, U41 & 0xFFFF, U42 & 0xFFFF, U21 & 0xFF, U22 & 0xFF, MAC_SPLIT(mac));
}

static int upnpd_write_conf(struct iface_info *info)
{
	int i, nr = 0;
	FILE *fp = NULL;
	char uuid[128];
	
	for (i = 0; i < 32; i++) {
		if (ntohl(info->mask.s_addr) & (1 << i))
			nr++;
	}

	memset(uuid, 0x0, sizeof(uuid));
	upnpd_get_uuid(uuid, sizeof(uuid), info->mac);

	fp = fopen(UPNPD_CONFIG_FILE, "w");
	if (NULL == fp)
		return -1;
	fprintf(fp,
		"enable_natpmp=yes\n"
		"enable_upnp=yes\n"
		"bitrate_up=1000000\n"
		"bitrate_down=10000000\n"
		"secure_mode=no\n"
		"system_uptime=yes\n"
		"notify_interval=30\n"
		"clean_ruleset_interval=600\n"
		"uuid=%s\n"
		"serial=12345678\n"
		"model_number=1\n"
		"allow 1024-65535 %s/%d 1024-65535\n"
		"deny 0-65535 0.0.0.0/0 0-65535\n",
		uuid, inet_ntoa(info->ip), nr);
	fclose(fp);
	return 0;

}

static void upnpd_cb(void *data)
{
	struct timeval tv;
	struct rule_state state;
	struct upnpd_info *tmp, *cur;

	list_for_each_entry_safe(cur, tmp, &upnpd_h, list) {
		memset(&state, 0x0, sizeof(state));
		if (get_ipt_rule_state(&cur->upnp, &state))
			continue;
		if (cur->state.packets != state.packets || cur->state.bytes != state.bytes) {
			cur->state = state;
			continue;
		}
		ipt_rule_action(&cur->upnp, NLK_ACTION_DEL);
		list_del(&cur->list);
		free(cur);
	}
	tv.tv_sec = 3600;
	tv.tv_usec = 0;
	upnp_cb = schedule(tv, upnpd_cb, NULL);
}

static int upnpd_write_enable(int *enable, int len)
{
	char buf[32] = {0};

	if (sizeof(*enable) != len)
		return -1;
	snprintf(buf, sizeof(buf), "dhcp.wan.upnp=%d", *enable);
	if (uuci_set(buf))
		return -1;
	config_status = *enable;
	return 0;
}

static int upnpd_restart(int enable)
{
	int uid = 0;
	struct timeval tv;
	struct schedule_entry *cb;
	struct iface_info linfo, winfo;
	struct upnpd_info *tmp, *upnpd;
	
	if (upnp_cb) {
		dschedule(upnp_cb);
		upnp_cb = NULL;
	}
	if (mu_call(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &linfo, sizeof(linfo)))
		return -1;
	uid = 1;
	if (mu_call(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &winfo, sizeof(winfo)))
		return -1;
	if (upnpd_write_conf(&linfo))
		return -1;
	exec_cmd("killall -kill miniupnpd");
	exec_cmd("iptables -t nat -F %s", UPNNPD_CHAIN);
	list_for_each_entry_safe(upnpd, tmp, &upnpd_h, list) {
		list_del(&upnpd->list);
		free(upnpd);
	}
	INIT_LIST_HEAD(&upnpd_h);
	remove(UPNPD_PID_FILE);
	exec_cmd("iptables -t nat -D PREROUTING -i %s -j %s", winfo.devname, UPNNPD_CHAIN);
	server_status = 0;
	if (!enable)
		return 0;
	tv.tv_sec = 600;
	tv.tv_usec = 0;
	cb = schedule(tv, upnpd_cb, NULL);
	if (!cb)
		return -1;
	upnp_cb = cb;
	exec_cmd("iptables -t nat -A PREROUTING -i %s -j %s", winfo.devname, UPNNPD_CHAIN);
	exec_cmd("miniupnpd -i %s -a %s -f %s -P %s &", winfo.devname, inet_ntoa(linfo.ip), UPNPD_CONFIG_FILE, UPNPD_PID_FILE);
	system("sleep 1 && echo 499 > /proc/$(pidof miniupnpd)/oom_score_adj &");
	server_status = 1;
	return 0;
}

static int upnpd_set(int *enable, int len)
{
	if (sizeof(*enable) != len)
		return -1;
	if (*enable && server_status)
		return 0;
	if (!(*enable) && !server_status)
		return 0;
	return upnpd_restart(*enable);
}

static int upnpd_get(int *upnpd_state, int rlen)
{
	if (rlen != sizeof(*upnpd_state))
		return -1;
	*upnpd_state = server_status;
	return 0;
}

static int load_upnp_cfg(void)
{
	char **val = NULL;
	int num = 0;
	int enable = 1;

	if (!uuci_get("dhcp.wan.upnp", &val, &num))
		enable = atoi(val[0]);
	uuci_get_free(val, num);
	return  enable;
}

static int upnpd_init(void)
{
	INIT_LIST_HEAD(&upnpd_h);
	exec_cmd("iptables -t nat -N %s", UPNNPD_CHAIN);
	config_status = load_upnp_cfg();
	return 0;
}

int upnpd_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;

	switch (msgid) {
	case UPNPD_MOD_INIT:
		ret = upnpd_init();
		break;
	case UPNPD_SERVER_SET:
		ret = upnpd_restart(config_status);
		break;
	case UPNPD_STATE_SET:
		ret = upnpd_set(data, len);
		if (!ret)
			upnpd_write_enable(data, len);
		break;
	case UPNPD_STATE_GET:
		ret = upnpd_get(rbuf, rlen);
		break;
	case UPNPD_RULE_ADD:
	case UPNPD_RULE_DEL:
		if (!data || len != sizeof(struct mu_upnpd))
			return -1;
		ret = upnpd_rule_action((struct mu_upnpd *)data, NULL);
		break;
	case UPNPD_RULE_DUMP:
		if (!data || len != sizeof(struct mu_upnpd))
			return -1;
		if (!rbuf || rlen != sizeof(struct upnpd))
			return -1;
		ret = upnpd_rule_action((struct mu_upnpd *)data, (struct upnpd *)rbuf);
	case UPNPD_HOSTOFFLINE:
		if (!data || len != sizeof(struct nlk_host))
			return -1;
		ret = del_host_upnpd_rule((struct nlk_host *)data);
		break;
	default:
		break;
	}
	
	return ret;
}
