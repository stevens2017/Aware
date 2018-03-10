#ifndef AP_IP_H
#define AP_IP_H

#include "al_to_api.h"
#include "al_make_msg.h"
#include "db/al_db.h"
#include "al_ip_convert.h"
#include "al_log.h"
#include "al_element.h"
#include <net/if.h>

namespace api{

    class al_ip_base{
    public:
        std::string get_id(const std::string& args){
            
            try{
                std::string idstr;
                api::Set outall;
                if(api::nics::all_nics(outall)){
                    return std::string();
                }
                
                json j = json::parse(args);
                if( j.find("port_id") == j.end()){
                    if( j.find("port_name") != j.end()){
                        for(auto i=outall.begin(); i!=outall.end(); i++){
                            if(i->second["name"].compare(j["port_name"].get<std::string>())==0){
                                return i->first;
                            }
                        }
                    }
                }else{
                    idstr=j["port_id"].get<std::string>();
                    if(outall.find(idstr) == outall.end()){
                        errno=AL_PORT_NOT_FOUND;
                        return std::string();
                    }else{
                        return idstr;
                    }
                }

                return std::string();
            }catch(std::runtime_error err){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception:%s", err.what());
                return std::string();
            }
        }
        
//        std::string get_name_byid(const std::string id){
        std::string get_name_by_ipid(const std::string id, std::string& portid){
            try{
                std::string idstr;
                api::Set outall;
                if(api::nics::all_nics(outall)){
                    return std::string();
                }

                api::Set ipall;
                if(api::ips::all_ips(ipall)){
                    return std::string();
                }

                if(ipall.find(id) != ipall.end()){
                    if(outall.find(ipall[id]["port_id"]) != outall.end()){
                        portid=ipall[id]["port_id"];
                        return outall[ipall[id]["port_id"]]["name"];  
                    }else{
                        return std::string(); 
                    }
                }else{
                    return std::string();
                }
            }catch(std::runtime_error err){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception:%s", err.what());
                return std::string();
            }
        }

        std::string get_name_by_nicid(const std::string id){
            try{
                std::string idstr;
                api::Set outall;
                if(api::nics::all_nics(outall)){
                    return std::string();
                }

                if(outall.find(id) != outall.end()){
                    return outall[id]["name"];  
                }else{
                    return std::string(); 
                }

            }catch(std::runtime_error err){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception:%s", err.what());
                return std::string();
            }
        }
        
    };

    
    struct al_ip_get: public al_ip_base{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){

            int rc=0;
            try{
                std::string idstr=get_id(args);
                json j;
                api::Set iplist;
                if(api::ips::all_ips(iplist)){
                    rc=-1;
                    goto END;
                }
                
                if( idstr.length()){
                    api::Set ret;
                    for( auto k=iplist.begin(); k!=iplist.end(); k++){
                        if(k->second["port_id"].compare(idstr)==0){
                            ret[k->first]=k->second;
                        }
                    }
                    j=ret;
                }else{
                    j=iplist;
                }

                retmsg=al_make_msg().make_success(j);
            }catch(std::runtime_error err){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception:%s", err.what());
                rc = -1;
                errno = AL_INTERVAL_ERROR;
                goto END;
            }

