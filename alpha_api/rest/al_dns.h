#ifndef _AL_DNS_H_
#define _AL_DNS_H_

#include "al_to_api.h"
#include "al_make_msg.h"
#include "db/al_db.h"
#include "al_ip_convert.h"
#include "al_log.h"
#include "al_element.h"
#include <net/if.h>

namespace api{

    class al_dns_base{
    public:
        
        int get_dns(std::map<std::string, std::string>& out){
            try{
                
                al_db db;
                std::string sql=std::string("select key2, key3, value from awrare where is_live=1 and key1='dns'");
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

        std::string get_reverse_domain(std::string& d){
            
            std::vector<std::string> vec;
            std::string strs, ret;
            std::string pattern=".";
            size_t size;

            strs=d;
            size=d.length();

            size_t pos = strs.find(pattern);
            while (pos != std::string::npos){
                std::string x = strs.substr(0,pos);
                vec.push_back(x);
                strs = strs.substr(pos+1,size);
                pos = strs.find(pattern);
            }
            vec.push_back(strs);

            std::reverse(vec.begin(), vec.end());

            for(std::vector<std::string>::iterator ite=vec.begin(); ite != vec.end(); ite++){
                if( ret.length() != 0 ){
                    ret+=std::string(".")+*ite;    
                }else{
                    ret+=*ite;
                }
            }

            return ret;
        }

        int get_dns_with_id(std::map<std::string, std::string>& out, std::string &id){
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
    
    struct al_dns_get: public al_dns_base{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            json j;

            std::map<std::string, std::string> out;
            if(get_dns(out)){
                retmsg = al_make_msg().make_failed(std::string());
            }else{
                j=out;
                retmsg=al_make_msg().make_success(j);
            }
                
            return 0;
        }
    };
    
    struct al_dns_add: public al_dns_base{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){

            int rc=0;
            try{
                json j = json::parse(args);

                if(j.find("domain") == j.end()){
                    rc = -1;
                    goto END;
                }

                std::string domain=j["domain"];

                if(j.find("gid") == j.end()){
                    rc = -1;
                    goto END;
                }

                std::string gid=j["gid"];

                int64_t dnsid=al_db::newid();
                if( dnsid < 0 ){
                    rc = -1;
                    goto END;
                }

                al_db db;
                if( (rc = db.begin(dnsid)) ){
                    rc = -1;
                    goto END;
                }

                std::string sql = std::string("insert into aware(key1, key2, key3, value) values ");
                sql += std::string("( 'dns', '")+nts<int64_t>(dnsid)+std::string("', 'domain', '")+domain+std::string("'),");
                sql += std::string("( 'dns', '")+nts<int64_t>(dnsid)+std::string("', 'gid', '")+gid+std::string("')");
	       
                if( db.insert_with_sql(sql)){
                    errno = AL_DB_INSERT_FAILED;
                    LOG(LOG_ERROR, LAYOUT_CONF, "Insert data fialed, sql:%s", sql.c_str());
                    rc = -1;
                    goto END;
                }
                
                if(al_dns_insert((uint8_t*)domain.c_str(), domain.length(), stn<int32_t>(gid),  dnsid&0xFFFFFFFF)){
                    rc = -1;
                    goto END;
                }

                if(db.commit() ){
                    rc = -1;
                    errno = AL_DB_COMMIT_FAILED;
                    goto END;
                }

                json r;
                r["dns_id"]=nts<int16_t>(dnsid);
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

    struct al_dns_del : public al_dns_base{

        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            int rc=0;
            try{
                std::string idstr;
                al_db db;
                std::string name, sql;
                std::map<std::string, std::string> dns;

                json j = json::parse(args);
                if( j.find("dns_id") == j.end()){
                    rc = -1;
                    goto END;
                }else{
                    idstr=j["dns_id"];
                }
                
                if(get_dns_with_id(dns, idstr)){
                    rc=-1;
                    goto END;
                }

                sql=std::string("update aware set is_live = 0 where key2='")+idstr+std::string("'");
                if( db.update_with_sql(sql)){
                    rc = -1;
                    errno = AL_OBJECT_EEXIST;
                    LOG(LOG_ERROR, LAYOUT_CONF, "Delete route failed, %s", sql.c_str());
                    goto END;
                }

                if(al_dns_delete((uint8_t*)dns["domain"].c_str(), dns["domain"].length())){
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
