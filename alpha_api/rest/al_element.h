#ifndef AL_ELEMENT_H
#define AL_ELEMENT_H

#include "al_to_api.h"
#include "al_make_msg.h"
#include "db/al_db.h"
#include "al_log.h"

namespace api{

    typedef std::map<std::string, std::map<std::string, std::string>>  Set;
    
    struct groups{
        static int all_groups(api::Set& g){
            std::string sql=std::string("select key2, key3, value from aware where is_live = 1 and key1='group'");
            al_db db;
            int rc=0;
            if(db.select(sql, [&g](int argc, char* argv[]){
                        g[argv[0]][argv[1]]=argv[2];
                    })){
                LOG(LOG_ERROR, LAYOUT_CONF, "Select failed, sql:%s", sql.c_str());
                rc= -1;
                errno=AL_DB_SELECT_FAILED;
            }
            return rc;
        }
    };
    
    struct filters{
        static int all_filters(api::Set& g){
            std::string sql=std::string("SELECT key2, key3, value FROM aware where is_live=1 and key1 = 'filter'");
            al_db db;
            int rc=0;
            if(db.select(sql, [&g](int argc, char* argv[]){
                        g[argv[0]][argv[1]]=argv[2];
                    })){
                LOG(LOG_ERROR, LAYOUT_CONF, "Select failed, sql:%s", sql.c_str());
                rc= -1;
                errno=AL_DB_SELECT_FAILED;
            }
            return rc;
        }
    };

    struct members{
        static int all_members(api::Set& g){
            std::string sql=std::string("SELECT key2, key3, value FROM aware where is_live=1 and key1 = 'member'");
            al_db db;
            int rc=0;
            if(db.select(sql, [&g](int argc, char* argv[]){
                        g[argv[0]][argv[1]]=argv[2];
                    })){
                LOG(LOG_ERROR, LAYOUT_CONF, "Select failed, sql:%s", sql.c_str());
                rc= -1;
                errno=AL_DB_SELECT_FAILED;
            }
            return rc;
        }
    };

    struct nics{
        static int all_nics(api::Set& g){
            std::string sql=std::string("SELECT key2, key3, value FROM aware where key1 = 'physical_port'");
            al_db db;
            int rc=0;
            if(db.select(sql, [&g](int argc, char* argv[]){
                        g[argv[0]][argv[1]]=argv[2];
                    })){
                LOG(LOG_ERROR, LAYOUT_CONF, "Select failed, sql:%s", sql.c_str());
                rc= -1;
                errno=AL_DB_SELECT_FAILED;
            }
            return rc;
        }
    };

    struct ips{
        static int all_ips(api::Set& g){
            std::string sql=std::string("SELECT key2, key3, value FROM aware where key1 = 'ipaddress' and is_live=1");
            al_db db;
            int rc=0;
            if(db.select(sql, [&g](int argc, char* argv[]){
                        g[argv[0]][argv[1]]=argv[2];
                    })){
                LOG(LOG_ERROR, LAYOUT_CONF, "Select failed, sql:%s", sql.c_str());
                rc= -1;
                errno=AL_DB_SELECT_FAILED;
            }
            return rc;
        }
    };

    struct hwaddr{
        static int all_hwaddr(std::map<std::string, std::string>& g){
            std::string sql=std::string("SELECT ifname, hwaddr FROM hwaddr");
            al_db db;
            int rc=0;
            if(db.select(sql, [&g](int argc, char* argv[]){
                        g[argv[0]]=argv[1];
                    })){
                LOG(LOG_ERROR, LAYOUT_CONF, "Select failed, sql:%s", sql.c_str());
                rc= -1;
                errno=AL_DB_SELECT_FAILED;
            }
            return rc;
        }
    };    
}

#endif
