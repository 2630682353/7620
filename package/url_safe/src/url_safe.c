
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
#include <linux/netfilter_ipv4/nf_igd/nf_igd.h>
#include "url_safe_common.h"
#include "url_safe.h"
#include "md5.h"

static int debug;
static struct proc_dir_entry *debug_proc;
static struct wh_url_safe_head url_h;
static inline struct wh_url_safe_head *wh_get_head(void)
{
	return &url_h;
}

static void wh_url_head_init(struct wh_url_cache_head *h, int mx)
{
	memset(h, 0x0, sizeof(*h));
	INIT_LIST_HEAD(&h->list);
	nf_trie_root_init(&h->root);
	h->mx = mx;
}

static inline void __wh_skb_list_free(struct wh_skb_list *list)
{
	if (list->check_skb) 
		kfree_skb(list->check_skb);
	igd_put(list->http_info);
	list_del_init(&list->http_list);
	list_del_init(&list->check_list);
	kfree(list);
	return ;
}

static void wh_skb_list_free(struct wh_skb_list *list)
{
	struct sk_buff *skb;
	struct nf_conn *ct;

	while ((skb = __skb_dequeue(&list->skb_queue)) != NULL) {
		ct = (struct nf_conn *)skb->nfct;
		set_bit(CT_MATCH_URL_SAFE_BIT, ct_flags(ct));
		ip_output(skb);
	}
	__wh_skb_list_free(list);
	return ;
}

static void wh_add_safe_ip(struct wh_url_cache *tmp, uint32_t addr)
{
	struct wh_safe_ip *ip = NULL;
	
	if (!tmp)
		return;
	ip = (struct wh_safe_ip*)kzalloc(sizeof(*ip), GFP_ATOMIC);
	if (!ip)
		return;
	memset(ip, 0x0, sizeof(*ip));
	ip->ip = addr;
	list_add_tail(&ip->list, &tmp->list_ip);
}

static void url_safe_refresh(void)
{	
	struct wh_skb_list *http;
	struct wh_skb_list *tmp;
	struct wh_url_safe_head *h = wh_get_head();

	if (h->enable) {
		if (!timer_pending(&h->http_pkg_timer.timer)) 
			mod_timer(&h->http_pkg_timer.timer, jiffies + WH_HTTP_TIMER_TIMEOUT);
		if (!timer_pending(&h->check_pkg_timer.timer))
			mod_timer(&h->check_pkg_timer.timer, jiffies + WH_HTTP_TIMER_TIMEOUT);
		return;
	}

	list_for_each_entry_safe(http, tmp, &h->check_skb_list, check_list) {
		wh_skb_list_free(http);
	}
	return;
}

static int wh_url_safe_action(struct nlmsghdr *nlh)
{
	int offset = 0;
	int nr, len;
	char host[IGD_DNS_LEN];
	struct nlk_msg_comm nlk;
	struct nlk_url_ip ip;
	struct nlk_url_sec param;
	struct nlk_url_set enable;
	struct wh_url_cache *url = NULL;
	struct wh_url_cache *tmp = NULL;
	struct nf_trie_node *node = NULL;
	struct wh_url_safe_head *h = wh_get_head();

	memset(&nlk, 0x0, sizeof(nlk));
	if (nlk_data_pop(nlh, &offset, &nlk, sizeof(nlk)))
		return IGD_ERR_DATA_ERR;
	KERNEL_DEBUG(debug, "recv nlk msg id is %d\n", nlk.mid);
	nr = nlk.obj_nr;
	len = nlk.obj_len;
	WH_HOST_LOCK();
	switch (nlk.mid) {
	case URL_SEC_ENABLE:
		offset = 0;
		memset(&enable, 0x0, sizeof(enable));
		if (nlk_data_pop(nlh, &offset, &enable, sizeof(enable)))
			goto ERROR;
		if (h->enable != enable.enable) {
			h->enable = enable.enable;
			url_safe_refresh();
		}
		break;
	case URL_SET_PARAM:
		offset = 0;
		memset(&param, 0x0, sizeof(param));
		if (nlk_data_pop(nlh, &offset, &param, sizeof(param)))
			goto ERROR;
		h->device_id = param.devid;
		loop_for_each(0, DNS_IP_MX) {
			h->url_server_ip[i] = param.sip[i];
		} loop_for_each_end();
		h->url_server_port = param.sport;
		h->web_ip = param.rip;
		h->web_port = param.rport;
		snprintf(h->rd_url, sizeof(h->rd_url), "%s/intercept.html", param.rdhost);
		KERNEL_DEBUG(debug, 
					"param, ip is %u.%u.%u.%u,"
					"port %d, wip %u.%u.%u.%u,"
					"wport %d, devid %u,"
					"rdhost %s\n",
					NIPQUAD(param.sip[0]),
					param.sport,
					NIPQUAD(param.rip),
					param.rport,
					param.devid,
					h->rd_url);
		break;
	case URL_SET_SAFE_IP:
		offset = 0;
		memset(&ip, 0x0, sizeof(ip));
		if (nlk_data_pop(nlh, &offset, &ip, sizeof(ip)))
			goto ERROR;
		KERNEL_DEBUG(debug, "recv user space msg, ip is %u.%u.%u.%u, url is %s\n", NIPQUAD(ip.addr), ip.url);
		node = nf_trie_match(&h->url_evil.root, ip.url, NULL, NULL);
		if (!node)
			goto ERROR;
		tmp = container_of(node, struct wh_url_cache, node);
		wh_add_safe_ip(tmp, ip.addr);
		break;
	case URL_SEC_WLIST:
		if (nlk.action == NLK_ACTION_ADD) {
			if (nr <= 0 || nr > URL_WLIST_PER_MX)
				goto ERROR;
			if (len != IGD_DNS_LEN)
				goto ERROR;
			loop_for_each(0, nr) {
				memset(host, 0x0, sizeof(host));
				if (nlk_data_pop(nlh, &offset, host, len))
					goto ERROR;
				wh_url_add(&h->url_vip, host, NULL);
			}loop_for_each_end();
		} else if (nlk.action == NLK_ACTION_DEL) {
			list_for_each_entry_safe(url, tmp, &h->url_vip.list, list) {
				wh_url_cache_free(&h->url_vip, url);
			}
			nf_trie_free(&h->url_vip.root);
			wh_url_head_init(&h->url_vip, WH_URL_WLIST_MX_NR);
		}
		break;
	default:
		break;
	}
	WH_HOST_UNLOCK();
	return NRET_TRUE;
ERROR:
	WH_HOST_UNLOCK();
	return IGD_ERR_DATA_ERR;
}

