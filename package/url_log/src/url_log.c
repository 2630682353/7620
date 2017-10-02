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
#include "url_log.h"

#define HTTP_HOST_LOG_MX 200
#define HTTP_HOST_DUMP_NR 60

#if 1
#define URL_LOG_DEBUG(fmt,arg...) do{}while(0)
#else
#define URL_LOG_DEBUG(fmt,args...) do{printk("%s[%d]"fmt, __FUNCTION__, __LINE__,##args);}while(0)
#endif

static struct url_log_head urlh;
static int host_url_switch = 1; // 1: on, 0: off
static unsigned long notify_jiffies;

struct http_host_node {
	struct list_head list;
	struct nf_http *http;
	unsigned long seconds;
	char mac[ETH_ALEN];
	uint8_t l7_done;
};

/* host need be captured */
struct http_node_cap {
	struct nf_trie_comm comm;
	pid_t pid;
};

struct nf_trie_head cap_root;

LIST_HEAD(http_node_head);
struct kmem_cache *http_node_cache;

static inline struct url_log_head *get_url_log_head(void)
{
	return &urlh;
}

static void url_log_refresh(void)
{
	struct url_log_head *h = get_url_log_head();
	
	nf_trie_free(&h->root);
	nf_trie_root_init(&h->root);
	if (h->data) 
		kfree(h->data);
	h->data = NULL;
	return;
}

static int url_log_data_generic_add(void *dst, void *src, int len)
{
	struct url_log_head *h = get_url_log_head();
	struct url_log_cache *node = (struct url_log_cache *)dst;
	struct nlk_url_data *data = (struct nlk_url_data *)src;

	node->id = data->id;
	node->type = data->type;
	arr_strcpy(node->uri, data->uri);
	arr_strcpy(node->suffix, data->suffix);
	node->suffix_len = strlen(node->suffix);
	node->uri_len = strlen(node->uri);
	str2host(data->url, node->node.name, sizeof(node->node.name));
	node->node.len = strlen(node->node.name);
	nf_trie_add_node(&h->root, &node->node, NULL);
	return NRET_TRUE;
}

static int http_need_upload(int mid)
{
	struct nlk_msg_comm msg;

	if (time_before(jiffies, notify_jiffies + HZ)) 
		return 0;
	memset(&msg, 0x0, sizeof(msg));
	notify_jiffies = jiffies;
	msg.gid = NLKMSG_GRP_LOG;
	msg.mid = mid;
	nlk_send_broadcast(NLKMSG_GRP_LOG, &msg, sizeof(msg));
	return 0;
}

int url_log_generic_set(struct nlmsghdr *nlh, int *offset, struct nlk_msg_comm *nlk)
{
	int len = 0, nr = 0;
	unsigned char buf[512];
	unsigned char *data = NULL;
	struct url_log_head *h = get_url_log_head();
	int cnt = 0;

	len = nlk->obj_len;
	nr = nlk->obj_nr;
	data = kzalloc(h->obj_len * nr, GFP_ATOMIC);
	if (NULL == data)
		return IGD_ERR_NO_MEM;

	url_log_refresh();
	loop_for_each(0, nr) {
		memset(buf, 0x0, sizeof(buf));
		if (nlk_data_pop(nlh, offset, buf, len)) { 
			kernel_msg("pop error, nr = %d\n", i);
			continue;
		}
		url_log_data_generic_add(data + i * h->obj_len, buf, len);
		cnt++;
	} loop_for_each_end();
	h->data = (struct url_log_cache *)data;

	return cnt;
}

static inline void http_host_node_free(struct http_host_node *node)
{
	list_del(&node->list);
	igd_put(node->http);
	kmem_cache_free(http_node_cache, node);
	gb_http_log_cnt()--;
	return ;
}

static void *http_host_node_get_next(void *last, void *args)
{
	struct list_head *head = &http_node_head;

	if (list_empty(head)) 
		return NULL;
	return list_first_entry(head, struct http_host_node, list);
}

