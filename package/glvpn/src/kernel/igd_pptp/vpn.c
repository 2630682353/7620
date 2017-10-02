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
#include "vpn.h"

static struct vpn_head vhead;
DEFINE_SPINLOCK(vpn_lock);

static struct vpn_head *vpn_get_head(void)
{
	return &vhead;
}

static void vpn_skb_list_free(struct vpn_dns_list *list)
{
	struct sk_buff *skb;
	struct nf_conn *ct;

	while ((skb = __skb_dequeue(&list->dns_queue)) != NULL) {
		ct = (struct nf_conn *)skb->nfct;
		if (!ct) {
			kfree_skb(skb);
			continue;
		}
		if (!list->iscn) {
			skb->mark = 1000;
			set_bit(CT_DNS_VPN_BIT, ct_flags(ct));
		}
		set_bit(SKB_SKIP_VPN, skb_flags(skb));
		netif_rx(skb);
	}
	list_del_init(&list->list);
	kfree(list);
	return;
}

void vpn_dns_cache_free(struct vpn_dns_cache *dns)
{
	list_del(&dns->list);
	nf_trie_del_node(dns->root, &dns->node);
	kfree(dns);
	return;
}

static void vpn_dns_node_add_fn(struct nf_trie_root *root, struct nf_trie_node *node)
{
	struct vpn_dns_cache *dns = container_of(node, struct vpn_dns_cache, node);

	dns->root = root;
	return;
}

struct vpn_dns_cache *vpn_dns_cache_add(struct vpn_dns_head *h, unsigned char *host)
{
	struct vpn_dns_cache *dns;

	if (h->nr >= h->mx)
		return NULL;
	dns = kzalloc(sizeof(*dns), GFP_ATOMIC);
	if (!dns) 
		return NULL;
	if (str2host(host, dns->node.name, sizeof(dns->node.name)) < 0)
		goto ERROR;;
	dns->node.len = strlen(dns->node.name);
	if (nf_trie_add_node(&h->root, &dns->node, vpn_dns_node_add_fn))
		goto ERROR;
	list_add_tail(&dns->list, &h->list);
	h->nr++;
	return dns;
ERROR:
	kfree(dns);
	return NULL;
}

static void vpn_check_pkg_timer_fn(unsigned long arg)
{
	int del = 0;
	struct vpn_dns_list *dns;
	struct vpn_dns_list *tmp;
	struct vpn_head *vpnh = vpn_get_head();

	spin_lock_bh(&vpn_lock);
	list_for_each_entry_safe_reverse(dns, tmp, &vpnh->dns_queue, list) {
		if (time_before(jiffies, dns->jiffies + VPN_TIMER_DNS_TIMEOUT))
			break;
		if (del++ > 100)
			break;
		vpn_skb_list_free(dns);
	}
	spin_unlock_bh(&vpn_lock);
	if (!vpnh->enable)
		return;
	vpnh->vtimer.expires = jiffies + VPN_TIMER_TIMEOUT;
	add_timer(&vpnh->vtimer);
}

static void vpn_refresh(void)
{	
	struct vpn_dns_list *dnsq, *tmp;
	struct vpn_head *vpnh = vpn_get_head();

	if (vpnh->enable) {
		if (!timer_pending(&vpnh->vtimer))
			mod_timer(&vpnh->vtimer, jiffies + VPN_TIMER_TIMEOUT);
		return;
	}
	list_for_each_entry_safe(dnsq, tmp, &vpnh->dns_queue, list) {
		vpn_skb_list_free(dnsq);
	}
	return;
}

