#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "al_api.h"
#include "al_to_api.h"
int al_group_del(uint16_t gid){};
int al_member_del(uint16_t gid){};
int al_vserver_del(uint16_t gid){};

const char* al_get_errmsg(int no){

    
}
    void al_output_session(){
    }

    void al_output_route(FILE* fp){
        
    }
    void al_output_status(FILE* fp){
    }
    void al_output_mac(FILE* fp){
        
    }
    void al_output_arp(FILE* fp){
        
    }
    int al_route_insert(uint16_t id, uint32_t* subnet, uint32_t* mask, uint32_t* nexthop, uint8_t iptype, uint16_t vlanid, rtype_t rtype){
        return 0;
    }

    /*load all module*/
    int al_vlan_load(void){return 0;}
    int al_ip_load(void){return 0;}
    int al_group_load(void){return 0;}
    int al_member_load(void){return 0;}
    int al_vserver_load(void){return 0;}

    /*vlan opt*/
    int al_vlan_new(uint16_t vlanid){return 0;}
    int al_vlan_exist(uint16_t vid){return 0;}
    int al_vlan_del_portid(uint16_t vid, uint16_t portid){return 0;}
    int al_vlan_add_portid(uint16_t vid, uint16_t portid){return 0;}
    int al_vlan_enable_vport(int id, uint16_t vid, const char* mac){return 0;}

    /*ip*/
    int al_vlan_add_ipaddr(uint16_t vlanid,  const char* ip, const char* mask, uint8_t iptype, uint8_t ipsrc){return 0;}

    /*group*/
    int al_group_add(uint16_t id, uint8_t algorithm, uint8_t hcheck){return 0;}

    /*member*/
    int al_member_add(uint16_t id, uint8_t hcheck, uint8_t iptype, uint32_t* addr, uint16_t port, uint16_t pool){return 0;}

    /*vserver*/
    int al_vserver_add(uint16_t id, uint8_t service_type, uint8_t iptype, uint32_t* start, uint32_t* end, uint16_t port_start, uint16_t port_end, uint16_t pool, const char* domain){return 0;}

    void al_dump_vport_ip( FILE* fp){
        
    }
    void al_dump_group(FILE* fp){
        
    }
    void al_dump_member(FILE* fp){
        
    }
    void al_dump_vserver(FILE* fp){
        
    }

    void log_core(int level, int module, const char* format, ...){}
int main(int argc, char* argv[]){
   al_start_api("127.0.0.1", 8080);
   return 0;
}
