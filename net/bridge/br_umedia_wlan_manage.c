/*
* This file is created for support to control if allow to manage device by GUI via wireless
* We intercept TCP packets in bridge input (br_input.c) 
* and check if its destination is port 80,
* if it is, then drop/accept it per configuration,
*
*						by U-Media Ricky CAO
*/

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/netfilter_bridge.h>
#include "br_private.h"

#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/inet.h>
#include <linux/inetdevice.h>

unsigned char deny_manage_by_wlan = 0;

int br_deny_http_to_device(struct net_bridge_port *p, struct sk_buff *pskb)
{
	const unsigned short protoId = ntohs(eth_hdr(pskb)->h_proto);
	struct iphdr *iph = ip_hdr(pskb);
	struct tcphdr *tcph = (struct tcphdr*)(pskb->data+(iph->ihl<<2));
	struct net_device *pdev=NULL;
	struct in_device *pindev=NULL;
	int deny = 0;
	
	if( protoId == 0x0800/*IP*/ && iph->protocol == 0x06/*TCP*/ && ntohs(tcph->dest) != 0x0080 /*Port 80 for DNS*/) { 
		pdev = dev_get_by_name(sock_net(pskb->sk), "br0");
		if (!pdev){
			return -EINVAL;
		}

		pindev = in_dev_get(pdev);
		if(iph->daddr == pindev->ifa_list->ifa_address && !memcmp(p->dev->name, "ra0", 3)) {
			deny = 1;
		}

		in_dev_put(pindev);
		dev_put(pdev);
	} 
	
	return deny;
}

