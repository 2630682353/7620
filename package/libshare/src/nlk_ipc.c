#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <assert.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/if_ether.h>
#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <stdarg.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/wait.h>
#include "nlk_ipc.h"
#include "ipc_msg.h"
#include "aes.h"
#include "uci_fn.h"
#include "ioos_uci.h"


int get_sys_info(struct sys_info *info)
{
	info->http_mmap_size = HTTP_MMAP_SIZE;
	return 0;
}

/* 
input:  type(0:LAN, 1:WAN1, 2:WAN2, ...)
output:  stat
return:  0:succ, <0:fail
*/
int get_if_statistics(int type, struct if_statistics *stat)
{
	struct nlk_msg_comm nlk;

	if (!stat)
		return IGD_ERR_DATA_ERR;

	memset(&nlk, 0, sizeof(nlk));
	nlk.key = STATISTICS_PKT;
	nlk.id = type;
	nlk.action = NLK_ACTION_DUMP;

	return nlk_head_send_recv(NLK_MSG_GET_STATISTICS,
		&nlk, sizeof(nlk), stat, sizeof(*stat));
}

/* set bitmap bit 0 if port 0 link, and so on... */
int get_link_status(unsigned long *bitmap)
{
	struct nlk_msg_comm nlk;

	if (!bitmap) 
		return IGD_ERR_DATA_ERR;
	memset(&nlk, 0, sizeof(nlk));
	nlk.key = STATISTICS_LINK;
	nlk.action = NLK_ACTION_DUMP;

	return nlk_head_send_recv(NLK_MSG_GET_STATISTICS,
		       	&nlk, sizeof(nlk), bitmap, sizeof(*bitmap));
}

unsigned int get_usrid(void)
{
	unsigned int usr;
	long long id;
	char*usrid;
	usrid = uci_getval("qos_rule","id","usrid");
	if(usrid == NULL){
		return 0;
	}
	id =atoll(usrid);
	usr = (unsigned int)id;
	free(usrid);
	return usr;
}

int set_usrid(unsigned int usrid)
{
	char cmdbuf[64];
	sprintf(cmdbuf,"qos_rule.id.usrid=%u",usrid);
	return uuci_set(cmdbuf);
}

/* 
 * dump host runing app info * set *nr to dump nr,
 * and return success, caller need free return pointer
 * return NULL if failed 
 */
struct app_conn_info *dump_host_app(struct in_addr addr, int *nr)
{
	struct nlk_host nlk;
	struct app_conn_info *info;
	int size;

	memset(&nlk, 0, sizeof(nlk));
	if (!nr) 
		return NULL;
	size = sizeof(struct app_conn_info) * CONN_DUMP_MX;
	info = malloc(size);
	*nr = IGD_ERR_NO_MEM;
	if (!info)
		return NULL;
	nlk.addr = addr;
	nlk.comm.key = NLK_HOST_APP;
	nlk.comm.action = NLK_ACTION_DUMP;
	nlk.comm.obj_nr = 40;
	nlk.comm.obj_len = sizeof(struct app_conn_info);
	*nr = nlk_dump_from_kernel(NLK_MSG_HOST, &nlk,
		       	sizeof(nlk), info, CONN_DUMP_MX);
	if (*nr > 0)
		return info;
	free(info);
	return NULL;
}
/* 
 * set *nr to host conn cnt, and return 
 * success, caller need free return pointer;
 * return NULL if failed 
 */
struct conn_info *dump_host_conn(struct in_addr addr, int *nr)
{
	struct nlk_host nlk;
	struct conn_info *info;
	int size;

	memset(&nlk, 0, sizeof(nlk));
	if (!nr) 
		return NULL;
	size = sizeof(struct conn_info) * CONN_DUMP_MX;
	info = malloc(size);
	*nr = IGD_ERR_NO_MEM;
	if (!info)
		return NULL;
	nlk.addr = addr;
	nlk.comm.key = NLK_HOST_CONN;
	nlk.comm.action = NLK_ACTION_DUMP;
	nlk.comm.obj_nr = 40;
	nlk.comm.obj_len = sizeof(struct conn_info);
	*nr = nlk_dump_from_kernel(NLK_MSG_HOST, &nlk,
		       	sizeof(nlk), info, CONN_DUMP_MX);
	if (*nr > 0)
		return info;
	free(info);
	return NULL;
}

int dump_conn_info(int id, struct conn_info *ci)
{
	struct nlk_msg_comm nlk;

	if (!ci)
		return IGD_ERR_DATA_ERR;

	memset(&nlk, 0, sizeof(nlk));
	nlk.key = STATISTICS_CONN;
	nlk.id = id;
	nlk.action = NLK_ACTION_DUMP;

	return nlk_head_send_recv(NLK_MSG_GET_STATISTICS,
		&nlk, sizeof(nlk), ci, sizeof(*ci));
}

int dump_host_info(struct in_addr addr, struct host_info *info)
{
	struct nlk_host nlk;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.key = NLK_HOST;
	nlk.comm.action = NLK_ACTION_DUMP;
	nlk.addr = addr;
	nlk.comm.mid = 1;
	return nlk_head_send_recv(NLK_MSG_HOST, &nlk,
		       	sizeof(nlk), info, sizeof(*info));
}

/* 
 * set *nr to read host nr, and return 
 * success, caller need free return pointer;
 * return NULL if failed 
 */
