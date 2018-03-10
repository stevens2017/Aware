abs_srcdir=$(ROOT_PATH)/alpha
enable_debug = yes 

include $(RTE_SDK)/mk/rte.vars.mk

APP = alpha

SRCS-y := alpha.c \
     al_mempool.c\
     al_vport.c \
     al_dev.c \
     al_dpdk.c \
     al_tuple5.c \
     al_router.c \
     al_log.c\
     al_ip.c \
     al_icmp.c \
     al_session.c \
     common/al_deamon.c \
     common/al_shmem.c \
     common/al_nic.c \
     statistics/statistics.c \
     al_pktdump.c \
     al_arp.c \
     al_master.c \
     al_worker.c \
     al_macport.c \
     al_main_process.c \
     al_mainloop.c \
     al_ip_input.c \
     al_member.c \
     al_service.c \
     al_time.c \
     al_status.c \
     al_chksum.c \
     al_error.c \
     al_deamon.c \
     al_load_configure.c \
     al_radix.c \
     al_radix_tree.c \
     al_slab.c \
     al_tree.c \
     al_filter.c \
     al_process.c \
     al_update_route.c \
     al_geoip.c \
     al_dns.c \
     al_gslb_process.c


EXTRA_CFLAGS += -Wall -I$(abs_srcdir)  -I$(ROOT_PATH)/include

#ifeq ($(enable_debug),yes)
EXTRA_CFLAGS += -DLWIP_DEBUG=1 -O0 -g -pg
#endif

EXTRA_LDFLAGS += -L$(ROOT_PATH)/library -rpath-link=$(ROOT_PATH)/library  -rpath-link=$(GCC_PATH)/lib64

EXTRA_LDLIBS += -lalapi

include $(RTE_SDK)/mk/rte.extapp.mk

