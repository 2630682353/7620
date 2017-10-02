#include "igd_lib.h"
#include "igd_nat.h"
#include "igd_interface.h"
#include "uci.h"
#include "uci_fn.h"

#define NAT_CONFIG  "qos_rule"
#define NAT_SECTION  "nat"

#if 0
#define NAT_ERR(fmt,args...) do{}while(0)
#else
#define NAT_ERR(fmt,args...) do {\
	igd_log("/tmp/nat_log", "%s[%d]:"fmt, __FUNCTION__, __LINE__, ##args);\
} while(0)
#endif

static unsigned char nat_section_flag = 0;
static unsigned char igd_nat_num = 0;
static struct list_head igd_nat_list = LIST_HEAD_INIT(igd_nat_list);

static int nat_get_ip(unsigned char *mac, struct in_addr *ip)
{
	int nr = 0, i = 0;
	struct host_info *host = NULL;

	ip->s_addr = 0;
	host = dump_host_alive(&nr);
	if (!host)
		return -1;

	for (i = 0; i < nr; i++) {
		if (!memcmp(host[i].mac, mac, 6)) {
			ip->s_addr = host[i].addr.s_addr;
			break;
		}			
	}
	free(host);
	if (i >= nr)
		return -1;
	return 0;
}

static int nat_chain(int add, struct igd_nat_param *inp)
{
	int uid = 1;
	struct iface_info winfo;

	if (!inp->ip.s_addr)
		return 0;
	if (add && !inp->enable)
		return 0;

	if (mu_call(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &winfo, sizeof(winfo))) {
		NAT_ERR("call IF_MOD_IFACE_INFO fail\n");
		return -1;
	}

	if (inp->proto == NAT_PROTO_TCP) {
		exec_cmd("iptables -t nat -%c %s -i %s -p %s"
			" --dport %d:%d -j DNAT --to-destination %s:%d-%d",
				add ? 'I' : 'D' , STATIC_DNAT_CHAIN, winfo.devname, 
				"tcp", inp->out_port, inp->out_port_end,
				inet_ntoa(inp->ip), inp->in_port, inp->in_port_end);
	} else if (inp->proto == NAT_PROTO_UDP) {
		exec_cmd("iptables -t nat -%c %s -i %s -p %s"
			" --dport %d:%d -j DNAT --to-destination %s:%d-%d",
				add ? 'I' : 'D' , STATIC_DNAT_CHAIN, winfo.devname, 
				"udp", inp->out_port, inp->out_port_end,
				inet_ntoa(inp->ip), inp->in_port, inp->in_port_end);
	} else if (inp->proto == NAT_PROTO_TCPUDP) {
		exec_cmd("iptables -t nat -%c %s -i %s -p %s"
			" --dport %d:%d -j DNAT --to-destination %s:%d-%d",
				add ? 'I' : 'D' , STATIC_DNAT_CHAIN, winfo.devname, 
				"tcp", inp->out_port, inp->out_port_end,
				inet_ntoa(inp->ip), inp->in_port, inp->in_port_end);
		exec_cmd("iptables -t nat -%c %s -i %s -p %s"
			" --dport %d:%d -j DNAT --to-destination %s:%d-%d",
				add ? 'I' : 'D' , STATIC_DNAT_CHAIN, winfo.devname, 
				"udp", inp->out_port, inp->out_port_end,
				inet_ntoa(inp->ip), inp->in_port, inp->in_port_end);
	} else if (inp->proto == NAT_PROTO_DMZ) {
		exec_cmd("iptables -t nat -%c %s -i %s -j DNAT --to-destination %s",
				add ? 'A' : 'D' , STATIC_DNAT_CHAIN, winfo.devname, 
				inet_ntoa(inp->ip));
	} else {
		NAT_ERR("inp->proto err, %d\n", inp->proto);
		return -1;
	}
	return 0;
}

static int nat_restart(void)
{
	int uid = 1;
	struct iface_info winfo;
	struct igd_nat_param *inp = NULL;

	memset(&winfo, 0, sizeof(winfo));
	if (mu_call(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &winfo, sizeof(winfo))) {
		NAT_ERR("call IF_MOD_IFACE_INFO fail\n");
		return -1;
	}

	exec_cmd("iptables -t nat -F %s", STATIC_DNAT_CHAIN);
	exec_cmd("iptables -t nat -D PREROUTING -i %s -j %s", winfo.devname, STATIC_DNAT_CHAIN);
	exec_cmd("iptables -t nat -A PREROUTING -i %s -j %s", winfo.devname, STATIC_DNAT_CHAIN);

	list_for_each_entry(inp, &igd_nat_list, list) {
		nat_chain(1, inp);
	}
	return 0;
}

static struct igd_nat_param *nat_search(struct igd_nat_param *info)
{
	struct igd_nat_param *inp = NULL;

