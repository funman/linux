//Oct 29, 2012--Modifications were made by U-Media Communication, inc.
/*
 *	Sysfs attributes of bridge ports
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Stephen Hemminger		<shemminger@osdl.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/if_bridge.h>
#include <linux/rtnetlink.h>
#include <linux/spinlock.h>
#include <linux/times.h>

#include "br_private.h"

//support redirect url to configuration web, tim.wang@u-media.com.tw, 2012-12-14
#if defined(CONFIG_BRIDGE_UMEDIA_REDIRECT_URL)
#include "br_umedia_redirect_url.h"
#endif

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

#define to_dev(obj)	container_of(obj, struct device, kobj)
#define to_bridge(cd)	((struct net_bridge *)netdev_priv(to_net_dev(cd)))

/*
 * Common code for storing bridge parameters.
 */
static ssize_t store_bridge_parm(struct device *d,
				 const char *buf, size_t len,
				 int (*set)(struct net_bridge *, unsigned long))
{
	struct net_bridge *br = to_bridge(d);
	char *endp;
	unsigned long val;
	int err;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	val = simple_strtoul(buf, &endp, 0);
	if (endp == buf)
		return -EINVAL;

	spin_lock_bh(&br->lock);
	err = (*set)(br, val);
	spin_unlock_bh(&br->lock);
	return err ? err : len;
}


static ssize_t show_forward_delay(struct device *d,
				  struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%lu\n", jiffies_to_clock_t(br->forward_delay));
}

static int set_forward_delay(struct net_bridge *br, unsigned long val)
{
	unsigned long delay = clock_t_to_jiffies(val);
	br->forward_delay = delay;
	if (br_is_root_bridge(br))
		br->bridge_forward_delay = delay;
	return 0;
}

static ssize_t store_forward_delay(struct device *d,
				   struct device_attribute *attr,
				   const char *buf, size_t len)
{
	return store_bridge_parm(d, buf, len, set_forward_delay);
}
static DEVICE_ATTR(forward_delay, S_IRUGO | S_IWUSR,
		   show_forward_delay, store_forward_delay);

static ssize_t show_hello_time(struct device *d, struct device_attribute *attr,
			       char *buf)
{
	return sprintf(buf, "%lu\n",
		       jiffies_to_clock_t(to_bridge(d)->hello_time));
}

static int set_hello_time(struct net_bridge *br, unsigned long val)
{
	unsigned long t = clock_t_to_jiffies(val);

	if (t < HZ)
		return -EINVAL;

	br->hello_time = t;
	if (br_is_root_bridge(br))
		br->bridge_hello_time = t;
	return 0;
}

static ssize_t store_hello_time(struct device *d,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	return store_bridge_parm(d, buf, len, set_hello_time);
}
static DEVICE_ATTR(hello_time, S_IRUGO | S_IWUSR, show_hello_time,
		   store_hello_time);

static ssize_t show_max_age(struct device *d, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%lu\n",
		       jiffies_to_clock_t(to_bridge(d)->max_age));
}

static int set_max_age(struct net_bridge *br, unsigned long val)
{
	unsigned long t = clock_t_to_jiffies(val);
	br->max_age = t;
	if (br_is_root_bridge(br))
		br->bridge_max_age = t;
	return 0;
}

static ssize_t store_max_age(struct device *d, struct device_attribute *attr,
			     const char *buf, size_t len)
{
	return store_bridge_parm(d, buf, len, set_max_age);
}
static DEVICE_ATTR(max_age, S_IRUGO | S_IWUSR, show_max_age, store_max_age);

static ssize_t show_ageing_time(struct device *d,
				struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%lu\n", jiffies_to_clock_t(br->ageing_time));
}

static int set_ageing_time(struct net_bridge *br, unsigned long val)
{
	br->ageing_time = clock_t_to_jiffies(val);
	return 0;
}

