#include <cstdio>
#include <iostream>
#include <inttypes.h>
#include <string>
#include <cstring>
#include <errno.h>
#include <map>
#include <sstream>
#include <iostream>
#include <sys/socket.h>
#include "al_db.h"
#include "al_errmsg.h"
#include "al_to_main.h"
#include "al_to_api.h"
#include "al_comm.h"
#include "al_log.h"
#include "al_ip_convert.h"

int al_route_load(){
    
    std::string sql=std::string("select key2, value from aware  where key1='vlan' and key3='vlanid' and is_live=1");
    std::map<std::string, std::string> vlan_id;
    al_db db;
    
    int rc = db.select(sql, [&vlan_id](int argc, char** argv)->int{
            vlan_id[argv[0]]=argv[1];
        });
    if( rc ){
        errno=AL_FETCH_VLAN_INFO_FAILED;
        return -1;
    }

    sql=std::string("SELECT key3, key2, key4, value FROM aware where key1='route' and is_live=1");
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> out;
    rc = db.select(sql, [&out, &vlan_id](int argc, char** argv)->int{
            if( strcmp(argv[2], "vlan_id") == 0 ){
                out[argv[0]][argv[1]][argv[2]]=vlan_id[argv[3]];
            }else{
                out[argv[0]][argv[1]][argv[2]]=argv[3];    
            }
        });
    if( rc ){
        errno=AL_FETCH_ROUTE_FAILED;
        return -1;
    }
    
    for(auto k=out.begin(); k != out.end(); k++){
        std::string rid=k->first;
        /*k->first is route id*/
        std::map<std::string, std::map<std::string, std::string>> d=k->second;
        for(auto j=d.begin(); j!=d.end(); j++){
            /*j->first is route type*/
            std::map<std::string, std::string> e=j->second;
            rtype_t rtype;
            std::string subnet;
            std::string srt=j->first;
            if(srt.compare("policy") == 0){
                rtype=POLICY_TABL;
                subnet=e["srcip"];
            }else if(srt.compare("direct") == 0){
                rtype=DIRECT_TABL;
                subnet=e["dstip"];
            }else if(srt.compare("static") == 0){
                rtype=STATIC_TABL;
                subnet=e["dstip"];                
            }else if(srt.compare("dynamic") == 0){
                rtype=DYNAMIC_TABL;
                subnet=e["dstip"];                
            }else if(srt.compare("gateway") == 0){
                rtype=GATEWAY_TABL;
            }else{
                LOG(LOG_ERROR, LAYOUT_CONF, "Invalid route type:%s", rid.c_str());
                continue;
            }

            uint32_t* sbn, *prefix, *nexthop;
            uint8_t iptype;
            if(e["iptype"].compare("ipv4") == 0 ){
                sbn=al_ip_convert<struct in_addr>().load(subnet);
                prefix=al_ip_convert<struct in_addr>().load(e["netmask"]);
                nexthop=al_ip_convert<struct in_addr>().load(e["nexthop"]);
                iptype=AF_INET;
            }else{
                sbn=al_ip_convert<struct in6_addr>().load(subnet);
                prefix=al_ip_convert<struct in6_addr>().load(e["netmask"]);
                nexthop=al_ip_convert<struct in6_addr>().load(e["nexthop"]);
                iptype=AF_INET6;
            }

            uint32_t of;
            if( al_get_nic_id(if_nametoindex(e["name"].c_str()), &of)){
                LOG(LOG_ERROR, LAYOUT_CONF, "Invalid route interface:%s", e["name"].c_str());
                continue;
            }

            if(al_router_insert(stn<uint16_t>(rid), (uint8_t*)sbn, (uint8_t*)prefix, (uint8_t*)nexthop, iptype, rtype, of)){
                LOG(LOG_ERROR, LAYOUT_CONF, "Add route failed, id:%s", e["id"].c_str());
            }
        }
    }
    return 0;
}

