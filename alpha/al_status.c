#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <rte_memzone.h>
#include "al_common.h"
#include "al_status.h"
#include "al_to_api.h"

struct al_status* g_status;

int al_status_init(void){
    
    const struct rte_memzone* pool = rte_memzone_reserve_aligned("status", sizeof(struct al_status), SOCKET_ID_ANY, RTE_MEMZONE_SIZE_HINT_ONLY, CACHE_ALIGN);
    if( pool == NULL ){
        fprintf(stderr, "Allocate status memory failed\n");
        return -1;
    }else{
        g_status=(struct al_status*)pool->addr;
        memset(g_status, '\0', sizeof(struct al_status));
    }
    
    return 0;
}

void al_output_status(FILE* fp){
    fprintf(fp, "Switch:\n");
    fprintf(fp, "  drop with src or dst addr zero: %lu\n", SS_VALUE(sw_drop_zero_sord));
    fprintf(fp, "  drop tag frame with access:%lu\n", SS_VALUE(sw_drop_with_vlan_tag_pkg));
    fprintf(fp, "  drop with src or dst addr zero: %lu\n", SS_VALUE(sw_drop_vlan_not_exist));
    fprintf(fp, "  drop frame with trunk not permit:%lu\n", SS_VALUE(sw_drop_trunk_not_permit));
    fprintf(fp, "  drop frame with hybird not permit:%lu\n", SS_VALUE(sw_drop_hybird_not_permit));
    fprintf(fp, "  clone pkg failed:%lu\n",                 SS_VALUE(sw_drop_clone_pkg_failed));
    fprintf(fp, "  drop  with port not found:%lu\n",        SS_VALUE(sw_drop_port_no_found));
    fprintf(fp, "  drop with invalid port type:%lu\n",      SS_VALUE(sw_drop_invalid_port_type));

    fprintf(fp, "\nLink layer\n");
    fprintf(fp, "  send to switch failed:%lu\n", SS_VALUE(ll_send_to_switch_failed));
    fprintf(fp, "  send to kni failed:%lu\n", SS_VALUE(ll_send_to_kni_failed));
    fprintf(fp, "  send to kni queue failed:%lu\n", SS_VALUE(ll_send_to_kni_queue_failed));
    fprintf(fp, "  vlan info not be triped:%lu\n",  SS_VALUE(ll_have_vlan_info));
    fprintf(fp, "  Bad pkg len:%lu\n", SS_VALUE(ll_bad_pkg_len));
    fprintf(fp, "  unsurport icmp type:%lu\n", SS_VALUE(ll_drop_others_icmp));
    fprintf(fp, "  unknow internet proto:%lu\n", SS_VALUE(ll_unknown_proto));
    fprintf(fp, "  not implement rarp:%lu\n", SS_VALUE(ll_not_implement_rarp));
    fprintf(fp, "  frame type unknow:%lu\n", SS_VALUE(ll_frame_type_unknown));

    fprintf(fp, "\nIP layer\n");
    fprintf(fp, "  distribute to thread failed:%lu\n", SS_VALUE(ip_send_to_dst_thread_failed));
    fprintf(fp, "  no service:%lu\n", SS_VALUE(ip_drop_no_service));
    fprintf(fp, "  ip conflict:%lu\n", SS_VALUE(ip_conflict));

    fprintf(fp, "\nArp \n");
    fprintf(fp, "  arp search failed:%lu\n", SS_VALUE(arp_search_failed));
    fprintf(fp, "  arp not for us:%lu\n", SS_VALUE(arp_not_for_us));
    fprintf(fp, "  arp unsurport opt code:%lu\n", SS_VALUE(arp_unsurport_opt_code));

    fprintf(fp, "\nICMP \n");
    fprintf(fp, "  icmp not for us:%lu\n", SS_VALUE(icmp_not_for_us));
    fprintf(fp, "  icmp not router:%lu\n", SS_VALUE(icmp_router_to_resp));

    fprintf(fp, "\nCreate session\n");
    fprintf(fp, "  create session failed:%lu\n", SS_VALUE(create_session_failed));

    fprintf(fp, "\nRouter\n");
    fprintf(fp, "  Router miss:%lu\n", SS_VALUE(route_miss));    

}

