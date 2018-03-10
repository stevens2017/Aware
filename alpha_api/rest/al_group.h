#ifndef AL_GROUP_H
#define AL_GROUP_H

#include "al_to_api.h"
#include "al_log.h"
#include "al_make_msg.h"
#include "db/al_db.h"
#include "al_ip_convert.h"
#include "al_comm.h"

namespace api{

    struct al_group{
    public:
        std::string search_group_by_name(const std::string& gname){
            std::string sql=std::string("SELECT key2 FROM aware where is_live=1 and key1 = 'group' and key3='name' and value='")+gname+std::string("'");

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

        int del_group_byid(al_db& db, const std::string& id){

            std::string sql=std::string("select count(*) from aware where key1='filter' and key3='group' and value=\'")+id+std::string("\'");
            int rc=-1;
            if(db.select(sql, [&rc](int argc, char* argv[])->int{
                rc=stn<int>(argv[0]);
            })){
                errno = AL_DB_SELECT_FAILED;
                return -1;
            }

            if( rc ){
                errno = AL_RESOURCE_BUSY;
                return -1;
            }
            
            sql=std::string("update aware set is_live = 0 where key2= \'")+id+std::string("\'");
            if( db.update_with_sql(sql) ){
                errno = AL_DELETE_OBJ_FAILED;
                return -1;
            }

            return 0;
        }

        int insert_group(al_db& db, int64_t gid, std::map<std::string, std::string>& map){
            std::string gids=nts<int64_t>(gid);
            return insert_group(db, gids, map);
        }

        int insert_group(al_db& db, std::string& gid, std::map<std::string, std::string>& map){

            std::string sql = std::string("insert into aware(key1, key2, key3, value) values ");
            sql += std::string("( 'group', '")+gid+std::string("', 'name', '")+map["name"]+std::string("'),");
            sql += std::string("( 'group', '")+gid+std::string("', 'hcheck', '")+map["hcheck"]+std::string("'),");
            sql += std::string("( 'group', '")+gid+std::string("', 'algorithm', '")+map["algorithm"]+std::string("')");
            if( db.insert_with_sql(sql)){
                return -1;
            }

            return 0;
        }
    };
    
    struct al_group_add:public al_group{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){

            int rc=0;
            try{
                json j=json::parse(msg);
                if( j.find("name") == j.end()){
                    rc = -1;
                    errno = AL_GROUP_NAME_NEEDED;
                    goto END;
                }

                uint8_t hcheck=0;
                uint8_t alg=0;

                al_db db;
                int i=0;
                std::string sql=std::string("SELECT count(*) FROM aware where is_live='1' and key1 = 'group' and key3='name' and value='")+j["name"].get<std::string>()+std::string("'");
                if( db.select(sql, [&i](int argc, char* argv[]){
                            i=stn<int>(argv[0]);
                        })){
                    rc=-1;
                    errno = AL_DB_SELECT_FAILED;
                    goto END;
                }

                if( i > 0){
                    rc=-1;
                    errno = AL_OBJECT_EXIST;
                    goto END;
                }else{

                    int64_t gid=al_db::newid();
                    if( gid < 0 ){
                        rc = -1;
                        goto END;
                    }

                    if( (rc = db.begin()) ){
                        rc = -1;
                        goto END;
                    }

                    std::map<std::string, std::string> data;
                    data["name"]=j["name"].get<std::string>();
                    data["hcheck"]="0";
                    data["algorithm"]="0";
                    if(insert_group(db, gid, data)){
                        rc = -1;
                        goto END;
                    }

                    if(db.commit() ){
                        rc = -1;
                        errno = AL_DB_COMMIT_FAILED;
                        goto END;
                    }

                    json r;
                    r["group_id"]=nts<int64_t>(gid);
                    retmsg=al_make_msg().make_success(r);
                }
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

    struct al_group_get:public al_group{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){

            int rc=0;
            try{

                std::string sql;
                std::string kid;
                al_db db;
                std::map<std::string, std::map<std::string, std::string>> out;
                
                json j= json::parse(args);
                if(j.find("id") == j.end()){
                    if( j.find("name") != j.end()){
                        kid=search_group_by_name(j["name"].get<std::string>());
                        if( kid.empty() ){
                            rc=-1;
                            goto END;
                        }
                    }
                }else{
                    kid=j["id"].get<std::string>();
                }

                if(kid.length()==0){
                    sql=std::string("SELECT key2, key3, value  FROM aware where key1='group' and is_live='1'");    
                }else{
                    sql=std::string("SELECT key2, key3, value  FROM aware where is_live='1' and key1='group'  and key2='")+kid+std::string("'");
                }

                
                if(db.select(sql, [&out](int argc, char* argv[])->int{
                            out[argv[0]][argv[1]]=argv[2];
                        })){
                    rc = -1;
                    errno = AL_DELETE_OBJ_FAILED;
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

    struct al_group_put:public al_group{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
#if 0
            int rc=0;
            try{

                std::string sql;
                std::string kid;
                json j= json::parse(args);
                if(j.find("id") == j.end()){
                    if( j.find("name") == j.end()){
                        rc = -1;
                        errno=AL_GROUP_DEL_FAILED;
                        goto END;
                    }else{
                        kid=search_group_byid(j["name"].get<std::string>());
                        if( kid.empty() ){
                            rc=-1;
                            goto END;
                        }
                    }
                }else{
                    kid=j["id"].get<std::string>();
                }

                sql=std::string("SELECT key2, key3, value FROM aware where key1 = 'group' and key2='")+kid+std::string("\'");

                al_db db;
                std::map<std::string, std::map<std::string, std::string>> out
                if( db.select(sql, [&out](int argc, char* argv[]){
                            out[argv[0]][argv[1]]=argv[2];
                        })){
                    rc=-1;
                    errno = AL_DB_SELECT_FAILED;
                    goto END;
                }

                retmsg=al_make_msg().make_success(ret.dump());
            }catch(std::runtime_error err){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception:%s", err.what());
                rc = -1;
                errno = AL_INTERVAL_ERROR;
                goto END;
            }
            rc = 0 ;
        END:
            retmsg=al_make_msg().make_failed(std::string());
            return rc;
#endif
            return 0;
        }
    };

    struct al_group_del:public al_group{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            int rc=0;
            try{
                std::string kid;
                std::string sql;
                json j= json::parse(args);
                if(j.find("id") == j.end()){
                    if( j.find("name") == j.end()){
                        rc = -1;
                        errno=AL_GROUP_DEL_FAILED;
                        goto END;
                    }else{
                        kid=search_group_by_name(j["name"].get<std::string>());
                        if( kid.empty() ){
                            goto END;
                        }
                    }
                }else{
                    kid = j["id"].get<std::string>();
                }

                al_db db;
                if( (rc = db.begin()) ){
                    rc = -1;
                    goto END;
                }

                if(del_group_byid(db, kid)){
                    rc = -1;
                    goto END;
                }

                if(db.commit() ){
                    rc = -1;
                    errno = AL_DB_COMMIT_FAILED;
                    goto END;
                }
                
                retmsg=al_make_msg().make_success(std::string(""));
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
}
#endif

    
