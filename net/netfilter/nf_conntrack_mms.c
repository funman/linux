/* MMS extension for IP connection tracking
 * (C) 2002 by Filip Sneppe <filip.sneppe@cronos.be>
 * based on ip_conntrack_ftp.c and ip_conntrack_irc.c
 *
 * ip_conntrack_mms.c v0.3 2002-09-22
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Module load syntax:
 *      insmod ip_conntrack_mms.o ports=port1,port2,...port<MAX_PORTS>
 *
 *      Please give the ports of all MMS servers You wish to connect to.
 *      If you don't specify ports, the default will be TCP port 1755.
 *
 *      More info on MMS protocol, firewalls and NAT:
 *      http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dnwmt/html/MMSFirewall.asp
 *      http://www.microsoft.com/windows/windowsmedia/serve/firewall.asp
 *
 *      The SDP project people are reverse-engineering MMS:
 *      http://get.to/sdp
 *
 * base on ip_conntrack_mms.c
 * 
 * nf_conntrack_mms.c v0.3.1 2008-09-06
 *      for 2.6.21.x, by Arthur Tang <tangjingbiao@gmail.com>
 *      
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netfilter.h>
#include <linux/ip.h>
#include <linux/ctype.h>
#include <net/checksum.h>
#include <net/tcp.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_ecache.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <linux/netfilter/nf_conntrack_mms.h>

DEFINE_RWLOCK(nf_mms_lock);

#define MAX_PORTS 8
static int ports[MAX_PORTS];
static int ports_c;
#ifdef MODULE_PARM
module_param_array(ports, int, &ports_c, 0600);
MODULE_PARM_DESC(ports, "port numbers of MMS");
#endif

static char mms_buffer[65536];

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

MODULE_AUTHOR("Filip Sneppe <filip.sneppe@cronos.be>");
MODULE_DESCRIPTION("Microsoft Windows Media Services (MMS) connection tracking module");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ip_conntrack_mms");

unsigned int (*nf_nat_mms_hook)(struct sk_buff **pskb,
				enum ip_conntrack_info ctinfo,
				struct nf_conntrack_expect *exp,
				struct nf_ct_mms_expect *expinfo);

EXPORT_SYMBOL_GPL(nf_nat_mms_hook);

#ifdef CONFIG_IP_NF_NAT_NEEDED
EXPORT_SYMBOL_GPL(nf_mms_lock);
#endif

/* #define isdigit(c) (c >= '0' && c <= '9') */

/* copied from drivers/usb/serial/io_edgeport.c - not perfect but will do the trick */
static void unicode_to_ascii (char *string, short *unicode, int unicode_size)
{
	int i;
	for (i = 0; i < unicode_size; ++i) {
		string[i] = (char)(unicode[i]);
	}
	string[unicode_size] = 0x00;
}

__inline static int atoi(char *s) 
{
	int i=0;
	while (isdigit(*s)) {
		i = i*10 + *(s++) - '0';
	}
	return i;
}

/* convert ip address string like "192.168.0.10" to unsigned int */
__inline static u_int32_t asciiiptoi(char *s)
{
	unsigned int i, j, k;

	for(i=k=0; k<3; ++k, ++s, i<<=8) {
		i+=atoi(s);
		for(j=0; (*(++s) != '.') && (j<3); ++j)
			;
	}
	i+=atoi(s);
	return ntohl(i);
}

int parse_mms(const char *data, 
	      const unsigned int datalen,
	      __be32 *mms_ip,
	      u_int8_t *mms_proto,
	      u_int16_t *mms_port,
	      char **mms_string_b,
	      char **mms_string_e,
	      char **mms_padding_e)
{
	int unicode_size, i;
	char tempstring[28];       /* "\\255.255.255.255\UDP\65535" */
	char getlengthstring[28];
	
	for(unicode_size=0; 
	    (char) *(data+(MMS_SRV_UNICODE_STRING_OFFSET+unicode_size*2)) != (char)0;
	    unicode_size++)
		if ((unicode_size == 28) || (MMS_SRV_UNICODE_STRING_OFFSET+unicode_size*2 >= datalen)) 
			return -1; /* out of bounds - incomplete packet */
	
	unicode_to_ascii(tempstring, (short *)(data+MMS_SRV_UNICODE_STRING_OFFSET), unicode_size);
	DEBUGP("nf_conntrack_mms: offset 60: %s\n", (const char *)(tempstring));
	
	/* IP address ? */
	*mms_ip = asciiiptoi(tempstring+2);
	
	i=sprintf(getlengthstring, "%u.%u.%u.%u", HIPQUAD(*mms_ip));
		
	/* protocol ? */
	if(strncmp(tempstring+3+i, "TCP", 3)==0)
		*mms_proto = IPPROTO_TCP;
	else if(strncmp(tempstring+3+i, "UDP", 3)==0)
		*mms_proto = IPPROTO_UDP;

	/* port ? */
	*mms_port = atoi(tempstring+7+i);

	/* we store a pointer to the beginning of the "\\a.b.c.d\proto\port" 
	   unicode string, one to the end of the string, and one to the end 
	   of the packet, since we must keep track of the number of bytes 
	   between end of the unicode string and the end of packet (padding) */
	*mms_string_b  = (char *)(data + MMS_SRV_UNICODE_STRING_OFFSET);
	*mms_string_e  = (char *)(data + MMS_SRV_UNICODE_STRING_OFFSET + unicode_size * 2);
	*mms_padding_e = (char *)(data + datalen); /* looks funny, doesn't it */
	return 0;
}