static int http_host_node_copy(void *dst, void *src, int len)
{
	struct http_host_log *log = dst;
	struct http_host_node *node = src;
	struct nf_http *http;

	http = node->http;
	memcpy(log->buf, http->data, 
		http->data_len > sizeof(log->buf) ?
	       	sizeof(log->buf) : http->data_len);
	loop_for_each(0, HTTP_KEY_TYPE_MX) {
		log->key_len[i] = http->key_len[i];
		log->key_offset[i] = http->key_offset[i] * NF_HTTP_SHIFT;
	} loop_for_each_end();

	if (http->post)
		arr_strcpy(log->post, http->post->buf);
	if (!test_bit(HTTP_GET_BIT, &http->flags)) 
		log->is_post = 1;
	memcpy(log->mac, node->mac, ETH_ALEN);
	log->seconds = node->seconds;
	log->l7_done = node->l7_done;

	http_host_node_free(node);
	return 0;
}

/* aready hold lock*/
static int do_http_node(struct nf_conn *ct,
	       	struct host_struct *host, struct nf_http *http)
{
	struct http_host_node *node;
	struct host_app_head *stat;

	/* create new node */
	if (gb_http_log_cnt() >= HTTP_HOST_LOG_MX) {
		kernel_msg_rate("root full %d\n", gb_http_log_cnt());
		return -1;
	}

	node = kmem_cache_zalloc(http_node_cache, GFP_ATOMIC);
	if (!node)
		return -1;

	node->http = igd_hold(http);
	node->seconds = get_seconds();
	memcpy(node->mac, host->mac, ETH_ALEN);

	stat = ct_app(ct);
	if (stat && stat->comm.id)
		node->l7_done = 1;
	else if (test_bit(HTTP_SKIP_BIT, &http->flags)) 
		node->l7_done = 2;
		
	gb_http_log_cnt()++;
	list_add_tail(&node->list, &http_node_head);
	URL_LOG_DEBUG("create %s%s, sufix %s, cnt %d\n",
		       	http_key_value(http, HTTP_KEY_HOST),
		       	http_key_value(http, HTTP_KEY_URI),
			http_key_value(http, HTTP_KEY_SUFFIX), gb_http_log_cnt());
	if (gb_http_log_cnt() < HTTP_HOST_DUMP_NR)
		return 0;
	http_need_upload(LOG_GRP_MID_HTTP);
	return 0;
}

/*  free all http node*/
static void http_host_node_clean(void)
{
	struct http_host_node *node;
	struct http_host_node *tmp;

	URL_COUNT_LOCK();
	list_for_each_entry_safe(node, tmp, &http_node_head, list) {
		http_host_node_free(node);
	}
	URL_COUNT_UNLOCK();

	return ;
}

static unsigned int host_need_log(struct nf_http *http, struct nf_conn *ct)
{
	struct url_log_head *h = get_url_log_head();
	struct url_log_cache *url;
	struct nf_trie_node *node;
	unsigned char *tmp;
	
	URL_COUNT_LOCK();
	//match safe url list
	tmp = http_key_value(http, HTTP_KEY_HOST);
	node = nf_trie_match(&h->root, tmp, NULL, NULL);
	if (!node)
		goto out;
	url = container_of(node, struct url_log_cache, node);
	tmp = http_key_value(http, HTTP_KEY_SUFFIX);
	if (url->suffix_len && 
		strncmp(tmp, url->suffix, url->suffix_len))
		goto out;
	tmp = http_key_value(http, HTTP_KEY_URI);
	if (url->uri_len && 
		strncmp(tmp + 1, url->uri, url->uri_len))
		goto out;
	URL_COUNT_UNLOCK();
	return 0;
out:
	URL_COUNT_UNLOCK();
	return -1;
}
#define TYPE_REQ_HEAD	1
#define TYPE_ACK_INFO	2

static int nlk_http2_send(char *data, int data_len, int type, 
			struct nf_conn *ct, int gid);

static int http_detach_ct(int action, void *data, void *extra)
{
	struct nf_http *http = data;
	struct nf_conn *ct = extra;
	struct host_struct *host;
	
	if (!host_url_switch)
		goto out;

	host = ct_host(ct);
	if (!host) 
		goto out;
	
	if (!test_bit(CT_WAN_BIT, ct_flags(ct)))
		goto out;
	
	if (!http_key_len(http, HTTP_KEY_HOST)) 
		goto out;
	
	if (test_bit(HTTP_LANXUN_BIT, &http->flags)) {
		lanxun_http_info lhi;
		memset(&lhi, 0, sizeof(lanxun_http_info));
		lhi.rep_code = http->rep_code;
		lhi.rep_len = http->rep_len;

		nlk_http2_send((void *)&lhi, sizeof(lanxun_http_info), 
			TYPE_ACK_INFO, ct, NLKMSG_GRP_HTTP2);
	}
	
	if (!host_need_log(http, ct))
		goto log;
	if (test_bit(HTTP_SKIP_LOG_BIT, &http->flags))
		goto out;
log:
	URL_COUNT_LOCK();
	do_http_node(ct, host, http);
	URL_COUNT_UNLOCK();
out:
	return 0;
}

