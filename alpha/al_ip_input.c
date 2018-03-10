#include<rte_mbuf.h>
#include<rte_ip.h>
#include "alpha.h"
#include "al_mempool.h"
#include "al_log.h"
#include "al_ip.h"
#include "al_ip_input.h"
#include "al_session.h"
#include "al_tuple5.h"
#include "al_status.h"
#include "al_filter.h"
#include "al_to_api.h"
#include "al_service.h"
#include "al_dev.h"

void al_ipv4_input(struct rte_mbuf* buf, struct ipv4_hdr* hdr, void* nextptr, uint8_t from, uint8_t type){

    struct tcp_addr* tcpaddr;
    session_t* session;
    int rc, ret;
    al_fargs fargs;

    tcpaddr=(struct tcp_addr*)nextptr;
    session=tcp4_session_get(hdr->src_addr, tcpaddr->src_port, hdr->dst_addr, tcpaddr->dst_port, hdr->next_proto_id);
    if(likely(session != NULL)){
        al_process(session, buf, hdr, nextptr);
    }else{
        if(likely((from == AL_PKT_FROM_NIC) &&
                  ((rc = which_thread_v4(hdr->src_addr, hdr->dst_addr, tcpaddr->src_port, tcpaddr->dst_port, hdr->next_proto_id))!= -1))){
            //send to owner thread
            buf->udata64=type;
            if(unlikely((ret = rte_ring_enqueue_burst(g_thread_ring[rc][g_lcoreid], (void* const*)&buf, 1, NULL)) != 1)){
                SS_INCR(ip_send_to_dst_thread_failed);
                rte_pktmbuf_free(buf);
	        //LOG(LOG_ERROR, LAYOUT_CONF, "Send package to thread id %d failed, code: %d", rc, ret);
            }
            return;
        }else{
            rc=al_filter4_search(hdr->dst_addr, tcpaddr->dst_port, hdr->next_proto_id, &fargs);
            if( !rc ){
		LOG(LOG_TRACE, LAYOUT_CONF, "hit vserver id:%d servcie group:%d type:%d", fargs.fid, fargs.gid, fargs.ftype);
                al_service(&fargs, NULL, buf, hdr, nextptr);
            }else{

                if(likely(from == AL_PKT_FROM_NIC)){
                    send_to_switch(g_lcoreid, buf->port, &buf, 1, type == AL_PORT_TYPE_KNI  ?  0 : 1);    
                }else{
                    send_to_switch(g_lcoreid, buf->port, &buf, 1, buf->udata64 == AL_PORT_TYPE_KNI  ?  0 : 1);
                }

            }
        }
    }

}

void al_ipv6_input(struct rte_mbuf* buf, struct ipv6_hdr* hdr, void* nextptr, uint8_t from, uint8_t type){

    struct tcp_addr* tcpaddr;
    session_t* session;
    int rc;
    al_fargs fargs;    
	
    tcpaddr=(struct tcp_addr*)nextptr;
    session=tcp6_session_get(*(uint128_t*)hdr->src_addr, tcpaddr->src_port, *(uint128_t*)hdr->dst_addr, tcpaddr->src_port, hdr->proto);
    if(likely(session != NULL)){
        session->handler(session, buf, hdr, nextptr); 
    }else{
        if(likely((from == AL_PKT_FROM_NIC) &&
                  ((rc = which_thread_v6(*(uint128_t*)hdr->src_addr, *(uint128_t*)hdr->dst_addr, tcpaddr->src_port, tcpaddr->dst_port, hdr->proto))!= -1))){
            //send to owner thread
            buf->udata64=type;
            if(unlikely(rte_ring_enqueue_burst(g_thread_ring[rc][g_lcoreid], (void* const*)&buf, 1, NULL) != 1)){
                SS_INCR(ip_send_to_dst_thread_failed);
                rte_pktmbuf_free(buf);
            }
            return;
        }else{
            rc=al_filter6_search(*(uint128_t*)&hdr->dst_addr, tcpaddr->dst_port, hdr->proto, &fargs);
            if( !rc ){
		LOG(LOG_TRACE, LAYOUT_CONF, "hit vserver id:%d servcie group:%d type:%d", fargs.fid, fargs.gid, fargs.ftype);                
                al_service(&fargs, NULL, buf, hdr, nextptr);
            }else{
                if(likely(from == AL_PKT_FROM_NIC)){
                    send_to_switch(g_lcoreid, buf->port, &buf, 1, type == AL_PORT_TYPE_KNI  ?  0 : 1);    
                }else{
                    send_to_switch(g_lcoreid, buf->port, &buf, 1, buf->udata64 == AL_PORT_TYPE_KNI  ?  0 : 1);
                }
            }
        }
    }
}

