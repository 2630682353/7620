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

/* len < sizeof(struct url_rd) */
struct gwd_rule {
	unsigned long pad[2];
	int sid; /*  site id */
	int (*skip)(struct url_rule *rule, struct url_match_info *info);
	int (*get_sku)(struct url_rule *rule, struct url_match_info *info);
};

int gwd_hid;

static inline int sku_copy(struct url_match_info *info, unsigned char *src)
{
	unsigned char *dst = info->data;
	int len = sizelen(info->data);
	int i = 0;

	while (src[i] && i < len) {
		if (src[i] == '.') {
			dst[i] = 0;
			return i;
		}
		dst[i] = src[i];
		i++;
	}
	dst[i] = 0;
	return -1;
}

static int question_skip(struct url_rule *rule, struct url_match_info *info)
{
	struct http_skb_info *sinfo;
	struct nf_http *http;

	http = info->http;
	sinfo = skb_http(info->skb);
	if (!sinfo) 
		return -1;
	/*  skip if ? exist */
	if (sinfo->key[2].key)
		return -1;
	if (strcmp(http->suffix, "html"))
		return -1;
	return 0;
}

static inline int __refer_skip(unsigned char *tmp)
{
	unsigned char *refer = tmp + 7;

	if (!strncmp(refer, "c.duomai", 8) ||
		!strncmp(refer, "click.linktech", 14) || 
		!strncmp(refer, "m.emarbox", 9))
		return -1;
	return 0;
}

static int html_doumai_skip(struct url_rule *rule, struct url_match_info *info)
{
	struct http_skb_info *sinfo;
	struct nf_http *http;

	http = info->http;
	sinfo = skb_http(info->skb);
	if (!sinfo) 
		return -1;
	if (strcmp(http->suffix, "html"))
		return -1;
	return __refer_skip(http->refer);
}

static int jd_skip(struct url_rule *rule, struct url_match_info *info)
{
	struct nf_http *http;

	http = info->http;
	if (!strncmp(&http->refer[7], "re.jd.com", 9)) 
		return -1;
	return html_doumai_skip(rule, info);
}

static int yixun_skip(struct url_rule *rule, struct url_match_info *info)
{
	struct http_skb_info *sinfo;
	struct nf_http *http;

	http = info->http;
	if (strncmp(http->uri + 1, "item-", 5)) 
		return -1;
	return html_doumai_skip(rule, info);
}

static int yougou_skip(struct url_rule *rule, struct url_match_info *info)
{
	struct http_skb_info *sinfo;
	struct nf_http *http;

	http = info->http;
	sinfo = skb_http(info->skb);
	if (!sinfo) 
		return -1;
	if (strcmp(http->suffix, "shtml"))
		return -1;
	return __refer_skip(http->refer);
}

static int yixun_get_sku(struct url_rule *rule, struct url_match_info *info)
{
	struct nf_http *http;
	struct http_skb_info *sinfo;

	http = info->http;
	sinfo = skb_http(info->skb);
	/* doesn't need check sinfo, because skip fn have checked  */
	if (!sinfo->key[1].key)
		return -1;
	/*  skip item- */
	if (sku_copy(info, &http->uri[6]) < 0) 
		return -1;
	return 0;
}

static int yougou_get_sku(struct url_rule *rule, struct url_match_info *info)
{
	struct nf_http *http;
	struct http_skb_info *sinfo;
	unsigned char *tmp;
	int mx = 32;

	http = info->http;
	sinfo = skb_http(info->skb);
	/* doesn't need check sinfo, because skip fn have checked  */
	if (!sinfo->key[1].key)
		return -1;

	if (http->uri_len < mx) 
		mx = http->uri_len;
	tmp = strnchr(&http->uri[1], mx, '/');
	if (!tmp)
		return -1;
	tmp++;
	if (strncmp(tmp, "sku-", 4)) 
		return -1;

	tmp += 4;
	mx = 16;
	tmp = strnchr(tmp, mx, '-');
	if (!tmp)
		return -1;
	tmp++;
	if (sku_copy(info, tmp) < 0)
		return -1;
	return 0;
}

