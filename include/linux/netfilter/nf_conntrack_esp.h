#ifndef _NF_CONNTRACK_ESP_H
#define _NF_CONNTRACK_ESP_H
/* ESP tracking. */

#ifdef __KERNEL__

#include <linux/spinlock.h>

struct nf_ct_esp_master {
        int mangled;
        u_int32_t seq;
};

extern unsigned int (*nf_esp_forwarding_hook)(struct sk_buff **pskb,
                                       enum ip_conntrack_info ctinfo,
                                       struct nf_conn *ct,
                                       struct nf_conntrack_expect *exp,
                                       struct nf_ct_esp_master *ct_esp_info);

#define ESP_TIMEOUT     (50*HZ)     /* timeout */

#endif /* __KERNEL__ */

#endif /* _IP_CONNTRACK_ESP_H */