static ssize_t store_ageing_time(struct device *d,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
	return store_bridge_parm(d, buf, len, set_ageing_time);
}
static DEVICE_ATTR(ageing_time, S_IRUGO | S_IWUSR, show_ageing_time,
		   store_ageing_time);

static ssize_t show_stp_state(struct device *d,
			      struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%d\n", br->stp_enabled);
}


static ssize_t store_stp_state(struct device *d,
			       struct device_attribute *attr, const char *buf,
			       size_t len)
{
	struct net_bridge *br = to_bridge(d);
	char *endp;
	unsigned long val;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	val = simple_strtoul(buf, &endp, 0);
	if (endp == buf)
		return -EINVAL;

	if (!rtnl_trylock())
		return restart_syscall();
	br_stp_set_enabled(br, val);
	rtnl_unlock();

	return len;
}
static DEVICE_ATTR(stp_state, S_IRUGO | S_IWUSR, show_stp_state,
		   store_stp_state);

static ssize_t show_priority(struct device *d, struct device_attribute *attr,
			     char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%d\n",
		       (br->bridge_id.prio[0] << 8) | br->bridge_id.prio[1]);
}

static int set_priority(struct net_bridge *br, unsigned long val)
{
	br_stp_set_bridge_priority(br, (u16) val);
	return 0;
}

static ssize_t store_priority(struct device *d, struct device_attribute *attr,
			       const char *buf, size_t len)
{
	return store_bridge_parm(d, buf, len, set_priority);
}
static DEVICE_ATTR(priority, S_IRUGO | S_IWUSR, show_priority, store_priority);

static ssize_t show_root_id(struct device *d, struct device_attribute *attr,
			    char *buf)
{
	return br_show_bridge_id(buf, &to_bridge(d)->designated_root);
}
static DEVICE_ATTR(root_id, S_IRUGO, show_root_id, NULL);

static ssize_t show_bridge_id(struct device *d, struct device_attribute *attr,
			      char *buf)
{
	return br_show_bridge_id(buf, &to_bridge(d)->bridge_id);
}
static DEVICE_ATTR(bridge_id, S_IRUGO, show_bridge_id, NULL);

static ssize_t show_root_port(struct device *d, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%d\n", to_bridge(d)->root_port);
}
static DEVICE_ATTR(root_port, S_IRUGO, show_root_port, NULL);

static ssize_t show_root_path_cost(struct device *d,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", to_bridge(d)->root_path_cost);
}
static DEVICE_ATTR(root_path_cost, S_IRUGO, show_root_path_cost, NULL);

static ssize_t show_topology_change(struct device *d,
				    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", to_bridge(d)->topology_change);
}
static DEVICE_ATTR(topology_change, S_IRUGO, show_topology_change, NULL);

static ssize_t show_topology_change_detected(struct device *d,
					     struct device_attribute *attr,
					     char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%d\n", br->topology_change_detected);
}
static DEVICE_ATTR(topology_change_detected, S_IRUGO,
		   show_topology_change_detected, NULL);

static ssize_t show_hello_timer(struct device *d,
				struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%ld\n", br_timer_value(&br->hello_timer));
}
static DEVICE_ATTR(hello_timer, S_IRUGO, show_hello_timer, NULL);

static ssize_t show_tcn_timer(struct device *d, struct device_attribute *attr,
			      char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%ld\n", br_timer_value(&br->tcn_timer));
}
static DEVICE_ATTR(tcn_timer, S_IRUGO, show_tcn_timer, NULL);

static ssize_t show_topology_change_timer(struct device *d,
					  struct device_attribute *attr,
					  char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%ld\n", br_timer_value(&br->topology_change_timer));
}
static DEVICE_ATTR(topology_change_timer, S_IRUGO, show_topology_change_timer,
		   NULL);

