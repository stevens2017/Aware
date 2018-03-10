#include<vector>
#include<cstring>
#include <stdio.h>
#include <inttypes.h>
#include <string>
#include <iostream>
#include <errno.h>
#include <map>
#include <sstream>
#include "al_db.h"
#include "al_comm.h"
#include "al_to_api.h"
#include "al_to_main.h"
#include "rest/al_element.h"
#include <net/if.h>

int  al_write_kni_nic_conf(uint32_t kid, char* ifname, uint8_t* hwaddr){

    char buf[1024];
    FILE* fp;
    std::string name, mac;

    std::vector<std::string> field;
    field.push_back(std::string("key3"));
    field.push_back(std::string("value"));
    std::map<std::string, std::string> map;
    map["key1"]="physical_port";
    sprintf(buf, "%u", kid);
    map["key2"]=buf;
    std::map<std::string, std::string> out;
    
    al_db db;
    if( db.select(field, map, [&out](int argc, char** argv)->void{
                out[argv[0]]=argv[1];
            })){
        return -1;
    }

    name=out["name"];
    sprintf(ifname, "%s", name.c_str());

    std::map<std::string, std::string> macs;
    if(api::hwaddr::all_hwaddr(macs)){
        return -1;
    }else{
        if( macs.find(name)==macs.end() ){
            return -1;
        }
    }
    mac=macs[name];
    if( hwaddr ){
        sprintf((char*)hwaddr, mac.c_str());    
    }
    
    sprintf(buf, "/etc/sysconfig/network-scripts/ifcfg-%s", ifname);
    fp = fopen(buf, "w");
    if( fp == NULL ){
        return -1;
    }

    sprintf(buf, "DEVICE=%s\nTYPE=Ethernet\nBOOTPROTO=DHCP\nDEFROUTE=yes\nPEERDNS=yes\nPEERROUTES=yes\nIPV4_FAILURE_FATAL=no\nIPV6INIT=yes\nIPV6_AUTOCONF=yes\nIPV6_DEFROUTE=yes\nIPV6_PEERDNS=yes\nIPV6_PEERROUTES=yes\nIPV6_FAILURE_FATAL=no\nNAME=%s\nONBOOT=yes\nMACADDR=%s\n", name.c_str(), name.c_str(), mac.c_str());

    if( fwrite(buf, strlen(buf), 1, fp) != 1){
        fclose(fp);
        return -1;
    }

    api::Set nic_ips;
    int index=0;
    if(api::ips::all_ips(nic_ips)){
        fclose(fp);        
        return -1;
    }else{
        std::string nid=nts<uint32_t>(kid);
        for(auto i=nic_ips.begin(); i!=nic_ips.end(); i++){
            auto j=i->second;
            if( j["port_id"].compare(nid) ==0 ){
                fprintf(fp, "IPADDR%d=%s\n", index, j["address"].c_str());
                fprintf(fp, "NETMASK%d=%s\n", index, j["netmask"].c_str());
                index++;
                break;
            }
        }
    }

    fclose(fp);

    return 0;
}

int16_t al_get_port_id(int domain, int bus, int devid, int function){

    int rc;
    char buf[32];
    std::string value;
    std::string sql("select key2 from aware where key1='physical_port' and key3='devid' and is_live = 1 and value=");
    sprintf(buf, "'%04d:%02d:%02d.%d'", domain, bus, devid, function);
    sql+=buf;

    al_db db;
    rc=-1;
    if( db.select(sql, [&rc, &value](int argc, char** argv)->void{
                rc =0;
                value=argv[0];
            })){
        return -1;
    }

    if( !rc ){
        rc=-1;
        rc=stn<int16_t>(value);
    }
    return rc;
}

int al_get_nic_id(uint32_t ifindex,  uint32_t* ifid){

    char buf[1024];
    char* ptr=if_indextoname(ifindex, buf);
    if( ptr == NULL ){
        return -1;
    }
    
    std::string name=buf;
    api::Set g;
    if( api::nics::all_nics(g)){
        return -1;
    }

    for(auto i=g.begin(); i!=g.end(); i++){
        auto j=i->second;
        if( j["name"].compare(name) == 0){
            *ifid = stn<uint32_t>(i->first);
            return 0;
        }
    }
    
    return -1;
}