static int vpn_action(struct nlmsghdr *nlh)
{
	int nr, len;
	int offset = 0;
	char host[IGD_DNS_LEN];
	struct nlk_msg_comm nlk;
	struct nlk_vpn param;
	struct vpn_dns_cache *dns, *tmp;
	struct vpn_head *vpnh = vpn_get_head();

	memset(&nlk, 0x0, sizeof(nlk));
	if (nlk_data_pop(nlh, &offset, &nlk, sizeof(nlk)))
		return IGD_ERR_DATA_ERR;
	nr = nlk.obj_nr;
	len = nlk.obj_len;
	
	spin_lock_bh(&vpn_lock);
	switch (nlk.mid) {
	case VPN_SET_PARAM:
		offset = 0;
		memset(&param, 0x0, sizeof(param));
		if (nlk_data_pop(nlh, &offset, &param, sizeof(param)))
			goto ERROR;
		if (vpnh->enable != param.enable) {
			vpnh->enable = param.enable;
			vpn_refresh();
			printk(KERN_WARNING"rcv vpn param msg, enable=%d, "
						"devid=%u, ip %u.%u.%u.%u",
						param.enable, param.devid,
						NIPQUAD(param.sip[0].s_addr));
		}
		vpnh->devid = param.devid;
		loop_for_each(0, DNS_IP_MX) {
			vpnh->vpn_sip[i] = param.sip[i].s_addr;
		} loop_for_each_end();
		break;
	case VPN_SET_DNS_LIST:
		if (nr <= 0 || nr > VPN_DNS_PER_MX)
			goto ERROR;
		if (len != IGD_DNS_LEN)
			goto ERROR;
		list_for_each_entry_safe(dns, tmp, &vpnh->dns_h.list, list) {
			vpn_dns_cache_free(dns);
		}
		nf_trie_free(&vpnh->dns_h.root);
		INIT_LIST_HEAD(&vpnh->dns_h.list);
		nf_trie_root_init(&vpnh->dns_h.root);
		loop_for_each(0, nr) {
			memset(host, 0x0, sizeof(host));
			if (nlk_data_pop(nlh, &offset, host, len))
				goto ERROR;
			vpn_dns_cache_add(&vpnh->dns_h, host);
		}loop_for_each_end();
		break;
	default:
		break;
	}
	spin_unlock_bh(&vpn_lock);
	return NRET_TRUE;
ERROR:
	spin_unlock_bh(&vpn_lock);
	return IGD_ERR_DATA_ERR;
}

static int vpn_dns_str_match(struct vpn_dns_list *dnsq, unsigned char *dns, int nlen)
{
	int klen;
	int index = 0;
	unsigned char *tmp;
	unsigned char *key;

	if (dnsq->nlen >= nlen) {
		tmp = dnsq->dns;
		key = dns;
		klen = nlen;
	} else {
		tmp = dns;
		key = dnsq->dns;
		klen = dnsq->nlen;
	}
	index = *tmp;
	while (index) {
		if (!memcmp(tmp, key, klen))
			return 0;
		tmp += index + 1;
		index = *tmp;
	}
	return -1;
}