struct net_notifier http_notify = {
	.fn = http_detach_ct,
};

static int nlk_http_reply(struct sk_buff *skb,
	       struct nf_conn *ct, struct host_struct *host, int gid)
{
	struct nlk_http_header *h;
	struct sk_buff *skb2;
	struct nf_http *http = ct_http(ct);
	struct nlmsghdr *nlh;
	int len;

	/*  iph + tcph <= 60 */
	len = NLMSG_LENGTH(sizeof(struct nlk_http_header) + 60);
	skb2 = alloc_skb(len + 256, GFP_ATOMIC);
	if (!skb2) 
		return NF_ACCEPT;
	skb_reserve(skb2, 64);
	skb_put(skb2, len);
	nlh = (struct nlmsghdr *)skb2->data;
	nlh->nlmsg_len = len;
	nlh->nlmsg_pid = 0;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_type = 0;
	nlh->nlmsg_seq = 0;
	h = NLMSG_DATA(nlh);
	h->comm.gid = gid,
	h->id = ct_id(ct);
	h->syn = ct_syn(ct, 0) + 1;
	h->seq = http->get_seq;
	memcpy(h->mac, host->mac, ETH_ALEN);
	netlink_broadcast(nlk_broadcast_sock, skb2,
		       	0, gid, GFP_ATOMIC);
	return NF_ACCEPT;
}

static int nlk_http_send(struct sk_buff *skb, struct nf_conn *ct,
      			 struct host_struct *host, pid_t pid, int gid)
{
	struct nlk_http_header *h;
	struct nlmsghdr *nlh;
	struct sk_buff *skb2;
	int len;

	len = NLMSG_LENGTH(sizeof(struct nlk_http_header) + skb->len);
	if (len > 1800) 
		return -1;
	skb2 = alloc_skb(len + 128, GFP_ATOMIC);
	if (!skb2) 
		return 0;

	skb_reserve(skb2, 64);
	skb_put(skb2, len);
	nlh = (struct nlmsghdr *)skb2->data;
	nlh->nlmsg_len = len;
	nlh->nlmsg_pid = 0;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_type = 0;
	nlh->nlmsg_seq = 0;
	h = NLMSG_DATA(nlh);
	h->comm.gid = gid,
	h->id = ct_id(ct);
	h->syn = ct_syn(ct, 0) + 1;
	h->seq = 0;
	memcpy(h->mac, host->mac, ETH_ALEN);
	memcpy(h->data, skb->data, skb->len);
	if (pid)
		netlink_unicast(nlk_broadcast_sock, skb2, pid, GFP_ATOMIC);
	else
		netlink_broadcast(nlk_broadcast_sock, skb2,
		 	      		0, gid, GFP_ATOMIC);
	return 0;
}
static int nlk_http2_send(char *data, int data_len, int type, 
			struct nf_conn *ct, int gid)
{
	struct nlk_http_header *h;
	struct nlmsghdr *nlh;
	struct sk_buff *skb2;
	int len;

	struct host_struct *host = ct_host(ct);
	
	len = NLMSG_LENGTH(sizeof(struct nlk_http_header) + data_len);
	if (len > 1800) 
		return -1;
	skb2 = alloc_skb(len + 128, GFP_ATOMIC);
	if (!skb2) 
		return 0;

	skb_reserve(skb2, 64);
	skb_put(skb2, len);
	nlh = (struct nlmsghdr *)skb2->data;
	nlh->nlmsg_len = len;
	nlh->nlmsg_pid = 0;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_type = 0;
	nlh->nlmsg_seq = 0;
	h = NLMSG_DATA(nlh);
	h->comm.gid = gid,
	h->id = ct_id(ct);
	h->syn = ct_syn(ct, 0) + 1;
	h->seq = 0;
	h->mtype = type;
	
	memcpy(h->mac, host->mac, ETH_ALEN);
	if (data) {
		memcpy(h->data, data, data_len);
	}

	netlink_broadcast(nlk_broadcast_sock, skb2, 0, gid, GFP_ATOMIC);
	return 0;
}


