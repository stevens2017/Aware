#ifndef _AL_ROUTER_H_
#define _AL_ROUTER_H_

#include "al_to_api.h"
#include "al_make_msg.h"
#include "db/al_db.h"
#include "al_ip_convert.h"
#include "al_log.h"
#include "al_to_main.h"
#include <net/if.h>

namespace api{
    struct al_router{
    public:
        int search_route_by_id(std::string& rid, std::string& dst, std::string& mask, uint8_t* iptype, uint8_t* rtype){
            std::string sql=std::string("SELECT key2, key4, value FROM aware where is_live=1 and key3 = \'")+rid+std::string("\'");

            al_db db;
            int rc;
            std::map<std::string, std::string> out;
            if( db.select(sql, [&out](int argc, char* argv[]){
                        out[argv[1]]=argv[2];
                        out["rtype"]=argv[0];
                    })){
                errno = AL_DB_SELECT_FAILED;
                return -1;
            }

            if( out.size() == 0){
                LOG(LOG_ERROR, LAYOUT_CONF, "Route not exist:%s", sql.c_str());
                errno=AL_OBJECT_EEXIST;
                return -1;
            }

            dst=out["dstip"];
            mask=out["netmask"];
            rc=al_router_rtype(out["rtype"].c_str());
            if( rc == -1 ){
                LOG(LOG_ERROR, LAYOUT_CONF, "Invalid route type:%s", out["rtype"].c_str());
            }
            
            *rtype = rc & 0xFF;

            if( out["iptype"].compare("ipv4") ==0 ){
                *iptype = AF_INET;
            }else{
                *iptype = AF_INET6;
            }

            return 0;
        }
    };
    
    struct al_router_add: public al_router{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            
            int rc=0;
            uint16_t vlanid;
            uint32_t* pptr, *paddr, *pmask, *pnexthop;
            uint32_t* gid;
            std::string  rtype;
            std::string ipaddr;
            std::string ifname;
            uint32_t oif;
            
