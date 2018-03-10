#include <rte_byteorder.h>
#include <rte_ring.h>
#include <rte_kni.h>
#include <rte_ethdev.h>
#include "alpha.h"
#include "al_log.h"
#include "al_dev.h"
#include "al_vport.h"
#include "al_hash.h"
#include "al_common.h"
#include "al_to_api.h"
#include "al_mempool.h"
#include "al_ip_input.h"
#include "al_status.h"
#include "al_pktdump.h"

al_dev  g_ports_ary[MAX_ETHER_COUTER];
al_dev  g_kni_ary[MAX_ETHER_COUTER];
al_dev  g_vport_ary[MAX_ETHER_COUTER];


static inline uint16_t port_burst_write(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, uint8_t type);
static inline uint16_t port_burst_read(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, uint8_t type);
static inline uint16_t kni_burst_write(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, uint8_t type);
static inline uint16_t kni_burst_read(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, uint8_t type);
static inline uint16_t vport_burst_read(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, uint8_t type);
static inline uint16_t vport_burst_write(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, uint8_t type);

int al_dev_init(void){
    memset(g_ports_ary, '\0', sizeof(g_ports_ary));
    memset(g_kni_ary, '\0', sizeof(g_kni_ary));
    memset(g_vport_ary, '\0', sizeof(g_vport_ary));
    return 0;
}

int init_port_dev(al_dev* ports, uint8_t type){

    int t,l;
    char ringname[MAX_NAMELEN];
	
    if( type == AL_PORT_TYPE_KNI ){
        ports->read=kni_burst_read;
        ports->write=kni_burst_write;
        t=ports->qcnt;
        sprintf(ringname, "KNI");
    }else if( type == AL_PORT_TYPE_VPORT ){
        ports->read=vport_burst_read;
        ports->write=vport_burst_write;
        vport_init(ports->port);
        t=0;
        sprintf(ringname, "VPORT");
    }else{
        ports->read=port_burst_read;
        ports->write=port_burst_write;
        t=ports->qcnt;
        sprintf(ringname, "PHYSIC");
    }
    ports->dev_type = type;
    ports->is_pcap = 0;

    l=strlen(ringname);
    for(; t<g_work_thread_count; t++){
        sprintf(ringname+l, "-ring-%d-%d", ports->port, t );
        ports->out[t]=rte_ring_create(ringname,RTE_AL_TX_DESC_DEFAULT, SOCKET_ID_ANY, RING_F_SP_ENQ | RING_F_SC_DEQ);
        if( ports->out[t] == NULL ){
            LOG(LOG_ERROR, LAYOUT_CONF, "Create ring failed, msg:%s\n", strerror(errno));
            return -1;
        }
    }

    fprintf(stderr, "Port:%u type:%u pysical queue:%u logical queue:%d\n", ports->port, type, ports->qcnt, t-ports->qcnt);
    return 0;
}

static inline uint16_t port_burst_read(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, uint8_t type){
    uint16_t rc = rte_eth_rx_burst(portid, lcore, buf, cnt);
    if( rc ){
        if( unlikely(g_ports_ary[portid].is_pcap) ){
            al_output(portid, AL_PORT_TYPE_PHYSICAL, buf, rc);    
        }
        vport_burst_write(g_lcoreid, portid, buf, rc, type);
    }
    return 0;
}

