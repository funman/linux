#Oct 29, 2012--Modifications were made by U-Media Communication, inc.
#
# Makefile for the IEEE 802.1d ethernet bridging layer.
#

obj-$(CONFIG_BRIDGE) += bridge.o

# Ricky Cao: Added "br_umedia_predefine_url.o" for support to predefined URL function
bridge-y	:= br.o br_device.o br_fdb.o br_forward.o br_if.o br_input.o \
			br_ioctl.o br_notify.o br_stp.o br_stp_bpdu.o \
			br_stp_if.o br_stp_timer.o br_netlink.o \
			br_umedia_predefine_url.o br_umedia_wlan_manage.o \
			br_umedia_redirect_url.o 

bridge-$(CONFIG_SYSFS) += br_sysfs_if.o br_sysfs_br.o

bridge-$(CONFIG_BRIDGE_NETFILTER) += br_netfilter.o

bridge-$(CONFIG_BRIDGE_IGMP_SNOOPING) += br_multicast.o

obj-$(CONFIG_BRIDGE_NF_EBTABLES) += netfilter/