static unsigned int vpn_send_dns_check(struct nf_conn *ct,
			struct sk_buff *old_skb, unsigned char *dns, int nlen,
			const struct net_device *in, const struct net_device *out)
{
	int len;
	unsigned char *tmp;
	struct ethhdr *eth;
	struct iphdr *iph;
	struct udphdr *udph;
	struct sk_buff *skb;
	struct rtable *rt;
	struct flowi fl;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
	struct flowi4 *fl4;
#endif
	struct vpn_dns_list *dnsq;
	struct vpn_check_head *h;
	struct vpn_head *vpnh = vpn_get_head();
	static uint32_t check_id; 

	memset(&fl, 0x0, sizeof(fl));
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
	fl.u.ip4.saddr = 0;
	fl.u.ip4.daddr = vpnh->vpn_sip[0];
	fl.u.ip4.flowi4_scope = RT_SCOPE_UNIVERSE;
	fl.u.ip4.flowi4_tos = RT_TOS(0),
	rt = ip_route_output_key(&init_net, (struct flowi4*)&fl);
	if (!rt)
#else
	fl.nl_u.ip4_u.saddr = 0;
	fl.nl_u.ip4_u.daddr = vpnh->vpn_sip[0];
	fl.nl_u.ip4_u.scope = RT_SCOPE_UNIVERSE;
	fl.nl_u.ip4_u.tos = RT_TOS(0);
	if (ip_route_output_key(&init_net, &rt, &fl))
#endif
		return -1;
	skb = alloc_skb(600, GFP_ATOMIC | __GFP_ZERO);
	if (!skb)
		return -1;
	skb->ip_summed = CHECKSUM_NONE;
	skb_reserve(skb, 64 + sizeof(struct iphdr) + sizeof(struct udphdr));
	/*add check header*/
	tmp = skb->data;
	h = (struct vpn_check_head *)tmp;
	h->cmd = htons(VPN_CHECK_CMD);
	h->devid = htonl(vpnh->devid);
	len = sizeof(struct vpn_check_head);
	tmp += sizeof(struct vpn_check_head);
	/*add host mac*/
	eth = (struct ethhdr *)skb_mac_header(old_skb);
	sprintf(tmp, "%02X%02X%02X%02X%02X%02X", MAC_SPLIT(eth->h_source));
	len += 12;
	tmp += 12;
	/*add check id*/
	*(uint32_t *)tmp = htonl(check_id);
	len += sizeof(uint32_t);
	tmp += sizeof(uint32_t);
	/*add dns len*/
	*(uint8_t *)tmp = nlen;
	len += sizeof(uint8_t);
	tmp += sizeof(uint8_t);
	/*add dns*/
	memcpy(tmp, dns, nlen);
	len+= nlen;
	h->tot_len = htons(len);
	/*fix skb*/
	skb_put(skb, len);
	skb_push(skb, sizeof(struct udphdr));
	skb_reset_transport_header(skb);
	udph = (struct udphdr *)skb_transport_header(skb);
	udph->source = htons(VPN_CHECK_SRC_PORT);
	udph->dest = htons(VPN_CHECK_DST_PORT);
	udph->len = htons(len + 8);
	udph->check = 0;
	skb_push(skb, sizeof(struct iphdr));
	skb_reset_network_header(skb);
	skb_reset_mac_header(skb);
	iph = ip_hdr(skb);
	memset(iph, 0, sizeof(*iph));
	iph->version = 4;
	iph->ihl = 5;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
	fl4 = (struct flowi4 *)&fl;
	iph->saddr	  = fl4->saddr;
#else
	iph->saddr = rt->rt_src;
#endif
	iph->daddr = vpnh->vpn_sip[0];
	iph->ttl = 128;
	iph->protocol = IPPROTO_UDP;
	iph->tot_len = htons(skb->len);
	ip_send_check(iph);
	skb_dst_set(skb, &rt->dst);
	skb->dev = skb_dst(skb)->dev;
	skb->protocol = htons(ETH_P_IP);
	set_bit(SKB_SKIP_QOS, skb_flags(skb));
	ip_finish_output(skb);
	dnsq = kzalloc(sizeof(*dnsq), GFP_ATOMIC);
	if (!dnsq) 
		return -1;
	dnsq->iscn = 1;
	dnsq->in = in;
	dnsq->out = out;
	dnsq->jiffies = jiffies;
	dnsq->check_id = check_id++;
	arr_strcpy(dnsq->dns, dns);
	skb_queue_head_init(&dnsq->dns_queue);
	skb_queue_tail(&dnsq->dns_queue, old_skb);
	list_add_tail(&dnsq->list, &vpnh->dns_queue);
	//printk(KERN_WARNING"send dns [%s] check package\n", dns);
	return 0;
}

