#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_arp.h>
#include <rte_memzone.h>
#include <rte_spinlock.h>
#include <rte_errno.h>
#include <rte_kni.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rte_ethdev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "al_mempool.h"
#include "alpha.h"
#include "al_common.h"
#include "al_vport.h"
#include "common/al_nic.h"
#include "al_to_api.h"
#include "al_dev.h"
#include "al_session.h"
#include "al_tuple5.h"
#include "al_ip.h"
#include "al_arp.h"
#include "al_router.h"
#include "al_filter.h"
#include "al_group.h"
#include "al_member.h"
#include "al_log.h"
#include "al_status.h"
#include "common/al_buf.h"
#include "al_geoip.h"
#include "al_dns.h"
#include "al_to_main.h"


#define MAX_PACKET_SZ			2048

int g_work_thread_count = 0;

int g_sys_port_counter=0;

static struct rte_eth_conf port_conf =
{
    .rxmode = {
        .mq_mode	= ETH_MQ_RX_RSS,
        .max_rx_pkt_len = ETHER_MAX_LEN,
        .split_hdr_size = 0,
        .header_split   = 0, /**< Header Split disabled */
        .hw_ip_checksum = 1, /**< IP checksum offload enabled */
        .hw_vlan_filter = 0, /**< VLAN filtering disabled */
        .hw_vlan_strip  = 1,
        .jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
        .hw_strip_crc   = 1, /**< CRC stripped by hardware */
    },
    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = NULL,
            .rss_hf = ETH_RSS_IP | ETH_RSS_UDP |
            ETH_RSS_TCP | ETH_RSS_SCTP,
        },
    },
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
    }
};

static struct rte_eth_rxconf rx_conf = {
    
    .rx_thresh = {
        .pthresh = 1,
        .hthresh = 1,
        .wthresh = 4,
    },
    .rx_free_thresh = 4,
};

static struct rte_eth_txconf tx_conf = {  
    .tx_thresh = {
        .pthresh = 1,
        .hthresh = 1,
        .wthresh = 0,
    },
};


const struct rte_memzone     *g_queue_pool=NULL;
static int al_kni_config_network_interface(uint8_t port_id, uint8_t if_up);
static int al_kni_change_mtu(uint8_t port_id, unsigned new_mtu);
static int al_init_port();

int al_init_dpdk(int argc, char *argv[])
{

    int i, k, j, rc, last=0;
    char hthread_name[32];

    if ((last=rte_eal_init(argc, argv)) < 0) {
        fprintf(stderr, "Init dpdk env failed");
        return (-1);
    }

    last = 0;

    if(init_log() != 0){
        return -1;
    }

    if(al_mempool_init() != 0){
        fprintf(stderr, "Allocate memory pool failed\n");
        return -1;
    }

    if(al_dev_init() != 0){
        fprintf(stderr, "Init ring dev failed\n");
        return -1;
    }

    if( session_init() != 0 ){
        return -1;
    } 

    if(port_init() != 0 ){
        return -1;
    }

    if( arp_init() != 0 ){
        return -1;
    }

    if( ip_pool_init() != 0 ){
        return -1;
    }

    if( al_router_init() ){
        return -1;
    }

    if(al_filter_init() != 0){
        return -1;
    }

    if(al_members_init() != 0){
        return -1;
    }

    if(al_status_init() != 0){
        return -1;
    }

    if(al_geoip_init() != 0){
        return -1;
    }

    if(al_dns_init() != 0 ){
        return -1;
    }

    /*init dev queue*/
    g_queue_pool = rte_memzone_reserve_aligned("g_queue_pool", MAX_QUEUE_CNT*MAX_QUEUE_PEER_NIC*sizeof(al_dev_queue), SOCKET_ID_ANY, RTE_MEMZONE_SIZE_HINT_ONLY, CACHE_ALIGN);
    if( g_queue_pool == NULL ){
        fprintf(stderr, "Allocate g_queue_pool failed\n");
        return -1;
    }else{
        g_max_queue_counter=0;
    }

    g_work_thread_count=0;
    rc=rte_get_master_lcore();
    for (i = 0, k=0; i < RTE_MAX_LCORE; i++)
    {
        g_thread_info[i].oss=0;
        g_thread_info[i].started=0;
        g_thread_info[i].start_lp=0;
        if ((rte_lcore_is_enabled(i) == 0) || (rc == i)) {
            if( rc == i ){
                g_thread_info[i].used=1;
            }else{
                g_thread_info[i].used=0;
            }
            continue;
        }else{

            g_work_thread_count++;
            g_thread_info[i].used=1;
            g_thread_info[i].proc = al_work_proc;

            g_thread_info[i].lcoreid = k++;
        }
    }

    for(i=0, j=0; j<g_work_thread_count; i++){
        if( i == rc ){
            continue;
        }

        if(g_thread_info[i].used){
            j++;
            for(k=0; k<g_work_thread_count; k++){
                if(g_thread_info[i].lcoreid==k){
                    g_thread_ring[k][k]=NULL;
                }else{
                    sprintf(hthread_name, "thread_%d_%d", i, k);
                    g_thread_ring[g_thread_info[i].lcoreid][k]=rte_ring_create(hthread_name, RTE_AL_RX_DESC_DEFAULT, SOCKET_ID_ANY, RING_F_SC_DEQ | RING_F_SP_ENQ);
                    if( g_thread_ring[g_thread_info[i].lcoreid][k] == NULL ){
                        return -1;
                    }
                }
            }
        }
    }

    //set master
    fprintf(stderr, "Master core is:%d\n", rc);
    g_thread_info[rc].proc=al_master_process;
    g_thread_info[rc].lcoreid = k;

    al_init_port();
    if( g_sys_port_counter == 0){
        fprintf(stderr, "Init physical port failed\n");
        return -1;
    }

    return last;
}