/* FIXME: This should be in userspace.  Later. */
static int help(struct sk_buff **pskb, unsigned int protoff,
		struct nf_conn *ct, enum ip_conntrack_info ctinfo)
{
	/* tcplen not negative guaranteed by ip_conntrack_tcp.c */
	struct tcphdr *tcph, _tcph;
	unsigned int dataoff, datalen;
	char *data, *mb_ptr;
	int dir = CTINFO2DIR(ctinfo);
	struct nf_conntrack_expect *exp; 
	struct nf_ct_mms_expect expinfo, *exp_mms_info;
	__be16 mport;
	u_int8_t mms_proto;
	char mms_proto_string[8];
	u_int16_t mms_port;
	char *mms_string_b, *mms_string_e, *mms_padding_e;
	union nf_conntrack_address taddr;
	typeof(nf_nat_mms_hook) nf_nat_mms;
	     
	/* Until there's been traffic both ways, don't look in packets. */
	if (ctinfo != IP_CT_ESTABLISHED
	    && ctinfo != IP_CT_ESTABLISHED+IP_CT_IS_REPLY) {
		DEBUGP("nf_conntrack_mms: Conntrackinfo = %u\n", ctinfo);
		return NF_ACCEPT;
	}

	/* Not whole TCP header? */
	tcph = skb_header_pointer(*pskb, protoff, sizeof(_tcph), &_tcph);
	if (!tcph)
		return NF_ACCEPT;

	/* No data? */
	dataoff = protoff + tcph->doff * 4;
	datalen = (*pskb)->len - dataoff;
	if (dataoff >= (*pskb)->len)
		return NF_ACCEPT;

	spin_lock_bh(&nf_mms_lock);
	mb_ptr = skb_header_pointer(*pskb, dataoff, (*pskb)->len - dataoff, mms_buffer);
	BUG_ON(mb_ptr == NULL);

	data = mb_ptr;
	exp_mms_info = &expinfo;
	