//2012-06-12, David Lin, [Merge from linux-2.6.21 of SDK3.6.0.0]
//Joan.Huang add start 
// for bridge loop detected
static ssize_t show_bridge_loop_detected(struct device *d,
					  struct device_attribute *attr,
					  char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%d\n", br->bridge_loop_detected);
}

static void set_bridge_loop_detected(struct net_bridge *br, unsigned long val)
{
	br->bridge_loop_detected = val;
}

static ssize_t store_bridge_loop_detected(struct device *d,
			       struct device_attribute *attr, const char *buf,
			       size_t len)
{
	return store_bridge_parm(d, buf, len, set_bridge_loop_detected);
}

static DEVICE_ATTR(bridge_loop_detected, S_IRUGO | S_IWUSR, show_bridge_loop_detected,
		   store_bridge_loop_detected);

static ssize_t show_local_topology_change(struct device *d,
					     struct device_attribute *attr,
					     char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%d\n", br->local_topology_change);
}
static void set_local_topology_change(struct net_bridge *br, unsigned long val)
{
	br->local_topology_change = val;
}
static ssize_t store_local_topology_change(struct device *d,
			       struct device_attribute *attr, const char *buf,
			       size_t len)
{
	return store_bridge_parm(d, buf, len, set_local_topology_change);
}
static DEVICE_ATTR(local_topology_change, S_IRUGO | S_IWUSR,
		   show_local_topology_change, store_local_topology_change);

//Joan.Huang add end

static ssize_t show_gc_timer(struct device *d, struct device_attribute *attr,
			     char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%ld\n", br_timer_value(&br->gc_timer));
}
static DEVICE_ATTR(gc_timer, S_IRUGO, show_gc_timer, NULL);

static ssize_t show_group_addr(struct device *d,
			       struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%x:%x:%x:%x:%x:%x\n",
		       br->group_addr[0], br->group_addr[1],
		       br->group_addr[2], br->group_addr[3],
		       br->group_addr[4], br->group_addr[5]);
}

static ssize_t store_group_addr(struct device *d,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct net_bridge *br = to_bridge(d);
	unsigned new_addr[6];
	int i;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (sscanf(buf, "%x:%x:%x:%x:%x:%x",
		   &new_addr[0], &new_addr[1], &new_addr[2],
		   &new_addr[3], &new_addr[4], &new_addr[5]) != 6)
		return -EINVAL;

	/* Must be 01:80:c2:00:00:0X */
	for (i = 0; i < 5; i++)
		if (new_addr[i] != br_group_address[i])
			return -EINVAL;

	if (new_addr[5] & ~0xf)
		return -EINVAL;

	if (new_addr[5] == 1 ||		/* 802.3x Pause address */
	    new_addr[5] == 2 ||		/* 802.3ad Slow protocols */
	    new_addr[5] == 3)		/* 802.1X PAE address */
		return -EINVAL;

	spin_lock_bh(&br->lock);
	for (i = 0; i < 6; i++)
		br->group_addr[i] = new_addr[i];
	spin_unlock_bh(&br->lock);
	return len;
}

static DEVICE_ATTR(group_addr, S_IRUGO | S_IWUSR,
		   show_group_addr, store_group_addr);

//2012-06-12, David Lin, [Merge from linux-2.6.21 of SDK3.6.0.0]
//+++Ricky Cao: Above is added for support predefined URL
#if defined(CONFIG_BRIDGE_UMEDIA_PREDEFINED_URL)
//Expose configured predefined url with command "cat /sys/class/net/br0/bridge/predefined_url"
static ssize_t show_predefined_url(struct device *d,
			       struct device_attribute *attr, char *buf)
{
	unsigned short idx = 0;

	if (pdUrl.len <= 0 || pdUrl.url == NULL) {
		return 0;
	}

	while ( idx < pdUrl.len ){
		printk("%c", pdUrl.url[idx]);
		idx++;
	}
	printk("\n");

	return 0;
}