static int al_init_port(){

    struct rte_eth_dev_info dev_info;
    struct dev_queue* qptr;
    char ifname[32];
    char ifmac[32];
    struct rte_kni_conf kni_conf;
    struct rte_kni_ops ops;
    struct rte_kni*    kni;
    int i, k, nb_ports, rc;
    int16_t pid;
    al_dev* dev;
    vport_t* vport;

    qptr=g_queue_pool->addr;

    nb_ports = rte_eth_dev_count();
    rte_kni_init(nb_ports);

    g_sys_port_counter=0;
    
    for (i = 0; i < nb_ports; i++)
    {
        rte_eth_dev_info_get(i, &dev_info);
        dev = &g_ports_ary[i];
        dev->qcnt = dev_info.max_rx_queues;
        dev->port = i;
        pid = al_get_port_id(dev_info.pci_dev->addr.domain, dev_info.pci_dev->addr.bus, dev_info.pci_dev->addr.devid, dev_info.pci_dev->addr.function);
        if( pid < 0 ){
            return -1;
        }else{
            dev->id = pid;
        }

        rc = rte_eth_dev_configure(i, dev->qcnt, dev->qcnt, &port_conf);
        if (rc < 0) {
            fprintf(stderr, "Cannot configure device: err=%d, port=%d\n", rc, i);
            return -1;
        }
	
        for (k = 0; k < g_ports_ary[i].qcnt; k++)
        {
            rc = rte_eth_rx_queue_setup(i, k, RTE_AL_RX_DESC_DEFAULT, SOCKET_ID_ANY, &rx_conf, al_pktmbuf_pool);
            if (rc < 0) {
                fprintf(stderr, "rte_eth_rx_queue_setup: err=%d, port=%d\n", rc, i);
                return -1;
            }else{
                qptr->portid=i;
                qptr->queueid=k;
                qptr->type=AL_PORT_TYPE_PHYSICAL;
                qptr->valid = DEV_VALID;
                qptr->port = dev;
                fprintf(stdout, "port id:%d qid:%d\n", qptr->portid, qptr->queueid);
            }
	
            if( k < g_work_thread_count){
                rc = rte_eth_tx_queue_setup(i, k, RTE_AL_TX_DESC_DEFAULT, SOCKET_ID_ANY, &tx_conf);
                if (rc < 0) {
                    fprintf(stderr, "rte_eth_tx_queue_setup: err=%d, port=%d\n", rc, i);
                    return -1;
                }
            }
            qptr++;
        }

        rte_eth_promiscuous_enable(i);
		
        if(init_port_dev(dev, AL_PORT_TYPE_PHYSICAL) != 0){
            fprintf(stderr, "Init dev port failed\n");
            return -1;
        }
		
        rc = rte_eth_dev_start(i);
        if (rc < 0){
            fprintf(stderr, "Could not start port%u,%s\n", i, rte_strerror(rte_errno));
            return -1;
        }else{
            fprintf(stderr, "Port %d start success\n", i);
        }

        //init vport
        dev = &g_vport_ary[i];
        dev->port = i;
        dev->id=pid;
        dev->pvid = DEV_VALID;
        dev->qcnt = g_work_thread_count;

        for( rc=0; rc < g_work_thread_count; rc++ ){
            qptr->portid = i;
            qptr->queueid = rc;
            qptr->valid = 1;
            qptr->type = AL_PORT_TYPE_VPORT;
            qptr->port = dev;
            qptr++;
        }

        if(init_port_dev(dev, AL_PORT_TYPE_VPORT) != 0){
            fprintf(stderr, "Init dev port failed\n");
            return -1;
        }

        vport=&dev->dev.vport.port;
        vport->portid = dev->port;

        //init kni
        if((al_write_kni_nic_conf(pid, ifname, (uint8_t*)&ifmac)) < 0){
            return -1;
        }else{
            dev->dev.vport.port.haddr = *al_ether_aton((const char*)ifmac);
            memcpy(dev->dev.vport.port.ifname, ifname, ETHER_NAME_LEN);
            dev->dev.vport.port.ifname[strlen(ifname)]='\0';
        }

        memset(&kni_conf, 0, sizeof(kni_conf));
        kni_conf.group_id = g_ports_ary[i].port;
        kni_conf.core_id = (qptr - (struct dev_queue*)g_queue_pool->addr + 1)%g_work_thread_count+1;
        snprintf(kni_conf.name, RTE_KNI_NAMESIZE, ifname);
        kni_conf.mbuf_size = RTE_MBUF_DEFAULT_BUF_SIZE;//RTE_AL_TX_DESC_DEFAULT;
        kni_conf.force_bind = 1;

        if( i == 0 ){
     	   kni_conf.addr = dev_info.pci_dev->addr;
           kni_conf.id = dev_info.pci_dev->id;

           memset(&ops, 0, sizeof(ops));
           ops.port_id =  g_ports_ary[i].port;
           ops.change_mtu = al_kni_change_mtu;
           ops.config_network_if = al_kni_config_network_interface;

           kni = rte_kni_alloc(al_pktmbuf_pool, &kni_conf, &ops);     
        }else{
           kni = rte_kni_alloc(al_pktmbuf_pool, &kni_conf, NULL);
        }
       
        if (kni == NULL) {
            fprintf(stderr,"Kni allocate failed\n");
            return -1;
        }else{
            dev=&g_kni_ary[i];
            dev->dev.kni.kni=kni;
            dev->qcnt = 1;
            dev->port = i;
            dev->id = pid;
        }

        qptr->portid = i;
        qptr->queueid = 0;
        qptr->type = AL_PORT_TYPE_KNI;
        qptr->valid = DEV_VALID;
        qptr->port = dev;
        qptr++;

        if(init_port_dev(dev, AL_PORT_TYPE_KNI) != 0){
            fprintf(stderr, "Init dev port failed\n");
            return -1;
        }

        g_sys_port_counter++;
		
    }

    g_max_queue_counter = qptr - (struct dev_queue*)g_queue_pool->addr;

    return nb_ports;
}

