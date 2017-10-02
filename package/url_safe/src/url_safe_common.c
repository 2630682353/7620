#include <linux/version.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/icmp.h>
#include <linux/kallsyms.h>
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
#include <net/tcp.h>
#include <net/arp.h>
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
#include <linux/netfilter_ipv4/nf_igd/product.h>
#include <linux/netfilter_ipv4/nf_igd/nf_igd.h>
#include <linux/netfilter_ipv4/nf_igd/nf_trie.h>
#include "url_safe_common.h"

void wh_url_cache_free(struct wh_url_cache_head *h, struct wh_url_cache *url)
{
	struct wh_safe_ip *ip;
	struct wh_safe_ip *tmp;

	list_del(&url->list);
	list_for_each_entry_safe(ip, tmp, &url->list_ip, list) {
		list_del(&ip->list);
		kfree(ip);
	}
	nf_trie_del_node(url->root, &url->node);
	kfree(url);
	h->nr--;
	return ;
}

static void wh_url_node_add_fn(struct nf_trie_root *root, struct nf_trie_node *node)
{
	struct wh_url_cache *url = container_of(node, struct wh_url_cache, node);

	url->root = root;
	return ;
}

struct wh_url_cache * wh_url_add(struct wh_url_cache_head *h, unsigned char *host, struct wh_skb_list *http)
{
	struct wh_url_cache *url;

	if (h->nr >= h->mx) {
		url = list_entry(h->list.next, struct wh_url_cache, list);
		wh_url_cache_free(h, url);
	}

	url = kzalloc(sizeof(*url), GFP_ATOMIC);
	if (!url) 
		return NULL;
	url->jiffies = jiffies;
	url->last_time = jiffies;
	if (NULL != http) {
		url->wh_type = http->wh_type;
		url->url_type = http->url_type;
	}
	INIT_LIST_HEAD(&url->list_ip);
	INIT_LIST_HEAD(&url->list);
	//例www.baidu.com change to [3www5baidu3com]
	str2host(host, url->node.name, sizeof(url->node.name));
	url->node.len = strlen(url->node.name);
	
	if (nf_trie_add_node(&h->root, &url->node, wh_url_node_add_fn))
		goto free;
	list_add_tail(&url->list, &h->list);
	h->nr++;
	return url;
free:
	kfree(url);
	return NULL;
}

int wh_url_del(struct wh_url_cache_head *h, unsigned char *host)
{
	struct wh_url_cache *url;
	struct nf_trie_node *tmp;

	tmp = nf_trie_match(&h->root, host, NULL, NULL);
	if (!tmp)
		return NRET_FALSE;
	url = container_of(tmp, struct wh_url_cache, node);
	wh_url_cache_free(h, url);
	return NRET_TRUE;
}

struct wh_url_cache * _wh_add_url_cache(struct wh_url_cache_head *h, unsigned char *name, struct wh_skb_list *http)
{
	struct nf_trie_node *url;
	
	url = nf_trie_match(&h->root, name, NULL, NULL);
	if (url)
		return NULL;
	return wh_url_add(h, name, http);
}

struct wh_url_cache * wh_add_url_cache(struct wh_url_cache_head *h, struct wh_skb_list *http)
{
	struct nf_host_dot dot[NF_HOST_DOT_MX];
	unsigned char *name;
	unsigned char *dns;


	memset(dot, 0x0, sizeof(dot));
	dns = http_key_value(http->http_info, HTTP_KEY_HOST);
	if (nf_spilt_host_by_dot(dns, dot))
		return NULL;
	//将URL的三段加入cache
	name = dot[1].name ? dot[1].name : dot[0].name;
	return _wh_add_url_cache(h, name, http);
}

