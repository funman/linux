/*
* This file is created for support to response to pre-defined URL with device's IP
* We intercept DNS query request in bridge input (br_input.c) 
* and check the queried domain name if is the one we monitored,
* if it is, then respone with the device IP,
* otherwise, follow normal procedure to forward this packet.
*
* User can use the predefined URL to easy broswe GUI of device.
*
*						by U-Media Ricky CAO
*/

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/netfilter_bridge.h>
#include "br_private.h"

#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/inet.h>
#include <linux/inetdevice.h>

#include "br_umedia_redirect_url.h"

#define UDP_HEADER_LEN					8

/* Below are the offset start from DNS reponse data */
#define OFFSET_DNS_ID					0
#define OFFSET_DNS_FLAG				2
#define OFFSET_DNS_QUESTION			4
#define OFFSET_DNS_ANSWER				6
#define OFFSET_DNS_AUTHORITY			8
#define OFFSET_DNS_ADITIONAL			10

#define DNS_ANSWER_LEN				16
/* Below are the offset start from Answer field of DNS reponse data */
#define OFFSET_DNS_ANSWER_NAME		0
#define OFFSET_DNS_ANSWER_TYPE		2
#define OFFSET_DNS_ANSWER_CLASS		4
#define OFFSET_DNS_ANSWER_TTL		6
#define OFFSET_DNS_ANSWER_DATALEN	10
#define OFFSET_DNS_ANSWER_IP			12

unsigned char redirect_url_enabled = 1;
struct local_url_t localUrl;

void br_init_rdu_localUrl(void)
{
	localUrl.url = "linksysre2000.local";
	localUrl.len = strlen(localUrl.url);
	
}


unsigned short br_compute_ip_checksum(unsigned short *addr, unsigned int count);


/* set tcp checksum: given IP header and UDP datagram */
#if 0
void compute_udp_checksum(struct iphdr *pIph, unsigned short *ipPayload) {
    register unsigned long sum = 0;
    struct udphdr *udphdrp = (struct udphdr*)(ipPayload);
    unsigned short udpLen = htons(udphdrp->len);
    //printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~udp len=%d\n", udpLen);
    //add the pseudo header 
    //printf("add pseudo header\n");
    //the source ip
    sum += (pIph->saddr>>16)&0xFFFF;
    sum += (pIph->saddr)&0xFFFF;
    //the dest ip
    sum += (pIph->daddr>>16)&0xFFFF;
    sum += (pIph->daddr)&0xFFFF;
    //protocol and reserved: 17
    sum += htons(IPPROTO_UDP);
    //the length
    sum += udphdrp->len;
 
    //add the IP payload
    //printf("add ip payload\n");
    //initialize checksum to 0
    udphdrp->check = 0;
    while (udpLen > 1) {
        sum += * ipPayload++;
        udpLen -= 2;
    }
    //if any bytes left, pad the bytes and add
    if(udpLen > 0) {
        //printf("+++++++++++++++padding: %d\n", udpLen);
        sum += ((*ipPayload)&htons(0xFF00));
    }
      //Fold sum to 16 bits: add carrier to result
    //printf("add carrier\n");
      while (sum>>16) {
          sum = (sum & 0xffff) + (sum >> 16);
      }
    //printf("one's complement\n");
      sum = ~sum;
    //set computation result
    udphdrp->check = ((unsigned short)sum == 0x0000)?0xFFFF:(unsigned short)sum;
}	
#endif


//Reply the DNS query request with device IP
int br_rdu_reply_dnsqry(struct net_bridge_port *p, struct sk_buff *pskb, struct in_device *pindev, int is_local_url)
{
	unsigned char tmpMac[6];
	struct iphdr *iph = ip_hdr(pskb);
	unsigned int tmpIp;
	struct udphdr *uh = (struct udphdr*)(pskb->data+(iph->ihl<<2));
	unsigned short tmpUdp;
	unsigned char *dnsAnswer;
	unsigned int rdu_host_ip;

	//Update the DNS content
	dnsAnswer = skb_put(pskb, DNS_ANSWER_LEN); //allocate the space for dns answer
	*(unsigned short *)((pskb->data) + (iph->ihl<<2) + UDP_HEADER_LEN + OFFSET_DNS_FLAG) = htons(0x8180);
	*(unsigned short *)((pskb->data) + (iph->ihl<<2) + UDP_HEADER_LEN + OFFSET_DNS_ANSWER) = htons(0x0001);
	*((unsigned short *)(dnsAnswer + OFFSET_DNS_ANSWER_NAME)) = htons(0xC00C);
	*((unsigned short *)(dnsAnswer + OFFSET_DNS_ANSWER_TYPE)) = htons(0x0001);
	*((unsigned short *)(dnsAnswer + OFFSET_DNS_ANSWER_CLASS)) = htons(0x0001);
	*((unsigned long *)(dnsAnswer + OFFSET_DNS_ANSWER_TTL)) = 0; 
	*((unsigned short *)(dnsAnswer + OFFSET_DNS_ANSWER_DATALEN)) = htons(4); //length of IPv4 address
//	*((unsigned int *)(dnsAnswer + OFFSET_DNS_ANSWER_IP)) = pindev->ifa_list->ifa_address;

	if (is_local_url == 1) {
		*((unsigned int *)(dnsAnswer + OFFSET_DNS_ANSWER_IP)) = pindev->ifa_list->ifa_address;
	} else {
		*((unsigned int *)(dnsAnswer + OFFSET_DNS_ANSWER_IP)) = in_aton("1.1.1.1");
	}
	//Update the UDP header
	uh->len = htons(ntohs(uh->len) + DNS_ANSWER_LEN);
	tmpUdp = uh->source;
	uh->source = uh->dest;
	uh->dest = tmpUdp;
	uh->check = 0;  //ignore the chksum

	//Update the IP header
	iph->tot_len = htons(ntohs(iph->tot_len) + DNS_ANSWER_LEN);
	tmpIp = iph->saddr;
	iph->saddr = iph->daddr;
	iph->daddr = tmpIp;

	//Update the MAC header
	memcpy(tmpMac, eth_hdr(pskb)->h_source, 6);
	memcpy(eth_hdr(pskb)->h_source, p->dev->dev_addr, 6);
	memcpy(eth_hdr(pskb)->h_dest, tmpMac, 6);
	skb_push(pskb, ETH_HLEN);

	pskb->data_len = pskb->len + ETH_HLEN + DNS_ANSWER_LEN;
	iph = ip_hdr(pskb);
	iph->check = 0;
	iph->check = br_compute_ip_checksum((unsigned short*)iph, iph->ihl<<2);
	
	//Send dns reply
	NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_OUT, pskb, NULL, pskb->dev,
		dev_queue_xmit);
}