//Configure to predefined url with command "echo "[predefined url]" > /sys/class/net/br0/bridge/predefined_url"
static ssize_t store_predefined_url(struct device *d, 
						struct device_attribute *attr, 
						const char *buf, size_t len)
{
	unsigned int idx = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if(pdUrl.url != NULL) {
		kfree(pdUrl.url);
	}
	pdUrl.len = 0;

	//The buf will be fill in 0x0a as terminated, so it len is 1 at latest
	if (len > 1){
		pdUrl.url = kmalloc(sizeof(char) * (len-1), GFP_USER);
		memset(pdUrl.url, 0, len-1);

		printk("Configure predefined URL to: \"");
		while( idx < len && buf[idx] != 0x0a ){
			printk("%c", buf[idx]);
			pdUrl.url[idx] = buf[idx];
			pdUrl.len++;
			idx++;
		}
		printk("\"\n");
	}

	return len;
}

//Create predefined_url to /sys/class/net/br0/bridge/
static DEVICE_ATTR(predefined_url, S_IRUGO | S_IWUSR,
		   show_predefined_url, store_predefined_url);
#endif
//---Ricky Cao: Above is added for support predefined URL

//Ricky Cao: added for control if allow manage device by GUI via wireless
#if defined(CONFIG_BRIDGE_UMEDIA_WLAN_MANAGE)
//Expose configured predefined url with command "cat /sys/class/net/br0/bridge/deny_manage_via_wlan"
static ssize_t show_deny_manage_via_wlan(struct device *d,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", deny_manage_by_wlan);
}

//Configure to deny manage by GUI via wlan with command "echo "[0 or 1]" > /sys/class/net/br0/bridge/deny_manage_via_wlan"
static void set_deny_manage_via_wlan(struct net_bridge *br, unsigned long val)
{
	deny_manage_by_wlan = (unsigned char)val;
}

static ssize_t store_deny_manage_via_wlan(struct device *d,
			       struct device_attribute *attr, const char *buf,
			       size_t len)
{
	return store_bridge_parm(d, buf, len, set_deny_manage_via_wlan);
}

static DEVICE_ATTR(deny_manage_via_wlan, S_IRUGO | S_IWUSR,
		   show_deny_manage_via_wlan, store_deny_manage_via_wlan);
#endif
//Ricky Cao added Done

//support redirect url to configuration web, tim.wang@u-media.com.tw, 2012-12-14
#if defined(CONFIG_BRIDGE_UMEDIA_REDIRECT_URL)
//Expose configured redirect url enabled with command "cat /sys/class/net/br0/bridge/redirect_url_enabled"
static ssize_t show_redirect_url_enabled(struct device *d,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", redirect_url_enabled);
}

//Configure to redirect url enabled by GUI via wlan with command "echo "[0 or 1]" > /sys/class/net/br0/bridge/redirect_url_enabled"
static void set_redirect_url_enabled(struct net_bridge *br, unsigned long val)
{
	redirect_url_enabled = (unsigned char)val;
}

static ssize_t store_redirect_url_enabled(struct device *d,
			       struct device_attribute *attr, const char *buf,
			       size_t len)
{
	return store_bridge_parm(d, buf, len, set_redirect_url_enabled);
}

static DEVICE_ATTR(redirect_url_enabled, S_IRUGO | S_IWUSR,
		   show_redirect_url_enabled, store_redirect_url_enabled);

#endif


static ssize_t store_flush(struct device *d,
			   struct device_attribute *attr,
			   const char *buf, size_t len)
{
	struct net_bridge *br = to_bridge(d);

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	br_fdb_flush(br);
	return len;
}
static DEVICE_ATTR(flush, S_IWUSR, NULL, store_flush);

#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
static ssize_t show_multicast_router(struct device *d,
				     struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%d\n", br->multicast_router);
}