static int wh_url_check_report(struct sk_buff *old_skb, uint8_t wh_type)
{
	uint8_t *tmp;
	uint16_t len = 0;
	struct sk_buff *skb;
	struct url_comm *h;
	struct nf_conn *ct;
	struct udphdr *udph;
	struct nf_http *http_info;
	struct iphdr *iph = ip_hdr(old_skb);
	struct tcphdr *tcph = l4_hdr(old_skb);
	enum ip_conntrack_info ctinfo;
	struct wh_url_safe_head *wh = wh_get_head();
	struct net_device *dev = skb_dst(old_skb)->dev;
	struct ethhdr  *eth = (struct ethhdr *)skb_mac_header(old_skb);
	unsigned char *host;
	unsigned char *uri;
	int host_len;
	int uri_len;
	
	if (!wh->url_server_ip[0]) 
		return -1;
	if (!(ct = nf_ct_get(old_skb, &ctinfo)))
		return -1;
	if (!(http_info = ct_http(ct)))
		return -1;
	host = http_key_value(http_info, HTTP_KEY_HOST);
	uri = http_key_value(http_info, HTTP_KEY_URI);
	skb = alloc_skb(800, GFP_ATOMIC | __GFP_ZERO);
	if (!skb) 
		return -1;
	skb->ip_summed = CHECKSUM_NONE;
	skb_reserve(skb, 64 + sizeof(struct iphdr) + sizeof(struct udphdr));
	tmp = skb->data;
	h = (struct url_comm *)tmp;
	/*add report head*/
	h->cmd = htons(HTTP_REPORT_CMD);
	h->devid = htonl(wh->device_id);
	len += sizeof(struct url_comm);
	tmp += sizeof(struct url_comm);
	/*add url type*/
	*tmp = wh_type;
	len += sizeof(uint8_t);
	tmp += sizeof(uint8_t);
	/*add host mac*/
	sprintf(tmp, "%02X%02X%02X%02X%02X%02X",
		       	eth->h_source[0], eth->h_source[1],
			eth->h_source[2], eth->h_source[3],
		       	eth->h_source[4], eth->h_source[5]);
	len += 0x0c;
	tmp += 0x0c;
	/*add url len*/
	host_len = http_key_len(http_info, HTTP_KEY_HOST);
	uri_len = http_key_len(http_info, HTTP_KEY_URI);

	if (host_len + uri_len > 0xff) {
		kfree(skb);
		return 0;
	}
	*tmp = host_len + uri_len;
	len++;
	tmp++;
	/*add url*/
	sprintf(tmp, "%s%s", host, uri);
	len += (host_len + uri_len);
	tmp = tmp + host_len + uri_len;
	/*add sport*/
	*(uint16_t *)tmp = tcph->source;
	tmp = tmp + 2;
	len += 2;

	/*add dport*/
	*(uint16_t *)tmp = tcph->dest;
	tmp = tmp + 2;
	len += 2;

	/*add sip*/
	*(uint32_t *)tmp = iph->saddr;
	len += 4;
	tmp = tmp + 4;

	/*add dip*/
	*(uint32_t *)tmp = iph->daddr;
	len += 4;
	h->tot_len = htons(len);
	
	skb_put(skb, len);
	skb_push(skb, sizeof(struct udphdr));
	skb_reset_transport_header(skb);
	udph = (struct udphdr *)skb_transport_header(skb);
	udph->source = htons(HTTP_REPORT_SRC_PORT);
	udph->dest = htons(wh->url_server_port);
	udph->len = htons(len + 8);
	udph->check = 0;

	skb_push(skb, sizeof(struct iphdr));
	skb_reset_network_header(skb);
	skb_reset_mac_header(skb);
	iph = ip_hdr(skb);
	memset(iph, 0, sizeof(*iph));
	iph->version = 4;
	iph->ihl = 5;
	iph->saddr = ct->tuplehash[1].tuple.dst.u3.ip;
	iph->daddr = wh->url_server_ip[0];
	iph->ttl = 128;
	iph->protocol = IPPROTO_UDP;
	iph->tot_len = htons(skb->len);
	//iph->id = h.check_id;
	ip_send_check(iph);
	skb_dst_set(skb, dst_clone(skb_dst(old_skb)));
	skb->dev = dev;
	skb->protocol = htons(ETH_P_IP);
	set_bit(SKB_SKIP_QOS, skb_flags(skb));
	ip_finish_output(skb);
	return 0;
}

