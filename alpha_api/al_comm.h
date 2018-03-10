#ifndef _AL_COMM_H_
#define _AL_COMM_H_

#include <cstring>
#include "json.h"

template <typename T>
std::string nts(T n){
    std::ostringstream ss;
    ss << n;
    return ss.str();
}

template <typename T>
T stn(const std::string &t){
    std::istringstream ss(t);
    T r;
    return ss >> r ? r : 0;
}

template <>
inline uint8_t stn<uint8_t>(const std::string &t){
    return atoi(t.c_str())&0xFF;
}

using json = nlohmann::json;

#endif /*_AL_COMM_H_*/