struct host_info *dump_host_alive(int *nr)
{
	struct nlk_host nlk;
	struct host_info *info;
	int size;

	memset(&nlk, 0, sizeof(nlk));
	if (!nr)
		return NULL;
	size = sizeof(struct host_info) * HOST_MX;
	info = malloc(size);
	*nr = IGD_ERR_NO_MEM;
	if (!info)
		return NULL;
	nlk.comm.key = NLK_HOST;
	nlk.comm.mid = 0;
	nlk.comm.action = NLK_ACTION_DUMP;
	nlk.comm.obj_nr = 40;
	nlk.comm.obj_len = sizeof(struct host_info);
	*nr = nlk_dump_from_kernel(NLK_MSG_HOST, &nlk, sizeof(nlk), info, HOST_MX);
	if (*nr > 0)
		return info;
	free(info);
	return NULL;
}

int set_host_info(struct in_addr ip, char *nick_name, char *name, unsigned char os_type, uint16_t vender)
{
	struct nlk_host nlk;
	struct host_info info;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.key = NLK_HOST;
	nlk.comm.action = NLK_ACTION_ADD;
	nlk.addr.s_addr = ip.s_addr;

	memset(&info, 0, sizeof(info));
	if (name) {
		strncpy(info.name, name, sizeof(info.name) - 1);
		info.name[sizeof(info.name) - 1] = '\0';
	}
	if (nick_name) {
		strncpy(info.nick_name, nick_name, sizeof(info.nick_name) - 1);
		info.nick_name[sizeof(info.nick_name) - 1] = '\0';
	}
	info.os_type = os_type;
	info.vender = vender;
	return nlk_data_send(NLK_MSG_HOST, &nlk, sizeof(nlk), &info, sizeof(info));
}

int set_dev_vlan(char *devname, uint16_t vlan_mask)
{
	struct nlk_netdev nlk;

	if (!devname)
		return IGD_ERR_DATA_ERR;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.comm.action = NLK_ACTION_ADD;
	nlk.comm.key = NLK_NETDEV_VLAN;
	arr_strcpy(nlk.devname, devname);
	nlk.vlan_mask = vlan_mask;
	return nlk_head_send(NLK_MSG_NETDEV, &nlk, sizeof(nlk));
}

int set_dev_uid(char *devname, int uid)
{
	struct nlk_netdev nlk;

	if (!devname)
		return IGD_ERR_DATA_ERR;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.comm.action = NLK_ACTION_ADD;
	nlk.comm.key = NLK_NETDEV_UID;
	arr_strcpy(nlk.devname, devname);
	nlk.uid = uid;
	return nlk_head_send(NLK_MSG_NETDEV, &nlk, sizeof(nlk));
}

struct nlk_dump_rule *dump_group(int mid, int *nr)
{
	struct nlk_grp_rule nlk;
	struct nlk_dump_rule *info;
	
	memset(&nlk, 0x0, sizeof(nlk));
	if (!nr)
		return NULL;
	info = malloc(sizeof(struct nlk_dump_rule) * GROUP_MX);
	if (!info)
		return NULL;
	memset(info, 0, sizeof(struct nlk_dump_rule) * GROUP_MX);
	nlk.comm.obj_nr = 40;
	nlk.comm.mid = mid;
	nlk.comm.action = NLK_ACTION_DUMP;
	*nr =  nlk_head_send_recv(NLK_MSG_GROUP, &nlk, sizeof(nlk), info, sizeof(struct nlk_dump_rule) * GROUP_MX);
	if (*nr <= 0)
		return NULL;
	return info;
}	

int del_group(int mid, int id)
{
	struct nlk_grp_rule nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.comm.mid = mid;
	nlk.comm.id = id;
	nlk.comm.action = NLK_ACTION_DEL;
	return nlk_head_send(NLK_MSG_GROUP, &nlk, sizeof(nlk));
}

#if 0
int clean_group(int mid)
{
	struct nlk_grp_rule nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.comm.mid = mid;
	nlk.comm.action = NLK_ACTION_CLEAN;
	return nlk_head_send(NLK_MSG_GROUP, &nlk, sizeof(nlk));
}
#endif

int add_user_grp_by_ip(char *name, int nr, struct ip_range *ip)
{
	struct nlk_grp_rule nlk; 
		
	if (!name || !strlen(name))
		return IGD_ERR_DATA_ERR;
	if ((nr && !ip) || (!nr && ip))
		return IGD_ERR_DATA_ERR;
	if (nr > IGD_USER_GRP_PER_MX)
		nr = IGD_USER_GRP_PER_MX;
	memset(&nlk, 0x0, sizeof(nlk));
	arr_strcpy(nlk.name, name);
	nlk.comm.mid = USER_GRP;
	nlk.comm.action = NLK_ACTION_ADD;
	nlk.comm.obj_nr = nr;
	nlk.type = USER_GRP_TYPE_IP;
	nlk.comm.obj_len = sizeof(struct ip_range);
	return nlk_data_send(NLK_MSG_GROUP, &nlk, sizeof(nlk), ip, nr * sizeof(struct ip_range));
}