            rc = 0;
        END:
            if( rc ){
                retmsg = al_make_msg().make_failed(std::string());    
            }
            return rc;
        }
    };
    
    struct al_ip_add: public al_ip_base{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){

            int rc=0;
            try{
                std::string idstr=get_id(args);
                json j;
                std::string name;
                api::Set iplist;
                if(api::ips::all_ips(iplist)){
                    rc=-1;
                    goto END;
                }
                
                if( idstr.length() == 0){
                    rc=-1;
                    goto END;
                }else{
                    j = json::parse(args);
                }

                uint32_t* pptr, *pmptr;
                al_addr4_t addr4, maddr4;
                al_addr6_t addr6, maddr6;
                al_addr_t* addr, *maddr;
                std::string iptype=j["iptype"].get<std::string>();
                if( !iptype.compare("ipv4")){
                    al_addr_af(addr4)=AF_INET;
                    pptr=ip::al_ipu<AF_INET>().to(j["address"].get<std::string>());
                    if( pptr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }else{
                        al_addr_addr(addr4)=*(uint32_t*)pptr;
                        addr=(al_addr_t*)&addr4;
                    }

                    al_addr_af(maddr4)=AF_INET;
                    pmptr=ip::al_ipu<AF_INET>().to(j["netmask"].get<std::string>());
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
                    pptr=ip::al_ipu<AF_INET6>().to(j["address"].get<std::string>());
                    if( pptr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }else{
                        al_addr_addr(addr6)=*(uint128_t*)pptr;
                        addr=(al_addr_t*)&addr6;
                    }

                    al_addr_af(maddr6)=AF_INET6;
                    pmptr=ip::al_ipu<AF_INET6>().to(j["netmask"].get<std::string>());
                    if( pmptr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }else{
                        al_addr_addr(maddr6)=*(uint128_t*)pmptr;
                        maddr=(al_addr_t*)&maddr6;
                    }
                    
                }else{
                    errno=AL_INVALID_IP_TYPE;
                    goto END;
                }

                int64_t ipid=al_db::newid();
                if( ipid < 0 ){
                    rc = -1;
                    goto END;
                }

                al_db db;
                if( (rc = db.begin(ipid)) ){
                    rc = -1;
                    goto END;
                }

                std::string sql = std::string("insert into aware(key1, key2, key3, value) values ");
                sql += std::string("( 'ipaddress', '")+nts<int64_t>(ipid)+std::string("', 'iptype', '")+j["iptype"].get<std::string>()+std::string("'),");
                sql += std::string("( 'ipaddress', '")+nts<int64_t>(ipid)+std::string("', 'address', '")+j["address"].get<std::string>()+std::string("'),");
                sql += std::string("( 'ipaddress', '")+nts<int64_t>(ipid)+std::string("', 'netmask', '")+j["netmask"].get<std::string>()+std::string("'),");
                sql += std::string("( 'ipaddress', '")+nts<int64_t>(ipid)+std::string("', 'port_id', '")+idstr+std::string("')");
	       
                if( db.insert_with_sql(sql)){
                    errno = AL_DB_INSERT_FAILED;
                    LOG(LOG_ERROR, LAYOUT_CONF, "Insert data fialed, sql:%s", sql.c_str());
                    rc = -1;
                    goto END;
                }

                name=get_name_by_nicid(idstr);
                uint8_t portid=stn<uint32_t>(idstr);
                if(::al_add_ip(if_nametoindex(name.c_str()), ipid, portid, addr, maddr, IP_ASS_STATIC) == -1){
                    rc = -1;
                    goto END;
                }

                if(db.commit() ){
                    rc = -1;
                    errno = AL_DB_COMMIT_FAILED;
                    goto END;
                }
#if 0
                char ifname_buf[1024];
                if( al_write_kni_nic_conf(portid, ifname_buf, NULL)){
                    LOG(LOG_ERROR, LAYOUT_CONF, "Write %s configuration failed", name.c_str());
                }
#endif                
                json r;
                r["ip_id"]=nts<int16_t>(ipid);
                retmsg = al_make_msg().make_success(r);
            }catch(std::runtime_error err){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception:%s", err.what());
                rc = -1;
                errno = AL_INTERVAL_ERROR;
                goto END;
            }

            rc = 0;
        END:
            if( rc ){
                retmsg = al_make_msg().make_failed(std::string());    
            }
            return rc;
        }
    };

    struct al_ip_del : public al_ip_base{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            int rc=0;
            try{
                std::string idstr;
                json j;
                std::string name;
                api::Set iplist;
                if(api::ips::all_ips(iplist)){
                    rc=-1;
                    goto END;
                }
                
                j=json::parse(args);
                if(iplist.find(j["ip_id"].get<std::string>()) == iplist.end()){
                    rc=-1;
                    goto END;
                }else{
                    idstr=j["ip_id"].get<std::string>();
                }
                
                al_db db;
                std::string sql=std::string("update aware set is_live = 0 where key2='")+idstr+std::string("'");
                if( db.update_with_sql(sql)){
                    rc = -1;
                    errno = AL_OBJECT_EEXIST;
                    LOG(LOG_ERROR, LAYOUT_CONF, "Delete route failed, %s", sql.c_str());
                    goto END;
                }

                name=get_name_by_ipid(idstr, idstr);
                if( !name.length() ){
                    rc = -1;
                    errno = AL_OBJECT_EEXIST;
                    goto END;
                }

                if(::al_del_ip(if_nametoindex(name.c_str()), stn<uint64_t>(j["ip_id"].get<std::string>()), stn<uint32_t>(idstr))){
                    rc = -1;
                    errno=AL_DELETE_IP_FAILED;
                    goto END;
                }

                if(db.commit() ){
                    rc = -1;
                    errno = AL_DB_COMMIT_FAILED;
                    goto END;
                }                
#if 0
                char ifname_buf[1024];
                if( al_write_kni_nic_conf(stn<uint32_t>(idstr), ifname_buf, NULL)){
                    LOG(LOG_ERROR, LAYOUT_CONF, "Write %s configuration failed", name.c_str());
                }
#endif                
                rc=0;
                retmsg=al_make_msg().make_success(std::string());
            }catch(...){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception, args:%s msg:%s", args.c_str(), msg.c_str());
                rc = -1;
                errno = AL_INTERVAL_ERROR;
                goto END;
            }
            rc = 0 ;
            
        END:
            if( rc ){
                retmsg=al_make_msg().make_failed(std::string());    
            }
            
            return rc;
        }
    };
}


#endif