static inline uint16_t port_burst_write(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, uint8_t type){

    unsigned int rc, k;
    uint16_t rcnt;
    int q;
    struct rte_mbuf*     mbuf[RTE_AL_TX_DESC_DEFAULT];
    al_dev* 		     port=&g_ports_ary[portid];

    if( unlikely(g_ports_ary[portid].is_pcap) ){
        if( cnt ){
            al_output(portid, AL_PORT_TYPE_PHYSICAL, buf, cnt);
        }
    }

    if( lcore < g_ports_ary[portid].qcnt  ){

        q=lcore;
        for( q+=port->qcnt; q < g_work_thread_count; q+=port->qcnt ){

            rcnt=rte_ring_dequeue_burst(port->out[q], (void**)mbuf, RTE_AL_TX_DESC_DEFAULT, NULL);
            if( rcnt == 0 ){
                continue;
            }

            k = rte_eth_tx_burst(portid, lcore, mbuf, rcnt);
            for( ; k<rcnt; k++ ){
                rte_pktmbuf_free(mbuf[k]);
            }
        }

        rc=rte_eth_tx_burst(portid, lcore, buf, cnt);
        for( k=rc; k<cnt; k++ ){
            rte_pktmbuf_free(buf[k]);
        }
 
    }else{
        k=rte_ring_enqueue_burst(port->out[lcore], (void* const *)buf, cnt, NULL);
        for( ; k<cnt; k++ ){
            rte_pktmbuf_free(buf[k]);
        }
    }
    
    return 0;
}

static inline uint16_t vport_burst_read(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, uint8_t type){
    uint16_t rc=rte_ring_dequeue_burst(g_vport_ary[portid].out[lcore], (void**)buf, cnt, NULL);
    if( rc ){
        if( unlikely(g_vport_ary[portid].is_pcap) ){
            al_output(portid, AL_PORT_TYPE_VPORT, buf, rc);
        }
        port_burst_write(g_lcoreid, portid, buf, rc, type);
    }
    return 0;
}

inline uint16_t send_to_switch(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, int to_kni){
    int i, rc;

#if 0
    struct ether_hdr* eth=rte_pktmbuf_mtod(buf[0], struct ether_hdr *);
    LOG(LOG_DEBUG2, LAYOUT_DATALINK, "To Lcoreid %d port %u src:"ETHER_ADDR_FORMART" dst:"ETHER_ADDR_FORMART, lcore, portid,  ETHER_ADDR(&eth->s_addr), ETHER_ADDR(&eth->d_addr));
#endif

    if( likely(!to_kni) ){
        rc=rte_ring_enqueue_burst(g_vport_ary[portid].out[lcore], (void**)buf, cnt, NULL);
        for(i=rc; i<cnt; i++){
            SS_INCR(ll_send_to_switch_failed);
            rte_pktmbuf_free(buf[i]);
        }
    }else{
        kni_burst_write(lcore, portid, buf, cnt, AL_PORT_TYPE_VPORT);
    }
    return 0;
}

static inline uint16_t vport_burst_write(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, uint8_t type){

    struct rte_mbuf* rte[RTE_AL_RX_DESC_DEFAULT];
    int rc;
    int k;

    if( unlikely(g_vport_ary[portid].is_pcap) ){
        al_output(portid, AL_PORT_TYPE_VPORT, buf, cnt);
    }

    for(k=0; k<g_work_thread_count; k++){
        if(unlikely(g_thread_ring[lcore][k] == NULL)){
            continue;
        }
        
        rc=rte_ring_dequeue_burst(g_thread_ring[lcore][k], (void**)rte, RTE_AL_RX_DESC_DEFAULT, NULL);
        if( likely(rc > 0) ){
            al_main_process(lcore, 0, rte, rc, AL_PKT_FROM_QUEUE, type);
        }
    }

    al_main_process(lcore, portid, buf, cnt, AL_PKT_FROM_NIC, type);
	
    return 0;
}

static inline uint16_t kni_burst_read(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, uint8_t type){
    rte_kni_handle_request(g_kni_ary[portid].dev.kni.kni);
    uint16_t rc=rte_kni_rx_burst(g_kni_ary[portid].dev.kni.kni, buf, cnt);
    if( rc ){
        if( unlikely(g_kni_ary[portid].is_pcap) ){
            al_output(portid, AL_PORT_TYPE_KNI, buf, rc);
        }
        vport_burst_write(g_lcoreid, portid, buf, rc, type);
    }
    return 0;
}