int add_user_grp_by_mac(char *name, int nr, unsigned char (*mac)[ETH_ALEN])
{
	struct nlk_grp_rule nlk; 
		
	if (!name || !strlen(name))
		return IGD_ERR_DATA_ERR;
	if ((nr && !mac) || (!nr && mac))
		return IGD_ERR_DATA_ERR;
	if (nr > IGD_USER_GRP_PER_MX)
		nr = IGD_USER_GRP_PER_MX;
	memset(&nlk, 0x0, sizeof(nlk));
	arr_strcpy(nlk.name, name);
	nlk.comm.mid = USER_GRP;
	nlk.comm.action = NLK_ACTION_ADD;
	nlk.comm.obj_nr = nr;
	nlk.type = USER_GRP_TYPE_MAC;
	nlk.comm.obj_len = ETH_ALEN;
	return nlk_data_send(NLK_MSG_GROUP, &nlk, sizeof(nlk), mac, nr * ETH_ALEN);
}

int add_dns_grp(char *name, int nr, char (*dns)[IGD_DNS_LEN])
{
	struct nlk_grp_rule nlk; 
		
	if (!name || !strlen(name))
		return IGD_ERR_DATA_ERR;
	if ((nr && !dns) || (!nr && dns))
		return IGD_ERR_DATA_ERR;
	if (nr > IGD_DNS_GRP_PER_MX)
		nr = IGD_DNS_GRP_PER_MX;
	memset(&nlk, 0x0, sizeof(nlk));
	arr_strcpy(nlk.name, name);
	nlk.comm.mid = DNS_GRP;
	nlk.comm.action = NLK_ACTION_ADD;
	nlk.comm.obj_nr = nr;
	nlk.comm.obj_len = IGD_DNS_LEN;
	return nlk_data_send(NLK_MSG_GROUP, &nlk, sizeof(nlk), dns, nr * IGD_DNS_LEN);
}

int add_url_grp2(char *name, int nr, struct inet_url *url)
{
	struct nlk_grp_rule nlk; 
		
	if (!name || !strlen(name))
		return IGD_ERR_DATA_ERR;
	if ((nr && !url) || (!nr && url))
		return IGD_ERR_DATA_ERR;
	if (nr > IGD_URL_GRP_PER_MX)
		nr = IGD_URL_GRP_PER_MX;
	memset(&nlk, 0x0, sizeof(nlk));
	arr_strcpy(nlk.name, name);
	nlk.comm.mid = URL_GRP;
	nlk.comm.action = NLK_ACTION_ADD;
	nlk.comm.obj_nr = nr;
	nlk.comm.obj_len = sizeof(struct inet_url);
	nlk.type = 1;
	return nlk_data_send(NLK_MSG_GROUP, &nlk,
		       	sizeof(nlk), url, nr * nlk.comm.obj_len);
}

int add_url_grp(char *name, int nr, char (*url)[IGD_URL_LEN])
{
	struct nlk_grp_rule nlk; 
		
	if (!name || !strlen(name))
		return IGD_ERR_DATA_ERR;
	if ((nr && !url) || (!nr && url))
		return IGD_ERR_DATA_ERR;
	if (nr > IGD_URL_GRP_PER_MX)
		nr = IGD_URL_GRP_PER_MX;
	memset(&nlk, 0x0, sizeof(nlk));
	arr_strcpy(nlk.name, name);
	nlk.comm.mid = URL_GRP;
	nlk.comm.action = NLK_ACTION_ADD;
	nlk.comm.obj_nr = nr;
	nlk.comm.obj_len = IGD_URL_LEN;
	return nlk_data_send(NLK_MSG_GROUP, &nlk, sizeof(nlk), url, nr * IGD_URL_LEN);
}

int mod_dns_grp(int id, char *name, int nr, char (*dns)[IGD_DNS_LEN])
{
	struct nlk_grp_rule nlk;

	if (!name || !strlen(name) || id < 0)
		return IGD_ERR_DATA_ERR;
	if ((nr && !dns) || (!nr && dns))
		return IGD_ERR_DATA_ERR;

	if (nr > IGD_DNS_GRP_PER_MX)
		nr = IGD_DNS_GRP_PER_MX;
	memset(&nlk, 0x0, sizeof(nlk));
	arr_strcpy(nlk.name, name);
	nlk.comm.mid = DNS_GRP;
	nlk.comm.id = id;
	nlk.comm.action = NLK_ACTION_MOD;
	nlk.comm.obj_len = IGD_DNS_LEN;
	nlk.comm.obj_nr = nr;
	return nlk_data_send(NLK_MSG_GROUP, &nlk, sizeof(nlk), dns, nr * IGD_DNS_LEN);
}

int mod_url_grp(int id, char *name, int nr, char (*url)[IGD_URL_LEN])
{
	struct nlk_grp_rule nlk;
	
	if (!name || !strlen(name) || id < 0)
		return IGD_ERR_DATA_ERR;
	if ((nr && !url) || (!nr && url))
		return IGD_ERR_DATA_ERR;

	if (nr > IGD_URL_GRP_PER_MX)
		nr = IGD_URL_GRP_PER_MX;
	memset(&nlk, 0x0, sizeof(nlk));
	arr_strcpy(nlk.name, name);
	nlk.comm.mid = URL_GRP;
	nlk.comm.id = id;
	nlk.comm.action = NLK_ACTION_MOD;
	nlk.comm.obj_len = IGD_URL_LEN;
	nlk.comm.obj_nr = nr;
	return nlk_data_send(NLK_MSG_GROUP, &nlk, sizeof(nlk), url, nr * IGD_URL_LEN);
}

