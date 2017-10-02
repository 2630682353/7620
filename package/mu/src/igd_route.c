#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "igd_route.h"
#include "igd_interface.h"
#include "igd_lib.h"

#define ROUTE_CONFIG "qos_rule"
#define ROUTE_SECTION "route"
#define ROUTE_TABLE_NUM 3
#if 0
#define ROUTE_ERR(fmt,args...) do{}while(0)
#else
#define ROUTE_ERR(fmt,args...) do {\
	igd_log("/tmp/route_log", "%s[%d]:"fmt, __FUNCTION__, __LINE__, ##args);\
} while(0)
#endif
LIST_HEAD(igd_route_list);
int igd_route_num = 0;

static struct igd_route_param *route_search(struct igd_route_param *info)
{
	struct igd_route_param *tmp = NULL;

	list_for_each_entry(tmp, &igd_route_list, list)
		if (!strcmp(tmp->dst_ip, info->dst_ip)
				&& !strcmp(tmp->gateway, info->gateway)
				&& !strcmp(tmp->netmask, info->netmask)
				&& tmp->dev_flag == info->dev_flag)
			return tmp;
	return NULL;
}

static int route_test_bit(unsigned char *s_addr, int offset)
{
	int i = offset / 8;
	unsigned char mask = 0x1 << (7 - offset % 8);
	unsigned char *p = s_addr + i;
	return (*p & mask);
}

static int get_netmask_bit(char *netmask)
{
	struct in_addr addr;
	int i = 0;
	unsigned char *s_addr = NULL;
	int bit_num = 0;
	int bit = 0;

	if (!inet_aton(netmask, &addr))
		return -1;
	s_addr = (unsigned char *)&addr.s_addr;
	bit_num = sizeof(__be32) * 8;
	for (i = 0; i < bit_num; i++) {
		if (route_test_bit(s_addr, i))
			bit++;
		else
			break;
	}
	return bit;
}

static int route_add(struct igd_route_param *info, int len, int load)
{
	char cmd[512] = {0};
	struct igd_route_param *tmp = NULL;
	int netmask_bit_len = 0;

	if (sizeof(*info) != len) {
		ROUTE_ERR("input err, %d,%d\n", sizeof(*info), len);
		return -1;
	}
	if (igd_route_num >= ROUTE_MAX)
		return -3;
	if (route_search(info)) {
		ROUTE_ERR("route exists, %s,%s,%d\n", 
				info->dst_ip, info->netmask,
			       	info->dev_flag);
		return -2;
	}
	if (!info->enable)
		goto next;
	netmask_bit_len = get_netmask_bit(info->netmask);
	if (netmask_bit_len < 0)
		return -1;
	snprintf(cmd, sizeof(cmd), "ip route add %s/%d dev %s via %s table %d",
		       	info->dst_ip, netmask_bit_len, info->dev_flag ? WAN1_DEVNAME : LAN_DEVNAME,
		       	info->gateway, ROUTE_TABLE_NUM);
	if (system(cmd)) {
		ROUTE_ERR("system err, cmd:%s\n", cmd);
		return -4;
	}
next:
	tmp = (struct igd_route_param *)malloc(sizeof(struct igd_route_param));
	if (!tmp) {
		ROUTE_ERR("malloc err\n");
		return -1;
	}
	memcpy(tmp, info, sizeof(*tmp));
	list_add(&tmp->list, &igd_route_list);
	igd_route_num++;
	if (load)
		return 0;
	snprintf(cmd, sizeof(cmd), "%s.%s.rt=%s,%s,%s,%d,%d", 
			ROUTE_CONFIG, ROUTE_SECTION, 
			info->dst_ip, info->gateway, 
			info->netmask, info->dev_flag, 
			info->enable);
	if (uuci_add_list(cmd)) {
		free(tmp);
		ROUTE_ERR("save fail, cmd:%s\n", cmd);
		return -1;
	}
	return 0;
}

