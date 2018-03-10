#ifndef _AL_STATUS_H_
#define _AL_STATUS_H_


#define AL_SS_DEFINE(name)  uint64_t name;

struct al_status{
    AL_SS_DEFINE(sw_drop_zero_sord)
    AL_SS_DEFINE(sw_drop_with_vlan_tag_pkg)
    AL_SS_DEFINE(sw_drop_vlan_not_exist)
    AL_SS_DEFINE(sw_drop_trunk_not_permit)
    AL_SS_DEFINE(sw_drop_hybird_not_permit)
    AL_SS_DEFINE(sw_drop_clone_pkg_failed)
    AL_SS_DEFINE(sw_drop_port_no_found)
    AL_SS_DEFINE(sw_drop_invalid_port_type)
    AL_SS_DEFINE(ll_send_to_switch_failed)
    AL_SS_DEFINE(ll_send_to_kni_failed)
    AL_SS_DEFINE(ll_send_to_kni_queue_failed)
    AL_SS_DEFINE(ll_have_vlan_info)
    AL_SS_DEFINE(ll_bad_pkg_len)
    AL_SS_DEFINE(ll_drop_others_icmp)
    AL_SS_DEFINE(ll_unknown_proto)
    AL_SS_DEFINE(ll_not_implement_rarp)
    AL_SS_DEFINE(ll_frame_type_unknown)
    AL_SS_DEFINE(ip_send_to_dst_thread_failed)
    AL_SS_DEFINE(ip_drop_no_service)
    AL_SS_DEFINE(ip_conflict)    
    AL_SS_DEFINE(arp_search_failed)
    AL_SS_DEFINE(ndp_search_failed)    
    AL_SS_DEFINE(arp_not_for_us)
    AL_SS_DEFINE(arp_unsurport_opt_code)
    AL_SS_DEFINE(icmp_not_for_us)
    AL_SS_DEFINE(icmp_router_to_resp)
    /*create session*/
    AL_SS_DEFINE(create_session_failed)
    /*route*/
    AL_SS_DEFINE(route_miss)
    /*ICMP6*/
}__attribute__((__packed__));

extern struct al_status* g_status;

#define SS_INCR(name) __sync_add_and_fetch(&g_status->name, 1)

#define SS_VALUE(key) g_status->key

int al_status_init(void);

#endif /*_AL_STATUS_H_*/