int mod_user_grp_by_ip(int id, char *name, int nr, struct ip_range *ip)
{
	struct nlk_grp_rule nlk;

	if (!name || !strlen(name) || id < 0)
		return IGD_ERR_DATA_ERR;
	if ((nr && !ip) || (!nr && ip))
		return IGD_ERR_DATA_ERR;

	if (nr > IGD_USER_GRP_PER_MX)
		nr = IGD_USER_GRP_PER_MX;
	memset(&nlk, 0x0, sizeof(nlk));
	arr_strcpy(nlk.name, name);
	nlk.comm.mid = USER_GRP;
	nlk.comm.id = id;
	nlk.comm.action = NLK_ACTION_MOD;
	nlk.comm.obj_len = sizeof(struct ip_range);
	nlk.comm.obj_nr = nr;
	nlk.type = USER_GRP_TYPE_IP;
	return nlk_data_send(NLK_MSG_GROUP, &nlk, sizeof(nlk), ip, nr * sizeof(struct ip_range));
}

int mod_user_grp_by_mac(int id, char *name, int nr, char (*mac)[ETH_ALEN])
{
	struct nlk_grp_rule nlk;

	if (!name || !strlen(name) || id < 0)
		return IGD_ERR_DATA_ERR;
	if ((nr && !mac) || (!nr && mac))
		return IGD_ERR_DATA_ERR;

	if (nr > IGD_USER_GRP_PER_MX)
		nr = IGD_USER_GRP_PER_MX;
	memset(&nlk, 0x0, sizeof(nlk));
	arr_strcpy(nlk.name, name);
	nlk.comm.mid = USER_GRP;
	nlk.comm.id = id;
	nlk.comm.action = NLK_ACTION_MOD;
	nlk.comm.obj_len = ETH_ALEN;
	nlk.comm.obj_nr = nr;
	nlk.type = USER_GRP_TYPE_MAC;
	return nlk_data_send(NLK_MSG_GROUP, &nlk, sizeof(nlk), mac, nr * ETH_ALEN);
}

static inline void swap(int *a, int *b)
{
	int tmp;

	tmp = *a;
	*a = *b;
	*b = tmp;
	return ;
}

int quick_sort(int *arr, int l, int r)
{
	int data;
	int i;
	int j;

	if (l >= r) 
		return 0;
	data = arr[r];
	i = l;
	j = l;
	while (j++ < r) {
		if (arr[j - 1] > data) 
			continue;
		swap(&arr[i], &arr[j - 1]);
		i++;
	}
	swap(&arr[i], &arr[r]);
	quick_sort(arr, l, i - 1);
	quick_sort(arr, i + 1, r);
	return 0;
}

static int *l7_deal_appid(unsigned long *map, int *nr, int *app)
{
	int *tmp;
	int *org;
	int cnt = 0;
	int mid;
	int appid;

	if (*nr >= 1024)
		return NULL;
	tmp = malloc(sizeof(int) * (*nr));
	if (!tmp)
		return NULL;
	org = tmp;
	memcpy(tmp, app, sizeof(int) * (*nr));
	/*  sort  first*/
	quick_sort(tmp, 0, *nr - 1);

	loop_for_each(0, *nr) {
		mid = L7_GET_MID(org[i]);
		appid = L7_GET_APPID(org[i]);
		if (mid < 0 || mid >= L7_MID_MX) {
			printf("app %d error\n", appid);
			goto error;
		}
		if (!appid) {
			igd_set_bit(mid, map);
			continue;
		}
		if (igd_test_bit(mid, map)) 
			continue;
		*tmp = org[i];
		tmp++;
		cnt++;
	} loop_for_each_end();
	*nr = cnt;

	return org;
error:
	free(org);
	return NULL;
}

int register_app_filter(struct inet_host *host, struct app_filter_rule *app)
{
	struct nf_rule_u rule;
	unsigned long map = 0;
	int *tmp;
	int ret;

	memset(&rule, 0, sizeof(rule));
	tmp = l7_deal_appid(&map, &app->nr, app->appid);
	if (!tmp)
		return -1;
	rule.comm.obj_nr = app->nr;
	rule.comm.obj_len = sizeof(int);
	rule.comm.mid = NF_MOD_NET;
	rule.comm.prio = app->prio;
	rule.comm.action = NLK_ACTION_ADD;
	if (host) 
		rule.base.host = *host;
	rule.target.t_type = app->type;
	rule.bitmap = map;
	rule.extra = 1;
	ret = nlk_data_send(NLK_MSG_RULE_ACTION, &rule,
		       	sizeof(rule), tmp, app->nr * sizeof(int));
	free(tmp);
	return ret;

}

int unregister_app_filter(int id)
{
	struct nf_rule_u rule;

	memset(&rule, 0x0, sizeof(rule));
	rule.comm.id = id;
	rule.comm.mid = NF_MOD_NET;
	rule.comm.action = NLK_ACTION_DEL;
	return nlk_head_send(NLK_MSG_RULE_ACTION, &rule, sizeof(rule));
}

int register_app_rate_limit(struct inet_host *host,
	       	int mid, int id, struct rate_entry *rate)
{
	return 0;
}

int unregister_app_rate_limit(int id)
{
	return 0;
}

