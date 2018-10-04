/*
 * ip_conntrack_esp.c - Version 0.01
 *
 * Connection tracking support for ESP
 *
 * this module tracking SA's SPI
 *
 * (C) 2007-2008 by Arthur Tang <tangjingbiao@gmail.com>
 *
 * 2008-9-15: Porting to 2.6.21.x by Arthur Tang <tangjingbiao@gmail.com>
 *
 */

#include <linux/module.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <linux/netfilter/nf_conntrack_esp.h>

MODULE_AUTHOR("Arthur Tang <tangjingbiao@gmail.com>");
MODULE_DESCRIPTION("Netfilter connection tracking helper module for ESP");
MODULE_ALIAS("ip_conntrack_esp");

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

unsigned int (*nf_esp_forwarding_hook)(struct sk_buff **pskb,
				       enum ip_conntrack_info ctinfo,
				       struct nf_conn *ct,
				       struct nf_conntrack_expect *exp,
				       struct nf_ct_esp_master *ct_esp_info);

static int help(struct sk_buff **pskb, unsigned int protoff,
 		struct nf_conn *ct, enum ip_conntrack_info ctinfo)
{
	int dir = CTINFO2DIR(ctinfo);
	struct iphdr *iph;
	const void *datah;
	const u_int32_t *seq;
	u_int32_t dstip = 0;
	u_int32_t srcip = 0;
	int ret = 0;
	struct nf_conntrack_expect *exp = NULL;
	struct nf_ct_esp_master *ct_esp_info = &nfct_help(ct)->help.ct_esp_info;
	typeof(nf_esp_forwarding_hook) nf_esp_forwarding;

	//iph = (*pskb)->nh.iph;
	iph = ip_hdr(*pskb);
	datah = (void *)iph + iph->ihl * 4;
	seq = datah + 4;

	DEBUGP("\nconntrack_esp: help: DIR=%d, conntrackinfo=%u\n", dir, ctinfo);
        DEBUGP("conntrack_esp: %u.%u.%u.%u:0x%x -> %u.%u.%u.%u:0x%x\n",
                        NIPQUAD(ct->tuplehash[dir].tuple.src.u3.ip),
                        ntohl(seq),
                        NIPQUAD(ct->tuplehash[dir].tuple.dst.u3.ip),
                        ntohl(seq));

	if (dir == IP_CT_DIR_REPLY) {
		DEBUGP("conntrack_esp: help: Got a REPLY packet?\n");
		return NF_ACCEPT;
	}

	if (dir == IP_CT_DIR_ORIGINAL) {

		dstip = ct->tuplehash[dir].tuple.dst.u3.ip;
		srcip = ct->tuplehash[dir].tuple.src.u3.ip;

		ct_esp_info->seq = *seq;

		exp = nf_ct_expect_alloc(ct);

		DEBUGP("conntrack_esp: help: Got a ORIGINAL packet, ct = %p\n", ct);
		DEBUGP("conntrack_esp: srcip = %u.%u.%u.%u  dstip = %u.%u.%u.%u\n", 
				NIPQUAD(srcip), NIPQUAD(dstip));

		nf_ct_expect_init(exp, NF_CT_EXPECT_CLASS_DEFAULT, ct->tuplehash[!dir].tuple.src.l3num,
				&ct->tuplehash[!dir].tuple.src.u3,
				&ct->tuplehash[!dir].tuple.dst.u3,
				IPPROTO_ESP, NULL, NULL);

		nf_esp_forwarding = rcu_dereference(nf_esp_forwarding_hook);
		if (nf_esp_forwarding && ct->status & IPS_NAT_MASK)
			nf_esp_forwarding(pskb, ctinfo, ct, exp, ct_esp_info);
		else {
			DEBUGP("nf_conntrack_esp: Not define NAT Needed?? --Arthur\n");
			ret = nf_ct_expect_related(exp);
		}

		if (ret == 0) {
			DEBUGP("esp: nf_conntrack_expect_related successed\n");
		} else {
			DEBUGP("esp: nf_conntrack_expect_related failed\n");
		}

		return NF_ACCEPT;
	}

	DEBUGP("conntrack_esp: help: Got a packet not ORIGINAL not REPLY?\n");
	return NF_ACCEPT;
}

static struct nf_conntrack_helper esp;

static void nf_conntrack_esp_fini(void)
{
	DEBUGP("nf_conntrack_esp: unregistering helper\n");
	nf_conntrack_helper_unregister(&esp);
}

static int __init nf_conntrack_esp_init(void)
{
	int ret = 0;
	struct nf_conntrack_expect_policy ep;	

	memset(&esp, 0, sizeof(struct nf_conntrack_helper));
	esp.tuple.dst.protonum = 50;
	esp.mask.dst.protonum = 0xFF;
	esp.me = THIS_MODULE;
	esp.help = help;
	esp.expect_policy = &ep;
	ep.timeout = ESP_TIMEOUT;
	ep.max_expected = 20;

	DEBUGP("nf_conntrack_esp: registering helper for protonum = %u, help = %p\n", IPPROTO_ESP, help);

	ret = nf_conntrack_helper_register(&esp);

	if (ret) {
		nf_conntrack_esp_fini();
		return ret;
	}
	return 0;
}

module_init(nf_conntrack_esp_init);
module_exit(nf_conntrack_esp_fini);
