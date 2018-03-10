#ifndef AL_MAKE_MSG_H
#define AL_MAKE_MSG_H

#include <errno.h>
#include "al_errmsg.h"
#include "json.h"
#include "al_comm.h"

class al_make_msg{
public:
    std::string make_failed(std::string msg){
        json j, info;
        j["status"]="failed";
        info["message"]=al_get_errmsg(errno);;
        info["extra"]=msg;
        j["error-id"]=errno;
        j["data"]=info;
        return j.dump();
    }

    std::string make_failed(json jp){
        json j, info;
        j["status"]="failed";
        info["message"]=al_get_errmsg(errno);;
        info["extra"]=jp;
        j["error-id"]=errno;
        j["data"]=info;
        return j.dump();
    }
        
    std::string make_success(std::string msg){
        json j, info;
        j["status"]="success";
        info["message"]=msg;
        j["data"]=info;
        return j.dump();
    }

    std::string make_success(json jp){
        json j, info;
        j["status"]="success";
        info["message"]=jp;
        j["data"]=info;
        return j.dump();
    }    
};

#endif
