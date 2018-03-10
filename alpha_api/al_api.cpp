#include <restbed>
#include <vector>
#include <cstring>
#include <iostream>
#include "al_api.h"
#include "al_api_impl.h"
#include "debug/al_dump.h"
#include "rest/al_router.h"
#include "rest/al_filter.h"
#include "rest/al_group.h"
#include "rest/al_member.h"
#include "rest/al_nic.h"
#include "rest/al_ip.h"
#include "rest/al_dns.h"
#include "rest/al_geoip.h"

void al_start_api(char* ip, int port){

    auto service = std::make_shared< restbed::Service >();
    
    al_api("/dump").setup<al_dump>(service, new al_api_get<al_dump>());
    al_api("/dump/arp").setup<al_dump_arp>(service, new al_api_get<al_dump_arp>());
    al_api("/dump/filter").setup<al_dump_filter>(service, new al_api_get<al_dump_filter>());
    al_api("/dump/member").setup<al_dump_member>(service, new al_api_get<al_dump_member>());    
//    al_api("/dump/mac").setup<al_dump_mac>(service, new al_api_get<al_dump_mac>());
    al_api("/dump/status").setup<al_dump_sys_status>(service, new al_api_get<al_dump_sys_status>());
    al_api("/dump/route").setup<al_dump_route>(service, new al_api_get<al_dump_route>());
    al_api("/dump/session").setup<al_dump_session>(service, new al_api_get<al_dump_session>());
    al_api("/dump/ip").setup<al_dump_ip>(service, new al_api_get<al_dump_ip>());
    al_api("/dump/geoip").setup<al_dump_geoip>(service, new al_api_get<al_dump_geoip>());
    al_api("/dump/dns").setup<al_dump_dns>(service, new al_api_get<al_dump_dns>());

    /*pcap*/
    al_api("/pcap").setup<al_pcap>(service, new al_api_get<al_pcap>());
    al_api("/pcap/duration/{duration:.*}").setup<al_pcap>(service, new al_api_get<al_pcap>());
    al_api("/pcap/id/{id:.*}").setup<al_pcap>(service, new al_api_get<al_pcap>());
    al_api("/pcap/id/{id:.*}/duration/{duration:.*}").setup<al_pcap>(service, new al_api_get<al_pcap>());
    al_api("/pcap/type/{type:.*}").setup<al_pcap>(service, new al_api_get<al_pcap>());
    al_api("/pcap/type/{type:.*}/duration/{duration:.*}").setup<al_pcap>(service, new al_api_get<al_pcap>());
    al_api("/pcap/id/{id:.*}/type/{type:.*}").setup<al_pcap>(service, new al_api_get<al_pcap>());
    al_api("/pcap/id/{id:.*}/type/{type:.*}/duration/{duration:.*}").setup<al_pcap>(service, new al_api_get<al_pcap>());
    
    al_api("/v1/filter").setup<api::al_filter_get, api::al_filter_add>(service, new al_api_get<api::al_filter_get>(), new al_api_post<api::al_filter_add>());
    al_api("/v1/filter/name/{name:.*}").setup<api::al_filter_get, api::al_filter_delete>(service, new al_api_get<api::al_filter_get>(), new al_api_delete<api::al_filter_delete>());
    al_api("/v1/filter/id/{id:.*}").setup<api::al_filter_get, api::al_filter_delete>(service, new al_api_get<api::al_filter_get>(), new al_api_delete<api::al_filter_delete>());


    al_api("/v1/group").setup<api::al_group_add, api::al_group_get>(service, new al_api_post<api::al_group_add>(), new al_api_get<api::al_group_get>());
    al_api("/v1/group/name/{name:.*}").setup<api::al_group_get, api::al_group_del>(service, new al_api_get<api::al_group_get>(), new al_api_delete<api::al_group_del>());
    al_api("/v1/group/id/{id:.*}").setup<api::al_group_get, api::al_group_del>(service, new al_api_get<api::al_group_get>(), new al_api_delete<api::al_group_del>());

    al_api("/v1/member").setup<api::al_member_add>(service, new al_api_post<api::al_member_add>());
    al_api("/v1/member/group_id/{group_id:.*}").setup<api::al_member_get>(service, new al_api_get<api::al_member_get>());
    al_api("/v1/member/group_name/{group_name:.*}").setup<api::al_member_get>(service, new al_api_get<api::al_member_get>());
    al_api("/v1/member/group_id/{group_id:.*}/member_id/{member_id:.*}").setup<api::al_member_get, api::al_member_del>(service, new al_api_get<api::al_member_get>(), new al_api_delete<api::al_member_del>());
    al_api("/v1/member/group_name/{group_name:.*}/member_id/{member_id:.*}").setup<api::al_member_get, api::al_member_del>(service, new al_api_get<api::al_member_get>(), new al_api_delete<api::al_member_del>());
    
    /*route api*/
    al_api("/v1/router").setup<api::al_router_add, api::al_router_get>(service, new al_api_post<api::al_router_add>(), new al_api_get<api::al_router_get>());
    al_api("/v1/router/{id:.*}").setup<api::al_router_del>(service, new al_api_delete<api::al_router_del>());

    /*port api*/
    al_api("/v1/nic").setup<api::al_nic_get>(service, new al_api_get<api::al_nic_get>());
    al_api("/v1/nic/nic_id/{nic_id:.*}").setup<api::al_nic_modify, api::al_nic_del>(service, new al_api_put<api::al_nic_modify>(), new al_api_delete<api::al_nic_del>());

    /*ip address*/
    al_api("/v1/ip").setup<api::al_ip_get>(service, new al_api_get<api::al_ip_get>());
    al_api("/v1/ip/port_name/{port_name:.*}").setup<api::al_ip_get>(service, new al_api_get<api::al_ip_get>());
    al_api("/v1/ip/port_id/{port_id:.*}").setup<api::al_ip_get>(service, new al_api_get<api::al_ip_get>());
    al_api("/v1/ip/ip_id/{ip_id:.*}").setup<api::al_ip_del>(service, new al_api_delete<api::al_ip_del>());

    al_api("/v1/ip/port_name/{port_name:.*}/{iptype:.*}/{address:.*}/{netmask:.*}").setup<api::al_ip_add>(service, new al_api_post<api::al_ip_add>());
    al_api("/v1/ip/port_id/{port_id:.*}/{iptype:.*}/{address:.*}/{netmask:.*}").setup<api::al_ip_add>(service, new al_api_post<api::al_ip_add>());

    /*geoip*/
    al_api("/v1/geoip").setup<api::al_geoip_get>(service, new al_api_get<api::al_geoip_get>());
    al_api("/v1/geoip/{geoip_id:.*}").setup<api::al_geoip_del>(service, new al_api_delete<api::al_geoip_del>());
    al_api("/v1/geoip").setup<api::al_geoip_add>(service, new al_api_post<api::al_geoip_add>());

    /*dns*/
    al_api("/v1/dns").setup<api::al_dns_get>(service, new al_api_get<api::al_dns_get>());
    al_api("/v1/dns/{dns_id:.*}").setup<api::al_dns_del>(service, new al_api_delete<api::al_dns_del>());
    al_api("/v1/dns").setup<api::al_dns_add>(service, new al_api_post<api::al_dns_add>());
    
    
    auto settings = std::make_shared< restbed::Settings >( );
    settings->set_port( port );
    settings->set_bind_address( ip );
    settings->set_default_header("Connection", "close");
    service->start(settings);
    
}