/* 
 * type : HTTP_RSP_RST HTTP_RSP_XXX...,
 * rd: redirect url.
 */
int register_http_rsp(int type, struct url_rd *rd)
{
	struct nlk_url nlk;

	if (!rd) 
		return -1;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.action = NLK_ACTION_ADD;
	nlk.comm.key = NLK_URL_KEY_RSP;
	nlk.type = type;
		
	return nlk_data_send(NLK_MSG_URL, &nlk, sizeof(nlk), rd, sizeof(*rd));
}

int unregister_http_rsp(int type)
{
	struct nlk_url nlk;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.action = NLK_ACTION_DEL;
	nlk.comm.key = NLK_URL_KEY_RSP;
	nlk.type = type;
		
	return nlk_head_send(NLK_MSG_URL, &nlk, sizeof(nlk));
}

void static inline l3_u2k(struct inet_l3 *dst, struct api_l3 *src)
{
	dst->type = src->type;
	dst->gid = src->gid;
	dst->addr = src->addr;
	arr_strcpy(dst->dns.dns, src->dns);
	return ;
}

void static inline l4_u2k(struct inet_l4 *dst, struct api_l4 *src)
{
	dst->type = src->type;
	memcpy(&dst->src, &src->src, sizeof(dst->src));
	memcpy(&dst->dst, &src->dst, sizeof(dst->dst));
	return ;
}

int register_http_rule(struct inet_host *host, struct http_rule_api *arg)
{
	struct nlk_url nlk;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.action = NLK_ACTION_ADD;
	nlk.comm.key = NLK_URL_KEY_RULE;
	nlk.comm.prio = arg->prio;
	nlk.comm.mid = arg->mid;
	l3_u2k(&nlk.l3, &arg->l3);
	if (host) 
		nlk.src = *host;
	nlk.times = arg->times;
	nlk.period = arg->period;
	nlk.flags = arg->flags;
	nlk.comm.gid = arg->type;
	switch (arg->mid) {
	case URL_MID_WEB_AUTH:
	case URL_MID_REDIRECT:
	case URL_MID_ADVERT:
	case URL_MID_URI_SUB:
	case URL_MID_JSRET:
		return nlk_data_send(NLK_MSG_URL, &nlk,
		       	sizeof(nlk), &arg->rd, sizeof(arg->rd));
	default:
		break;
	}
	return nlk_head_send(NLK_MSG_URL, &nlk, sizeof(nlk));
}

int unregister_http_rule(int mid, int id)
{
	struct nlk_url nlk;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.action = NLK_ACTION_DEL;
	nlk.comm.key = NLK_URL_KEY_RULE;
	nlk.comm.mid = mid;
	nlk.comm.id = id;
	return nlk_head_send(NLK_MSG_URL, &nlk, sizeof(nlk));
}

int register_dns_rule(struct inet_host *host, struct dns_rule_api *arg)
{
	struct nf_rule_u rule;
	struct nf_rule_target_redirect *rd;

	memset(&rule, 0, sizeof(rule));

	rule.comm.mid = NF_MOD_DNS;
	rule.comm.action = NLK_ACTION_ADD;
	if (host) 
		rule.base.host = *host;
	l3_u2k(&rule.base.l3, &arg->l3);

	rule.comm.prio = arg->prio;
	rule.target.t_type = arg->target;
	if (arg->target == NF_TARGET_REDIRECT) {
		rd = &rule.target.target.redirect;
		rd->type = arg->type;
		rd->ip = arg->ip;
		arr_strcpy(rd->dns, arg->dns);
	}	
	return nlk_head_send(NLK_MSG_RULE_ACTION, &rule, sizeof(rule));
}

int unregister_dns_rule(int id)
{
	struct nf_rule_u nlk;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.action = NLK_ACTION_DEL;
	nlk.comm.mid = NF_MOD_DNS;
	nlk.comm.id = id;
	return nlk_head_send(NLK_MSG_RULE_ACTION, &nlk, sizeof(nlk));
}

int register_local_rule(struct inet_host *host, struct net_rule_api *arg)
{
	struct nf_rule_u rule;

	memset(&rule, 0, sizeof(rule));
	rule.comm.mid = NF_MOD_LOCAL;
	rule.comm.action = NLK_ACTION_ADD;
	rule.comm.prio = arg->prio;
	if (host) 
		rule.base.host = *host;
	l3_u2k(&rule.base.l3, &arg->l3);
	l4_u2k(&rule.base.l4, &arg->l4);
	rule.base.protocol = arg->l4.protocol;
	rule.target.t_type = arg->target;
	if (arg->target != NF_TARGET_DENY && arg->target != NF_TARGET_ACCEPT)
		return -1;
	return nlk_head_send(NLK_MSG_RULE_ACTION, &rule, sizeof(rule));
}

int unregister_local_rule(int id)
{
	struct nf_rule_u nlk;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.action = NLK_ACTION_DEL;
	nlk.comm.mid = NF_MOD_LOCAL;
	nlk.comm.id = id;
	return nlk_head_send(NLK_MSG_RULE_ACTION, &nlk, sizeof(nlk));
}