static ssize_t store_multicast_router(struct device *d,
				      struct device_attribute *attr,
				      const char *buf, size_t len)
{
	return store_bridge_parm(d, buf, len, br_multicast_set_router);
}
static DEVICE_ATTR(multicast_router, S_IRUGO | S_IWUSR, show_multicast_router,
		   store_multicast_router);

static ssize_t show_multicast_snooping(struct device *d,
				       struct device_attribute *attr,
				       char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%d\n", !br->multicast_disabled);
}

static ssize_t store_multicast_snooping(struct device *d,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	return store_bridge_parm(d, buf, len, br_multicast_toggle);
}
static DEVICE_ATTR(multicast_snooping, S_IRUGO | S_IWUSR,
		   show_multicast_snooping, store_multicast_snooping);

static ssize_t show_hash_elasticity(struct device *d,
				    struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%u\n", br->hash_elasticity);
}

static int set_elasticity(struct net_bridge *br, unsigned long val)
{
	br->hash_elasticity = val;
	return 0;
}

static ssize_t store_hash_elasticity(struct device *d,
				     struct device_attribute *attr,
				     const char *buf, size_t len)
{
	return store_bridge_parm(d, buf, len, set_elasticity);
}
static DEVICE_ATTR(hash_elasticity, S_IRUGO | S_IWUSR, show_hash_elasticity,
		   store_hash_elasticity);

static ssize_t show_hash_max(struct device *d, struct device_attribute *attr,
			     char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%u\n", br->hash_max);
}

static ssize_t store_hash_max(struct device *d, struct device_attribute *attr,
			      const char *buf, size_t len)
{
	return store_bridge_parm(d, buf, len, br_multicast_set_hash_max);
}
static DEVICE_ATTR(hash_max, S_IRUGO | S_IWUSR, show_hash_max,
		   store_hash_max);

static ssize_t show_multicast_last_member_count(struct device *d,
						struct device_attribute *attr,
						char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%u\n", br->multicast_last_member_count);
}

static int set_last_member_count(struct net_bridge *br, unsigned long val)
{
	br->multicast_last_member_count = val;
	return 0;
}

static ssize_t store_multicast_last_member_count(struct device *d,
						 struct device_attribute *attr,
						 const char *buf, size_t len)
{
	return store_bridge_parm(d, buf, len, set_last_member_count);
}
static DEVICE_ATTR(multicast_last_member_count, S_IRUGO | S_IWUSR,
		   show_multicast_last_member_count,
		   store_multicast_last_member_count);

static ssize_t show_multicast_startup_query_count(
	struct device *d, struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%u\n", br->multicast_startup_query_count);
}

static int set_startup_query_count(struct net_bridge *br, unsigned long val)
{
	br->multicast_startup_query_count = val;
	return 0;
}

static ssize_t store_multicast_startup_query_count(
	struct device *d, struct device_attribute *attr, const char *buf,
	size_t len)
{
	return store_bridge_parm(d, buf, len, set_startup_query_count);
}
static DEVICE_ATTR(multicast_startup_query_count, S_IRUGO | S_IWUSR,
		   show_multicast_startup_query_count,
		   store_multicast_startup_query_count);

static ssize_t show_multicast_last_member_interval(
	struct device *d, struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%lu\n",
		       jiffies_to_clock_t(br->multicast_last_member_interval));
}

static int set_last_member_interval(struct net_bridge *br, unsigned long val)
{
	br->multicast_last_member_interval = clock_t_to_jiffies(val);
	return 0;
}

static ssize_t store_multicast_last_member_interval(
	struct device *d, struct device_attribute *attr, const char *buf,
	size_t len)
{
	return store_bridge_parm(d, buf, len, set_last_member_interval);
}
static DEVICE_ATTR(multicast_last_member_interval, S_IRUGO | S_IWUSR,
		   show_multicast_last_member_interval,
		   store_multicast_last_member_interval);