static int jxdyf_skip(struct url_rule *rule, struct url_match_info *info)
{
	struct http_skb_info *sinfo;
	struct nf_http *http;

	http = info->http;
	if (strncmp(http->uri + 1, "product/", 8)) 
		return -1;
	return html_doumai_skip(rule, info);
}

static int jxdyf_get_sku(struct url_rule *rule, struct url_match_info *info)
{
	struct nf_http *http;
	struct http_skb_info *sinfo;

	http = info->http;
	sinfo = skb_http(info->skb);
	/* doesn't need check sinfo, because skip fn have checked  */
	if (!sinfo->key[1].key)
		return -1;
	if (sku_copy(info, &http->uri[9]) < 0)
		return -1;
	return 0;
}

static int jiuxian_skip(struct url_rule *rule, struct url_match_info *info)
{
	struct http_skb_info *sinfo;
	struct nf_http *http;

	http = info->http;
	if (strncmp(http->uri + 1, "goods-", 6)) 
		return -1;
	return html_doumai_skip(rule, info);
}

static int jiuxian_get_sku(struct url_rule *rule, struct url_match_info *info)
{
	struct nf_http *http;
	struct http_skb_info *sinfo;

	http = info->http;
	sinfo = skb_http(info->skb);
	/* doesn't need check sinfo, because skip fn have checked  */
	if (!sinfo->key[1].key)
		return -1;
	if (sku_copy(info, &http->uri[7]) < 0)
		return -1;
	return 0;
}

static int dangdang_get_sku(struct url_rule *rule, struct url_match_info *info)
{
	struct nf_http *http;
	struct http_skb_info *sinfo;

	http = info->http;
	sinfo = skb_http(info->skb);
	/* doesn't need check sinfo, because skip fn have checked  */
	if (!sinfo->key[1].key)
		return -1;
	if (sku_copy(info, &http->uri[1]) < 0) 
		return -1;
	return 0;
}

static int vip_get_sku(struct url_rule *rule, struct url_match_info *info)
{
	struct nf_http *http;
	struct http_skb_info *sinfo;
	unsigned char *tmp;
	int mx = 32;

	http = info->http;
	sinfo = skb_http(info->skb);
	/* doesn't need check sinfo, because skip fn have checked  */
	if (!sinfo->key[1].key)
		return -1;

	if (http->uri_len < mx) 
		mx = http->uri_len;
	/*  skip /detail- */
	tmp = strnchr(&http->uri[8], mx, '-');
	if (!tmp)
		return -1;
	tmp++;
	if (sku_copy(info, tmp) < 0)
		return -1;
	return 0;
}

static int vip_skip(struct url_rule *rule, struct url_match_info *info)
{
	struct http_skb_info *sinfo;
	struct nf_http *http;

	http = info->http;
	if (strncmp(http->uri + 1, "detail-", 7)) 
		return -1;
	return html_doumai_skip(rule, info);
}

static int gwd_match(struct url_rule *rule, struct url_match_info *info)
{
	struct nf_http *http;
	struct url_src *url;
	struct gwd_rule *gwd;

	http = info->http;
	url = &rule->src;
	gwd = (void *)rule->data;
	switch (url->type) {
	case TYPE_STD:
		if (strcmp(http->host, url->url.url))
			break;
		if (gwd->skip(rule, info)) {
			GWD_DEBUG("%s%s skip\n", http->host, http->uri);
			break;
		}
		if (gwd->get_sku(rule, info)) {
			GWD_DEBUG("get sku_id failed:%s%s\n", http->host, http->uri);
			break;
		}
		return 0;
	default:
		break;
	}
	return -1;
}

static int gwd_fill(unsigned char *res,
		struct url_rd *rd, struct url_match_info *info)
{
	struct gwd_rule *gwd;
	struct url_rule *rule;
	int len;

	rule = ct_url(info->ct)->rule;
	gwd = (void *)rule->data;
	len = snprintf(res, 500, "http://u.gwdang.com/go"
			"/%d/%s/?u=chuyun&column=gwdang&from=browser"
			"&m=b2c&title=", gwd->sid, info->data);
	return len;
}

