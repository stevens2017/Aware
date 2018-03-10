#ifndef _AL_DUMP_PARAMENT_H_
#define _AL_DUMP_PARAMENT_H_

#include <cstdio>
#include <cstdlib>
#include "json.h"
#include "al_comm.h"
#include "db/al_db.h"
#include "al_errmsg.h"
#include "al_to_api.h"
#include "al_log.h"

#define DUMP_PATH "/tmp"

struct al_dump{
    int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
        std::string str=std::string(DUMP_PATH)+std::string("/")+std::string("all.text");
        FILE* fp=fopen(str.c_str(), "w");
        if( fp == NULL){
            return -1;
        }

        al_dump_filter_api(fp);
        al_dump_member_api(fp);

        fclose(fp);
        return 0;
   }
};

struct al_dump_arp{
    int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
        std::string str=std::string(DUMP_PATH)+std::string("/")+std::string("arp.txt");
        FILE* fp=fopen(str.c_str(), "w");
        if( fp == NULL){
            return -1;
        }

        al_output_arp(fp);
        fclose(fp);
        return 0;
   }
};

struct al_dump_filter{
    int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
        std::string str=std::string(DUMP_PATH)+std::string("/")+std::string("filter.txt");
        FILE* fp=fopen(str.c_str(), "w");
        if( fp == NULL){
            return -1;
        }

        al_dump_filter_api(fp);
        fclose(fp);
        return 0;
   }
};

struct al_dump_member{
    int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
        std::string str=std::string(DUMP_PATH)+std::string("/")+std::string("member.txt");
        FILE* fp=fopen(str.c_str(), "w");
        if( fp == NULL){
            return -1;
        }

        al_dump_member_api(fp);
        fclose(fp);
        return 0;
   }
};

#if 0
struct al_dump_mac{
    int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
        std::string str=std::string(DUMP_PATH)+std::string("/")+std::string("mac.txt");
        FILE* fp=fopen(str.c_str(), "w");
        if( fp == NULL){
            return -1;
        }

        al_output_mac(fp);

        fclose(fp);
        return 0;
   }
};
#endif
struct al_dump_sys_status{
    int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
        std::string str=std::string(DUMP_PATH)+std::string("/")+std::string("status.txt");
        FILE* fp=fopen(str.c_str(), "w");
        if( fp == NULL){
            return -1;
        }

        al_output_status(fp);

        fclose(fp);
        return 0;
   }
};

struct al_dump_route{
    int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
        std::string str=std::string(DUMP_PATH)+std::string("/")+std::string("route.txt");
        FILE* fp=fopen(str.c_str(), "w");
        if( fp == NULL){
            return -1;
        }

        al_dump_router(fp);

        fclose(fp);
        return 0;
   }
};

struct al_dump_ip{
    int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
        std::string str=std::string(DUMP_PATH)+std::string("/")+std::string("ip.text");
        FILE* fp=fopen(str.c_str(), "w");
        if( fp == NULL){
            return -1;
        }

        al_dump_vport_ip(fp);

        fclose(fp);
        return 0;
   }
};

struct al_dump_session{
    int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
        al_output_session();
        return 0;
   }
};

struct al_dump_geoip{
    int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
        
        std::string str=std::string(DUMP_PATH)+std::string("/")+std::string("geoip.text");
        FILE* fp=fopen(str.c_str(), "w");
        if( fp == NULL){
            return -1;
        }
        al_geoip_dump(fp);
        fclose(fp);
        
        return 0;
   }
};

struct al_dump_dns{
    int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
        std::string str=std::string(DUMP_PATH)+std::string("/")+std::string("dns.text");
        FILE* fp=fopen(str.c_str(), "w");
        if( fp == NULL){
            return -1;
        }
        al_dns_dump(fp);
        fclose(fp);
        return 0;
   }
};

struct al_pcap{
    int operator()(const std::string &args,  const std::string &msg, std::string &retmsg){
      int id=-1;
      int t=-1;
      int type=-1;
      
      try{
           json j= msg.length() == 0 ? json::parse("{}") : json::parse(msg);
           std::string name;
	   LOG(LOG_ERROR, LAYOUT_CONF, "Pcap log");
           if( j.find("name") != j.end()){
               name=j["name"].get<std::string>();
               al_db db;
               std::string sql=std::string("SELECT key2 FROM aware where key1 = 'physical_port' and key3='name' and value='")+name+std::string("'");
               if( db.select(sql, [&id](int argc, char* argv[])->void{
                           id=atoi(argv[0]);
                       })){
                   errno=AL_PORT_NOT_FOUND;
                   return -1;
               }

               if((id=al_dev_internal_id(id)) > 0){
                   errno=AL_PORT_NOT_FOUND;
                   return -1;
               }
	   }

	   if( j.find("duration") != j.end() ){
	     t=j["duration"].get<int>();
	   }
	   
	   if( j.find("type") != j.end() ){
	     type=j["type"].get<int>();
	   }
	   LOG(LOG_ERROR, LAYOUT_CONF, "Pcap log %d %d %d", id, t, type);
           al_pcap_set(id, t, type);
       }catch(...){
           errno = AL_INTERVAL_ERROR;
           return -1;
       }
      return 0;
   }
};

#endif /*_AL_DUMP_PARAMENT_H_*/

