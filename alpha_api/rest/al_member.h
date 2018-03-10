#ifndef AL_MEMBER_H
#define AL_MEMBER_H

#include "al_to_api.h"
#include "json.h"
#include "al_comm.h"
#include "al_ip_convert.h"
#include "al_log.h"
#include "al_errmsg.h"
#include "db/al_db.h"
#include "al_make_msg.h"
#include "al_ip_convert.h"
#include "al_make_msg.h"
#include "al_element.h"

namespace api{

    struct al_member_base{

        static std::string group_id(std::string name){
            api::Set g;
            if( api::groups::all_groups(g)){
                return 0;
            }
            
            for(auto i=g.begin(); i!=g.end(); i++){
                for(auto k=i->second.begin(); k!=i->second.end(); k++){
                    if( (!k->first.compare("name")) ||(!k->second.compare(name))){
                        return i->first;
                    }
                }
            }
            return std::string();
        }

        static std::string group_exist_byid(std::string gid){
            api::Set g;
            if(api::groups::all_groups(g)){
                return 0;
            }
            
            for(auto i=g.begin(); i!=g.end(); i++){
                 if( i->first.compare(gid) == 0 ){
                     return gid;
                 }
            }
            return std::string();
        }
        
        static int  member_info(const std::string msg, std::string& gid, std::string& mid){
            json j = json::parse(msg);
            std::string group_name;
            gid="";
            mid="";
            if(j.find("group_id") == j.end()){
                if(j.find("group_name") != j.end()){
                    group_name=j["group_name"].get<std::string>();
                    gid=al_member_base::group_id(group_name);
                }else{
                    return -1;
                }
            }else{
                gid=j["group_id"].get<std::string>();
            }

            if(j.find("member_id")!=j.end()){
                mid=j["member_id"].get<std::string>();
            }
            
            return 0;
        }

        static int group_already_used(std::string& gid) {
            api::Set g;
            if( api::filters::all_filters(g)){
                return 0;
            }

            for(auto i=g.begin(); i!=g.end(); i++){
                for(auto k=i->second.begin(); k!=i->second.end(); k++){
                    if( (!k->first.compare("group")) ||(!k->second.compare(gid))){
                        return -1;
                    }
                }
            }

            return 0;
        }

        static int al_all_filters_with_group_id(std::string& gid, std::vector<uint32_t>& ary){
            int rc=0;
            api::Set g;
            if(api::filters::all_filters(g)){
                return -1;
            }

            for(auto i=g.begin(); i!=g.end(); i++){
                for(auto k=i->second.begin(); k!=i->second.end(); k++){
                    if( (!k->first.compare("group")) &&(!k->second.compare(gid))){
                        ary.push_back(stn<uint32_t>(i->first));
                    }
                }
            }
            
            return 0;
        }
        
    };

    struct al_member_add{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){

            int rc=0;
            uint32_t* pptr;
            al_member_conf_t conf;

            try{
                json j = json::parse(msg);

                if( ( j.find("iptype") == j.end() ) ||
                    ( j.find("address") == j.end() ) ||
                    ( j.find("port") == j.end() ) ){
                    errno = AL_MEMBER_PARAMENTS_ERROR;
                    goto END;
                }

                std::string gid;
                if( j.find("group") != j.end() ){
                    gid=al_member_base::group_exist_byid(j["group"].get<std::string>());
                }else if(j.find("group_name") != j.end()){
                    gid=al_member_base::group_id(j["group_name"].get<std::string>());
                }else{
                    errno = AL_MEMBER_PARAMENTS_ERROR;
                    goto END;
                }

                if( gid.length() == 0 ){
                    errno = AL_GROUP_EEXIST;
                    rc = -1;
                    goto END;
                }

                uint8_t iptype;
                if( j["iptype"].get<std::string>().compare("ipv4") == 0 ){
                    al_addr_af(conf.addr.addr4)=AF_INET;
                    pptr=ip::al_ipu<AF_INET>().to(j["address"].get<std::string>());
                    if( pptr == NULL){
                        errno=AL_INVALID_IP;
                        return -1;
                    }else{
                        al_addr_addr(conf.addr.addr4)=*(uint32_t*)pptr;
                    }
                }else if( j["iptype"].get<std::string>().compare("ipv6") == 0 ){
                    al_addr_af(conf.addr.addr6)=AF_INET6;
                    pptr=ip::al_ipu<AF_INET6>().to(j["address"].get<std::string>());
                    if( pptr == NULL){
                        errno=AL_INVALID_IP;
                        return -1;
                    }else{
                        al_addr_addr(conf.addr.addr6)=*(uint128_t*)pptr;
                    }
                }else{
                    errno = AL_INVALID_IP_TYPE;
                    rc = -1;
                    goto END;
                }

                if((conf.port=stn<uint16_t>(j["port"].get<std::string>())) > 65535 ){
                    errno = AL_INVALID_PORT;
                    rc = - 1;
                    goto END;
                }else{
                    conf.port=htons(conf.port);
                }

                al_db db;
                std::string sql;
                rc = 0;

                int64_t mid=al_db::newid();
                if( mid < 0 ){
                    rc = -1;
                    goto END;
                }

                std::vector<uint32_t> ary;
                if(al_member_base::al_all_filters_with_group_id(gid, ary)){
                    rc=-1;
                    goto END;
                }

                if( (rc = db.begin(mid)) ){
                    rc = -1;
                    goto END;
                }

                sql = std::string("insert into aware(key1, key2, key3, value) values ");
                sql += std::string("( 'member', '")+nts<int64_t>(mid)+std::string("', 'iptype', '")+j["iptype"].get<std::string>()+std::string("'),");
                sql += std::string("( 'member', '")+nts<int64_t>(mid)+std::string("', 'hcheck', '")+"0"+std::string("'),");
                sql += std::string("( 'member', '")+nts<int64_t>(mid)+std::string("', 'port', '")+j["port"].get<std::string>()+std::string("'),");
                sql += std::string("( 'member', '")+nts<int64_t>(mid)+std::string("', 'address', '")+j["address"].get<std::string>()+std::string("'),");
                sql += std::string("( 'member', '")+nts<int64_t>(mid)+std::string("', 'group', '")+j["group"].get<std::string>()+std::string("')");
                if( j.find("weight") == j.end()){
                    sql +=",";
                    sql += std::string("( 'member', '")+nts<int64_t>(mid)+std::string("', 'weight', '")+j["weight"].get<std::string>()+std::string("')");
                    conf.weight=stn<uint16_t>(j["weight"].get<std::string>());
                }else{
                    conf.weight=0;
                }
                conf.mid=mid&0xFFFF;

                if( db.insert_with_sql(sql)){
                    rc = -1;
                    goto END;
                }

                for(auto k=ary.begin(); k != ary.end(); k++){
                    if(::al_member_add(*k, stn<uint32_t>(gid), &conf)){
                        rc = -1;
                        goto END;
                    }
                }

                if(db.commit()){
                    rc = -1;
                    errno = AL_DB_COMMIT_FAILED;
                    goto END;
                }

                json r;
                r["member_id"]=nts<int16_t>(mid);
                retmsg = al_make_msg().make_success(r);
                rc = 0;
            }catch(std::runtime_error err){
                LOG(LOG_EMERG, LAYOUT_CONF, "Exception:%s", err.what());
                rc = -1;
                errno = AL_INTERVAL_ERROR;
                goto END;
            }