static ssize_t show_multicast_membership_interval(
	struct device *d, struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%lu\n",
		       jiffies_to_clock_t(br->multicast_membership_interval));
}

static int set_membership_interval(struct net_bridge *br, unsigned long val)
{
	br->multicast_membership_interval = clock_t_to_jiffies(val);
	return 0;
}

static ssize_t store_multicast_membership_interval(
	struct device *d, struct device_attribute *attr, const char *buf,
	size_t len)
{
	return store_bridge_parm(d, buf, len, set_membership_interval);
}
static DEVICE_ATTR(multicast_membership_interval, S_IRUGO | S_IWUSR,
		   show_multicast_membership_interval,
		   store_multicast_membership_interval);

static ssize_t show_multicast_querier_interval(struct device *d,
					       struct device_attribute *attr,
					       char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%lu\n",
		       jiffies_to_clock_t(br->multicast_querier_interval));
}

static int set_querier_interval(struct net_bridge *br, unsigned long val)
{
	br->multicast_querier_interval = clock_t_to_jiffies(val);
	return 0;
}

static ssize_t store_multicast_querier_interval(struct device *d,
						struct device_attribute *attr,
						const char *buf, size_t len)
{
	return store_bridge_parm(d, buf, len, set_querier_interval);
}
static DEVICE_ATTR(multicast_querier_interval, S_IRUGO | S_IWUSR,
		   show_multicast_querier_interval,
		   store_multicast_querier_interval);

static ssize_t show_multicast_query_interval(struct device *d,
					     struct device_attribute *attr,
					     char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%lu\n",
		       jiffies_to_clock_t(br->multicast_query_interval));
}

static int set_query_interval(struct net_bridge *br, unsigned long val)
{
	br->multicast_query_interval = clock_t_to_jiffies(val);
	return 0;
}

static ssize_t store_multicast_query_interval(struct device *d,
					      struct device_attribute *attr,
					      const char *buf, size_t len)
{
	return store_bridge_parm(d, buf, len, set_query_interval);
}
static DEVICE_ATTR(multicast_query_interval, S_IRUGO | S_IWUSR,
		   show_multicast_query_interval,
		   store_multicast_query_interval);

static ssize_t show_multicast_query_response_interval(
	struct device *d, struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(
		buf, "%lu\n",
		jiffies_to_clock_t(br->multicast_query_response_interval));
}

static int set_query_response_interval(struct net_bridge *br, unsigned long val)
{
	br->multicast_query_response_interval = clock_t_to_jiffies(val);
	return 0;
}

static ssize_t store_multicast_query_response_interval(
	struct device *d, struct device_attribute *attr, const char *buf,
	size_t len)
{
	return store_bridge_parm(d, buf, len, set_query_response_interval);
}
static DEVICE_ATTR(multicast_query_response_interval, S_IRUGO | S_IWUSR,
		   show_multicast_query_response_interval,
		   store_multicast_query_response_interval);

static ssize_t show_multicast_startup_query_interval(
	struct device *d, struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(
		buf, "%lu\n",
		jiffies_to_clock_t(br->multicast_startup_query_interval));
}

static int set_startup_query_interval(struct net_bridge *br, unsigned long val)
{
	br->multicast_startup_query_interval = clock_t_to_jiffies(val);
	return 0;
}

static ssize_t store_multicast_startup_query_interval(
	struct device *d, struct device_attribute *attr, const char *buf,
	size_t len)
{
	return store_bridge_parm(d, buf, len, set_startup_query_interval);
}
static DEVICE_ATTR(multicast_startup_query_interval, S_IRUGO | S_IWUSR,
		   show_multicast_startup_query_interval,
		   store_multicast_startup_query_interval);
#endif
#ifdef CONFIG_BRIDGE_NETFILTER
static ssize_t show_nf_call_iptables(
	struct device *d, struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%u\n", br->nf_call_iptables);
}