            try{
                json j = json::parse(msg);

                if( ( j.find("iptype") == j.end() ) ||
                    ( j.find("nexthop") == j.end() ) ||
                    ( j.find("netmask") == j.end() ) ||
                    ( j.find("name") == j.end())){
                    errno = AL_ROUTER_PARAMENTS_MISS;
                    goto END;
                }
#if 0
                vlanid = stn<uint16_t>((*j.find("vlanid")).get<std::string>());
                if( al_vlan_exist(vlanid) != 1){
                    errno = AL_VLAN_NOT_FOUND;
                    goto END;
                }
#endif
                ifname=j["name"].get<std::string>();
                if( al_get_nic_id(if_nametoindex(ifname.c_str()), &oif)){
                    LOG(LOG_ERROR, LAYOUT_CONF, "Invalid route interface:%s", ifname.c_str());
                    goto END;
                }

                if( j.find("dstip") != j.end()){
                    ipaddr=(*j.find("dstip")).get<std::string>();
                }else if( j.find("srcip") != j.end()) {
                    ipaddr=(*j.find("srcip")).get<std::string>();
                    rtype="policy";
                }else{
                    errno = AL_ROUTER_PARAMENTS_MISS;
                    goto END;
                }

                uint8_t iptype;
                if( j["iptype"].get<std::string>().compare("ipv4") == 0 ){
                    iptype = AF_INET;
                    paddr=ip::al_ipu<AF_INET>().to(ipaddr);
                    if( paddr == NULL){
                        errno=AL_INVALID_IP;
                        return -1;
                    }

                    pmask=ip::al_ipu<AF_INET>().to(j["netmask"].get<std::string>());
                    if( pmask == NULL){
                        errno=AL_INVALID_IP;
                        return -1;
                    }

                    pnexthop=ip::al_ipu<AF_INET>().to(j["nexthop"].get<std::string>());
                    if( pnexthop == NULL){
                        errno=AL_INVALID_IP;
                        return -1;
                    }

                    if( rtype.length() == 0 ){
                        if(*paddr == 0){
                            if(*pmask == 0){
                                rtype="getway";
                            }
                        }else{
                            if(*pnexthop == 0){
                                rtype="direct";
                            }else{
                                rtype="static";
                            }
                        }
                    }
                }else if( j["iptype"].get<std::string>().compare("ipv6") == 0 ){
                    iptype = AF_INET6;
                    paddr=ip::al_ipu<AF_INET6>().to(ipaddr);
                    if( paddr == NULL){
                        errno=AL_INVALID_IP;
                        return -1;
                    }

                    pmask=ip::al_ipu<AF_INET6>().to(j["netmask"].get<std::string>());
                    if( pmask == NULL){
                        errno=AL_INVALID_IP;
                        return -1;
                    }

                    pnexthop=ip::al_ipu<AF_INET6>().to(j["nexthop"].get<std::string>());
                    if( pnexthop == NULL){
                        errno=AL_INVALID_IP;
                        return -1;
                    }

                    if( rtype.length() == 0 ){
                        if(*paddr == 0){
                            if(*pmask == 0){
                                rtype="getway";
                            }
                        }else{
                            if(*pnexthop == 0){
                                rtype="direct";
                            }else{
                                rtype="static";
                            }
                        }
                    }
                }else{
                    errno = AL_INVALID_IP_TYPE;
                    rc = -1;
                    goto END;
                }

                if( rtype.length() == -1 ){
                    errno = AL_ROUTER_TYPE_UNKNOW;
                    goto END;
                }
                
                al_db db;
                std::map<std::string, std::map<std::string, std::string>> out;
                std::string sql=std::string("SELECT key3, key4,value FROM aware where key1 = 'route'");
                db.select(sql, [&out](int argc, char** argv){
                        out[argv[0]][argv[1]]=argv[2];
                    });

                rc = 0;
                for( auto i=out.begin(); i != out.end(); i++){
                    auto v=i->second;
                    if(v["dstip"].compare(ipaddr)==0){
                        if(v["netmask"].compare(j["netmask"].get<std::string>())==0){
                            if(v["nexthop"].compare(j["nexthop"].get<std::string>())){
                                rc =1;
                                break;
                            }
                        }
                    }
                }

                if( rc == 1 ){
                    errno = AL_OBJECT_EXIST;
                    rc = -1;
                    goto END;
                }

                int64_t vid=al_db::newid();
                if( vid < 0 ){
                    rc = -1;
                    goto END;
                }

                if( (rc = db.begin(vid)) ){
                    rc = -1;
                    goto END;
                }

                sql = std::string("insert into aware(key1, key3, key2, key4, value) values ");
                sql += std::string("( 'route', '")+nts<int64_t>(vid)+std::string("', '")+rtype+std::string("', 'iptype', '")+j["iptype"].get<std::string>()+std::string("'),");
                if( rtype.compare("policy") == 0){
                    sql += std::string("( 'route', '")+nts<int64_t>(vid)+std::string("', '")+rtype+std::string("', 'srcip', '")+ipaddr+std::string("'),");
                }else{
                    sql += std::string("( 'route', '")+nts<int64_t>(vid)+std::string("', '")+rtype+std::string("', 'dstip', '")+ipaddr+std::string("'),");
                }
                sql += std::string("( 'route', '")+nts<int64_t>(vid)+std::string("', '")+rtype+std::string("', 'rtype', '")+rtype+std::string("'),");
                sql += std::string("( 'route', '")+nts<int64_t>(vid)+std::string("', '")+rtype+std::string("', 'netmask', '")+j["netmask"].get<std::string>()+std::string("'),");
                sql += std::string("( 'route', '")+nts<int64_t>(vid)+std::string("', '")+rtype+std::string("', 'nexthop', '")+j["nexthop"].get<std::string>()+std::string("'),");
                sql += std::string("( 'route', '")+nts<int64_t>(vid)+std::string("', '")+rtype+std::string("', 'name', '")+ifname+std::string("')");                
	       
                if( db.insert_with_sql(sql)){
                    errno = AL_DB_INSERT_FAILED;
                    LOG(LOG_ERROR, LAYOUT_CONF, "Insert data fialed, sql:%s", sql.c_str());
                    rc = -1;
                    goto END;
                }

                if(::al_router_insert(vid&0xFFFF,(uint8_t*)paddr, (uint8_t*)pmask, (uint8_t*)pnexthop, iptype, al_router_rtype(rtype.c_str()), oif)){
                    rc = -1;
                    goto END;
                }

                if(db.commit() ){
                    rc = -1;
                    errno = AL_DB_COMMIT_FAILED;
                    goto END;
                }

                json r;
                r["route_id"]=nts<int16_t>(vid);
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

    struct al_router_get: public al_router{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            
            int rc=0;
            try{
                json j;
                std::string sql=std::string("SELECT key3, key4, value FROM aware where key1='route' and is_live='1'");

                al_db db;
                std::map<std::string, std::map<std::string, std::string>> out;
                if( db.select(sql, [&out](int argc, char* argv[]){
                            out[argv[0]][argv[1]]=argv[2];
                        })){
                    rc=-1;
                    errno = AL_DB_SELECT_FAILED;
                    goto END;
                }

                j=out;
                retmsg=al_make_msg().make_success(j);
            }catch(std::runtime_error err){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception:%s", err.what());
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


    struct al_router_del : public al_router{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            
            int rc=0;
            try{

                std::string sql;
                std::string rid;
                std::string dst;
                std::string mask;
                uint32_t *paddr, *pmask;
                uint8_t iptype;
                uint8_t rtype;
				
                al_db db;
                LOG(LOG_INFO, LAYOUT_CONF, "Args:%s, msg:%s", args.c_str(),  msg.c_str());
                json j = json::parse(args);
                rid = j["id"].get<std::string>();

                if(search_route_by_id(rid, dst, mask, &iptype, &rtype)){
                    rc = -1;
                    goto END;
                }
				
                if( (rc = db.begin()) ){
                    rc = -1;
                    goto END;
                }
                
                sql=std::string("update aware set is_live = 0 where key3='")+rid+std::string("'");
                if( db.update_with_sql(sql)){
                    rc = -1;
                    errno = AL_DELETE_OBJ_FAILED;
                    LOG(LOG_ERROR, LAYOUT_CONF, "Delete route failed, %s", sql.c_str());
                    goto END;
                }

                if( iptype == AF_INET ){
                    paddr=ip::al_ipu<AF_INET>().to(dst);
                    if( paddr == NULL){
                        errno=AL_INVALID_IP;
                        return -1;
                    }

                    pmask=ip::al_ipu<AF_INET>().to(mask);
                    if( pmask == NULL){
                        errno=AL_INVALID_IP;
                        return -1;
                    }
                }else{
                    paddr=ip::al_ipu<AF_INET6>().to(dst);
                    if( paddr == NULL){
                        errno=AL_INVALID_IP;
                        return -1;
                    }

                    pmask=ip::al_ipu<AF_INET6>().to(mask);
                    if( pmask == NULL){
                        errno=AL_INVALID_IP;
                        return -1;
                    }
                }

                if(::al_router_delete((uint8_t*)paddr, (uint8_t*)pmask, iptype, rtype) < 0){
                    LOG(LOG_ERROR, LAYOUT_CONF, "Delete route from memory failed");
                }

                if(db.commit() ){
                    rc = -1;
                    errno = AL_DB_COMMIT_FAILED;
                    goto END;
                }
                
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


#endif /*_AL_ROUTER_H_*/
