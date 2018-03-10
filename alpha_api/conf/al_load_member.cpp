#include <stdio.h>
#include <inttypes.h>
#include <string>
#include <iostream>
#include <errno.h>
#include <map>
#include <sstream>
#include <sys/socket.h>
#include "al_db.h"
#include "al_errmsg.h"
#include "al_to_main.h"
#include "al_to_api.h"
#include "al_comm.h"
#include "al_ip_convert.h"

int al_member_load(void){
    std::string sql=std::string("SELECT key2, key3, value FROM aware where key1='member' and is_live = 1");
    std::map<std::string, std::map<std::string, std::string>> out;
    al_db db;
    int rc = db.select(sql, [&out](int argc, char** argv)->int{
            out[argv[0]][argv[1]]=argv[2];
        });
    if( rc ){
        errno=AL_FETCH_MEMBER_FAILED;
        return -1;
    }

    for(auto k=out.begin(); k != out.end(); k++){
        std::map<std::string, std::string> d=k->second;
        uint8_t iptype = d["iptype"].compare("ipv4")  ?  AF_INET6 : AF_INET;
        uint32_t* ptr;
        if( iptype == AF_INET ){
            al_ip_convert<struct in_addr> aic;
            ptr=aic.load(d["address"]);
            if( ptr == NULL){
                errno=AL_INVALID_IP;
                return -1;
            }
            rc=al_member_add(stn<uint16_t>(k->first), stn<uint16_t>(d["hcheck"]), iptype, ptr, stn<uint16_t>(d["port"]), stn<uint16_t>(d["group"]));
        }else{
            al_ip_convert<struct in6_addr> aic;
            ptr=aic.load(d["address"]);
            if( ptr == NULL ){
                errno=AL_INVALID_IP;
                return -1;
            }
            rc=al_member_add(stn<uint16_t>(k->first), stn<uint16_t>(d["hcheck"]), iptype, ptr, stn<uint16_t>(d["port"]), stn<uint16_t>(d["group"]));
        }
        if(rc){
            errno=AL_ADD_MEMBER_FAILED;
            return -1;
        }
    }
    return 0;
}