static int route_del(struct igd_route_param *info, int len)
{
	char cmd[512] = {0};
	struct igd_route_param *tmp = NULL;
	int netmask_bit_len = 0;

	if (sizeof(*info) != len) {
		ROUTE_ERR("input err, %d,%d\n", sizeof(*info), len);
		return -1;
	}
	tmp = route_search(info);
	if (!tmp) {
		ROUTE_ERR("route not exist, %s,%s,%s,%d,%d\n", 
				info->dst_ip, info->netmask, 
				info->gateway, 
				info->dev_flag);
		return -2;
	}
	if (!tmp->enable)
		goto next;
	netmask_bit_len = get_netmask_bit(info->netmask);
	if (netmask_bit_len < 0)
		return -4;
	snprintf(cmd, sizeof(cmd), "ip route del %s/%d dev %s via %s table %d",
		       	info->dst_ip, netmask_bit_len, info->dev_flag ? WAN1_DEVNAME : LAN_DEVNAME,
		       	info->gateway, ROUTE_TABLE_NUM);
	if (system(cmd)) {
		ROUTE_ERR("system err, cmd:%s\n", cmd);
		return -3;
	}
next:
	snprintf(cmd, sizeof(cmd), "%s.%s.rt=%s,%s,%s,%d,%d", 
			ROUTE_CONFIG, ROUTE_SECTION, 
			tmp->dst_ip, tmp->gateway,
			tmp->netmask, tmp->dev_flag, 
			tmp->enable);
	if (uuci_del_list(cmd)) {
		ROUTE_ERR("del fail, cmd:%s\n", cmd);
		return -1;
	}
	igd_route_num--;
	list_del(&tmp->list);
	free(tmp);
	return 0;
}

static int route_dump(struct igd_route_param *info, int len)
{
	struct igd_route_param *tmp = NULL;
	int i = 0;

	if (sizeof(*info) * ROUTE_MAX != len) {
		ROUTE_ERR("input err, %d,%d\n", sizeof(*info) * ROUTE_MAX, len);
		return -1;
	}
	list_for_each_entry(tmp, &igd_route_list, list) {
		memcpy(&info[i], tmp, sizeof(*tmp));
		i++;
	}
	return i;
}

static int add_route(struct uci_element *e)
{
	struct igd_route_param tmp;
	int ret = 0;

	memset(&tmp, 0, sizeof(tmp));
	ret = sscanf(e->name, "%[^,],%[^,],%[^,],%d,%d", 
				tmp.dst_ip, tmp.gateway,
				tmp.netmask, &tmp.dev_flag, 
				&tmp.enable);
	if (ret != 5) {
		ROUTE_ERR("sscanf err, %d, %s\n", ret, e->name);
		return -1;
	}
	if (route_add(&tmp, sizeof(tmp), 1))
		return -1;
	return 0;
}

static int route_load_section(struct uci_section *s)
{
	struct uci_element *oe = NULL;
	struct uci_option *o = NULL;
	struct uci_element *e = NULL;

	uci_foreach_element(&s->options, oe) {
		o = uci_to_option(oe);
		if (o->type != UCI_TYPE_LIST) {
			ROUTE_ERR("not list op:%s\n", oe->name);
			continue;
		}
		uci_foreach_element(&o->v.list, e)
			add_route(e);
	}
	return 0;
}

static int route_load(void)
{
	struct uci_package *pkg = NULL;
	struct uci_context *ctx = NULL;
	struct uci_element *se = NULL;
	struct uci_section *s = NULL;
	int found = 0;

	ctx = uci_alloc_context();
	if (0 != uci_load(ctx, ROUTE_CONFIG, &pkg)) {
		uci_free_context(ctx);
		ROUTE_ERR("uci load %s fail\n", ROUTE_CONFIG);
		return -1;
	}
	uci_foreach_element(&pkg->sections, se) {
		s = uci_to_section(se);
		if (strcmp(se->name, ROUTE_SECTION))
			continue;
		route_load_section(s);
		found = 1;
		break;
	}
	if (!found)
		if (uuci_set(ROUTE_CONFIG"."ROUTE_SECTION"=host"))
			return -1;
	uci_unload(ctx,pkg);
	uci_free_context(ctx);
	return 0;
}

static int route_init(void)
{
	char cmd[512] = {0};

	snprintf(cmd, sizeof(cmd), 
			"ip rule add prio 200 table %d", ROUTE_TABLE_NUM);
	if (system(cmd)) {
		ROUTE_ERR("add table err, cmd:%s\n", cmd);
		return -1;
	}
	return route_load();
}

static int route_reload(void)
{
	struct igd_route_param *tmp = NULL;
	struct igd_route_param *n = NULL;

	list_for_each_entry_safe(tmp, n, &igd_route_list, list) {
		list_del(&tmp->list);
		free(tmp);
	}
	if (exec_cmd("ip rule del table %d", ROUTE_TABLE_NUM))
		return -1;
	if (exec_cmd("ip route flush table %d", ROUTE_TABLE_NUM))
		return -1;
	return route_init();
}

int route_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	switch (msgid) {
	case ROUTE_MOD_INIT:
		return route_init();
	case ROUTE_MOD_ADD:
		return route_add(data, len, 0);
	case ROUTE_MOD_DEL:
		return route_del(data, len);
	case ROUTE_MOD_DUMP:
		return route_dump(rbuf, rlen);
	case ROUTE_MOD_RELOAD:
		return route_reload();
	default:
		break;
	}
	return 0;
}