static unsigned int nlk_http_forward(unsigned int hooknum, struct sk_buff *skb,
	      		const struct net_device *in, const struct net_device *out,
	       					int (*okfn)(struct sk_buff *))
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;
	struct host_struct *host;
	struct http_node_cap *cap;
	struct nf_http *http;
	char dns[IGD_DNS_LEN];
	struct tcphdr *tcph;
	unsigned char *tmp;

	ct = nf_ct_get(skb, &ctinfo);
	if (unlikely(!ct)) 
		return NF_ACCEPT;
	if (!test_bit(SKB_HTTP_MMAP, skb_flags(skb)))
		return NF_ACCEPT;
       	host = ct_host(ct);
	if (!host) 
		return NF_ACCEPT;
	http = ct_http(ct);
	if (!http) 
		return NF_ACCEPT;
	tmp = http_key_value(http, HTTP_KEY_HOST);
	if (str2host(tmp, dns, sizeof(dns)) < 0)
		goto all;

	URL_COUNT_LOCK();
	cap = (void *)__nf_trie_match2(&cap_root.root, dns, NULL, NULL);
	if (!cap) {
		URL_COUNT_UNLOCK();
		goto all;
	}
	URL_COUNT_UNLOCK();
	
	tcph = l4_hdr(skb);
	if (ntohl(tcph->seq) != ct_syn(ct, 0) + 1)
		goto all;

	set_bit(HTTP_LANXUN_BIT, &http->flags);
	nlk_http2_send(skb->data, skb->len, TYPE_REQ_HEAD, ct, NLKMSG_GRP_HTTP2);
all:
	if (in->uid)
		return nlk_http_reply(skb, ct, host, NLKMSG_GRP_HTTP);
	nlk_http_send(skb, ct, host, 0, NLKMSG_GRP_HTTP);
	return NF_ACCEPT;
}

static struct nf_hook_ops nlk_http_ops[] __read_mostly = {
	{
		.hook		= nlk_http_forward,
		.owner		= THIS_MODULE,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_FORWARD,
		.priority	= NF_IP_PRI_HOST_FORWARD + 1,
	},
};

static int nlk_http_init(void)
{
	nf_register_hook(nlk_http_ops);
	return 0;
}

static int nlk_http_cap_action(struct nlmsghdr *nlh,
	       	struct nlk_http_host *nlk, int *offset)
{
	struct http_node_cap *node;
	char buf[IGD_DNS_LEN];
	char dns[IGD_DNS_LEN];
	int cnt = 0;
	int len;
	pid_t pid = task_pid_nr(current);

	switch (nlk->comm.action) {
	case NLK_ACTION_ADD:
		if (nlk->comm.obj_len != IGD_DNS_LEN)
			return -1;
		if (nlk->comm.obj_nr <= 0) 
			return -1;
		break;
	case NLK_ACTION_CLEAN:
		if (nlk->comm.mid < 0) 
			return -1;
		URL_COUNT_LOCK();
		cnt = nf_trie_head_clean(&cap_root, nlk->comm.mid);
		URL_COUNT_UNLOCK();
		return cnt;
	default:
		return -1;
	}

	URL_COUNT_LOCK();
	loop_for_each(0, nlk->comm.obj_nr) {
		if (nlk_data_pop(nlh, offset, buf, nlk->comm.obj_len)) 
			break;
		len = str2host(buf, dns, sizeof(dns));
		if (len < 0)
			continue;
		node = kzalloc(sizeof(struct http_node_cap), GFP_ATOMIC);
		if (!node) 
			break;
		arr_strcpy(node->comm.node.name, dns);
		node->comm.node.len = len;
		node->comm.mid = nlk->comm.mid;
		node->pid = pid;
		if (nf_trie_head_add_node(&cap_root,
			       	&node->comm.node, nf_trie_add_fn)) {
			kfree(node);
			continue;
		}
		list_add_tail(&node->comm.list, &cap_root.list);
		cnt++;
	} loop_for_each_end();
	URL_COUNT_UNLOCK();

	return cnt;
}