int register_net_rule(struct inet_host *host, struct net_rule_api *arg)
{
	struct nf_rule_u rule;

	memset(&rule, 0, sizeof(rule));
	rule.comm.mid = NF_MOD_NET;
	rule.comm.action = NLK_ACTION_ADD;
	rule.comm.prio = arg->prio;
	if (host) 
		rule.base.host = *host;
	l3_u2k(&rule.base.l3, &arg->l3);
	l4_u2k(&rule.base.l4, &arg->l4);
	rule.base.protocol = arg->l4.protocol;
	rule.target.t_type = arg->target;
	if (arg->target != NF_TARGET_DENY && arg->target != NF_TARGET_ACCEPT)
		return -1;
	return nlk_head_send(NLK_MSG_RULE_ACTION, &rule, sizeof(rule));
}

int unregister_net_rule(int id)
{
	struct nf_rule_u nlk;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.action = NLK_ACTION_DEL;
	nlk.comm.mid = NF_MOD_NET;
	nlk.comm.id = id;
	return nlk_head_send(NLK_MSG_RULE_ACTION, &nlk, sizeof(nlk));
}

int register_rate_rule(struct inet_host *host, struct rate_rule_api *arg)
{
	struct nf_rule_u rule;
	
	memset(&rule, 0, sizeof(rule));
	if (host) 
		rule.base.host = *host;
	rule.comm.mid = NF_MOD_RATE;
	rule.comm.action = NLK_ACTION_ADD;
	rule.comm.prio = arg->prio;
	rule.target.t_type = NF_TARGET_RATE;
	rule.target.target.rate.up = (arg->up >= 7000) ? 6999 : arg->up;
	rule.target.target.rate.down = (arg->down >= 7000) ? 6999 : arg->down;
	return nlk_head_send(NLK_MSG_RULE_ACTION, &rule, sizeof(rule));
}

int unregister_rate_rule(int id)
{
	struct nf_rule_u nlk;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.action = NLK_ACTION_DEL;
	nlk.comm.mid = NF_MOD_RATE;
	nlk.comm.id = id;
	return nlk_head_send(NLK_MSG_RULE_ACTION, &nlk, sizeof(nlk));
}

int register_qos_rule(struct inet_host *host, struct qos_rule_api *arg)
{
	struct nf_rule_u rule;
	
	memset(&rule, 0, sizeof(rule));
	if (host) 
		rule.base.host = *host;
	rule.comm.mid = NF_MOD_QOS;
	rule.comm.action = NLK_ACTION_ADD;
	rule.comm.prio = arg->prio;
	rule.target.t_type = NF_TARGET_QOS;
	rule.target.target.qos.queue = arg->queue;
	return nlk_head_send(NLK_MSG_RULE_ACTION, &rule, sizeof(rule));
}

int unregister_qos_rule(int id)
{
	struct nf_rule_u nlk;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.action = NLK_ACTION_DEL;
	nlk.comm.mid = NF_MOD_QOS;
	nlk.comm.id = id;
	return nlk_head_send(NLK_MSG_RULE_ACTION, &nlk, sizeof(nlk));
}

int register_app_qos(struct inet_host *host, int queue, int nr, int *app)
{
	struct nf_rule_u rule;
	unsigned long map = 0;
	int *tmp;
	int ret;

	memset(&rule, 0, sizeof(rule));
	tmp = l7_deal_appid(&map, &nr, app);
	if (!tmp)
		return -1;
	rule.comm.obj_nr = nr;
	rule.comm.obj_len = sizeof(int);
	rule.comm.mid = NF_MOD_QOS;
	rule.comm.prio = -1;
	rule.comm.action = NLK_ACTION_ADD;
	if (host) 
		rule.base.host = *host;
	rule.target.t_type = NF_TARGET_QOS;
	rule.target.target.qos.queue = queue;
	rule.bitmap = map;
	rule.extra = 1;
	ret = nlk_data_send(NLK_MSG_RULE_ACTION, &rule,
		       	sizeof(rule), tmp, nr * sizeof(int));
	free(tmp);
	return ret;
}

int unregister_app_qos(int id)
{
	struct nf_rule_u rule;

	memset(&rule, 0x0, sizeof(rule));
	rule.comm.id = id;
	rule.comm.mid = NF_MOD_QOS;
	rule.comm.action = NLK_ACTION_DEL;
	return nlk_head_send(NLK_MSG_RULE_ACTION, &rule, sizeof(rule));
}

int mod_filter_rule(int mid, int id, int prio, struct nf_rule_info *info)
{	
	struct nf_rule_u rule;
	if (!info)
		return IGD_ERR_DATA_ERR;
	memset(&rule, 0x0, sizeof(rule));
	rule.comm.id = id;
	rule.comm.mid = mid;
	rule.comm.prio = prio;
	rule.comm.action = NLK_ACTION_MOD;
	memcpy(&rule.base, &info->base, sizeof(rule.base));
	memcpy(&rule.target, &info->target, sizeof(rule.target));
	return nlk_head_send(NLK_MSG_RULE_ACTION, &rule, sizeof(rule));
}

/*  return number of host add success ,  < 0 for error*/
int register_http_capture(int mid, int nr, char (*host)[IGD_DNS_LEN])
{
	struct nlk_http_host nlk;
		
	if (mid < 0 || nr <= 0 || !host)
		return IGD_ERR_DATA_ERR;
	if (nr > 512)
		nr = 512;
	memset(&nlk, 0x0, sizeof(nlk));
	nlk.comm.key = NLK_HTTP_HOST_CAP;
	nlk.comm.mid = mid;
	nlk.comm.action = NLK_ACTION_ADD;
	nlk.comm.obj_nr = nr;
	nlk.comm.obj_len = IGD_DNS_LEN;
	return nlk_data_send(NLK_MSG_URL_LOG, &nlk, sizeof(nlk),
		       			host, nr * IGD_DNS_LEN);
}