static int wh_url_check_request(struct sk_buff *old_skb, struct wh_skb_list *http)
{
	int len = 0, i;
	struct iphdr *iph;
	struct udphdr *udph;
	struct sk_buff *skb;
	struct url_comm *h;
	unsigned char buff[256];
	unsigned char md5[16];
	static uint32_t check_id;
	unsigned char *tmp, *data;
	struct net_device *dev = skb_dst(old_skb)->dev;
	struct wh_url_safe_head *wh = wh_get_head();
	unsigned char *dns;
	unsigned char *uri;

	if (!http->dip) 
		return -1;
	skb = alloc_skb(800, GFP_ATOMIC | __GFP_ZERO);
	if (!skb) 
		return -1;
	skb->ip_summed = CHECKSUM_NONE;
	skb_reserve(skb, 64 + sizeof(struct iphdr) + sizeof(struct udphdr));
	/*add check header*/
	tmp = skb->data;
	h = (struct url_comm *)tmp;
	h->cmd = htons(HTTP_CHECK_CMD);
	h->devid = htonl(wh->device_id);
	len += sizeof(struct url_comm);
	tmp += sizeof(struct url_comm);
	/*add check id*/
	*(uint32_t *)tmp = htonl(check_id);
	http->check_id = check_id++;
	len += sizeof(uint32_t);
	tmp += sizeof(uint32_t);
	dns = http_key_value(http->http_info, HTTP_KEY_HOST);
	uri = http_key_value(http->http_info, HTTP_KEY_URI);
	/*get url md5*/
	snprintf(buff, sizelen(buff), "%s%s%s", HTTP_CHECK_KEY, dns, uri);
	get_url_md5(buff, md5, strlen(buff));
	data = buff;
	for (i = 0; i < sizeof(md5); i++) {
		sprintf(data, "%02x", md5[i]);
		data += 2;
	}
	*data = '\0';
	/*add url md5 len*/
	*(uint16_t *)tmp = htons(strlen(buff));
	len += sizeof(uint16_t);
	tmp += sizeof(uint16_t);
	/*add url md5*/
	memcpy(tmp, buff, strlen(buff));
	len += strlen(buff);
	tmp += strlen(buff);
	/*add new version byte*/
	*tmp = 1;
	len++;
	h->tot_len = htons(len);
	
	skb_put(skb, len);
	skb_push(skb, sizeof(struct udphdr));
	skb_reset_transport_header(skb);
	udph = (struct udphdr *)skb_transport_header(skb);
	udph->source = htons(HTTP_CHECK_SRC_PORT);
	udph->dest = htons(wh->url_server_port);
	udph->len = htons(len + 8);
	udph->check = 0;

	skb_push(skb, sizeof(struct iphdr));
	skb_reset_network_header(skb);
	skb_reset_mac_header(skb);
	iph = ip_hdr(skb);
	memset(iph, 0, sizeof(*iph));
	iph->version = 4;
	iph->ihl = 5;
	iph->saddr = http->sip;
	iph->daddr = http->dip;
	iph->ttl = 128;
	iph->protocol = IPPROTO_UDP;
	iph->tot_len = htons(skb->len);
	//iph->id = h.check_id;
	ip_send_check(iph);
	skb_dst_set(skb, dst_clone(skb_dst(old_skb)));
	skb->dev = dev;
	skb->protocol = htons(ETH_P_IP);
	http->check_skb = skb_copy(skb, GFP_ATOMIC);
	set_bit(SKB_SKIP_QOS, skb_flags(skb));
	ip_finish_output(skb);
	return 0;
}