static int nlk_url_log_action(struct nlmsghdr *nlh)
{
	int ret = IGD_ERR_DATA_ERR;
	struct nlk_http_host nlk;
	int offset = 0;

	memset(&nlk, 0x0, sizeof(nlk));
	if (nlk_data_pop(nlh, &offset, &nlk, sizeof(nlk)))
		return IGD_ERR_DATA_ERR;

	switch (nlk.comm.key) {
	case NLK_HTTP_HOST_SHOP:
		URL_COUNT_LOCK();
		switch (nlk.comm.mid) {
		case NLK_URL_LOG_PARAM:
			if (nlk.comm.obj_nr > URL_LOG_LIST_MX || nlk.comm.obj_nr <= 0
				|| nlk.comm.obj_len != sizeof(struct nlk_url_data))
				break;
			ret = url_log_generic_set(nlh, &offset, &nlk.comm);
			break;
		default:
			break;
		}
		URL_COUNT_UNLOCK();
		break;
	
	case NLK_HTTP_HOST_URL:
		switch (nlk.comm.action) {
		case NLK_ACTION_ADD:
			host_url_switch = 1;
			return 0;
		case NLK_ACTION_DEL:
			host_url_switch = 0;
			return 0;
		case NLK_ACTION_DUMP:
			if (nlk.comm.obj_len != sizeof(struct http_host_log))
				break;
			URL_COUNT_LOCK();
			ret = nlk_rule_dump2(nlh, http_host_node_get_next,
				       		NULL, http_host_node_copy);
			URL_COUNT_UNLOCK();
			break;
		default:
			break;
		}
		break;
	case NLK_HTTP_HOST_CAP:
		return nlk_http_cap_action(nlh, &nlk, &offset);
	default:
		break;
	}
	return ret;
}

static int dns_notify_fn(int action, void *data, void *extra)
{
	struct dns_cap_k *info = data;
	struct nlmsghdr *nlh;
	struct nlk_dns_msg *nlk; 
	struct sk_buff *skb;
	int len;

	len = NLMSG_LENGTH(sizeof(struct nlk_dns_msg));
	skb = alloc_skb(128 + len, GFP_ATOMIC | __GFP_ZERO);
	if (!skb)
		return 0;

	skb_reserve(skb, 64);
	skb_put(skb, len);
	nlh = (struct nlmsghdr *)skb->data;
	nlh->nlmsg_len = len;
	nlh->nlmsg_pid = 0;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_type = 0;
	nlh->nlmsg_seq = 0;
	nlk = NLMSG_DATA(nlh);
	nlk->comm.gid = NLKMSG_GRP_DNS,
	memcpy(nlk->mac, info->host->mac, ETH_ALEN);
	host2str(info->dns, nlk->dns, sizeof(nlk->dns));
	loop_for_each(0, info->addr_cnt) {
		nlk->addr[i] = ntohl(info->addr[i]);
	} loop_for_each_end();
	netlink_broadcast(nlk_broadcast_sock, skb, 0, NLKMSG_GRP_DNS, GFP_ATOMIC);

	return 0;
}

struct net_notifier dns_notify = {
	.fn = dns_notify_fn,
};

static int __init wh_url_count_init(void)
{
	struct url_log_head *h = get_url_log_head();

	http_node_cache = kmem_cache_create("tracert",
		       	sizeof(struct http_host_node), 0, 0, NULL);
	if (!http_node_cache) 
		return -1;
	spin_lock_init(&url_count_lock);
	memset(&url_log_fn, 0x0, sizeof(url_log_fn));
	url_log_fn.url_log_action = nlk_url_log_action;
	memset(h, 0x0, sizeof(*h));
	nf_trie_root_init(&h->root);
	h->mx = URL_LOG_MX_NR;
	h->obj_len = sizeof(struct url_log_cache);
	register_http_notifier(&http_notify);
	register_dns_notifier(&dns_notify);
	nlk_http_init();

	nf_trie_head_init(&cap_root, 512);
	notify_jiffies = jiffies;

	return 0;
}


/*  doesn't support rmmod */
#if 0
static void __exit wh_url_count_fini(void)
{

	memset(&url_log_fn, 0x0, sizeof(url_log_fn));
	unregister_http_notifier(&http_notify);
	URL_COUNT_LOCK();
	url_log_refresh();
	URL_COUNT_UNLOCK();

	http_host_node_clean();
	kmem_cache_destroy(http_node_cache);
	nf_unregister_hook(nlk_http_ops);
#if 0
#ifdef CONFIG_PROC_FS
	remove_proc_entry("url_log_debug", proc_dir);
#endif
#endif
	return;
}
#endif

module_init(wh_url_count_init);
