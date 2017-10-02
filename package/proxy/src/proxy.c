#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/netlink.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4/nf_igd/nf_igd.h>
#include <linux/tcp.h>
#include "proxy.h"

#if 1
#define PROXY_DBG(fmt,arg...) do{}while(0)
#else
#define PROXY_DBG(fmt,args...) do{printk("%s[%d]:"fmt, __FUNCTION__, __LINE__,##args);}while(0)
#endif

#define PROXY_ERR(fmt,args...) do{printk("%s[%d]:"fmt, __FUNCTION__, __LINE__,##args);}while(0)

static unsigned char proxy_dns_enable = 1;
static struct hashh2 *proxy_dns_h2;
static struct list_head proxy_dns_list = LIST_HEAD_INIT(proxy_dns_list);
DEFINE_MUTEX(proxy_dns_lock);

static int proxy_dns_add_addr(struct dns_node_comm *comm, __be32 addr, int idx)
{
	struct proxy_dns_node *node = (void *)comm;
	struct dns_addr *entry;

	if (!idx)
		node->comm.ops->free_addr(node);
	if (idx >= PROXY_DNS_ADDR_MX) 
		return -1;

	entry = hash2_entry_create(proxy_dns_h2, &addr, 4);
	if (!entry)
		return -1;

	entry->dns = (void *)node;
	node->addr[idx] = entry;
	PROXY_DBG("add %s, addr %u.%u.%u.%u\n",
		   host2str(node->comm.node.name, NULL, 0), NIPQUAD(addr));
	return 0;
}

static void proxy_dns_free_addr(void *data)
{
	struct proxy_dns_node *node = (void *)data;

	loop_for_each(0, PROXY_DNS_ADDR_MX) {
		if (!node->addr[i])
			break;
		PROXY_DBG("free %s addr:%u.%u.%u.%u\n",
			  	host2str(node->comm.node.name, NULL, 0),
			       		NIPQUAD(node->addr[i]->addr));
		hash2_entry_free(proxy_dns_h2, node->addr[i], 4);
		node->addr[i] = NULL;
	} loop_for_each_end();
}

static struct dns_node_ops proxy_dns_ops = {
	.add_addr = proxy_dns_add_addr,
	.free_addr = proxy_dns_free_addr,
};

static int nlk_proxy_dns_add(char *url)
{
	struct proxy_dns_node *pdn;

	PROXY_DBG("\n");

	if (!url[0]) {
		PROXY_ERR("url is null\n");
		return -1;
	}

	pdn = kzalloc(sizeof(*pdn), GFP_ATOMIC);
	if (!pdn) {
		PROXY_ERR("kzalloc fail\n");
		return -1;
	}

	str2host(url, pdn->comm.node.name, sizeof(pdn->comm.node.name));
	pdn->comm.node.len = strlen(pdn->comm.node.name);
	pdn->comm.ops = &proxy_dns_ops;

	if (register_dns_node(&pdn->comm)) {
		kfree(pdn);
		PROXY_ERR("add dns node fail\n");
		return -1;
	}

	mutex_lock(&proxy_dns_lock);
	list_add(&pdn->list, &proxy_dns_list);
	mutex_unlock(&proxy_dns_lock);

	PROXY_DBG("add [%s], [%s], [%d]\n", url, pdn->comm.node.name, pdn->comm.node.len);
	return 0;
}

static int nlk_proxy_dns_free(void)
{
	struct proxy_dns_node *pdn, *_pdn;

	PROXY_DBG("\n");

	mutex_lock(&proxy_dns_lock);
	list_for_each_entry_safe(pdn, _pdn, &proxy_dns_list, list) {
		if (unregister_dns_node(&pdn->comm)) {
			PROXY_ERR("del dns node fail\n");
		} else {
			list_del(&pdn->list);
			kfree(pdn);
		}
	}
	mutex_unlock(&proxy_dns_lock);
	return 0;
}

int nlk_proxy_dns_action(struct nlmsghdr *nlh)
{
	int offset = 0, i = 0;
	struct nlk_msg_comm nlk;
	char url[IGD_DNS_LEN];

	if (nlk_data_pop(nlh, &offset, &nlk, sizeof(nlk)))
		return -1;

	if (nlk.key == 0) {
		switch (nlk.action) {
		case NLK_ACTION_ADD:
			if (nlk.obj_len != IGD_DNS_LEN)
				return -1;
			for (i = 0; i < nlk.obj_nr; i++) {
				if (nlk_data_pop(nlh, &offset, url, sizeof(url)))
					return -1;
				url[IGD_DNS_LEN - 1] = '\0';
				if (nlk_proxy_dns_add(url))
					return -1;
			}
			break;
		case NLK_ACTION_DEL:
			return nlk_proxy_dns_free();
		default:
			return -1;
		}
	} else if (nlk.key == 1) {
		switch (nlk.action) {
		case NLK_ACTION_ADD:
			proxy_dns_enable = 1;
			break;
		case NLK_ACTION_DEL:
			proxy_dns_enable = 0;
			break;
		default:
			return -1;
		}
	}
	return 0;
}

static unsigned int proxy_hook(unsigned int hook, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,	
		int (*okfn)(struct sk_buff *))
{
	struct iphdr *iph;
	struct tcphdr *tcph;

	if (!proxy_dns_enable)
		return NF_ACCEPT;
	iph = ip_hdr(skb);
	if (iph->protocol != IPPROTO_TCP)
		return NF_ACCEPT;
	tcph = (struct tcphdr *)l4_hdr(skb);
	if (ntohs(tcph->dest) != 80)
		return NF_ACCEPT;

	spin_lock_bh(&dns_lock);
	if (hash2_entry_search(proxy_dns_h2, &iph->daddr, 4))
		skb->mark = 3000;
	spin_unlock_bh(&dns_lock);

	return NF_ACCEPT;
}

static struct nf_hook_ops proxy_ops = {
	.hook		= proxy_hook,
	.owner		= THIS_MODULE,
	.pf			= NFPROTO_IPV4,
	.hooknum	= NF_INET_PRE_ROUTING,
	.priority	= NF_IP_PRI_PROXY_PREROUTING,
};

static int proxy_dns_addr_compare(void *entry, void *data, int len)
{
	return memcmp(entry, data, 4);
}

static u32 proxy_dns_addr_hash(void *entry, int dlen)
{
	u32 addr = ntohl(*(__be32 *)entry);
	return addr % PROXY_DNS_HMX;
}

static int __init proxy_init(void)
{
	proxy_dns_h2 = hash2_create("proxy_dns", PROXY_DNS_HMX, sizeof(struct dns_addr),
	       	PROXY_DNS_ENTRY_ADDR_MX, proxy_dns_addr_hash, proxy_dns_addr_compare);
	kernel_nomem(proxy_dns_h2);

	if (nf_register_hook(&proxy_ops)) {
		PROXY_DBG("register hook fail\n");
		goto err;
	}

	proxy_fn.proxy_action = nlk_proxy_dns_action;
	PROXY_DBG("proxy succ\n");
	return 0;

err:
	if (proxy_dns_h2)
		hash2_free(proxy_dns_h2);
	proxy_dns_h2 = NULL;
	return -1;
}

static void __exit proxy_exit(void)
{
	proxy_fn.proxy_action = NULL;
	nf_unregister_hook(&proxy_ops);

	nlk_proxy_dns_free();

	hash2_free(proxy_dns_h2);
	proxy_dns_h2 = NULL;

	PROXY_DBG("proxy exit\n");
}

module_init(proxy_init);
module_exit(proxy_exit);
EXPORT_SYMBOL(nlk_proxy_dns_action);

