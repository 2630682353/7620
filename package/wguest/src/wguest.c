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
#define GUEST_DEBUG(fmt,arg...) do{}while(0)
#else
#define GUEST_DEBUG(fmt,args...) do{printk("%s[%d]:"fmt, __FUNCTION__, __LINE__,##args);}while(0)
#endif

int guest_hid;

static int guest_match(struct url_rule *rule, struct url_match_info *info)
{
	struct nf_http *http;
	struct url_src *url;
	unsigned char *dns;
	
	http = info->http;
	url = &rule->src;
	switch (url->type) {
	case TYPE_ALL:
		dns = http_key_value(http, HTTP_KEY_HOST);
		if (!strcmp(dns, "captive.apple.com"))
			break;
		return 0;
	default:
		break;
	}
	return -1;
}

static int url_rule_guest_ip_skip(struct url_rule *rule, struct url_match_info *info)
{
	struct url_ip *ip;

	if (!rule->period)
		return 0;

	list_for_each_entry(ip, &rule->ip_head, list) {
		if (ip->ip != info->host->ip)
			continue;
		/* fresh */
		if (time_after(jiffies, ip->jiffies + rule->period * 60 * HZ)) {
			ip->matchs = 0;
			ip->jiffies = jiffies;
		}
		if (ip->matchs < rule->times) 
			break;
		return -1;
	}
	return 0;
}

int guest_rule_hook(struct url_rule *rule, struct nlk_url *nlk)
{
	struct url_src *url;
	
	url = &rule->src;
	if (!test_bit(URL_RULE_GUEST_BIT, &nlk->flags)) 
		return RET_GOON;
	switch(url->type) {
	case TYPE_ALL:
		rule->match = guest_match;
		break;
	case TYPE_STD:
		break;
	default:
		return RET_ERR;
	}
	rule->ipskip = url_rule_guest_ip_skip;
	return RET_DONE;
}

static int __init guest_init(void)
{
	guest_hid = register_url_rule_hook(guest_rule_hook);

	if (guest_hid < 0) 
		return -1;
	return 0;
}

static void __exit guest_fini(void)
{
	unregister_url_rule_hook(guest_hid);
	return ;
}

module_init(guest_init);
module_exit(guest_fini);
