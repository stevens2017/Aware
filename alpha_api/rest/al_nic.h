#ifndef _AL_NIC_H_
#define _AL_NIC_H_

#include "al_to_api.h"
#include "al_make_msg.h"
#include "db/al_db.h"
#include "al_ip_convert.h"
#include "al_log.h"

namespace api{

    class al_nic{ 
    };
    
    struct al_nic_modify: public al_nic{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){

            int rc=0;
            try{
                json j = json::parse(args);
                al_db db;
                std::string sql=std::string("update aware set is_live=1 where key2='")+j["nic_id"].get<std::string>()+std::string("'");
                if( db.update_with_sql(sql)){
                    rc = -1;
                    errno = AL_OBJECT_EEXIST;
                    goto END;
                }

                retmsg = al_make_msg().make_success(std::string());
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

    struct al_nic_get: public al_nic{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            int rc=0;
            try{
                api::Set outall;
                if(api::nics::all_nics(outall)){
                    goto END;
                }
                json j=outall;
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

    struct al_nic_del : public al_nic{
        int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
            int rc=0;
            try{
                json j=json::parse(args);
                al_db db;
                std::string sql=std::string("update aware set is_live = 0 where key2='")+j["nic_id"].get<std::string>()+std::string("'");
                if( db.update_with_sql(sql)){
                    rc = -1;
                    errno = AL_OBJECT_EEXIST;
                    LOG(LOG_ERROR, LAYOUT_CONF, "Delete route failed, %s", sql.c_str());
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


#endif /*_AL_NIC_H_*/