static int wh_evil_url_warn_xmit(struct nf_conn *ct, struct sk_buff *skb, struct sk_buff *warn_skb)
{
	struct iphdr *iph, *o_iph;
	struct tcphdr *tcph, *o_tcph;
	struct ethhdr *n_eth, *o_eth= (struct ethhdr *)skb_mac_header(skb);
	struct nf_conntrack_tuple *tuple = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	int old_len;

	o_iph = ip_hdr(skb);
	o_tcph = (struct tcphdr *)((char *)o_iph + o_iph->ihl * 4);
	old_len = ntohs(o_iph->tot_len) - o_iph->ihl * 4 - o_tcph->doff * 4;

	skb_push(warn_skb, sizeof(struct tcphdr));
	skb_reset_transport_header(warn_skb);
	tcph = (struct tcphdr *)skb_transport_header(warn_skb);
	memcpy(warn_skb->data, o_tcph, sizeof(struct tcphdr));
	tcph->doff = 5;
	tcph->source = tuple->dst.u.tcp.port;
	tcph->dest = tuple->src.u.tcp.port;
	tcph->ack = 1;
	tcph->fin = 1;
	tcph->psh = 1;
	tcph->seq = o_tcph->ack_seq;
	tcph->ack_seq = htonl(ntohl(o_tcph->seq) + old_len);
	tcph->check = 0;
	tcph->urg_ptr = 0;
	tcph->check = tcp_v4_check(warn_skb->len, tuple->dst.u3.ip, tuple->src.u3.ip, csum_partial(tcph, warn_skb->len, 0));

	skb_push(warn_skb, sizeof(struct iphdr));
	skb_reset_network_header(warn_skb);
	iph = ip_hdr(warn_skb);
	memcpy(warn_skb->data, o_iph, sizeof(struct iphdr));

	iph->check = 0;
	iph->saddr = tuple->dst.u3.ip;
	iph->daddr = tuple->src.u3.ip;
	iph->tot_len = htons(warn_skb->len);
	iph->frag_off = 0;
	iph->ihl = 5;
	iph->version = 4;
	iph->tos = 0;	
	iph->check = ip_fast_csum(iph, iph->ihl);

	if (warn_skb->dev->type != ARPHRD_PPP) {
		skb_push(warn_skb, sizeof(struct ethhdr));
		skb_reset_mac_header(warn_skb);
		n_eth = (struct ethhdr *) warn_skb->data;
		n_eth->h_proto = o_eth->h_proto;
		memcpy(n_eth->h_dest, o_eth->h_source, ETH_ALEN);
		memcpy(n_eth->h_source, o_eth->h_dest, ETH_ALEN);
	}

	dev_queue_xmit(warn_skb);	

	return NRET_TRUE;
}

static int wh_evil_url_warn_send(struct nf_conn *ct, struct sk_buff *skb,
			uint16_t type, uint16_t wh_type, struct nf_http *http)
{
	char *tmp;
	int count = 0;
	struct host_struct *host = NULL;
	struct sk_buff *warn_skb;
	char redirturl[WH_REDIRECT_URL_LEN];
	struct wh_url_safe_head *h = wh_get_head();
	unsigned char *dns;
	unsigned char *uri;

	if (!h->web_ip || !h->web_port)
		return 1;
	if (!(host = ct_host(ct)))
		return 1;
	memset(redirturl, 0x0, sizeof(redirturl));
	warn_skb = alloc_skb(1500, GFP_ATOMIC | __GFP_ZERO);
	if (warn_skb== NULL) 
		goto out;

	warn_skb->dev = skb->dev;
	warn_skb->protocol = __constant_htons(ETH_P_IP);
	skb_copy_secmark(warn_skb, skb);
	skb_reserve(warn_skb, 64 + sizeof(struct iphdr) + sizeof(struct tcphdr));
	dns = http_key_value(http, HTTP_KEY_HOST);
	uri = http_key_value(http, HTTP_KEY_URI);
	
	count = sprintf(redirturl, "%s", h->rd_url);
	count += sprintf(redirturl + count, "?");
	count += sprintf(redirturl + count, "code=%d&", type);
	count += sprintf(redirturl + count, "fcode=%d&", wh_type);
	count += sprintf(redirturl + count, "sip=%u.%u.%u.%u&", NIPQUAD(h->web_ip));
	count += sprintf(redirturl + count, "port=%d&", h->web_port);
	count += sprintf(redirturl + count, "cip=%u.%u.%u.%u&", NIPQUAD(host->ip));
	count += snprintf(redirturl + count, 256, "url=%s%s", dns, uri);
	count = 0;
	memcpy(warn_skb->data, WH_HTTP_HEAD, WH_HTTP_HEAD_SIZE);
	tmp = warn_skb->data + WH_HTTP_HEAD_SIZE;
	count += WH_HTTP_HEAD_SIZE;
	count += snprintf(tmp, WH_HTTP_WARN_SIZE_MX, WH_HTML_HEAD_BEGIN""
					WH_HTML_HEAD_MID_2""WH_HTML_BODY""
					WH_HTML_BR"<span>"WH_HTML_END, redirturl);
	
	if (unlikely(count > WH_HTTP_WARN_SIZE_MX + WH_HTTP_HEAD_SIZE))
		goto free_skb;
	skb_put(warn_skb, count);
	wh_evil_url_warn_xmit(ct, skb, warn_skb);
	return 0;
free_skb:
	kfree(warn_skb);
out:
	return 1;
}

