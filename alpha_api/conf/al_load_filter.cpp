#include <stdio.h>
#include <inttypes.h>
#include <string>
#include <errno.h>
#include <map>
#include <sstream>
#include <iostream>
#include <sys/socket.h>
#include "al_db.h"
#include "al_errmsg.h"
#include "al_to_api.h"
#include "al_to_main.h"
#include "al_comm.h"
#include "al_log.h"
#include "al_ip_convert.h"

int al_filter_load(){

    al_api_filter f;
    std::string sql=std::string("SELECT key2, key3, value FROM aware where key1='filter' and is_live = 1");
    std::map<std::string, std::map<std::string, std::string>> out;
    al_db db;
    int rc = db.select(sql, [&out](int argc, char** argv)->int{
            out[argv[0]][argv[1]]=argv[2];
        });
    if( rc ){
        errno=AL_FETCH_FILTER_FAILED;
        return -1;
    }

    for(auto k=out.begin(); k != out.end(); k++){
        std::map<std::string, std::string> d=k->second;
        f.iptype = d["iptype"].compare("ipv4")  ?  AF_INET6 : AF_INET;

        if( f.iptype == AF_INET){
            f.ip4=*ip::al_ipu<AF_INET>().to(d["address"]);
            al_addr_af(f.addr4)=AF_INET;
            al_addr_len(f.addr4)=sizeof(al_addr4_t);
            al_addr_addr(f.addr4)=f.ip4;
            
            if(d.find("address_mask") != d.end()){
                f.msk4=*ip::al_ipu<AF_INET>().to(d["address_mask"]);
            }else{
                f.msk4=~((uint32_t)0);
            }
            al_addr_af(f.mask4)=AF_INET;
            al_addr_len(f.mask4)=sizeof(al_addr4_t);
            al_addr_addr(f.mask4)=f.msk4;
        }else{
            f.ip6=*ip::al_ipu<AF_INET6>().to(d["address"]);
            al_addr_af(f.addr6)=AF_INET6;
            al_addr_len(f.addr6)=sizeof(al_addr6_t);
            al_addr_addr(f.addr6)=f.ip6;                 
            
            if(d.find("address_mask") != d.end()){
                f.msk6=*ip::al_ipu<AF_INET6>().to(d["address_mask"]);
            }else{
                f.msk6=~((__uint128_t)0);
            }
            al_addr_af(f.mask6)=AF_INET6;
            al_addr_len(f.mask6)=sizeof(al_addr6_t);
            al_addr_addr(f.mask6)=f.msk6;
        }

        f.port=stn<uint16_t>(d["port"]);
        f.port=f.port;
        f.filter_type=al_filter_type(d["filter_type"].c_str());

        std::string proto=d["proto"];
        if(d["proto"].compare("tcp") == 0 ){
            f.proto=IPPROTO_TCP;
        }else if(d["proto"].compare("tcp") == 0 ){
            f.proto=IPPROTO_UDP;
        }else if(d["proto"].compare("all") == 0 ){
            f.proto=IPPROTO_IP;
        }
                
        f.gid=stn<uint32_t>(d["group"]);
        f.id=stn<uint32_t>(k->first);

        if(al_filter_insert(&f)){
            return -1;
        }
    }

    return 0;
}

