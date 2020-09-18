//Oct 29, 2012--Modifications were made by U-Media Communication, inc.
/*
 *	Generic parts
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/llc.h>
#include <net/llc.h>
#include <net/stp.h>

#include "br_private.h"

/* Compute checksum for count bytes starting at addr, using one's complement of one's complement sum*/
unsigned short br_compute_ip_checksum(unsigned short *addr, unsigned int count)
{  
	register unsigned long sum = 0;  
	while (count > 1) {    
		sum += * addr++;    
		count -= 2;  
	}  
	//if any bytes left, pad the bytes and add  
	if(count > 0) {    
		sum += ((*addr)&htons(0xFF00));  
	}  
	//Fold sum to 16 bits: add carrier to result  
	while (sum>>16) {      
		sum = (sum & 0xffff) + (sum >> 16);  
	}  //one's complement  
	sum = ~sum;  
	return ((unsigned short)sum);
}


//2012-06-12, David Lin, [Merge from linux-2.6.21 of SDK3.6.0.0]
//+++Ricky Cao: Below is added for support predefined url
#if defined(CONFIG_BRIDGE_UMEDIA_PREDEFINED_URL)
#include "br_umedia_predefine_url.h"
#endif
//---Ricky Cao: Above is added for support predefined url

//Ricky Cao: added for control if allow manage device by GUI via wireless
#if defined(CONFIG_BRIDGE_UMEDIA_WLAN_MANAGE)
extern unsigned char deny_manage_by_wlan;
#endif
//Ricky Cao added DONE

//support redirect url to configuration web, tim.wang@u-media.com.tw, 2012-12-14
#if defined(CONFIG_BRIDGE_UMEDIA_REDIRECT_URL)
#include "br_umedia_redirect_url.h"
#endif

int (*br_should_route_hook)(struct sk_buff *skb);

static const struct stp_proto br_stp_proto = {
	.rcv	= br_stp_rcv,
};

static struct pernet_operations br_net_ops = {
	.exit	= br_net_exit,
};

static int __init br_init(void)
{
	int err;

	err = stp_proto_register(&br_stp_proto);
	if (err < 0) {
		pr_err("bridge: can't register sap for STP\n");
		return err;
	}

//2012-06-12, David Lin, [Merge from linux-2.6.21 of SDK3.6.0.0]
//+++Ricky Cao: Below is added for support predefined url
#if defined(CONFIG_BRIDGE_UMEDIA_PREDEFINED_URL)
	br_init_predefined_url();
#endif
//---Ricky Cao: Above is added for support predefined url

//Ricky Cao: added for control if allow manage device by GUI via wireless
#if defined(CONFIG_BRIDGE_UMEDIA_WLAN_MANAGE)
	deny_manage_by_wlan = 0;
#endif
//Ricky Cao added DONE

//support redirect url to configuration web, tim.wang@u-media.com.tw, 2012-12-14
#if defined(CONFIG_BRIDGE_UMEDIA_REDIRECT_URL)
	br_init_rdu_localUrl();
#endif


	err = br_fdb_init();
	if (err)
		goto err_out;

	err = register_pernet_subsys(&br_net_ops);
	if (err)
		goto err_out1;

	err = br_netfilter_init();
	if (err)
		goto err_out2;

	err = register_netdevice_notifier(&br_device_notifier);
	if (err)
		goto err_out3;

	err = br_netlink_init();
	if (err)
		goto err_out4;

	brioctl_set(br_ioctl_deviceless_stub);

#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
	br_fdb_test_addr_hook = br_fdb_test_addr;
#endif

	return 0;
err_out4:
	unregister_netdevice_notifier(&br_device_notifier);
err_out3:
	br_netfilter_fini();
err_out2:
	unregister_pernet_subsys(&br_net_ops);
err_out1:
	br_fdb_fini();
err_out:
	stp_proto_unregister(&br_stp_proto);
	return err;
}

static void __exit br_deinit(void)
{
	stp_proto_unregister(&br_stp_proto);

	br_netlink_fini();
	unregister_netdevice_notifier(&br_device_notifier);
	brioctl_set(NULL);

	unregister_pernet_subsys(&br_net_ops);

	rcu_barrier(); /* Wait for completion of call_rcu()'s */

	br_netfilter_fini();
#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
	br_fdb_test_addr_hook = NULL;
#endif

	br_fdb_fini();
}

EXPORT_SYMBOL(br_should_route_hook);

module_init(br_init)
module_exit(br_deinit)
MODULE_LICENSE("GPL");
MODULE_VERSION(BR_VERSION);
