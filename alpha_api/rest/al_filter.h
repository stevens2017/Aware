#ifndef AL_VSERVER_H
#define AL_VSERVER_H

#include "al_to_api.h"
#include "al_make_msg.h"
#include "db/al_db.h"
#include "al_ip_convert.h"
#include "al_log.h"
#include "al_to_main.h"
#include "al_errmsg.h"

namespace api{
    struct al_filter{
    public:
        std::string search_filter_by_name(std::string& name){
            std::string sql=std::string("SELECT key2 FROM aware where is_live=1 and key1 = 'filter' and key3='name' and value=\'")+name+std::string("\'");

            al_db db;
            std::vector<std::string> out;
            if( db.select(sql, [&out](int argc, char* argv[]){
                        out.push_back(argv[0]);
                    })){
                errno = AL_DB_SELECT_FAILED;
                return std::string();
            }

            if( out.size() != 1){
                errno=AL_OBJECT_EEXIST;
                return std::string();
            }

            return out[0];
        }
    };

    struct al_filter_add: public al_filter{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            
            int rc = 0;
            uint32_t* pptr, *paddr, *pmaddr;
            uint32_t* gid;
            uint16_t  port;
            uint8_t    sched;
            std::string sql;

            try{
                json j = json::parse(msg);
                al_api_filter f;
                if( ( j.find("iptype") == j.end() ) ||
                    ( j.find("port") == j.end() ) ||
                    ( j.find("address") == j.end() ) ||
                    ( j.find("name") == j.end() ) ||
                    ( j.find("filter_type") == j.end() ) ||
                    ( j.find("group") == j.end() ) ||                    
                    ( j.find("proto") == j.end() )){
                    errno = AL_FILTER_PARAMENTS_ERROR;
                    rc = -1;
                    goto END;
                }

                uint8_t iptype;
                if( j["iptype"].get<std::string>().compare("ipv4") == 0 ){
                    iptype = AF_INET;
                    paddr=ip::al_ipu<AF_INET>().to(j["address"].get<std::string>());
                    if( paddr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }else{
                        f.ip4=*paddr;
                        al_addr_af(f.addr4)=AF_INET;
                        al_addr_len(f.addr4)=sizeof(al_addr4_t);
                        al_addr_addr(f.addr4)=f.ip4;
                    }

                    if(j.find("address_mask") != j.end()){
                        pmaddr=ip::al_ipu<AF_INET>().to(j["address_mask"].get<std::string>());
                        if( pmaddr == NULL){
                            errno=AL_INVALID_IP;
                            rc = -1;
                            goto END;
                        }else{
                            f.msk4=*pmaddr;
                            al_addr_af(f.mask4)=AF_INET;
                            al_addr_len(f.mask4)=sizeof(al_addr4_t);
                            al_addr_addr(f.mask4)=f.msk4;
                        }
                    }else{
                        f.msk4=0;
                    }
                }else if( j["iptype"].get<std::string>().compare("ipv6") == 0 ){
                    iptype = AF_INET6;
                    paddr=ip::al_ipu<AF_INET6>().to(j["address"].get<std::string>());
                    if( paddr == NULL){
                        errno=AL_INVALID_IP;
                        rc = -1;
                        goto END;
                    }else{
                        f.ip6=*(__uint128_t*)paddr;
                        al_addr_af(f.addr6)=AF_INET6;
                        al_addr_len(f.addr6)=sizeof(al_addr6_t);
                        al_addr_addr(f.addr6)=f.ip6;                        
                    }
                    
                    if(j.find("address_mask") != j.end()){
                        pmaddr=ip::al_ipu<AF_INET6>().to(j["address_mask"].get<std::string>());
                        if( pmaddr == NULL){
                            errno=AL_INVALID_IP;
                            rc= -1;
                            goto END;
                        }else{
                            f.msk6=*(__uint128_t*)pmaddr;
                            al_addr_af(f.mask6)=AF_INET6;
                            al_addr_len(f.mask6)=sizeof(al_addr6_t);
                            al_addr_addr(f.mask6)=f.msk6;
                        }
                    }else{
                        f.msk6=0;  
                    }
                }else{
                    errno = AL_INVALID_IP_TYPE;
                    rc = -1;
                    goto END;
                }

                if((port=stn<uint16_t>(j["port"].get<std::string>())) > 65535 ){
                    errno = AL_INVALID_PORT;
                    rc = - 1;
                    goto END;
                }else{
                    port=port;
                }

                int filter_type=al_filter_type(j["filter_type"].get<std::string>().c_str());
                if( filter_type == -1 ){
                    errno = AL_FILTER_TYPE_INVALID;
                    rc = -1;
                    goto END;
                }

                uint8_t int_proto;
                std::string proto=j["proto"].get<std::string>();
                if( !(proto.compare("tcp") || proto.compare("udp"))){
                    errno = AL_PROTO_TYPE_UNKNOWN;
                    rc = -1;
                    goto END;
                }else{
                    if(proto.compare("tcp") == 0 ){
                        int_proto=IPPROTO_TCP;
                    }else if(proto.compare("udp") == 0 ){
                        int_proto=IPPROTO_UDP;
                    }else if(proto.compare("all") == 0 ){
                        int_proto=IPPROTO_IP;
                    }else{
                        errno=AL_PROTO_UNKNOW;
                        rc=-1;
                        goto END;
                    }
                }
                
                std::string vname=j["name"].get<std::string>();
                rc = 0;
                std::string sql_name=search_filter_by_name(vname);
                if( sql_name.length() > 0){
                    errno = AL_OBJECT_EXIST;
                    rc = -1;
                    goto END;
                }

                int64_t vid=al_db::newid();
                if( vid < 0 ){
                    rc = -1;
                    goto END;
                }

                al_db db;
                uint32_t tgid=stn<uint32_t>(j["group"].get<std::string>());
                if(al_api_group_exist(tgid, &sched)){
                    rc = -1;
                    errno = AL_OBJECT_EEXIST;
                    goto END;
                }
                
                if( (rc = db.begin(vid)) ){
                    rc = -1;
                    goto END;
                }

                sql = std::string("insert into aware(key1, key2, key3, value) values ");
                sql += std::string("( 'filter', '")+nts<int64_t>(vid)+std::string("', 'iptype', '")+j["iptype"].get<std::string>()+std::string("'),");
                sql += std::string("( 'filter', '")+nts<int64_t>(vid)+std::string("', 'port', '")+j["port"].get<std::string>()+std::string("'),");
                sql += std::string("( 'filter', '")+nts<int64_t>(vid)+std::string("', 'address', '")+j["address"].get<std::string>()+std::string("'),");
                sql += std::string("( 'filter', '")+nts<int64_t>(vid)+std::string("', 'name', '")+j["name"].get<std::string>()+std::string("'),");
                sql += std::string("( 'filter', '")+nts<int64_t>(vid)+std::string("', 'filter_type', '")+j["filter_type"].get<std::string>()+std::string("'),");
                sql += std::string("( 'filter', '")+nts<int64_t>(vid)+std::string("', 'group', '")+j["group"].get<std::string>()+std::string("'),");
                sql += std::string("( 'filter', '")+nts<int64_t>(vid)+std::string("', 'proto', '")+j["proto"].get<std::string>()+std::string("')");
                if( j.find("next_fitler") == j.end()){
                    sql += ",";
                    sql += std::string("( 'filter', '")+nts<int64_t>(vid)+std::string("', 'next_filter', '")+j["next_filter"].get<std::string>()+std::string("')");    
                }
                if(j.find("address_mask") != j.end()){
                    sql += ",";
                    sql += std::string("( 'filter', '")+nts<int64_t>(vid)+std::string("', 'address_mask', '")+j["address_mask"].get<std::string>()+std::string("')"); 
                }

                if( db.insert_with_sql(sql)){
                    rc = -1;
                    goto END;
                }

                f.iptype=iptype;
                f.proto=int_proto;
                f.filter_type=filter_type;
                f.port=port;
                f.is_gslb=0;
                f.sched=sched;
                f.next_filter=stn<uid_t>(j["next_filter"].get<std::string>());
                f.id=vid;
                f.gid = tgid;
                
                if(::al_filter_insert(&f)){
                    rc = -1;
                    goto END;
                }

                if(db.commit() ){
                    rc = -1;
                    errno = AL_DB_COMMIT_FAILED;
                    goto END;
                }

                json r;
                r["filter_id"]=nts<int16_t>(vid);
                retmsg = al_make_msg().make_success(r);
            }catch(std::runtime_error err){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception:%s", err.what());
                rc = -1;
                errno = AL_INTERVAL_ERROR;
                goto END;
            }catch(...){
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

    struct al_filter_get: public al_filter{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            
            int rc=0;
            try{
                std::string sql;
                std::string kid;
                std::string vname;
                json j= json::parse(args);
                if(j.find("id") == j.end()){
                    if( j.find("name") != j.end()){
                        vname = j["name"].get<std::string>();
                        kid = search_filter_by_name(vname);
                        if( kid.length() == 0 ){
                            rc = -1;
                            goto END;
                        }
                    }
                }else{
                    kid=j["id"].get<std::string>();
                }

                if( kid.length() ){
                    sql=std::string("SELECT key2, key3, value FROM aware  where key1 = 'filter' and key2 = \'"+kid+"\'");    
                }else{
                    sql=std::string("SELECT key2, key3, value FROM aware  where key1 = 'filter'");
                }

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

    struct al_filter_put : public al_filter{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
#if 0
            int rc;
            try{
                std::string sql;
                std::string kid;
                json j= json::parse(msg);
                if(j.find("id") == j.end()){
                    if( j.find("name") == j.end()){
                        rc = -1;
                        errno = AL_KEYWORDS_MISS;
                        goto END;
                    }else{
                        kid= search_filter_by_name(j["name"].get<std::string>());
                        if( kid.length() == 0 ){
                            rc = -1;
                            goto END;
                        }
                    }
                }else{
                    kid=j["id"].get<std::string>();
                }

                sql=std::string("SELECT key3, value FROM aware  where key1 = 'filter' and key2 = \'"+kid+"\'");

                al_db db;
                std::map<std::string, std::string> out;
                if( db.select(sql, [&out](int argc, char* argv[]){
                            out[argv[0]]=argv[1];
                        })){
                    rc=-1;
                    errno = AL_DB_SELECT_FAILED;
                    goto END;
                }
                
            }catch(std::runtime_error err){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception:%s", err.what());
                rc = -1;
                errno = AL_INTERVAL_ERROR;
                goto END;
            }
            rc=0;
        END:
            if( rc ){
                retmsg=al_make_msg().make_failed(std::string());     
            } 
            return rc;
#endif
            return 0;
       }
    };

    struct al_filter_delete : public al_filter{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            
            int rc=0;
            try{
                
                std::string sql;
                std::string kid;
                std::string vname;
                al_api_filter f;
                al_db db;
                LOG(LOG_INFO, LAYOUT_CONF, "Args:%s, msg:%s", args.c_str(),  msg.c_str());
                json j= json::parse(args);
                if(j.find("id") == j.end()){
                    if( j.find("name") == j.end()){
                        rc = -1;
                        errno=AL_GROUP_DEL_FAILED;
                        goto END;
                    }else{
                        vname=j["name"].get<std::string>();
                        kid= search_filter_by_name(vname);
                        if( kid.length() == 0 ){
                            rc = -1;
                            goto END;
                        }
                    }
                }else{
                    kid = j["id"].get<std::string>();
                }

                if( (rc = db.begin()) ){
                    rc = -1;
                    goto END;
                }
                
                sql=std::string("update aware set is_live = 0 where key2='")+kid+std::string("'");
                if( db.update_with_sql(sql)){
                    rc = -1;
                    errno = AL_DELETE_OBJ_FAILED;
                    goto END;
                }


                sql=std::string("SELECT key3, value FROM aware  where key1 = 'filter' and key2 = \'")+kid+std::string("\'");
                std::map<std::string, std::string> out;
                if( db.select(sql, [&out](int argc, char* argv[]){
                            out[argv[0]]=argv[1];
                        })){
                    rc=-1;
                    errno = AL_DB_SELECT_FAILED;
                    goto END;
                }else{
                    uint32_t* pptr;
                    uint32_t* gid;
                    uint16_t  port;
                    
                    uint8_t iptype;
                    if( out["iptype"].compare("ipv4") == 0 ){
                        iptype = AF_INET;
                        f.ip4=*ip::al_ipu<AF_INET>().to(out["address"]);
                        al_addr_af(f.addr4)=AF_INET;
                        al_addr_len(f.addr4)=sizeof(al_addr4_t);
                        al_addr_addr(f.addr4)=f.ip4;
                        
                        if( out.find("address_mask") != out.end()){
                            f.msk4=*ip::al_ipu<AF_INET>().to(out["address_mask"]);    
                        }else{
                            f.msk4=0;
                        }
                            
                        al_addr_af(f.mask4)=AF_INET;
                        al_addr_len(f.mask4)=sizeof(al_addr4_t);
                        al_addr_addr(f.mask4)=f.msk4;
                    }else if( out["iptype"].compare("ipv6") == 0 ){
                        iptype = AF_INET6;
                        f.ip6=*(__uint128_t*)ip::al_ipu<AF_INET6>().to(out["address"]);
                        al_addr_af(f.addr6)=AF_INET6;
                        al_addr_len(f.addr6)=sizeof(al_addr6_t);
                        al_addr_addr(f.addr6)=f.ip6;
                        
                        if( out.find("address_mask") != out.end()){
                            f.msk6=*(__uint128_t*)ip::al_ipu<AF_INET6>().to(out["address_mask"]);
                        }else{
                            f.msk6=0;
                        }
                        al_addr_af(f.mask6)=AF_INET6;
                        al_addr_len(f.mask6)=sizeof(al_addr6_t);
                        al_addr_addr(f.mask6)=f.msk6;                        
                    }

                    port=stn<uint16_t>(out["port"]);


                    int filter_type=al_filter_type(out["filter_type"].c_str());

                    uint8_t int_proto;
                    std::string proto=out["proto"];
                    if(proto.compare("tcp") == 0 ){
                        int_proto=IPPROTO_TCP;
                    }else if(proto.compare("udp") == 0 ){
                        int_proto=IPPROTO_UDP;
                    }else{
                        int_proto=IPPROTO_IP;                        
                    }

                    f.iptype=iptype;
                    f.proto=int_proto;
                    f.filter_type=filter_type;
                    f.port=port;
                    f.is_gslb=0;
                    f.next_filter=stn<uid_t>(out["next_filter"]);
                    f.id=stn<uid_t>(kid);
                    f.gid=stn<uid_t>(out["group"]);
             }                

                if(::al_filter_del(&f)){
                    rc = -1;
                    errno = AL_DELETE_OBJ_FAILED;
                    goto END;
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
#endif

