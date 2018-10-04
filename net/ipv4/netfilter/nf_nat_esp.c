/*
 * ip_nat_esp.c - Version 0.01
 *
 * NAT support for ESP
 *
 * this module NAT the esp packet
 *
 * (C) 2007-2008 by Arthur Tang <tangjingbiao@gmail.com>
 *
 *
 * 2008-9-15: Porting to kernel 2.6.21.x by Arthur Tang
 *
 */

#include <linux/module.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/netfilter_ipv4.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_helper.h>
#include <net/netfilter/nf_nat_rule.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <linux/netfilter/nf_conntrack_esp.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

MODULE_AUTHOR("Arthur Tang <tangjingbiao@gmail.com>");
MODULE_DESCRIPTION("Netfilter connection tracking helper module for ESP");
MODULE_ALIAS("ip_nat_esp");

#if 0
static unsigned int esp_nat_expected(struct nf_conn *ct,
				     struct nf_conntrack_expect *exp)
{
        struct ip_nat_multi_range mr;
        u_int32_t newdstip, newsrcip, newip;
        struct nf_ct_esp_master *ct_esp_info = &nfct_help(master)->help.ct_esp_info;
        struct nf_conn *master = ct->master;

	DEBUGP("nf_nat_esp: esp_nat_expected entered, ct = %p, master_ct = %p\n", ct, master);

	mr.rangesize = 1;
	mr.range[0].flags = IP_NAT_RANGE_MAP_IPS;
	mr.range[0].min_ip = mr.range[0].max_ip = newip;

	return ip_nat_setup_info(ct, &mr, hooknum);
}
#endif

unsigned int nf_esp_forwarding(struct sk_buff **pskb,
				       enum ip_conntrack_info ctinfo,
				       struct nf_conn *ct,
				       struct nf_conntrack_expect *exp,
				       struct nf_ct_esp_master *ct_esp_info)
{
        int dir;
	//struct nf_conntrack_tuple newtuple;
        //struct iphdr *iph = (*pskb)->nh.iph;

	//u_int32_t ipaddr;

	dir = CTINFO2DIR(ctinfo);

	DEBUGP("nf_nat_esp: DIR %d, ct = %p, exp = %p\n", dir, ct, exp);

	if (dir == IP_CT_DIR_ORIGINAL) {
		exp->dir = !dir;

		exp->expectfn = nf_nat_follow_master;

		DEBUGP("exp tuple: "); NF_CT_DUMP_TUPLE(&exp->tuple);
		DEBUGP("ct tuple: ");
		NF_CT_DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
		NF_CT_DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_REPLY].tuple);

		if (nf_conntrack_expect_related(exp) == 0)
		{
			DEBUGP("nf_nat_esp: expect related success\n");
		}
	}

	return NF_ACCEPT;

}

static void nf_esp_forwarding_fini(void)
{
	rcu_assign_pointer(nf_esp_forwarding_hook, NULL);
	synchronize_rcu();
}

static int __init nf_esp_forwarding_init(void)
{
	BUG_ON(rcu_dereference(nf_esp_forwarding_hook));
	rcu_assign_pointer(nf_esp_forwarding_hook, nf_esp_forwarding);
	return 0;
}

module_init(nf_esp_forwarding_init);
module_exit(nf_esp_forwarding_fini);