        END:
            if( rc ){
                retmsg = al_make_msg().make_failed(std::string());    
            }
            
            return rc;
        }
    };

    struct al_member_del{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            
            int rc=0;
            try{
                std::string mid, gid;
                api::Set outall;
                std::map<std::string, std::map<std::string, std::string>> out;
                std::string sql;
                
                al_db db;
                if(al_member_base::member_info(args, gid, mid)){
                    rc = -1;
                    errno=AL_GROUP_INFO_NEED;
                    goto END;
                }

                if(api::members::all_members(outall)){
                    rc=-1;
                    goto END;
                }

                for(auto k=outall.begin(); k!=outall.end(); k++){
                    if( mid.length() ){
                        if( (k->first.compare(mid) == 0) && (k->second["group"].compare(gid)==0) ){
                            out[mid]=k->second;
                        }
                    }else{
                        if( k->second["group"].compare(gid)==0){
                            out[mid]=k->second;
                        }
                    }
                }

                if( !out.size() ){
                    errno=AL_OBJECT_EEXIST;
                    rc = -1;
                    goto END;
                }

                std::vector<uint32_t> ary;
                if(al_member_base::al_all_filters_with_group_id(gid, ary)){
                    rc=-1;
                    goto END;
                }
                
                if( (rc = db.begin()) ){
                    rc = -1;
                    goto END;
                }

                sql=std::string("update aware set is_live = 0 where key2= '")+mid+std::string("'");
                if( db.update_with_sql(sql) ){
                    rc = -1;
                    errno = AL_DELETE_OBJ_FAILED;
                    goto END;
                }

                for(auto k=ary.begin(); k != ary.end(); k++){
                    if(::al_member_delete(*k, stn<uint32_t>(gid), stn<uint32_t>(mid))){
                        rc = -1;
                        goto END;
                    }
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

    struct al_member_get{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            std::string mid, gid;
            api::Set outall;
            std::map<std::string, std::map<std::string, std::string>> out;
            std::string sql;
            al_db db;
            int rc=0;
            if(al_member_base::member_info(args, gid, mid)){
                rc = -1;
                errno=AL_GROUP_INFO_NEED;
                goto END;
            }

            if(api::members::all_members(outall)){
                rc=-1;
                goto END;
            }

            for(auto k=outall.begin(); k!=outall.end(); k++){
                if( mid.length() ){
                    if( (k->first.compare(mid) == 0) && (k->second["group"].compare(gid)==0) ){
                        out[k->first]=k->second;
                    }
                }else{
                    if( k->second["group"].compare(gid)==0){
                        out[k->first]=k->second;
                    }
                }
            }
            
            if( out.size() ){
                json j=out;
                retmsg = al_make_msg().make_success(j);
                rc = 0;
            }else{
                rc = -1;
                errno = AL_OBJECT_EEXIST;
                goto END;
            }

            rc=0;
            
        END:
            if ( rc )
                retmsg = al_make_msg().make_failed(std::string());
            return rc;
        }
    };

}

#endif /*AL_MEMBER_H*/