/* Callback for request of changing MTU */
static int al_kni_change_mtu(uint8_t port_id, unsigned new_mtu)
{

#if 0
    int ret;
    struct rte_eth_conf conf;

    if (port_id >= rte_eth_dev_count()) {
        LOG(LOG_ERROR, LAYOUT_APP, "Invalid port id %d\n", port_id);
        return -EINVAL;
    }

    LOG(LOG_INFO, LAYOUT_APP, "Change MTU of port %d to %u\n", port_id, new_mtu);

    /* Stop specific port */

    rte_eth_dev_stop(port_id);

    memcpy(&conf, &port_conf, sizeof(conf));

    /* Set new MTU */

    if (new_mtu > ETHER_MAX_LEN)
        conf.rxmode.jumbo_frame = 1;
    else
        conf.rxmode.jumbo_frame = 0;

    /* mtu + length of header + length of FCS = max pkt length */

    conf.rxmode.max_rx_pkt_len = new_mtu + KNI_ENET_HEADER_SIZE + KNI_ENET_FCS_SIZE;

    ret = rte_eth_dev_configure(port_id, 1, 1, &conf);
    if (ret < 0) {
        LOG(LOG_ERROR, LAYOUT_APP, "Fail to reconfigure port %d\n", port_id);
        return ret;
    }

    /* Restart specific port */

    ret = rte_eth_dev_start(port_id);
    if (ret < 0) {
        LOG(LOG_ERROR, LAYOUT_APP, "Fail to restart port %d\n", port_id);
        return ret;
    }

    return 0;
    
    
    int ret;
    struct rte_eth_conf conf;

    /* Stop specific port */
    rte_eth_dev_stop(port_id);

    memcpy(&conf, &port_conf, sizeof(conf));
    /* Set new MTU */
    if (new_mtu > ETHER_MAX_LEN)
        conf.rxmode.jumbo_frame = 1;
    else
        conf.rxmode.jumbo_frame = 0;

    /* mtu + length of header + length of FCS = max pkt length */
    conf.rxmode.max_rx_pkt_len = new_mtu + KNI_ENET_HEADER_SIZE +
        KNI_ENET_FCS_SIZE;
    ret = rte_eth_dev_configure(port_id, 1, 1, &conf);
    if (ret < 0) {
        log_core(LOG_ERROR, LAYOUT_CONF, "Fail to reconfigure port %d", port_id);
        return ret;
    }

    /* Restart specific port */
    ret = rte_eth_dev_start(port_id);
    if (ret < 0) {
        log_core(LOG_ERROR, LAYOUT_CONF, "Fail to restart port %d\n", port_id);
        return ret;
    }
#endif
    return 0;
}

/* Callback for request of configuring network interface up/down */
static int al_kni_config_network_interface(uint8_t port_id, uint8_t if_up)
{
    int ret = 0;
#if 0

    if (if_up != 0) { /* Configure network interface up */
        rte_eth_dev_stop(port_id);
        ret = rte_eth_dev_start(port_id);
    } else /* Configure network interface down */
        rte_eth_dev_stop(port_id);

    if (ret < 0)
        log_core(LOG_ERROR, LAYOUT_CONF, "Failed to start port %d\n", port_id);
#endif

    return ret;
}