	list_for_each_entry(inp, &igd_nat_list, list) {
		if (info->proto == NAT_PROTO_DMZ) {
			if (inp->proto == NAT_PROTO_DMZ)
				return inp;
		} else {
			if (inp->proto == NAT_PROTO_DMZ)
				continue;
			if (((inp->out_port >= info->out_port)
				&& (inp->out_port <= info->out_port_end))
				|| ((inp->out_port_end >= info->out_port)
				&& (inp->out_port_end <= info->out_port_end))) {
				if ((info->proto == NAT_PROTO_TCPUDP)
					|| (inp->proto == NAT_PROTO_TCPUDP)) {
					return inp;
				} else if (info->proto == inp->proto) {
					return inp;
				}
			}
		}
	}
	return NULL;
}

static int nat_add(struct igd_nat_param *info, int len, int load)
{
	char cmd[512] = {0};
	struct igd_nat_param *inp = NULL;

	if (sizeof(*info) != len) {
		NAT_ERR("input err, %d,%d\n", sizeof(*info), len);
		return -1;
	}

	if (igd_nat_num >= IGD_NAT_MAX)
		return -3;

	if (nat_search(info)) {
		NAT_ERR("nat is exist, %d -> "
			"%02X:%02X:%02X:%02X:%02X:%02X,%d,%d\n", info->out_port,
			MAC_SPLIT(info->mac), info->in_port, info->proto);
		return -2;
	}

	inp = malloc(sizeof(*info));
	if (!inp) {
		NAT_ERR("malloc fail\n");
		return -1;
	}
	memcpy(inp, info, sizeof(*info));
	nat_get_ip(inp->mac, &inp->ip);

	if (nat_chain(1, inp) < 0) {
		free(inp);
		return -1;
	}

	if (!load) {
		snprintf(cmd, sizeof(cmd),
			"%s.%s.%02X%02X%02X%02X%02X%02X="
			"%hd,%hd,%hhd,%hhd,%hd,%hd",
			NAT_CONFIG, NAT_SECTION, MAC_SPLIT(inp->mac),
			inp->out_port, inp->in_port, inp->proto,
			inp->enable, inp->out_port_end, inp->in_port_end);

		if (uuci_add_list(cmd)) {
			NAT_ERR("save fail, cmd:%s\n", cmd);
			free(inp);
			return -1;
		}
	}

	igd_nat_num++;
	list_add(&inp->list, &igd_nat_list);
	return 0;
}

static int nat_del(struct igd_nat_param *info, int len)
{
	char cmd[512] = {0};
	struct igd_nat_param *inp = NULL;

	if (sizeof(*info) != len) {
		NAT_ERR("input err, %d,%d\n", sizeof(*info), len);
		return -1;
	}

	inp = nat_search(info);
	if (!inp) {
		NAT_ERR("nat is nonexist, %d -> "
			"%02X:%02X:%02X:%02X:%02X:%02X,%d,%d\n", info->out_port,
			MAC_SPLIT(info->mac), info->in_port, info->proto);
		return -2;
	}

	if (nat_chain(0, inp) < 0)
		return -1;

	snprintf(cmd, sizeof(cmd),
		"%s.%s.%02X%02X%02X%02X%02X%02X="
		"%hd,%hd,%hhd,%hhd,%hd,%hd",
		NAT_CONFIG, NAT_SECTION, MAC_SPLIT(inp->mac),
		inp->out_port, inp->in_port, inp->proto,
		inp->enable, inp->out_port_end, inp->in_port_end);

	if (uuci_del_list(cmd)) {
		NAT_ERR("del fail, cmd:%s\n", cmd);
		return -1;
	}

	igd_nat_num--;
	list_del(&inp->list);
	free(inp);
	return 0;
}

static int nat_dump(unsigned char *mac, int mac_len, struct igd_nat_param *info, int len)
{
	int num = 0;
	struct igd_nat_param *inp = NULL;

	if (sizeof(*info)*IGD_NAT_MAX != len 
		|| mac_len != 6 || !mac) {
		NAT_ERR("input err, %d,%d,%d,%p\n", 
			sizeof(*info), len, mac_len, mac);
		return -1;
	}

	list_for_each_entry(inp, &igd_nat_list, list) {
		if (memcmp(mac, inp->mac, 6))
			continue;
		memcpy(&info[num], inp, sizeof(*info));
		num++;
	}
	return num;
}

static int nat_host(struct nlk_host *nlk, int on)
{
	struct igd_nat_param *inp = NULL;

	list_for_each_entry(inp, &igd_nat_list, list) {
		if (memcmp(inp->mac, nlk->mac, 6))
			continue;
		if (on) {
			if (inp->ip.s_addr)
				nat_chain(0, inp);
			inp->ip.s_addr = nlk->addr.s_addr;
			nat_chain(1, inp);
		} else if (inp->ip.s_addr != nlk->addr.s_addr) {
			continue;
		} else {
			nat_chain(0, inp);
			inp->ip.s_addr = 0;
		}
	}
	return 0;
}

