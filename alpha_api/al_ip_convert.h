#ifndef _AL_IP_CONVERT_H_
#define _AL_IP_CONVERT_H_

#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

template<typename T>
class al_ip_convert;

template<>
class al_ip_convert<struct in6_addr>{
    struct in6_addr data;
public:
    uint32_t* load(std::string& d){
        if(inet_pton(AF_INET6, d.c_str(), &data) != 1){
            return NULL;
        }else{
            return (uint32_t*)&data;
        }
    }
};

template<>
class al_ip_convert<struct in_addr>{
    struct in_addr data;
public:
    uint32_t* load(std::string& d){
        if(inet_pton(AF_INET, d.c_str(), &data) != 1){
            return NULL;
        }else{
            return (uint32_t*)&data;
        }        
    }
};

template<>
class al_ip_convert<uint32_t*>{
    char m_data[128];
public:
    const char*  load(uint32_t* d){
         if (inet_ntop(AF_INET6, d, m_data, 128) == NULL) {
             return m_data;
         }
    }
};

template<>
class al_ip_convert<uint32_t>{
    char m_data[128];
public:
    const char* load(uint32_t d){
         if (inet_ntop(AF_INET, &d, m_data, 128) == NULL) {
             return m_data;
         }
    }
};


/**********************************************************/
namespace ip{

template<int n>
class al_ipu;

template<>
class al_ipu<AF_INET>{
    struct in_addr data;
public:
    uint32_t* to(const std::string& d){
        if(inet_pton(AF_INET, d.c_str(), &data) != 1){
            return NULL;
        }else{
            return (uint32_t*)&data;
        }        
    }
};

template<>
class al_ipu<AF_INET6>{
    struct in6_addr data;
public:
    uint32_t* to(const std::string& d){
        if(inet_pton(AF_INET6, d.c_str(), &data) != 1){
            return NULL;
        }else{
            return (uint32_t*)&data;
        }
    }
};

template<int n>
class al_ips;

template<>
class al_ips<AF_INET6>{
    char m_data[128];
public:
    const char*  to(const uint32_t* d){
         if (inet_ntop(AF_INET6, d, m_data, 128) == NULL) {
             return m_data;
         }
    }
};

template<>
class al_ips<AF_INET>{
    char m_data[128];
public:
    const char*  to(const uint32_t* d){
         if (inet_ntop(AF_INET, d, m_data, 128) == NULL) {
             return m_data;
         }
    }
};

}
#endif