static unsigned int vpn_check_dns(struct nf_conn *ct,
				struct sk_buff *skb,const struct net_device *in,
				const struct net_device *out)
{
	int nlen;
	int index;
	int len;
	int last;
	int ofs = 0;
	unsigned char *name;
	unsigned char *tmp;
	struct dns_h *dnsh;
	struct iphdr *iph;
	struct nf_trie_node *node;
	struct vpn_dns_list *dnsq;
	struct vpn_head *vpnh = vpn_get_head();
	
	if (!ct_is_dns(ct) || test_bit(SKB_SKIP_VPN, skb_flags(skb)))
		return NF_ACCEPT;
	dnsh = (void *)((char *)l4_hdr(skb) + 8);
	if (dnsh->qr || (ntohs(dnsh->request) != 1))
		return NF_ACCEPT;
	iph = ip_hdr(skb);
	len = skb->len - iph->ihl * 4 - 8 - sizeof(struct dns_h);
	if (len < 10 || len > 100)
		return NF_ACCEPT;
	name = (unsigned char *)dnsh + sizeof(struct dns_h);
	tmp = name;
	index = *tmp;
	if (!index) 
		return NF_ACCEPT;
	while (index) {
		tmp += index + 1;
		ofs += index + 1;
		if (ofs >= IGD_DNS_LEN)
			return NF_ACCEPT;
		last = *tmp;
		if (!last)
			last = ofs - index;
		index = *tmp;
	}
	if (!last)
		return NF_ACCEPT;
	nlen = tmp - name;
	tmp++;
	if (*(int *)tmp != ntohl(0x010001))
		return NF_ACCEPT;
	if (!strncmp(name + last, "cn", 2))
		goto out;
	spin_lock_bh(&vpn_lock);
	node = nf_trie_dns_match(&vpnh->dns_h.root, name);
	if (node) {
		spin_unlock_bh(&vpn_lock);
		goto out;
	}
	list_for_each_entry(dnsq, &vpnh->dns_queue, list) {
		if (vpn_dns_str_match(dnsq, name, nlen))
			continue;
		skb_queue_tail(&dnsq->dns_queue, skb);
		spin_unlock_bh(&vpn_lock);
		return NF_STOLEN;
	}
	spin_unlock_bh(&vpn_lock);
	if (vpn_send_dns_check(ct, skb, name, nlen, in, out))
		goto out;
	return NF_STOLEN;
out:
	return NF_ACCEPT;
}

static unsigned int vpn_check_reply(struct nf_conn *ct,
				struct sk_buff *skb,const struct net_device *in,
				const struct net_device *out)
{
	unsigned char iscn;
	unsigned char *tmp;
	uint32_t checkid;
	struct udphdr *udph;
	struct vpn_check_head *h;
	struct iphdr *iph = ip_hdr(skb);
	struct vpn_dns_list *dnsq, *dnsq1;
	struct vpn_head *vpnh = vpn_get_head();

	if (iph ->protocol != IPPROTO_UDP)
		return NF_ACCEPT;
	udph = (struct udphdr *)l4_hdr(skb);
	if (udph->source !=  htons(VPN_CHECK_DST_PORT)
		|| udph->dest !=  htons(VPN_CHECK_SRC_PORT))
		return NF_ACCEPT;
	if (ntohs(udph->len) != sizeof(*h) + 5 + 8)
		return NF_ACCEPT;
	h = (struct vpn_check_head *)&udph[1];
	if (h->cmd != htons(VPN_RECV_CMD))
		return NF_DROP;
	if (h->devid != htonl(vpnh->devid))
		return NF_ACCEPT;
	tmp = (unsigned char *)h;
	tmp += sizeof(*h);
	checkid = ntohl(*(uint32_t *)tmp);
	tmp += sizeof(uint32_t);
	iscn = *(uint8_t *)tmp;
	
	spin_lock_bh(&vpn_lock);
	list_for_each_entry_safe(dnsq, dnsq1, &vpnh->dns_queue, list) {
		if (dnsq->check_id != checkid)
			continue;
		//printk("recv server reply dns [%s] msg type is %d\n", dnsq->dns, iscn);
		dnsq->iscn = iscn;
		vpn_skb_list_free(dnsq);
		break;
	}
	spin_unlock_bh(&vpn_lock);
	return NF_DROP;
}

