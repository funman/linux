
/*
* This file is created for support to response to pre-defined URL with device's IP
* User can use the predefined URL to easy broswe GUI of device.
*
*						by U-Media Ricky CAO
*/

struct local_url_t {
	unsigned char *url;
	unsigned short len;
};

extern struct local_url_t localUrl;

extern unsigned char redirect_url_enabled;

extern int br_rdu_chk_dnsqry(struct net_bridge_port *p, struct sk_buff *pskb);
extern void br_init_rdu_localUrl(void);