static inline uint16_t kni_burst_write(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, uint8_t type){

    unsigned int rc, k;
    int q;
    uint32_t rcnt;
    struct rte_mbuf* mbuf[RTE_AL_TX_DESC_DEFAULT];
    al_dev*          port=&g_kni_ary[portid];
    int qcnt  =  port->qcnt;

    if( unlikely(g_kni_ary[portid].is_pcap) ){
        if(cnt){
            al_output(portid, AL_PORT_TYPE_KNI, buf, cnt);
        }
    }
    
    if( lcore < qcnt  ){

        q=lcore;
        for( q+=qcnt; q<g_work_thread_count; q+=qcnt ){
            rcnt=rte_ring_dequeue_burst(port->out[q], (void**)mbuf, RTE_AL_TX_DESC_DEFAULT, NULL);
            if( rcnt == 0 ){
                continue;
            }
		
            k = rte_kni_tx_burst(port->dev.kni.kni, mbuf, rcnt);
            for( ; k<rcnt; k++ ){
                SS_INCR(ll_send_to_kni_failed);
                rte_pktmbuf_free(mbuf[k]);
            }
        }

        rc=rte_kni_tx_burst(port->dev.kni.kni, buf, cnt);
        for( k=rc; k<cnt; k++ ){
            SS_INCR(ll_send_to_kni_failed);
            rte_pktmbuf_free(buf[k]);
        }        

    }else{
        k=rte_ring_enqueue_burst(port->out[lcore], (void* const *)buf, cnt, NULL);
        for( ; k<cnt; k++ ){
            SS_INCR(ll_send_to_kni_queue_failed);
            rte_pktmbuf_free(buf[k]);
        }
        return 0;
    }

    return 0;
}

void al_pcap_set(int id, int duration, int type){
    int k;

    if( duration == -1 ){
      duration=300;
    }

    if( id != -1 ){
      if( type != -1 ){
	if( type == AL_PORT_TYPE_KNI ){
	  al_set_pcap_duration(id, AL_PORT_TYPE_KNI, duration);
	  g_kni_ary[id].is_pcap =1;
	}else if( type == AL_PORT_TYPE_VPORT ){
	  al_set_pcap_duration(id, AL_PORT_TYPE_VPORT, duration);
	  g_vport_ary[id].is_pcap =1;
	}else{
	  al_set_pcap_duration(id, AL_PORT_TYPE_PHYSICAL, duration);
	  g_ports_ary[id].is_pcap =1;
	}
      }else{
	al_set_pcap_duration(id, AL_PORT_TYPE_PHYSICAL, duration);
	g_ports_ary[id].is_pcap = 1;
	al_set_pcap_duration(id, AL_PORT_TYPE_KNI, duration);
	g_kni_ary[id].is_pcap = 1;
	al_set_pcap_duration(id, AL_PORT_TYPE_VPORT, duration);
	g_vport_ary[id].is_pcap = 1;
      }
    }else{
      if( type == -1 ){
        for(k=0; k<g_sys_port_counter; k++){
            al_set_pcap_duration(k, AL_PORT_TYPE_PHYSICAL, duration);
            g_ports_ary[k].is_pcap = 1;
            al_set_pcap_duration(k, AL_PORT_TYPE_KNI, duration);
            g_kni_ary[k].is_pcap = 1;
            al_set_pcap_duration(k, AL_PORT_TYPE_VPORT, duration);
            g_vport_ary[k].is_pcap = 1;
        }
      }else{
        for(k=0; k<g_sys_port_counter; k++){
	  al_set_pcap_duration(k, type, duration);
	  g_ports_ary[k].is_pcap = 1;
        }
      }
    }
}

void al_finished_pcap(uint8_t id, int type){
    if( type == AL_PORT_TYPE_KNI ){
        g_kni_ary[id].is_pcap=0;
    }else if( type == AL_PORT_TYPE_VPORT ){
        g_vport_ary[id].is_pcap=0;
    }else{
        g_ports_ary[id].is_pcap=0;
    }
}

int al_dev_internal_id(int id){
    int k;
    for(k=0; k<g_sys_port_counter; k++){
        if( g_ports_ary[k].id == id ){
            return k;
        }
    }
    return -1;
}