static unsigned int wh_url_safe_hook(unsigned int hook, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,	
		int (*okfn)(struct sk_buff *))
{
	struct wh_safe_ip *tmp = NULL;
	struct nf_http *http_info = NULL;
	struct nf_conn *ct = NULL;
	struct host_struct *host = NULL;
	enum ip_conntrack_info ctinfo;
	struct wh_skb_list *http = NULL;
	struct sk_buff *new_retx = NULL;
	struct nf_trie_node *node = NULL;
	struct wh_url_cache *url = NULL;
	struct nf_host_dot dot [NF_HOST_DOT_MX];
	struct wh_url_safe_head *wh = wh_get_head();
	unsigned char *dns;
	unsigned char *dns2;
	unsigned char *uri;

	if (bridgemode)
		return NF_ACCEPT;
	if (!wh->enable || !wh->url_server_ip[0])
		return NF_ACCEPT;

	if (!(ct = nf_ct_get(skb, &ctinfo)))
		return NF_ACCEPT;
	
	if (!(host = ct_host(ct)))
		return NF_ACCEPT;
	
	if (!(http_info = ct_http(ct)))
		return NF_ACCEPT;

	if (test_bit(HTTP_SKIP_BIT, &http_info->flags)) 
		return NF_ACCEPT;

	if (!skb_http(skb) || !http_key_len(http_info, HTTP_KEY_HOST))
		return NF_ACCEPT;
	dns = http_key_value(http_info, HTTP_KEY_HOST);
	uri = http_key_value(http_info, HTTP_KEY_URI);
	if (in->uid) 
		return NF_ACCEPT;
	
	clear_bit(CT_MATCH_URL_SAFE_BIT, ct_flags(ct));
	
	WH_HOST_LOCK();
	//match vip url list
	node = nf_trie_match(&wh->url_vip.root, dns, NULL, NULL);
	if (node) {
		KERNEL_DEBUG(debug, "match the wist url cache, %s+%s+\n", dns, uri);
		goto unlock_accept;
	}
	//match safe url list
	node = nf_trie_match(&wh->url_cache.root, dns, NULL, NULL);
	if (node) {
		KERNEL_DEBUG(debug, "match the safe url cache, %s+%s+\n", dns, uri);
		goto unlock_accept;
	}
	//match evil url list
	node = nf_trie_match(&wh->url_evil.root, dns, NULL, NULL);
	if (node) {
		KERNEL_DEBUG(debug, "match the evil url cache, %s+%s+\n", dns, uri);
		url = list_entry(node, struct wh_url_cache, node);
		list_for_each_entry(tmp, &url->list_ip, list) {
			if (tmp->ip != host->ip)
				continue;
			if (time_after(jiffies, url->last_time + WH_URL_REPORT_TIMOUT))
				wh_url_check_report(skb, url->wh_type);
			url->last_time = jiffies;
			KERNEL_DEBUG(debug, "user %u.%u.%u.%u,"
					" choose to visit %s+%s+\n", 
					NIPQUAD(host->ip), http_info->host,
				       		http_info->uri);
			goto unlock_accept;
		}
		if (time_after(jiffies, url->last_time + WH_URL_REPORT_TIMOUT))
			wh_url_check_report(skb, url->wh_type);
		url->last_time = jiffies;
		WH_HOST_UNLOCK();
		//send evil url warn packet
		if (wh_evil_url_warn_send(ct, skb, url->url_type, url->wh_type, http_info)) {
			KERNEL_DEBUG(debug, "send evil url warn error\n");
			goto accept;
		}
		return NF_DROP;
	}
	
	//if system has send the check packet of this url, stolen the packet
	list_for_each_entry(http, &wh->http_skb_list, http_list) {
		dns2 = http_key_value(http->http_info, HTTP_KEY_HOST);
		if (strcmp(dns2, dns))
			continue;
		/*
		 *if (strcmp(http->http_info->uri, http_info->uri))
		 *        continue;
		 */
		skb_queue_tail(&http->skb_queue, skb);
		if (time_before(jiffies , http->jiffies + 
				http->send_times * WH_URL_CHECK_TIMEOUT))
			goto unlock;
		if (!http->check_skb || time_after(http->jiffies +
			       	http->send_times * WH_URL_CHECK_TIMEOUT,
						http->jiffies + 2 * HZ))
			goto unlock;
		http->send_times++;
		//resend the check packet
		new_retx = skb_copy(http->check_skb, GFP_ATOMIC);
		WH_HOST_UNLOCK();
		if (new_retx)
			ip_finish_output(new_retx); 
		goto stolen;
	}
	WH_HOST_UNLOCK();

	memset(dot, 0x0, sizeof(dot));
	nf_spilt_host_by_dot(dns, dot);
	if (!dot[0].name)
		return NF_ACCEPT;
	http = kzalloc(sizeof(*http), GFP_ATOMIC);
	if (!http) 
		return NF_ACCEPT;
	http->jiffies = jiffies;
	http->send_times = 1;
	http->http_info = igd_hold(http_info);
	http->sip = ct->tuplehash[1].tuple.dst.u3.ip;
	http->dip = wh->url_server_ip[0];
	INIT_LIST_HEAD(&http->check_list);
	INIT_LIST_HEAD(&http->http_list);
	skb_queue_head_init(&http->skb_queue);

	if (wh_url_check_request(skb, http)) {
		KERNEL_DEBUG(debug, "send the check packet error, %s+%s+\n", dns, uri);
		__wh_skb_list_free(http);
		goto accept;
	}
	
	WH_HOST_LOCK();
	//将原HTTP请求包缓存
	skb_queue_tail(&http->skb_queue, skb);
	list_add_tail(&http->http_list, &wh->http_skb_list);
	list_add_tail(&http->check_list, &wh->check_skb_list);
unlock:
	WH_HOST_UNLOCK();
stolen:
	return NF_STOLEN;
unlock_accept:
	WH_HOST_UNLOCK();
accept:
	set_bit(CT_MATCH_URL_SAFE_BIT, ct_flags(ct));
	return NF_ACCEPT;
}

