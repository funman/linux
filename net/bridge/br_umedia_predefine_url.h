
/*
* This file is created for support to response to pre-defined URL with device's IP
* User can use the predefined URL to easy broswe GUI of device.
*
*						by U-Media Ricky CAO
*/

struct predefined_url {
	unsigned char *url;
	unsigned short len;
};

extern struct predefined_url pdUrl;

extern int br_check_dns_query(struct net_bridge_port *p, struct sk_buff *pskb);
extern void br_init_predefined_url(void);



