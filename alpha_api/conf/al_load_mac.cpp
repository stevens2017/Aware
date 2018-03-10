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
#include "al_ether.h"
#include <net/if.h>

int al_mac_load(void){

    int rc=0;
    std::map<std::string, std::string> hws;
    if(api::hwaddr::all_hwaddr(hws)){
        return -1;
    }

    for(auto i=hws.begin(); i!=hws.end(); i++){
        std::string ifname=i->first;
        std::string hwaddr=i->second;

        if(if_updown(ifname.c_str(), AL_IF_DOWN)){
            rc=-1;
            continue;
        }

        if(set_mac_addr(ifname.c_str(), hwaddr.c_str())){
            rc=-1;
            continue;
        }

        if(if_updown(ifname.c_str(), AL_IF_UP)){
            rc=-1;
            continue;
        }
    }

    return rc;
}

