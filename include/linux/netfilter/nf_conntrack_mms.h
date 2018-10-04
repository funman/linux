#ifndef _NF_CONNTRACK_MMS_H
#define _NF_CONNTRACK_MMS_H
/* MMS tracking. */

#ifdef __KERNEL__
#include <linux/spinlock.h>

extern rwlock_t nf_mms_lock;

#define MMS_PORT                         1755
#define MMS_SRV_MSG_ID                   196610

#define MMS_SRV_MSG_OFFSET               36
#define MMS_SRV_UNICODE_STRING_OFFSET    60
#define MMS_SRV_CHUNKLENLV_OFFSET        16
#define MMS_SRV_CHUNKLENLM_OFFSET        32
#define MMS_SRV_MESSAGELENGTH_OFFSET     8
#endif

/* This structure is per expected connection */
struct nf_ct_mms_expect {
	u_int32_t offset;
	u_int32_t len;
	u_int32_t padding;
	u_int16_t port;
};

extern unsigned int (*nf_nat_mms_hook)(struct sk_buff **pskb,
				       enum ip_conntrack_info ctinfo,
				       struct nf_conntrack_expect *exp,
				       struct nf_ct_mms_expect *expinfo);

#endif /* _NF_CONNTRACK_MMS_H */