static unsigned int vpn_hook(unsigned int hook, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,	
		int (*okfn)(struct sk_buff *))
{
	struct nf_conn *ct = NULL;
	enum ip_conntrack_info ctinfo;
	struct vpn_head *vpnh = vpn_get_head();

	if (!vpnh->enable)
		return NF_ACCEPT;
	if (!(ct = nf_ct_get(skb, &ctinfo)))
		return NF_ACCEPT;
	if (in->uid)
		return vpn_check_reply(ct, skb, in, out);
	else
		return vpn_check_dns(ct, skb, in, out);
}

struct nf_hook_ops vpn_ops = {
		.hook		= vpn_hook,
		.owner		= THIS_MODULE,
		.pf			= PF_INET,
		.hooknum	= NF_INET_PRE_ROUTING,
		.priority	= NF_IP_PRI_VPN_PREROUTING,
};

#ifdef CONFIG_PROC_FS
static void *vpn_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct vpn_head *vpnh = vpn_get_head();
	
	if (*pos >= vpnh->dns_h.nr)
		return NULL;
	return pos;
}

static void *vpn_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct vpn_head *vpnh = vpn_get_head();
	
	(*pos)++;
	if (*pos >= vpnh->dns_h.nr) 
		return NULL;
	return pos;
}

static void vpn_seq_stop(struct seq_file *s, void *v)
{
	return ;
}

static int vpn_seq_show(struct seq_file *s, void *v)
{
	int cnt = 0;
	int index = *(loff_t *)v;
	char buffer[IGD_DNS_LEN];
	struct vpn_dns_cache *dns = NULL;
	struct vpn_head *vpnh = vpn_get_head();

	if (!index) {
		snprintf(buffer, IGD_DNS_LEN, "enable=%d\n", vpnh->enable);
		seq_printf(s, buffer);
	}
	spin_lock_bh(&vpn_lock);
	list_for_each_entry(dns, &vpnh->dns_h.list, list) {
		if (cnt++ < index) 
			continue;
		memset(buffer, 0x0, sizeof(buffer));
		snprintf(buffer, IGD_DNS_LEN, "%s\n", host2str(dns->node.name, NULL, 0));
		seq_printf(s, buffer);
		break;
	}
	spin_unlock_bh(&vpn_lock);
	return 0;
}

static const struct seq_operations vpn_proc_ops = {
	.start = vpn_seq_start,
	.next  = vpn_seq_next,
	.stop  = vpn_seq_stop,
	.show  = vpn_seq_show
};

static int vpn_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &vpn_proc_ops);
}

static const struct file_operations vpn_proc_fops = {
	.owner   = THIS_MODULE,
	.open    = vpn_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};
#endif

static int __init vpn_init(void)
{
	struct vpn_head *vpnh = vpn_get_head();
	
	memset(vpnh, 0, sizeof(*vpnh));
	vpnh->dns_h.mx = VPN_DNS_CACHE_MX;
	INIT_LIST_HEAD(&vpnh->dns_queue);
	INIT_LIST_HEAD(&vpnh->dns_h.list);
	
	//dns check timer, timeout 200ms
	init_timer(&vpnh->vtimer);
	vpnh->vtimer.function = vpn_check_pkg_timer_fn;
	if (nf_register_hook(&vpn_ops))
		goto DEL_TIMER;
	vpn_fn.vpn_action = vpn_action;
#ifdef CONFIG_PROC_FS
		proc_create("vpndns", 0440, proc_dir, &vpn_proc_fops);
#endif
	printk("register vpn hooks success\n");
	return NRET_TRUE;
DEL_TIMER:
	del_timer_sync(&vpnh->vtimer);
	return NRET_FALSE;
}

static void __exit vpn_exit(void)
{ 
	struct vpn_head *vpnh = vpn_get_head();

	vpn_fn.vpn_action = NULL;
	nf_unregister_hook(&vpn_ops);
	del_timer_sync(&vpnh->vtimer);
#ifdef CONFIG_PROC_FS
	remove_proc_entry("vpndns", proc_dir);
#endif
	return;
}

module_init(vpn_init);
module_exit(vpn_exit);
MODULE_LICENSE("GPL");