static int nat_mac_del(unsigned char *mac, int len)
{
	struct igd_nat_param *inp, *_inp;

	if (len != 6) {
		NAT_ERR("input err, %d\n", len);
		return -1;
	}

	list_for_each_entry_safe(inp, _inp, &igd_nat_list, list) {
		if (memcmp(inp->mac, mac, 6))
			continue;
		nat_del(inp, sizeof(*inp));
	}
	return 0;
}

static int str_to_mac(char *str, unsigned char *mac)
{
	int i = 0, j = 0;
	unsigned char v = 0;

	for (i = 0; i < 17; i++) {
		if (str[i] >= '0' && str[i] <= '9') {
			v = str[i] - '0';
		} else if (str[i] >= 'a' && str[i] <= 'f') {
			v = str[i] - 'a' + 10;
		} else if (str[i] >= 'A' && str[i] <= 'F') {
			v = str[i] - 'A' + 10;
		} else if (str[i] == ':' || str[i] == '-' ||
					str[i] == ',' || str[i] == '\r' ||
					str[i] == '\n') {
			continue;
		} else if (str[i] == '\0') {
			return 0;
		} else {
			return -1;
		}
		if (j%2)
			mac[j/2] += v;
		else
			mac[j/2] = v*16;
		j++;
		if (j/2 > 5)
			break;
	}
	return 0;
}

static int nat_load_section(struct uci_section *s)
{
	struct uci_element *oe = NULL;
	struct uci_option *o = NULL;
	struct uci_element *e = NULL;
	struct igd_nat_param tmp;

	nat_section_flag = 1;

	uci_foreach_element(&s->options, oe) {
		o = uci_to_option(oe);
		if (o->type != UCI_TYPE_LIST) {
			NAT_ERR("not list op:%s\n", oe->name);
			continue;
		}
		memset(&tmp, 0, sizeof(tmp));
		if (str_to_mac(oe->name, tmp.mac)) {
			NAT_ERR("mac err, op:%s\n", oe->name);
			continue;
		}
		//NAT_ERR("mac:%s\n", oe->name);
		uci_foreach_element(&o->v.list, e) {
			//NAT_ERR("value:%s\n", e->name);
			if (sscanf(e->name, "%hd,%hd,%hhd,%hhd,%hd,%hd", 
				&tmp.out_port, &tmp.in_port, &tmp.proto,
				&tmp.enable, &tmp.out_port_end, &tmp.in_port_end) == 6) {
				;
			} else if (sscanf(e->name, "%hd,%hd,%hhd,%hhd", 
				&tmp.out_port, &tmp.in_port, &tmp.proto, &tmp.enable) == 4) {
				tmp.out_port_end = tmp.out_port;
				tmp.in_port_end = tmp.in_port;
			} else {
				NAT_ERR("mac err, op:%s:%s\n", oe->name, e->name);
				continue;
			}
			nat_add(&tmp, sizeof(tmp), 1);
		}
	}
	return 0;
}

static int nat_load(void)
{
	struct uci_package *pkg = NULL;
	struct uci_context *ctx = NULL;
	struct uci_element *se = NULL;
	struct uci_section *s = NULL;

	ctx = uci_alloc_context();
	if (0 != uci_load(ctx, NAT_CONFIG, &pkg)) {
		uci_free_context(ctx);
		NAT_ERR("uci load %s fail\n", NAT_CONFIG);
		return -1;
	}

	uci_foreach_element(&pkg->sections, se) {
		s = uci_to_section(se);
		if (!strcmp(se->name, NAT_SECTION)) {
			nat_load_section(s);
		}
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx);
	return 0;
}

int nat_init(void)
{
	nat_section_flag = 0;
	igd_nat_num = 0;

	exec_cmd("iptables -t nat -N %s", STATIC_DNAT_CHAIN);
	nat_load();

	if (!nat_section_flag)
		uuci_set(NAT_CONFIG"."NAT_SECTION"=host");
	return 0;
}

int nat_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	switch (msgid) {
	case NAT_MOD_INIT:
		return nat_init();
	case NAT_MOD_ADD:
		return nat_add(data, len, 0);
	case NAT_MOD_DEL:
		return nat_del(data, len);
	case NAT_MOD_DUMP:
		return nat_dump(data, len, rbuf, rlen);
	case NAT_MOD_RESTART:
		return nat_restart();
	case NAT_MOD_HOSTUP:
		return nat_host(data, 1);
	case NAT_MOD_HOSTDOWN:
		return nat_host(data, 0);
	case NAT_MOD_MAC_DEL:
		return nat_mac_del(data, len);
	default:
		break;
	}
	return 0;
}

