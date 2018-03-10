#ifndef AL_GEOIP_H
#define AL_GEOIP_H

#include "al_to_api.h"
#include "al_make_msg.h"
#include "db/al_db.h"
#include "al_ip_convert.h"
#include "al_log.h"
#include "al_element.h"
#include <net/if.h>

namespace api{

    class al_geoip_base{
    public:
        
        int get_geoip(api::Set& out){
            try{
                
                al_db db;
                std::string sql=std::string("select key2, key3, value from awrare where is_live=1 and key1='geoip'");
                if( db.select(sql, [&out](int argc, char* argv[]){
                            out[argv[0]][argv[1]]=argv[2];
                        })){
                    return -1;
                }

            }catch(std::runtime_error err){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception:%s", err.what());
                errno = AL_INTERVAL_ERROR;
                return -1;
            }
            return 0;
        }

        int get_geoip_with_id(std::map<std::string, std::string>& out, std::string &id){
            try{

                al_db db;
                std::string sql=std::string("select key3, value from awrare where is_live=1 and key1='geoip' and key2='")+id+std::string("'");
                if( db.select(sql, [&out](int argc, char* argv[]){
                            out[argv[0]]=argv[1];
                        })){
                    return -1;
                }

            }catch(std::runtime_error err){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception:%s", err.what());
                errno = AL_INTERVAL_ERROR;
                return -1;
            }
            return 0;
        }
        
    };
    
    struct al_geoip_get: public al_geoip_base{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            json j;

            api::Set out;
            if(get_geoip(out)){
                retmsg = al_make_msg().make_failed(std::string());
            }else{
                j=out;
                retmsg=al_make_msg().make_success(j);
            }
                
            return 0;
        }
    };
    
    struct al_geoip_add: public al_geoip_base{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){

            int rc=0;
            try{

                uint32_t* pptr, *pmptr;
                uint8_t iptype;
                json j = json::parse(args);

                std::string ip_type=j["iptype"].get<std::string>();
                if( !ip_type.compare("ipv4")){
                    iptype=AF_INET;
                    pptr=ip::al_ipu<AF_INET>().to(j["subnet"].get<std::string>());
                    if( pptr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }

                    pmptr=ip::al_ipu<AF_INET>().to(j["netmask"].get<std::string>());
                    if( pmptr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }
                    
                }else if( !ip_type.compare("ipv6")){
                    iptype=AF_INET6;
                    pptr=ip::al_ipu<AF_INET6>().to(j["subnet"].get<std::string>());
                    if( pptr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }

                    pmptr=ip::al_ipu<AF_INET6>().to(j["netmask"].get<std::string>());
                    if( pmptr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }
                    
                }else{
                    errno=AL_INVALID_IP_TYPE;
                    goto END;
                }

                int64_t geoid=al_db::newid();
                if( geoid < 0 ){
                    rc = -1;
                    goto END;
                }

                al_db db;
                if( (rc = db.begin(geoid)) ){
                    rc = -1;
                    goto END;
                }

                std::string sql = std::string("insert into aware(key1, key2, key3, value) values ");
                sql += std::string("( 'geoip', '")+nts<int64_t>(geoid)+std::string("', 'iptype', '")+j["iptype"].get<std::string>()+std::string("'),");
                sql += std::string("( 'geoip', '")+nts<int64_t>(geoid)+std::string("', 'subnet', '")+j["address"].get<std::string>()+std::string("'),");
                sql += std::string("( 'geoip', '")+nts<int64_t>(geoid)+std::string("', 'netmask', '")+j["netmask"].get<std::string>()+std::string("')");
	       
                if( db.insert_with_sql(sql)){
                    errno = AL_DB_INSERT_FAILED;
                    LOG(LOG_ERROR, LAYOUT_CONF, "Insert data fialed, sql:%s", sql.c_str());
                    rc = -1;
                    goto END;
                }

                if(al_geoip_insert(geoid&0xFFFFFFFF, (uint8_t*)pptr, (uint8_t*)pmptr, iptype)){
                    rc = -1;
                    goto END;
                }

                if(db.commit() ){
                    rc = -1;
                    errno = AL_DB_COMMIT_FAILED;
                    goto END;
                }

                json r;
                r["geoip_id"]=nts<int16_t>(geoid);
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

    struct al_geoip_del : public al_geoip_base{

        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            int rc=0;
            try{
                std::string idstr;
                json j;
                al_db db;
                std::string name;
                std::map<std::string, std::string> geo;
                std::string sql;
                uint32_t* pptr, *pmptr;
                uint8_t iptype;

                j=json::parse(args);

                if( j.find("geo_id") != j.end()){
                    rc=-1;
                    goto END;
                }else{
                    idstr=j["geo_id"].get<std::string>();
                }
                
                if(get_geoip_with_id(geo, idstr)){
                    rc=-1;
                    goto END;
                }

                std::string ip_type=geo["iptype"];
                if( !ip_type.compare("ipv4")){
                    iptype=AF_INET;
                    pptr=ip::al_ipu<AF_INET>().to(geo["subnet"]);
                    if( pptr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }

                    pmptr=ip::al_ipu<AF_INET>().to(geo["netmask"]);
                    if( pmptr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }                    
                }else if( !ip_type.compare("ipv6")){
                    iptype=AF_INET6;
                    pptr=ip::al_ipu<AF_INET6>().to(geo["subnet"]);
                    if( pptr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }
                    
                    pmptr=ip::al_ipu<AF_INET6>().to(geo["netmask"]);
                    if( pmptr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }
                    
                }else{
                    errno=AL_INVALID_IP_TYPE;
                    goto END;
                }
                
                sql=std::string("update aware set is_live = 0 where key2='")+j["geo_id"].get<std::string>()+std::string("'");
                if( db.update_with_sql(sql)){
                    rc = -1;
                    errno = AL_OBJECT_EEXIST;
                    LOG(LOG_ERROR, LAYOUT_CONF, "Delete route failed, %s", sql.c_str());
                    goto END;
                }

                if(al_geoip_delete((uint8_t*)pptr, (uint8_t*)pmptr, iptype)){
                    rc = -1;
                    goto END;
                }

                if(db.commit() ){
                    rc = -1;
                    errno = AL_DB_COMMIT_FAILED;
                    goto END;
                }

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