static int vip_fill(unsigned char *res,
		struct url_rd *rd, struct url_match_info *info)
{
	struct gwd_rule *gwd;
	struct url_rule *rule;
	int len;

	rule = ct_url(info->ct)->rule;
	gwd = (void *)rule->data;
	len = snprintf(res, 500, "http://u.gwdang.com/go"
			"/%d/%s/?u=chuyun&column=list_bijia_%s_%d&from=browser"
			"&m=b2c", gwd->sid, info->data, info->data, gwd->sid);
	return len;
}

int gwd_rule_rule_hook(struct url_rule *rule, struct nlk_url *nlk)
{
	struct url_src *url;
	struct gwd_rule *gwd;
		
	url = &rule->src;
	gwd = (void *)rule->data;

	if (!test_bit(URL_RULE_GWD_BIT, &nlk->flags)) 
		return RET_GOON;
	if (url->type != TYPE_STD) 
		return RET_ERR;

	if (!strcmp(url->url.url, "product.dangdang.com")) {
		memset(gwd, 0, sizeof(struct gwd_rule));
		gwd->get_sku = dangdang_get_sku;
		gwd->skip = question_skip;
		gwd->sid = 2;
		rule->fill = gwd_fill;
		rule->match = gwd_match;
		return RET_DONE;
	} 

	if (!strcmp(url->url.url, "item.yixun.com")) {
		memset(gwd, 0, sizeof(struct gwd_rule));
		gwd->get_sku = yixun_get_sku;
		gwd->skip = yixun_skip;
		gwd->sid = 15;
		rule->fill = gwd_fill;
		rule->match = gwd_match;
		return RET_DONE;
	} 

	if (!strcmp(url->url.url, "www.yougou.com")) {
		memset(gwd, 0, sizeof(struct gwd_rule));
		gwd->get_sku = yougou_get_sku;
		gwd->skip = yougou_skip;
		gwd->sid = 93;
		rule->fill = gwd_fill;
		rule->match = gwd_match;
		return RET_DONE;
	}
#if 0
	if (!strcmp(url->url.url, "product.suning.com")) {
		memset(gwd, 0, sizeof(struct gwd_rule));
		gwd->get_sku = dangdang_get_sku;
		gwd->skip = question_skip;
		gwd->sid = 25;
		rule->fill = gwd_fill;
		rule->match = gwd_match;
		return RET_DONE;
	}
#endif
	if (!strcmp(url->url.url, "item.jd.com")) {
		memset(gwd, 0, sizeof(struct gwd_rule));
		gwd->get_sku = dangdang_get_sku;
		gwd->skip = jd_skip;
		gwd->sid = 3;
		rule->fill = gwd_fill;
		rule->match = gwd_match;
		return RET_DONE;
	}

	if (!strcmp(url->url.url, "www.jxdyf.com")) {
		memset(gwd, 0, sizeof(struct gwd_rule));
		gwd->get_sku = jxdyf_get_sku;
		gwd->skip = jxdyf_skip;
		gwd->sid = 125;
		rule->fill = gwd_fill;
		rule->match = gwd_match;
		return RET_DONE;
	}

	if (!strcmp(url->url.url, "www.jiuxian.com")) {
		memset(gwd, 0, sizeof(struct gwd_rule));
		gwd->get_sku = jiuxian_get_sku;
		gwd->skip = jiuxian_skip;
		gwd->sid = 103;
		rule->fill = gwd_fill;
		rule->match = gwd_match;
		return RET_DONE;
	}

	if (!strcmp(url->url.url, "www.vip.com")) {
		memset(gwd, 0, sizeof(struct gwd_rule));
		gwd->get_sku = vip_get_sku;
		gwd->skip = vip_skip;
		gwd->sid = 129;
		rule->fill = vip_fill;
		rule->match = gwd_match;
		return RET_DONE;
	}

	return RET_ERR;
}

static int __init url_302_init(void)
{
	gwd_hid = register_url_rule_hook(gwd_rule_rule_hook);

	if (gwd_hid < 0) 
		return -1;
	return 0;
}

static void __exit url_302_fini(void)
{
	unregister_url_rule_hook(gwd_hid);
	return ;
}

module_init(url_302_init);
module_exit(url_302_fini);
