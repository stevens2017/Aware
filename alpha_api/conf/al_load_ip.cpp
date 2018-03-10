#include <stdio.h>
#include <inttypes.h>
#include <string>
#include <iostream>
#include <errno.h>
#include <map>
#include <sstream>
#include <sys/socket.h>
#include "al_log.h"
#include "al_db.h"
#include "al_to_api.h"
#include "al_to_main.h"
#include "al_errmsg.h"
#include "al_comm.h"
#include "al_ip_convert.h"
#include "rest/al_element.h"
#include <net/if.h>

int al_ip_load(void){

    /*fetch all vlan_id<->*/

    al_db db;
    int rc=0;
    std::map<std::string, std::map<std::string, std::string>> g;
    
    if(api::ips::all_ips(g)){
        return -1;
    }

    uint32_t* pptr, *pmptr;
    al_addr4_t addr4, maddr4;
    al_addr6_t addr6, maddr6;
    al_addr_t* addr, *maddr;

    api::Set ns;
    api::nics::all_nics(ns);


    for( auto i=g.begin(); i!=g.end(); i++){
        std::map<std::string, std::string> j=i->second;
        std::string iptype=j["iptype"];
        if( !iptype.compare("ipv4")){
            al_addr_af(addr4)=AF_INET;
            pptr=ip::al_ipu<AF_INET>().to(j["address"]);
            if( pptr == NULL){
                errno=AL_INVALID_IP;
                rc = -1;
                goto END;
            }else{
                al_addr_addr(addr4)=*(uint32_t*)pptr;
                addr=(al_addr_t*)&addr4;
            }

            al_addr_af(maddr4)=AF_INET;
            pmptr=ip::al_ipu<AF_INET>().to(j["netmask"]);
            if( pmptr == NULL){
                errno=AL_INVALID_IP;
                rc = -1;
                goto END;
            }else{
                al_addr_addr(maddr4)=*(uint32_t*)pmptr;
                maddr=(al_addr_t*)&maddr4;
            }
                    
        }else if( !iptype.compare("ipv6")){
            al_addr_af(addr6)=AF_INET6;
            pptr=ip::al_ipu<AF_INET6>().to(j["address"]);
            if( pptr == NULL){
                errno=AL_INVALID_IP;
                rc = -1;
                goto END;
            }else{
                al_addr_addr(addr6)=*(uint128_t*)pptr;
                addr=(al_addr_t*)&addr6;
            }

            al_addr_af(maddr6)=AF_INET6;
            pmptr=ip::al_ipu<AF_INET6>().to(j["netmask"]);
            if( pmptr == NULL){
                errno=AL_INVALID_IP;
                rc = -1;
                goto END;
            }else{
                al_addr_addr(maddr6)=*(uint128_t*)pmptr;
                maddr=(al_addr_t*)&maddr6;
            }
                    
        }

        std::string name=ns[j["port_id"]]["name"];
        if(::al_add_ip(if_nametoindex(name.c_str()), stn<uint64_t>(i->first), stn<uint32_t>(j["port_id"]), addr, maddr, IP_ASS_STATIC) == -1){
            LOG(LOG_ERROR, LAYOUT_CONF, "Load ip %s mask %s on port %s faliled", j["address"].c_str(), j["netmask"].c_str(), name.c_str());
        }
    }

END:
    return rc;
}