struct nf_hook_ops wh_url_ops = {
		.hook		= wh_url_safe_hook,
		.owner		= THIS_MODULE,
		.pf			= PF_INET,
		.hooknum	= NF_INET_FORWARD,
		.priority		= NF_IP_PRI_URL_SAFE_FORWARD,
};

static unsigned int wh_check_recv_hook(unsigned int hook, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,	
		int (*okfn)(struct sk_buff *))
{
	struct nf_conn *ct = NULL;
	struct sk_buff *http_skb = NULL;
	struct wh_skb_list *http = NULL;
	struct url_comm *h = NULL;
	unsigned char *tmp = NULL;
	uint32_t httpid = 0;
	int url_type = 0;
	uint8_t wh_type = 0;
	struct iphdr *iph = ip_hdr(skb);
	struct udphdr *udph =NULL;
	struct wh_url_safe_head *wh = wh_get_head();

	if (bridgemode)
		return NF_ACCEPT;
	if (!wh->enable)
		return NF_ACCEPT;
	if (!in->uid)
		return NF_ACCEPT;
	
	if (iph ->protocol != IPPROTO_UDP)
		return NF_ACCEPT;
	
	udph = (struct udphdr *)((unsigned char *)iph + iph->ihl * 4);

	if (udph->len != htons(HTTP_CHECK_RECV_LEN))
		return NF_ACCEPT;
	
	if (udph->source !=  htons(wh->url_server_port) || udph->dest !=  htons(HTTP_CHECK_SRC_PORT))
		return NF_ACCEPT;
	
	h = (struct url_comm *)&udph[1];
	if (h->tot_len != htons(sizeof(struct url_comm) + sizeof(uint32_t) + sizeof(int) + sizeof(uint8_t)))
		return NF_DROP;
	if (h->cmd !=  htons(HTTP_RECV_CMD))
		return NF_DROP;
	
	if (h->devid != htonl(wh->device_id))
		return NF_ACCEPT;

	tmp = (unsigned char *)h;
	tmp += sizeof(struct url_comm);
	if (!tmp)
		return NF_DROP;
	
	httpid = ntohl(*(uint32_t *)tmp);
	tmp += sizeof(uint32_t);
	if (!tmp)
		return NF_DROP;
	
	url_type = ntohl(*(int *)tmp);
	tmp += sizeof(int);
	if (!tmp)
		return NF_DROP;
	
	wh_type = *tmp;

	KERNEL_DEBUG(debug, "+++recv url check result id is %u, url type is %d, wh type is %d\n", httpid, url_type, wh_type);
	
	WH_HOST_LOCK();
	list_for_each_entry(http, &wh->check_skb_list, check_list) {
		if (http->check_id != httpid)
			continue;
		if (http->dip != iph->saddr)
			continue;
		http->wh_type = wh_type;
		http->url_type = url_type;
		if (!url_type)
			wh_add_url_cache(&wh->url_cache, http);
		else
			wh_add_url_cache(&wh->url_evil, http);
		goto find;
	}
	WH_HOST_UNLOCK();
	return NF_DROP;
	
find:
	while ((http_skb = __skb_dequeue(&http->skb_queue)) != NULL) {
		ct = (struct nf_conn *)http_skb->nfct;
		if (!ct) {
			kfree_skb(http_skb);
			continue;
		}
		if (!url_type) {
			set_bit(CT_MATCH_URL_SAFE_BIT, ct_flags(ct));
			WH_HOST_UNLOCK();
			ip_output(http_skb);
			WH_HOST_LOCK();
		} else {
			wh_url_check_report(http_skb, http->wh_type);
			wh_evil_url_warn_send(ct, http_skb, http->url_type, http->wh_type, http->http_info);
			kfree_skb(http_skb);
		}
	}
	wh_skb_list_free(http);
	WH_HOST_UNLOCK();
	return NF_DROP;
}

struct nf_hook_ops wh_check_recv_ops = {
	.hook		= wh_check_recv_hook,
	.owner		= THIS_MODULE,
	.pf			= PF_INET,
	.hooknum	= NF_INET_PRE_ROUTING,
	.priority		= INT_MIN,
};

static void wh_evil_url_timer_fn(unsigned long arg)
{
	int del = 0;
	struct wh_url_cache *url;
	struct wh_url_cache *tmp;
	struct wh_url_safe_head *h = wh_get_head();

	WH_HOST_LOCK();
	list_for_each_entry_safe_reverse(url, tmp, &h->url_evil.list, list) {
		if (time_before(jiffies, url->jiffies + h->evil_url_timer.wh_timeout))
			break;
		if (del++ > 100)
			break;
		wh_url_cache_free(&h->url_evil, url);
	}
	WH_HOST_UNLOCK();
	h->evil_url_timer.timer.expires = jiffies + 10 * HZ;
	add_timer(&h->evil_url_timer.timer);
	return ;
}

