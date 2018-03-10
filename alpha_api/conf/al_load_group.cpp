#include <stdio.h>
#include <inttypes.h>
#include <string>
#include <iostream>
#include <errno.h>
#include <map>
#include <sstream>
#include <sys/socket.h>
#include "al_db.h"
#include "al_to_api.h"
#include "al_to_main.h"
#include "al_errmsg.h"
#include "al_comm.h"
#include "al_ip_convert.h"


int al_group_load_member(uint32_t gid, void* mc, int (*insert)(void* mc, al_member_conf_t* conf)){

    std::string gidstr=nts<uint32_t>(gid);
    uint8_t proto;
    std::string sql=std::string("SELECT key2, key3, value FROM aware where key1='member' and is_live = 1");
    std::map<std::string, std::map<std::string, std::string>> out;
    al_db db;
    int rc = db.select(sql, [&out](int argc, char** argv)->int{
            out[argv[0]][argv[1]]=argv[2];
        });
    if( rc ){
        errno=AL_FETCH_GROUP_FAILED;
        return -1;
    }
    
    al_member_conf_t f;
    for( auto i=out.begin(); i!=out.end(); i++){
        std::map<std::string, std::string>& v=i->second;
        if( v["group"].compare(gidstr) == 0 ){
            if(v.find("weight") != v.end()){
                f.weight=stn<uint16_t>(v["weight"]);
            }else{
                f.weight=0;
            }
            f.port=stn<uint16_t>(v["port"]);
            f.mid=stn<uint32_t>(i->first);
            proto=v["iptype"].compare("ipv4") ? AF_INET6 : AF_INET;
            al_addr_af(f.addr.addr)=proto;

            if( proto == AF_INET ){
                al_addr_addr(f.addr.addr4)=*ip::al_ipu<AF_INET>().to(v["address"]);
                al_addr_len(f.addr.addr4)=sizeof(al_addr4_t);
            }else{
                al_addr_addr(f.addr.addr6)=*ip::al_ipu<AF_INET6>().to(v["address"]);
                al_addr_len(f.addr.addr6)=sizeof(al_addr6_t);
            }
            if(insert(mc, &f)){
                return -1;
            }
        }
    }
    
    return 0;
}

int al_api_group_exist(uint32_t gid, uint8_t* sched){

    std::string gidstr=nts<uint32_t>(gid);
    uint8_t proto;
    std::string sql=std::string("SELECT key3, value FROM aware where key1='group' and is_live = 1 and key2='")+gidstr+std::string("'");
    std::map<std::string, std::string> out;
    al_db db;
    int rc = db.select(sql, [&out](int argc, char** argv)->int{
            out[argv[0]]=argv[1];
        });
    if( rc ){
        errno=AL_FETCH_GROUP_FAILED;
        return -1;
    }

    *sched=stn<uint8_t>(out["algorithm"]);
    
    return 0;
}

int al_group_info(uint32_t fid, uint32_t gid, group_cb f){

    std::string gidstr=nts<uint32_t>(gid);
    uint8_t proto;
    al_group_conf_t conf;
    
    std::string sql=std::string("SELECT key3, value FROM aware where key1='group' and is_live = 1 and key2='")+gidstr+std::string("'");
    std::map<std::string, std::string> out;
    al_db db;
    int rc = db.select(sql, [&out](int argc, char** argv)->int{
            out[argv[0]]=argv[1];
        });
    if( rc ){
        errno=AL_FETCH_GROUP_FAILED;
        return -1;
    }

   if( out.find("algorithm") == out.end() ){
       errno = AL_GROUP_EEXIST;
       return -1;
   }

    conf.schd=stn<uint8_t>(out["algorithm"]);
    return f(fid, gid, &conf);
}

