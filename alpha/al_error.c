#include <stdio.h>
#include <string.h>
#include "al_errmsg.h"
#include "al_common.h"

char* al_errmsg[AL_END];

static void CONSTRUCT_FUN al_error_init(void){
    al_errmsg[AL_NO_ERROR]="No error";
    al_errmsg[AL_GEN_GLOBAL_KEY_FAILED]="Generator global key failed";
    al_errmsg[AL_INVALID_VLAN_ID]="Invalid vlan id";
    al_errmsg[AL_VLAN_ALREADY_EXIST]="Vlan alread exist";
    al_errmsg[AL_VLAN_NAME_AND_VLANID_NEED]="Vlan name and vlanid is needed";
    al_errmsg[AL_DB_INSERT_FAILED]="Db insert falied";
    al_errmsg[AL_NO_DATA_FOUND]="No data found";
    al_errmsg[AL_PORT_NOT_FOUND]="Port not found";
    al_errmsg[AL_VLAN_NOT_FOUND]="Vlan not found";
    al_errmsg[AL_API_NOT_IMPLEMENT]="No implement";
    al_errmsg[AL_DB_FETCH_KEY_FAILED]="Fetch key failed";
    al_errmsg[AL_DB_FETCH_VAL_FAILED]="Fetch val failed";
    al_errmsg[AL_DB_ADD_FAILED]="Add data failed";
    al_errmsg[AL_DB_DEL_FAILED]="Del data failed";
    al_errmsg[AL_DB_OPEN_DB_FAILED]="Open db file failed";
    al_errmsg[AL_DB_OPEN_CONF_DB_FAILED]="Disable auto commit failed";
    al_errmsg[AL_FETCH_VLANS_FAILED]="search vlan from db failed";
    al_errmsg[AL_VLAN_EXIST]="Vlan already exist";
    al_errmsg[AL_VLAN_PORT_FETCH_FAILED]="Fetch vlan port failed";
    al_errmsg[AL_ADD_PORT_TO_VLAN_FAILED]="Add port to vlan failed";
    al_errmsg[AL_ENABLE_VPORT_FAILED]="Enable vlan vport failed";
    al_errmsg[AL_FETCH_VLAN_INFO_FAILED]="Fetch vlan id relation with vlanid";
    al_errmsg[AL_FETCH_VLAN_IP_FAILED]="Fetch vlan ip";
    al_errmsg[AL_FETCH_ROUTE_FAILED]="Fetch route info failed";
    al_errmsg[AL_ADD_IP_TO_VLAN_FAILED]="Add ip to vlan failed";
    al_errmsg[AL_FETCH_GROUP_FAILED]="Fetch group failed";
    al_errmsg[AL_ADD_GROUP_FAILED]="Add group failed";
    al_errmsg[AL_FETCH_MEMBER_FAILED]="Fetch member failed";
    al_errmsg[AL_ADD_MEMBER_FAILED]="Add member failed";
    al_errmsg[AL_FETCH_FILTER_FAILED]="Fetch vserver failed";
    al_errmsg[AL_ADD_FILTER_FAILED]="Add vserver failed";
    al_errmsg[AL_INVALID_IP]="Invalid ip";
    al_errmsg[AL_INTERVAL_ERROR]="Interval error";
    al_errmsg[AL_MEMBER_PARAMENTS_ERROR]="Member paraments(iptype, group, address, port) is needed";
    al_errmsg[AL_INVALID_IP_TYPE]="Invalid ip type";
    al_errmsg[AL_INVALID_PORT]="Invalid port";
    al_errmsg[AL_DB_BEGIN_TRANS_FAILED]="Begin transaction failed";
    al_errmsg[AL_MEMBER_EXIST]="Member already exist";
    al_errmsg[AL_DB_COMMIT_FAILED]="DB commit failed";
    al_errmsg[AL_MEMBER_ID_NEEDED]="Member id is needed";
    al_errmsg[AL_DB_SELECT_FAILED]="DB select opt failed";
    al_errmsg[AL_OBJECT_EEXIST]="Object doesn't exist";
    al_errmsg[AL_GROUP_NAME_NEEDED]="Group name needed";
    al_errmsg[AL_DELETE_OBJ_FAILED]="Del object failed";
    al_errmsg[AL_GROUP_DEL_FAILED]="Group id or name exist";
    al_errmsg[AL_FILTER_PARAMENTS_ERROR]="Vserver paraments(iptype, port_start, port_end, addr_start, addr_end, group, name, service_type) is needed";
    al_errmsg[AL_FILTER_TYPE_INVALID]="Invalid service type";
    al_errmsg[AL_OBJECT_EXIST]="Object exist";
    al_errmsg[AL_OBJECT_DUMPLICATE]="Object dumplicate";
    al_errmsg[AL_KEYWORDS_MISS]="Keywords miss";
    al_errmsg[AL_ROUTER_PARAMENTS_MISS]="Router keywords miss";
    al_errmsg[AL_ROUTER_TYPE_UNKNOW]="Can't judge route type";
    al_errmsg[AL_ROUTER_TYPE_INVALID]="Router type invalid";
    al_errmsg[AL_ENOOBJ]="Object not found";
    al_errmsg[AL_PROTO_TYPE_UNKNOWN]="Unknown proto type";
    al_errmsg[AL_FILTER_TYPE_UNKNOWN]="Unknown filter type(support type: llb/simple-tcp/tcp/http/glsb)";
    al_errmsg[AL_FILTER_DEL_FAILED]="Delete filter failed";
    al_errmsg[AL_RESOURCE_BUSY]="Resource referrence by others";
    al_errmsg[AL_GROUP_EEXIST]="Group doesn't exist";
    al_errmsg[AL_GROUP_INFO_NEED]="Group id or name needed";
    al_errmsg[AL_GROUP_ALREADY_EEXIST]="Group already exist";
    al_errmsg[AL_LOW_MEMORY]="Memory is not enough";
    al_errmsg[AL_PROTO_UNKNOW]="Proto type unknow";
    al_errmsg[AL_DELETE_IP_FAILED]="Delete ip failed";
};

const char* al_get_errmsg(int no) {
    if(no < AL_NO_ERROR){
        return strerror(no);
    }else{
        return al_errmsg[no];
    }
}
