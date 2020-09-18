obj-$(CONFIG_RAETH) += raeth.obj
raeth-objs := raether.obj ra_mac.obj mii_mgr.obj ra_rfrw.obj

#EXTRA_CFLAGS   += -DCONFIG_RAETH_MULTIPLE_RX_RING

#Dias Kuo, Add Realtek RTL8367RB gigabit switch support
EXTRA_CFLAGS += -I. -I../lib/include/
EXTRA_CFLAGS += -DMDC_MDIO_OPERATION
EXTRA_CFLAGS += -D_LITTLE_ENDIAN
EXTRA_CFLAGS += -DCHIP_RTL8367RB

raeth-objs := u-media_rtk_api.o \
	raether.o \
        ra_mac.o \
        mii_mgr.o \
        ra_rfrw.o

ifeq ($(CONFIG_RAETH_QOS),y)
#raeth-objs += ra_qos.o
endif

ifeq ($(CONFIG_ETHTOOL),y)
ifeq ($(CONFIG_RAETH_ROUTER),y)
#raeth-objs += ra_ethtool.o
endif
endif

ifeq ($(CONFIG_RALINK_RT3052_MP2),y)
#raeth-objs += mcast.o
endif

ifeq ($(CONFIG_RAETH_NETLINK),y)
#raeth-objs += ra_netlink.o
endif

ifeq ($(CONFIG_RAETH_NAPI),y)
EXTRA_CFLAGS   += -DCONFIG_RAETH_NAPI 
endif

ifeq ($(CONFIG_RAETH_SKB_RECYCLE),y)
EXTRA_CFLAGS   += -DCONFIG_RAETH_SKB_RECYCLE
endif

ifeq ($(CONFIG_RALINK_BRIDGING_ONLY),y)
EXTRA_CFLAGS   += -DCONFIG_RALINK_BRIDGING_ONLY
endif

ifeq ($(CONFIG_RALINK_ENHANCE),y)
EXTRA_CFLAGS += -DCONFIG_RALINK_ENHANCE
endif

ifeq ($(CONFIG_RAETH_GMAC2),y)
EXTRA_CFLAGS += -DCONFIG_PSEUDO_SUPPORT
endif

ifeq ($(CONFIG_RAETH_JUMBOFRAME),y)
EXTRA_CFLAGS += -DCONFIG_RAETH_JUMBOFRAME
endif

ifeq ($(CONFIG_RT2860V2_AP_MEMORY_OPTIMIZATION),y)
EXTRA_CFLAGS   += -DMEMORY_OPTIMIZATION
endif

ifeq ($(CONFIG_RA_NETWORK_WORKQUEUE_BH),y)
EXTRA_CFLAGS   += -DWORKQUEUE_BH
endif

ifeq ($(CONFIG_RAETH_TSO),y)
EXTRA_CFLAGS   += -DCONFIG_RAETH_HW_VLAN_TX
endif