static void wh_safe_url_timer_fn(unsigned long arg)
{
	int del = 0;
	struct wh_url_cache *url;
	struct wh_url_cache *tmp;
	struct wh_url_safe_head *h = wh_get_head();

	WH_HOST_LOCK();
	list_for_each_entry_safe_reverse(url, tmp, &h->url_cache.list, list) {
		if (time_before(jiffies, url->jiffies + h->safe_url_timer.wh_timeout))
			break;
		if (del++ > 100)
			break;
		wh_url_cache_free(&h->url_cache, url);
	}
	WH_HOST_UNLOCK();
	h->safe_url_timer.timer.expires = jiffies + 10 * HZ;
	add_timer(&h->safe_url_timer.timer);
}

static void wh_http_pkg_timer_fn(unsigned long arg)
{
	int del = 0;
	struct nf_conn *ct;
	struct wh_skb_list *http;
	struct sk_buff *skb;
	struct wh_url_safe_head *h = wh_get_head();

	WH_HOST_LOCK();
	//200ms超时后将缓存的http报文发出去
	list_for_each_entry(http, &h->http_skb_list, http_list) {
		if (time_before(jiffies, http->jiffies + h->http_pkg_timer.wh_timeout)) 
			break;
		if (del++ > 50)
			break;
		while ((skb = __skb_dequeue(&http->skb_queue)) != NULL) {
			ct = (struct nf_conn *)skb->nfct;
			//printk("the skb is timeout send it---%s%s\n", http->http_info->host, http->http_info->uri);
			set_bit(CT_MATCH_URL_SAFE_BIT, ct_flags(ct));
			WH_HOST_UNLOCK();
			ip_output(skb);
			WH_HOST_LOCK();
		}
	}
	WH_HOST_UNLOCK();
	if (!h->enable)
		return;
	h->http_pkg_timer.timer.expires = jiffies + WH_HTTP_TIMER_TIMEOUT;
	add_timer(&h->http_pkg_timer.timer);
	return ;
}

static void wh_check_pkg_timer_fn(unsigned long arg)
{
	int del = 0;
	struct wh_skb_list *http;
	struct wh_skb_list *tmp;
	struct wh_url_safe_head *h = wh_get_head();

	WH_HOST_LOCK();
	/*2秒都没有收到查询回应则从老化表中删除*/
	list_for_each_entry_safe_reverse(http, tmp, &h->check_skb_list, check_list) {
		if (time_before(jiffies, http->jiffies + h->check_pkg_timer.wh_timeout))
			break;
		if (del++ > 100)
			break;
		//printk("the check skb is timeout send it+++ %s%s\n", http->http_info->host, http->http_info->uri);
		wh_skb_list_free(http);
	}
	WH_HOST_UNLOCK();
	if (!h->enable)
		return;
	h->check_pkg_timer.timer.expires = jiffies + WH_HTTP_TIMER_TIMEOUT;
	add_timer(&h->check_pkg_timer.timer);
	return ;
}

#ifdef CONFIG_PROC_FS
static void *url_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct wh_url_safe_head *h = wh_get_head();
	
	if (*pos >= h->url_vip.nr)
		return NULL;
	return pos;
}

static void *url_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct wh_url_safe_head *h = wh_get_head();
	
	(*pos)++;
	if (*pos >= h->url_vip.nr) 
		return NULL;
	return pos;
}

static void url_seq_stop(struct seq_file *s, void *v)
{
	return ;
}

static int url_seq_show(struct seq_file *s, void *v)
{
	int cnt = 0;
	int index = *(loff_t *)v;
	char buffer[IGD_URL_LEN];
	struct wh_url_cache *url = NULL;
	struct wh_url_safe_head *h = wh_get_head();
	
	WH_HOST_LOCK();
	list_for_each_entry(url, &h->url_vip.list, list) {
		if (cnt++ < index) 
			continue;
		memset(buffer, 0x0, sizeof(buffer));
		snprintf(buffer, IGD_URL_LEN, "%s\n", host2str(url->node.name, NULL, 0));
		seq_printf(s, buffer);
		break;
	}
	WH_HOST_UNLOCK();
	return 0;
}

static const struct seq_operations url_ops = {
	.start = url_seq_start,
	.next  = url_seq_next,
	.stop  = url_seq_stop,
	.show  = url_seq_show
};

static int url_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &url_ops);
}

static const struct file_operations url_fops = {
	.owner   = THIS_MODULE,
	.open    = url_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

static int safe_debug_show(struct seq_file *s, void *data)
{
	char buffer[8];
	sprintf(buffer, "%d\n", debug);
	seq_printf(s, buffer);
	return 0;
}

static int safe_debug_write(struct file *file, const char __user
	       		*buffer, size_t count, loff_t *ops)
{
	char buf[8];
	if (count > sizeof(buf))
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	if (buf[0] == '1')
		debug = 1;
	else
		debug = 0;
	return count;
}

static int safe_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, safe_debug_show, NULL);
}

static const struct file_operations safe_debug_fops = {
	.owner   = THIS_MODULE,
	.open    = safe_debug_open,
	.read    = seq_read,
	.write = safe_debug_write,
	.release = single_release,
};
#endif