//Detect to the DNS query request if is for query to the predefined URL.
int br_rdu_chk_dnsqry(struct net_bridge_port *p, struct sk_buff *pskb)
{
	struct sk_buff *clonedSkb = NULL;
	const unsigned short protoId = ntohs(eth_hdr(pskb)->h_proto);
	struct iphdr *iph = ip_hdr(pskb);
	struct udphdr *uh = (struct udphdr*)(pskb->data+(iph->ihl<<2));

	unsigned short idx = 0, cnt = 0, tmpLen = 0;
	unsigned short dataLen = pskb->len - (iph->ihl<<2) /*IP header len in IP header*/ - UDP_HEADER_LEN;
	unsigned char hdrOffset = (iph->ihl<<2) /*IP header len in IP header*/ + UDP_HEADER_LEN;
	unsigned short dnsFlags = ntohs(*(unsigned short  *)((pskb->data) + hdrOffset + 2));
	unsigned short questionType = ntohs(*(unsigned short  *)((pskb->data) + pskb->len - 4));
	struct net_device *pdev=NULL;
	struct in_device *pindev=NULL;	
	int is_local_url=0;
	
	if( protoId != 0x0800/*IP*/ || 
		iph->protocol != 0x11/*UDP*/ || 
			ntohs(uh->dest) != 0x0035 /*Port 53 for DNS*/ ){
		;//Do nothing if this is not a DNS query packet
	}else{
		if (!(dnsFlags & 0x8000) && !(dnsFlags & 0x7800) 
			&& questionType == 0x0001 /*A Record*/ ){
			dataLen = dataLen - 4 - 12; //Take out the type and class in query and take out header of dns request
			hdrOffset = hdrOffset + 12; //Skip the header of dns request

			idx = 0;
			while ( idx < dataLen ) {
				if ( idx == 0 || tmpLen == 0 ){
					tmpLen = pskb->data[idx+hdrOffset];
					if (tmpLen == 0){
						break;
					}else if( idx != 0 ) {
						if(cnt == localUrl.len){
							//the counter for compared characters had reach the maximum lenght of predefined url
							//But still has some characters in dns query request not be process,
							//So the lenght is exceed the length of predefined url, and it is not one we care
							
							//printk("Queried domain name exceed the length of predefined url\n");
							cnt = 100;
							break;
						}
						if (localUrl.url[cnt] != '.') {
							//printk("The DNS query requested is not for predefined url - 1\n"); //for debug
							cnt = 100;
							break;
						}						
						cnt++;
					}
				}

				idx++;

				if( tmpLen != 0 ){
					if(cnt == localUrl.len){
						//the counter for compared characters had reach the maximum lenght of predefined url
						//But still has some characters in dns query request not be process,
						//So the lenght is exceed the length of predefined url, and it is not one we care
					
						//printk("Queried domain name exceed the length of predefined url\n"); //for debug
						cnt = 100;
						break;
					}
					if (pskb->data[idx+hdrOffset] != localUrl.url[cnt] && 
						/*Ignore the difference for uppercase and lowercase*/
						pskb->data[idx+hdrOffset] != localUrl.url[cnt] + 32 && 
						pskb->data[idx+hdrOffset] != localUrl.url[cnt] - 32) {
						//printk("The DNS query requested is not for predefined url - 2\n"); //for debug
						cnt = 100;
						break;
					}
					cnt++;
					tmpLen--;
					if(tmpLen == 0){
						idx++;
					}
				}
			}
			
			if ( cnt == localUrl.len ) {
				is_local_url = 1;
			} 
			//clonedSkb = skb_clone(pskb, GFP_ATOMIC);
			//For the the IP address for the queried address of DNS response
			//2010.11.08 Joan.Huang modify , the caller(dev_get_by_name & in_dev_get ) must use dev_put() to release it
			pdev = dev_get_by_name(sock_net(pskb->sk),"br0");
			if (!pdev){
				return -EINVAL;
			}else{
				pindev = in_dev_get(pdev);
				br_rdu_reply_dnsqry(p, pskb,pindev, is_local_url);
				in_dev_put(pindev);
				dev_put(pdev);
			}
			//kfree_skb(clonedSkb);
			return 1;
			//	printk("The DNS query requested is not for predefined url - 3\n"); //for debug
		}
	}

	return 0;
}