static int set_nf_call_iptables(struct net_bridge *br, unsigned long val)
{
	br->nf_call_iptables = val ? true : false;
	return 0;
}

static ssize_t store_nf_call_iptables(
	struct device *d, struct device_attribute *attr, const char *buf,
	size_t len)
{
	return store_bridge_parm(d, buf, len, set_nf_call_iptables);
}
static DEVICE_ATTR(nf_call_iptables, S_IRUGO | S_IWUSR,
		   show_nf_call_iptables, store_nf_call_iptables);

static ssize_t show_nf_call_ip6tables(
	struct device *d, struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%u\n", br->nf_call_ip6tables);
}

static int set_nf_call_ip6tables(struct net_bridge *br, unsigned long val)
{
	br->nf_call_ip6tables = val ? true : false;
	return 0;
}

static ssize_t store_nf_call_ip6tables(
	struct device *d, struct device_attribute *attr, const char *buf,
	size_t len)
{
	return store_bridge_parm(d, buf, len, set_nf_call_ip6tables);
}
static DEVICE_ATTR(nf_call_ip6tables, S_IRUGO | S_IWUSR,
		   show_nf_call_ip6tables, store_nf_call_ip6tables);

static ssize_t show_nf_call_arptables(
	struct device *d, struct device_attribute *attr, char *buf)
{
	struct net_bridge *br = to_bridge(d);
	return sprintf(buf, "%u\n", br->nf_call_arptables);
}

static int set_nf_call_arptables(struct net_bridge *br, unsigned long val)
{
	br->nf_call_arptables = val ? true : false;
	return 0;
}

static ssize_t store_nf_call_arptables(
	struct device *d, struct device_attribute *attr, const char *buf,
	size_t len)
{
	return store_bridge_parm(d, buf, len, set_nf_call_arptables);
}
static DEVICE_ATTR(nf_call_arptables, S_IRUGO | S_IWUSR,
		   show_nf_call_arptables, store_nf_call_arptables);
#endif

static struct attribute *bridge_attrs[] = {
	&dev_attr_forward_delay.attr,
	&dev_attr_hello_time.attr,
	&dev_attr_max_age.attr,
	&dev_attr_ageing_time.attr,
	&dev_attr_stp_state.attr,
	&dev_attr_priority.attr,
	&dev_attr_bridge_id.attr,
	&dev_attr_root_id.attr,
	&dev_attr_root_path_cost.attr,
	&dev_attr_root_port.attr,
	&dev_attr_topology_change.attr,
	&dev_attr_topology_change_detected.attr,
	&dev_attr_hello_timer.attr,
	&dev_attr_tcn_timer.attr,
	&dev_attr_topology_change_timer.attr,
	&dev_attr_gc_timer.attr,
	&dev_attr_group_addr.attr,
	&dev_attr_flush.attr,
#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
	&dev_attr_multicast_router.attr,
	&dev_attr_multicast_snooping.attr,
	&dev_attr_hash_elasticity.attr,
	&dev_attr_hash_max.attr,
	&dev_attr_multicast_last_member_count.attr,
	&dev_attr_multicast_startup_query_count.attr,
	&dev_attr_multicast_last_member_interval.attr,
	&dev_attr_multicast_membership_interval.attr,
	&dev_attr_multicast_querier_interval.attr,
	&dev_attr_multicast_query_interval.attr,
	&dev_attr_multicast_query_response_interval.attr,
	&dev_attr_multicast_startup_query_interval.attr,
#endif
#ifdef CONFIG_BRIDGE_NETFILTER
	&dev_attr_nf_call_iptables.attr,
	&dev_attr_nf_call_ip6tables.attr,
	&dev_attr_nf_call_arptables.attr,
#endif
//2012-06-12, David Lin, [Merge from linux-2.6.21 of SDK3.6.0.0]
	&dev_attr_bridge_loop_detected.attr,     //Joan.Huang add
	&dev_attr_local_topology_change.attr,     //Joan.Huang add
//+++Ricky Cao: Above is added for support predefined URL
#if defined(CONFIG_BRIDGE_UMEDIA_PREDEFINED_URL)
	&dev_attr_predefined_url.attr,
#endif
//---Ricky Cao: Above is added for support predefined URL
//Ricky Cao: added for control if allow manage device by GUI via wireless
#if defined(CONFIG_BRIDGE_UMEDIA_WLAN_MANAGE)
	&dev_attr_deny_manage_via_wlan.attr,
#endif
//Ricky Cao added DONE

//support redirect url to configuration web, tim.wang@u-media.com.tw, 2012-12-14
#if defined(CONFIG_BRIDGE_UMEDIA_REDIRECT_URL)
	&dev_attr_redirect_url_enabled.attr,
#endif
	NULL
};