	/* Only look at packets with 0x00030002/196610 on bytes 36->39 of TCP payload */
	/* FIXME: There is an issue with only looking at this packet: before this packet, 
	   the client has already sent a packet to the server with the server's hostname 
	   according to the client (think of it as the "Host: " header in HTTP/1.1). The 
	   server will break the connection if this doesn't correspond to its own host 
	   header. The client can also connect to an IP address; if it's the server's IP
	   address, it will not break the connection. When doing DNAT on a connection 
	   where the client uses a server's IP address, the nat module should detect
	   this and change this string accordingly to the DNATed address. This should
	   probably be done by checking for an IP address, then storing it as a member
	   of struct ip_ct_mms_expect and checking for it in ip_nat_mms...
	   */
	if( (MMS_SRV_MSG_OFFSET < datalen) && 
	    ((*(u32 *)(data+MMS_SRV_MSG_OFFSET)) == MMS_SRV_MSG_ID)) {
		DEBUGP("nf_conntrack_mms: offset 37: %u %u %u %u, datalen:%u\n", 
		       (u8)*(data+36), (u8)*(data+37), 
		       (u8)*(data+38), (u8)*(data+39),
		       datalen);
		if(parse_mms(data, datalen, &taddr.ip, &mms_proto, &mms_port,
		             &mms_string_b, &mms_string_e, &mms_padding_e))
			if(net_ratelimit())
				/* FIXME: more verbose debugging ? */
				printk(KERN_WARNING
				       "nf_conntrack_mms: Unable to parse data payload\n");

		memset(exp_mms_info, 0, sizeof(struct nf_ct_mms_expect));

		sprintf(mms_proto_string, "(%u)", mms_proto);
		DEBUGP("nf_conntrack_mms: adding %s expectation " NIPQUAD_FMT " -> "
				NIPQUAD_FMT ":%u\n",
		       mms_proto == IPPROTO_TCP ? "TCP"
		       : mms_proto == IPPROTO_UDP ? "UDP":mms_proto_string,
		       NIPQUAD(ct->tuplehash[!dir].tuple.src.u3.ip),
		       NIPQUAD(taddr.ip),
		       mms_port);
		
		/* it's possible that the client will just ask the server to tunnel
		   the stream over the same TCP session (from port 1755): there's 
		   shouldn't be a need to add an expectation in that case, but it
		   makes NAT packet mangling so much easier */

		DEBUGP("nf_conntrack_mms: tcph->seq = %u\n", tcph->seq);

		if ((exp = nf_ct_expect_alloc(ct)) == NULL) {
			spin_unlock_bh(&nf_mms_lock);
			return NF_DROP;
		}
	
		exp_mms_info->offset  = (mms_string_b - data);	
		exp_mms_info->len     = (mms_string_e  - mms_string_b);
		exp_mms_info->padding = (mms_padding_e - mms_string_e);
		exp_mms_info->port    = mms_port;
		
		DEBUGP("nf_conntrack_mms: wrote info (ofs=%u), len=%d, padding=%u\n",
		       (mms_string_e - data), exp_mms_info->len, exp_mms_info->padding);

		mport = htons(mms_port);
		nf_ct_expect_init(exp, NF_CT_EXPECT_CLASS_DEFAULT, ct->tuplehash[!dir].tuple.src.l3num,
					 &ct->tuplehash[!dir].tuple.src.u3,
					 &taddr, mms_proto, NULL, &mport);

		DEBUGP("expect_related " NIPQUAD_FMT ":%u - " NIPQUAD_FMT ":%u    exp = %p\n",
			NIPQUAD(exp->tuple.src.u3.ip),
	                ntohs(exp->tuple.src.u.all),
	                NIPQUAD(exp->tuple.dst.u3.ip),
	                ntohs(exp->tuple.dst.u.all), exp);

		nf_nat_mms = rcu_dereference(nf_nat_mms_hook);
		if (nf_nat_mms && ct->status & IPS_NAT_MASK)
			nf_nat_mms(pskb, ctinfo, exp, exp_mms_info);
		else {
			if (nf_ct_expect_related(exp) == 0)
				DEBUGP("mms: nf_conntrack_expect_related succeeded\n");
			else
				DEBUGP("mms: nf_conntrack_expect_related failed\n");
		}

		if (exp) nf_ct_expect_put(exp);

	}

	spin_unlock_bh(&nf_mms_lock);

	return NF_ACCEPT;
}

static struct nf_conntrack_helper mms[MAX_PORTS] __read_mostly;
static char mms_names[MAX_PORTS][10] __read_mostly;

/* Not __exit: called from init() */
static void nf_conntrack_mms_fini(void)
{
	int i;
	for (i = 0; (i < MAX_PORTS) && ports[i]; i++) {
		DEBUGP("nf_conntrack_mms: unregistering helper for port %d\n",
				ports[i]);
		nf_conntrack_helper_unregister(&mms[i]);
	}
}

static int __init nf_conntrack_mms_init(void)
{
	int i, ret;
	char *tmpname;
	struct nf_conntrack_expect_policy ep[MAX_PORTS];

	if (ports[0] == 0)
		ports[0] = MMS_PORT;

	for (i = 0; (i < MAX_PORTS) && ports[i]; i++) {
		memset(&mms[i], 0, sizeof(struct nf_conntrack_helper));
		memset(&ep[i], 0, sizeof(struct nf_conntrack_expect_policy));

		mms[i].tuple.src.l3num = PF_INET;
		mms[i].tuple.src.u.tcp.port = htons(ports[i]);
		mms[i].tuple.dst.protonum = IPPROTO_TCP;
		mms[i].mask.src.l3num = 0xFFFF;
		mms[i].mask.src.u.tcp.port = 0xFFFF;
		mms[i].mask.dst.protonum = 0xFF;		
		//mms[i].max_expected = 1;
		//mms[i].timeout = 120;
		mms[i].me = THIS_MODULE;
		mms[i].help = help;
		mms[i].expect_policy = &ep[i];
		ep[i].timeout = 1;
		ep[i].max_expected = 120;				

		tmpname = &mms_names[i][0];
		if (ports[i] == MMS_PORT)
			sprintf(tmpname, "mms");
		else
			sprintf(tmpname, "mms-%d", ports[i]);
		mms[i].name = tmpname;

		DEBUGP("nf_conntrack_mms: registering helper for port %d\n", 
				ports[i]);
		ret = nf_conntrack_helper_register(&mms[i]);

		if (ret) {
			printk("nf_conntrack_mms: ERROR registering port %d\n", ports[i]);
			nf_conntrack_mms_fini();
			return ret;
		}
		ports_c++;
	}
	return 0;
}

module_init(nf_conntrack_mms_init);
module_exit(nf_conntrack_mms_fini);
