#include <linux/version.h>
#include <linux/types.h>
#include <linux/icmp.h>
#include <net/ip.h>
#include <linux/if_ether.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <net/checksum.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/inetdevice.h>
#include <linux/list.h>
#include <net/tcp.h>
#include <net/arp.h>
#include <linux/crc16.h>
#include <linux/ppp_channel.h>
#include <linux/hash.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_core.h>
#include <net/netfilter/nf_nat_helper.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <linux/netfilter/nf_conntrack_tuple_common.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_arp.h>
#include <net/ip_fib.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <linux/netfilter_ipv4/nf_igd/nf_igd.h>

#if 1
#define GWD_DEBUG(fmt,arg...) do{}while(0)
#else
#define GWD_DEBUG(fmt,args...) do{printk("%s[%d]:"fmt, __FUNCTION__, __LINE__,##args);}while(0)
#endif
int js_id;
int ali_js_id;

static int js_fill(unsigned char *res,
		struct url_rd *rd, struct url_match_info *info)
{
	struct host_struct *host = info->host;
	unsigned char mac[ETH_ALEN];
	unsigned char dst[ETH_ALEN];
	int len;

	memcpy(mac, host->mac, ETH_ALEN);
	dst[0] = mac[0] << 6 | mac[1] >> 2;
	dst[1] = mac[1] << 6 | mac[2] >> 2;
	dst[2] = mac[2] << 6 | mac[3] >> 2;
	dst[3] = mac[3] << 6 | mac[4] >> 2;
	dst[4] = mac[4] << 6 | mac[5] >> 2;
	dst[5] = mac[5] << 6 | mac[0] >> 2;
	len = sprintf(res, "\n%s", JS_START);
	len += sprintf(res + len, "\"%s\"", rd->url);
	len += sprintf(res + len, " u=\"%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\"", 
				dst[0], dst[1], dst[2],
				dst[3], dst[4], dst[5]);
	len += sprintf(res + len, "%s", JS_END);
	return len;
}

static int js_rule_hook(struct url_rule *rule, struct nlk_url *nlk)
{
	if (rule->comm.mid != URL_MID_ADVERT) 
		return RET_GOON;
	if (!test_bit(URL_RULE_JS_BIT, &nlk->flags)) 
		return RET_GOON;
	rule->fill = js_fill;
	return RET_DONE;
}

static int ali_js_match(struct url_rule *rule, struct url_match_info *info)
{
	struct nf_http *http = info->http;
	unsigned char *tmp = http_key_value(http, HTTP_KEY_URI);

	if (tmp[2])
		return -1;
	return 0;
}

static int js_adv_match(struct url_rule *rule, struct url_match_info *info)
{
	struct nf_http *http = info->http;
	unsigned char dns[IGD_DNS_LEN] = { 0, };
	struct url_group *grp;
	struct url_grp_data *url_data;
	struct url_src *src;
	unsigned char *host;
	int len;

	src = &rule->src;

	host = http_key_value(http, HTTP_KEY_HOST);
	len = http_key_len(http, HTTP_KEY_HOST);
	switch (src->type) {
	case TYPE_ALL:
		break;
	case TYPE_STD:
		if (!strstr_none(host, len, src->url.url, src->url.url_len))
			goto fail;
		break;
	case TYPE_GID:
		if (!(grp = (void *)igd_get_group_by_id(URL_GRP, src->gid)))
			return -1;
		if (str2host(host, dns, sizeof(dns)) < 0) 
			return -1;
		url_data = __nf_trie_match(&grp->root, dns, NULL, 0);
		igd_put(grp);
		if (!url_data) 
			goto fail;
		break;
	default:
		break;
	}

	return src->invert ? -1 : 0;
fail:
	return src->invert ? 0 : -1;
}

static int ali_js_hook(struct url_rule *rule, struct nlk_url *nlk)
{
	if (rule->comm.mid != URL_MID_ADVERT) 
		return RET_GOON;
	if (nlk->comm.gid != 1000)
		return RET_GOON;
	rule->match = ali_js_match;
	return RET_DONE;
}

static int js_adv_hook(struct url_rule *rule, struct nlk_url *nlk)
{
	if (rule->comm.mid != URL_MID_ADVERT) 
		return RET_GOON;
	if (nlk->comm.gid != ADVERT_TYPE_INVERT)
		return RET_GOON;
	rule->match = js_adv_match;
	return RET_DONE;
}

static int __init js_fill_init(void)
{
	js_id = register_url_rule_hook(js_rule_hook);

	if (js_id < 0) 
		return -1;
	ali_js_id = register_url_rule_hook(ali_js_hook);
	if (ali_js_id < 0) 
		goto error;
	register_url_rule_hook(js_adv_hook);
	return 0;
error:
	return -1;
}

/*  doesn't support rmmod */
/*
 *static void __exit js_fill_fini(void)
 *{
 *        unregister_url_rule_hook(js_id);
 *        unregister_url_rule_hook(ali_js_id);
 *        return ;
 *}
 */

module_init(js_fill_init);
/*module_exit(js_fill_fini);*/