static struct attribute_group bridge_group = {
	.name = SYSFS_BRIDGE_ATTR,
	.attrs = bridge_attrs,
};

/*
 * Export the forwarding information table as a binary file
 * The records are struct __fdb_entry.
 *
 * Returns the number of bytes read.
 */
static ssize_t brforward_read(struct file *filp, struct kobject *kobj,
			      struct bin_attribute *bin_attr,
			      char *buf, loff_t off, size_t count)
{
	struct device *dev = to_dev(kobj);
	struct net_bridge *br = to_bridge(dev);
	int n;

	/* must read whole records */
	if (off % sizeof(struct __fdb_entry) != 0)
		return -EINVAL;

	n =  br_fdb_fillbuf(br, buf,
			    count / sizeof(struct __fdb_entry),
			    off / sizeof(struct __fdb_entry));

	if (n > 0)
		n *= sizeof(struct __fdb_entry);

	return n;
}

static struct bin_attribute bridge_forward = {
	.attr = { .name = SYSFS_BRIDGE_FDB,
		  .mode = S_IRUGO, },
	.read = brforward_read,
};

/*
 * Add entries in sysfs onto the existing network class device
 * for the bridge.
 *   Adds a attribute group "bridge" containing tuning parameters.
 *   Binary attribute containing the forward table
 *   Sub directory to hold links to interfaces.
 *
 * Note: the ifobj exists only to be a subdirectory
 *   to hold links.  The ifobj exists in same data structure
 *   as it's parent the bridge so reference counting works.
 */
int br_sysfs_addbr(struct net_device *dev)
{
	struct kobject *brobj = &dev->dev.kobj;
	struct net_bridge *br = netdev_priv(dev);
	int err;

	err = sysfs_create_group(brobj, &bridge_group);
	if (err) {
		pr_info("%s: can't create group %s/%s\n",
			__func__, dev->name, bridge_group.name);
		goto out1;
	}

	err = sysfs_create_bin_file(brobj, &bridge_forward);
	if (err) {
		pr_info("%s: can't create attribute file %s/%s\n",
			__func__, dev->name, bridge_forward.attr.name);
		goto out2;
	}

	br->ifobj = kobject_create_and_add(SYSFS_BRIDGE_PORT_SUBDIR, brobj);
	if (!br->ifobj) {
		pr_info("%s: can't add kobject (directory) %s/%s\n",
			__func__, dev->name, SYSFS_BRIDGE_PORT_SUBDIR);
		goto out3;
	}
	return 0;
 out3:
	sysfs_remove_bin_file(&dev->dev.kobj, &bridge_forward);
 out2:
	sysfs_remove_group(&dev->dev.kobj, &bridge_group);
 out1:
	return err;

}

void br_sysfs_delbr(struct net_device *dev)
{
	struct kobject *kobj = &dev->dev.kobj;
	struct net_bridge *br = netdev_priv(dev);

	kobject_put(br->ifobj);
	sysfs_remove_bin_file(kobj, &bridge_forward);
	sysfs_remove_group(kobj, &bridge_group);
}
