#include<rte_mbuf.h>
#include<rte_ether.h>
#include<rte_memzone.h>
#include<sys/prctl.h>
#include<rte_errno.h>
#include "alpha.h"
#include "al_common.h"
#include "al_vport.h"
#include "al_dev.h"
#include "al_pktdump.h"
#include "al_macport.h"
#include "al_mempool.h"
#include "common/bit.h"
#include "al_log.h"
#include "al_status.h"
#include "al_to_api.h"
	
int al_work_proc(void* args){

    struct thread_info* info=(struct thread_info*)args;
    uint8_t lcoreid=info->lcoreid;
	
    struct rte_mbuf *rte[RTE_AL_RX_DESC_DEFAULT];
    uint8_t i, k;
    al_dev* port;
    uint8_t            portid;
    struct dev_queue*  qptr_h = (struct dev_queue*)g_queue_pool->addr, *qptr;

    prctl(PR_SET_NAME, "Work");

#if 0
    int f;
    for(f=0; f<g_max_queue_counter; f++){
        qptr=qptr_h+f;
        LOG(LOG_INFO, LAYOUT_DPDK, "Enter work portid %d type:%d qid:%d", qptr->portid, qptr->type, qptr->queueid);  
    }
#endif
    
    LOG(LOG_INFO, LAYOUT_DPDK, "Enter work cycle:%u max queue:%d max thread:%d", lcoreid, g_max_queue_counter, g_work_thread_count);
    rte_prefetch0(g_ports_ary);
    rte_prefetch0(g_kni_ary);
    rte_prefetch0(g_vport_ary);
    while (g_sys_stop)
    {
        for(i=lcoreid; i<g_max_queue_counter; i+=g_work_thread_count){
            qptr=qptr_h+i;
            portid=qptr->portid;
            port=qptr->port;

            port->read(qptr->queueid, portid, rte, RTE_AL_RX_DESC_DEFAULT, qptr->type);
#if 0            
            LOG(LOG_INFO, LAYOUT_DPDK, "Lcore:%d Port:%d Queue:%d ", lcoreid, portid, qptr->queueid);
#endif
        }

        for(k=0; k<g_sys_port_counter; k++){
            port=&g_ports_ary[k];
            port->write(lcoreid, k, NULL, 0, AL_PORT_TYPE_PHYSICAL);
            port=&g_vport_ary[k];
            port->write(lcoreid, k, NULL, 0, AL_PORT_TYPE_VPORT);
            port=&g_kni_ary[k];
            port->write(lcoreid, k, NULL, 0, AL_PORT_TYPE_KNI);            
        }

    }

    return 0;
}