static int __init wh_url_safe_init(void)
{
	struct wh_url_safe_head *wh = wh_get_head();
	
	memset(wh, 0, sizeof(*wh));
	wh->enable = 0;
	spin_lock_init(&wh_host_lock);
	nf_skip_domain_init();
	INIT_LIST_HEAD(&wh->http_skb_list);
	INIT_LIST_HEAD(&wh->check_skb_list);
	wh_url_head_init(&wh->url_cache, WH_URL_CACHE_MX_NR);
	wh_url_head_init(&wh->url_evil, WH_URL_EVIL_MX_NR);
	wh_url_head_init(&wh->url_vip, WH_URL_WLIST_MX_NR);
	
	//evil url cache timer
	init_timer(&wh->evil_url_timer.timer);
	wh->evil_url_timer.wh_timeout = WH_EVIL_TIMER_TIMEOUT;
	wh->evil_url_timer.timer.function = wh_evil_url_timer_fn;
	wh->evil_url_timer.timer.expires = jiffies + 10 * HZ;
	add_timer(&wh->evil_url_timer.timer);

	//safe url cache timer
	init_timer(&wh->safe_url_timer.timer);
	wh->safe_url_timer.wh_timeout = WH_CACHE_TIMER_TIMEOUT;
	wh->safe_url_timer.timer.function = wh_safe_url_timer_fn;
	wh->safe_url_timer.timer.expires = jiffies + 10 * HZ;
	add_timer(&wh->safe_url_timer.timer);

	//http get or post pacet timer
	init_timer(&wh->http_pkg_timer.timer);
	wh->http_pkg_timer.wh_timeout = WH_HTTP_PACK_TIMEOUT;
	wh->http_pkg_timer.timer.function = wh_http_pkg_timer_fn;
	wh->http_pkg_timer.timer.expires = jiffies + 10 * HZ;
	add_timer(&wh->http_pkg_timer.timer);

	//url check packet timer
	init_timer(&wh->check_pkg_timer.timer);
	wh->check_pkg_timer.wh_timeout = WH_URL_CHECK_TIMOUT;
	wh->check_pkg_timer.timer.function = wh_check_pkg_timer_fn;
	wh->check_pkg_timer.timer.expires = jiffies + 10 * HZ;
	add_timer(&wh->check_pkg_timer.timer);
	if (nf_register_hook(&wh_url_ops))
		goto DEL_TIMER;
	
	if (nf_register_hook(&wh_check_recv_ops))
		goto SEND_HOOK;
	url_safe_fn.url_safe_action = wh_url_safe_action;
#if 0
#ifdef CONFIG_PROC_FS
	proc_create("wlist", 0440, proc_dir, &url_fops);
	proc_create("url_safe_debug", 0440, proc_dir, &safe_debug_fops);
#endif
#endif
	printk("regiester url hooks finish\n");
	return NRET_TRUE;
SEND_HOOK:
	nf_unregister_hook(&wh_url_ops);
DEL_TIMER:
	del_timer_sync(&wh->evil_url_timer.timer);
	del_timer_sync(&wh->safe_url_timer.timer);
	del_timer_sync(&wh->check_pkg_timer.timer);
	del_timer_sync(&wh->http_pkg_timer.timer);
	return NRET_FALSE;
}

void __exit wh_url_safe_fini(void)
{
	struct wh_skb_list *http = NULL;
	struct wh_skb_list *tp = NULL;
	struct wh_url_cache *url = NULL;
	struct wh_url_cache *tmp = NULL;
	struct wh_url_safe_head *wh = wh_get_head();

	wh->enable = 0;
#if 0
#ifdef CONFIG_PROC_FS
	remove_proc_entry("url_safe_debug", proc_dir);
	remove_proc_entry("wlist", proc_dir);
#endif
#endif
	nf_unregister_hook(&wh_url_ops);
	nf_unregister_hook(&wh_check_recv_ops);

	del_timer_sync(&wh->evil_url_timer.timer);
	del_timer_sync(&wh->safe_url_timer.timer);
	del_timer_sync(&wh->http_pkg_timer.timer);
	del_timer_sync(&wh->check_pkg_timer.timer);
	
	WH_HOST_LOCK();
	list_for_each_entry_safe(url, tmp, &wh->url_cache.list, list) {
		wh_url_cache_free(&wh->url_cache, url);
	}
	nf_trie_free(&wh->url_cache.root);

	list_for_each_entry_safe(url, tmp, &wh->url_evil.list, list) {
		wh_url_cache_free(&wh->url_evil, url);
	}
	nf_trie_free(&wh->url_evil.root);

	list_for_each_entry_safe(url, tmp, &wh->url_vip.list, list) {
		wh_url_cache_free(&wh->url_vip, url);
	}
	nf_trie_free(&wh->url_vip.root);

	list_for_each_entry_safe(http, tp, &wh->check_skb_list, check_list) {
		wh_skb_list_free(http);
	}
	url_safe_fn.url_safe_action = NULL;

	WH_HOST_UNLOCK();
	return ;
}

module_init(wh_url_safe_init);
module_exit(wh_url_safe_fini);