/*  clean all */
int unregister_http_capture(int mid)
{
	struct nlk_http_host nlk;

	nlk.comm.key = NLK_HTTP_HOST_CAP;
	nlk.comm.mid = mid;
	nlk.comm.action = NLK_ACTION_CLEAN;
	return nlk_head_send(NLK_MSG_URL_LOG, &nlk, sizeof(nlk));
}

#if 0
int clean_filter_rule(int mid)
{
	struct nf_rule_u rule;

	memset(&rule, 0x0, sizeof(rule));
	rule.comm.mid = mid;
	rule.comm.action = NLK_ACTION_CLEAN;
	return nlk_head_send(NLK_MSG_RULE_ACTION, &rule, sizeof(rule));
}
#endif

/* read file , return n bytes read, or -1 if error */
int igd_safe_read(int fd, unsigned char *dst, int len)
{
	int count = 0;
	int ret;

	while (len > 0) {
		ret = read(fd, dst, len);
		if (!ret) 
			break;
		if (ret < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) 
				continue;
			if (errno == EINTR) 
				continue;
			return -1;
		}
		count += ret;
		len -= ret;
		dst += ret;
	}
	return count;
}

int igd_safe_write(int fd, unsigned char *src, int len)
{
	int count = 0;
	int ret;

	while (len > 0) {
		ret = write(fd, src, len);
		if (!ret) 
			continue;
		if (ret < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) 
				continue;
			if (errno == EINTR) 
				continue;
			return -1;
		}
		count += ret;
		len -= ret;
		src += ret;
	}
	return count;
}

/* return 0 if success, or -1 , buf must len >=8 */
int dump_flash_uid(unsigned char *buf)
{
	struct nlk_msg_comm nlk;
	unsigned char tmp[8] = {0, };
	int ret;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_HW_FLASH;
	ret = nlk_head_send_recv(NLK_MSG_HW, &nlk, sizeof(nlk), tmp, sizeof(tmp));
	memcpy(buf, tmp, sizeof(tmp));
	return ret;
}

void console_printf(const char *fmt, ...)
{
	FILE *fp = NULL;
	va_list ap;

	fp = fopen("/dev/console", "w");
	if (fp) {
		va_start(ap, fmt);
		vfprintf(fp, fmt, ap);
		va_end(ap);
		fclose(fp);
	}
}


char *read_firmware(char *key)
{
	static char value[64];
	char buf[128] = { 0, };
	FILE *fp;
	int klen = strlen(key);

	if (!mtd_get_val(key, value, sizeof(value)))
		return value;
	fp = fopen("/etc/firmware", "r");
	if (!fp) 
		return NULL;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (strncmp(key, buf, klen))
			continue;
		if ((buf[klen] != '=') && (buf[klen] != ':'))
			continue;
		__arr_strcpy_end((unsigned char *)value, (unsigned char *)&buf[klen+1], sizeof(value), '\n');
		fclose(fp);
		return value;
	}
	fclose(fp);
	return NULL;
}

void igd_log(char *file, const char *fmt, ...)
{
	va_list ap;
	FILE *fp = NULL;
	char bakfile[32] = {0,};

	fp = fopen(file, "a+");
	if (fp == NULL)
		return;
	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
	if (ftell(fp) > 10*1024)
		snprintf(bakfile, sizeof(bakfile), "%s.bak", file);
	fclose(fp);
	if (bakfile[0])
		rename(file, bakfile);
}

int igd_aes_dencrypt(const char *path, unsigned char *out, int len, unsigned char *pwd)
{
	int fd;
	int ret = -1;
	int size, nr;
	struct stat st;
	unsigned char *src = NULL;
	aes_decrypt_ctx de_ctx[1];
	
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;
	if (fstat(fd, &st))
		goto error;
	
	size = st.st_size;
	if (len < size)
		goto error;
	
	src = malloc(size);
	if (!src)
		goto error;

	memset(src, 0x0, size);
	memset(out, 0x0, len);
	nr = igd_safe_read(fd, src, size);
	if (nr != size)
		goto error;
	aes_decrypt_key128(pwd, de_ctx);
	aes_ecb_decrypt(src, out, size, de_ctx);
	ret = size;
error:
	if (fd > 0)
		close(fd);
	if (src)
		free(src);
	return ret;
}

int igd_aes_dencrypt2(
	void *in, void *out, int len, void *pwd)
{
	aes_decrypt_ctx de_ctx[1];

	if (!in || !out)
		return -2;

	memset(out, 0x0, len);
	if (aes_decrypt_key128(pwd, de_ctx))
		return -1;
	if (aes_ecb_decrypt(in, out, len, de_ctx))
		return -1;
	return 0;
}

int igd_aes_encrypt(const char *path, unsigned char *out, int len, unsigned char *pwd)
{
	int fd;
	int ret = -1;
	struct stat st;
	int size, newlen, nr;
	unsigned char *pDataBuf = NULL;
	int i_encry = 0, i_padding = 0;
	aes_encrypt_ctx en_ctx[1];
	unsigned char buf[AES_BLOCK_SIZE] = {0};
	
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;
	if (fstat(fd, &st))
		goto error;
	size = st.st_size;
	i_encry = size/AES_BLOCK_SIZE + 1;
	i_padding = AES_BLOCK_SIZE - size%AES_BLOCK_SIZE;
	newlen = size + i_padding;
	if (len < newlen)
		goto error;
	memset(out, 0x0, len);
	nr = igd_safe_read(fd, out, size);
	if (nr != size) {
		printf("igd safe read input file error\n");
		goto error;
	}
	pDataBuf = out;
	for (nr = 0; nr < i_encry; nr++) {
		memset(buf, 0x0, sizeof(buf));
		aes_encrypt_key128(pwd, en_ctx);
		aes_ecb_encrypt(pDataBuf, buf, AES_BLOCK_SIZE, en_ctx);
		memcpy(pDataBuf, buf, AES_BLOCK_SIZE);
		pDataBuf += AES_BLOCK_SIZE;
	}
	ret = newlen;
error:
	if (fd > 0)
		close(fd);
	return ret;
}

int igd_aes_encrypt2(
	void *in, int ilen, void *out, int olen, void *pwd)
{
	aes_encrypt_ctx en_ctx[1];
	int i = 0, nr = 0, len = 0, l;
	unsigned char buf[AES_BLOCK_SIZE], *pi = in, *po = out;

	if (!in || !out)
		return -2;

	nr = ilen/AES_BLOCK_SIZE;
	if (ilen % AES_BLOCK_SIZE)
		nr++;
	len = nr*AES_BLOCK_SIZE;
	if (len > olen)
		return -2;

	memset(out, 0, olen);
	for (i = 0; i < nr; i++) {
		l = (ilen > AES_BLOCK_SIZE) ? AES_BLOCK_SIZE : ilen;
		memcpy(buf, pi, l);
		if (l < AES_BLOCK_SIZE)
			memset(buf + l, 0, AES_BLOCK_SIZE - l);
		aes_encrypt_key128(pwd, en_ctx);
		aes_ecb_encrypt(buf, po, AES_BLOCK_SIZE, en_ctx);
		ilen -= l;
		pi += l;
		po += AES_BLOCK_SIZE;
	}
	return len;
}

int igd_time_cmp(struct tm *t, struct time_comm *m)
{
	int now, start, end, week, pre;

	now = t->tm_hour*60 + t->tm_min;
	start = m->start_hour*60 + m->start_min;
	end = m->end_hour*60 + m->end_min;
	week = t->tm_wday;
	pre = (week > 0) ? (week - 1) : 6;

	if ((pre < 0) || (pre > 6))
		return 0;
	if ((week < 0) || (week > 6))
		return 0;
	
	if (start < end) {
		if ((m->day_flags & (1 << week)) &&
			 (now >= start) && (now < end)) {
			return 1;
		}
	} else if (start > end) {
		if (((m->day_flags & (1 << pre)) && (now < end)) ||
			 ((m->day_flags & (1 << week)) && (now >= start))) {
			return 1;
		}
	}
	return 0;
}

long sys_uptime(void)
{
	struct sysinfo info;

	if (sysinfo(&info))
		return 0;

	return info.uptime;
}

struct tm *get_tm(void)
{
	time_t tt = 0;

	time(&tt);
	if (tt < 1452136362)
		return NULL;
	return localtime(&tt);
}

struct http_host_log *dump_http_log(int *nr)
{
	struct nlk_http_host nlk;
	struct http_host_log *log;
	int size;

	memset(&nlk, 0, sizeof(nlk));
	if (!nr)
		return NULL;
	size = sizeof(struct http_host_log) * HTTP_HOST_LOG_PER_MX;
	log = malloc(size);
	*nr = IGD_ERR_NO_MEM;
	if (!log)
		return NULL;
	memset(log, 0, size);
	nlk.comm.key = NLK_HTTP_HOST_URL;
	nlk.comm.mid = 0;
	nlk.comm.action = NLK_ACTION_DUMP;
	nlk.comm.obj_nr = 10;
	nlk.comm.obj_len = sizeof(struct http_host_log);
	*nr = nlk_dump_from_kernel(NLK_MSG_URL_LOG, &nlk,
		       	sizeof(nlk), log, HTTP_HOST_LOG_PER_MX);
	if (*nr > 0)
		return log;
	free(log);
	return NULL;
}

int set_host_url_switch(int act)
{
	struct nlk_http_host nlk;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.key = NLK_HTTP_HOST_URL;
	nlk.comm.action = act;
	return nlk_head_send(NLK_MSG_URL_LOG, &nlk, sizeof(nlk));
}

int shell_printf(char *cmd, char *dst, int dlen)
{
	FILE *fp;
	int rlen;

	if (!cmd || !dst|| dlen <= 1)
		return -1;
	if ((fp = popen(cmd, "r")) == NULL)
		return IGD_ERR_NO_RESOURCE;
	rlen = fread(dst, dlen - 1, 1, fp);
	dst[dlen-1] = 0;

	pclose(fp);
	return rlen;
}

int get_nlk_msgid(char *name)
{
	struct nlk_msgid nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	arr_strcpy(nlk.name, name);
	return nlk_head_send(NLK_MSG_MSGID, &nlk, sizeof(nlk));
}

