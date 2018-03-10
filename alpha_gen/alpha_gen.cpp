#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h> 
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <iostream>
#include <functional>
#include <string>
#include "../alpha_api/db/al_db.h"
#include "../alpha_api/al_to_main.h"

#define  MAX_NIC_NUM 255
#define  MAX_LINE 255
#define  EXE_CMD  "lspci -vmmkn"

#define K_SLOT  "Slot"
#define K_VENDOR "Vendor"
#define K_DEVICE   "Device"
#define K_DRIVER   "Driver"
#define K_CLASS      "Class"

#define GLOBAL_KEY "AGKEY"

struct nic_info{
    char devid[16];
    char odriver[16];
    char tclass[16];
    char nic_name[64];
    char hwaddr[64];
    char vendor[8];
    char device[8];
}nic[MAX_NIC_NUM];

static int ncouter=0;

static char* get_slot_prt(char* ptr);
static char* get_keyword(char* ptr);
static void output_nic_info(void);
static int  gen_initial_conf();
static int read_nic_name(const char* path, char name[64]);
static int bind_nic_to_driver(const char* driver, const char* devid, const char* vendor, const char* device, const char* odriver);
static int fetch_nic_hwaddr(const char* name, char* hwaddr);
//static int read_nic_ip(const char* name, int vlanid, int rc);

static int g_index=0;
static std::string hwinfo;
static int al_already_initial();

int al_db_set(std::string& key, std::string& val);

int main(int argc, char* argv[]){
    
    FILE* fp;
    int      s, rc;
    char    buf[MAX_LINE];
    char    fbuf[4096];
    char   fname[128];
    struct nic_info* nptr;
    int i;

#define CONFIG_FILE_DIR "/usr/local/aware"

    sprintf(buf, "mkdir -p %d", CONFIG_FILE_DIR);

    fp = popen( EXE_CMD, "r");
    if( fp == NULL ){
        return -1;
    }

    s=0;
    while (fgets(buf, MAX_LINE, fp) != NULL) {
        if( s == 0 ){
            if( memcmp(buf, K_SLOT, strlen(K_SLOT)) != 0 ){
                rc = -2;
                fclose(fp);
                return -2;
            }else{
                if(get_slot_prt(buf) == NULL){
                    s = 2;
                }else{
                    s=1;
                }
            }
        }else if( s == 1){
            if( buf[0] == '\n' ){
                if( memcmp(nic[ncouter].tclass, "0200", 4) == 0 ){
                    ncouter++;
                }
                s=0;
            }else{
                get_keyword(buf);                
            }
        }else{
            if( buf[0] == '\n' ){
                s=0;                            
            }
        }
    }

#if 0
    output_nic_info();
#endif

    nptr=nic;
    for(i=0; i<ncouter; i++){

        if(strlen(nptr->odriver) == 0) {
            nptr++;
            continue;
        }

#define NET_NIC_NAME  "/sys/bus/pci/devices/%s/driver/%s/net"
        sprintf(fname, NET_NIC_NAME, nptr->devid, nptr->devid);
        if(read_nic_name(fname, nptr->nic_name) < 0){
            pclose(fp);
            return -10;
        }else{
            fprintf(stderr, "Nic:%s %s\n", nptr->nic_name, nptr->devid);
            if(fetch_nic_hwaddr(nptr->nic_name, nptr->hwaddr) < 0){
                pclose(fp);
                return -11;
            }
        }        

        nptr++;
    }
    
    if( gen_initial_conf() < 0 ){
        pclose(fp);
        return -4;
    }


    std::vector<std::string> field;
    field.push_back(std::string("key2"));
    field.push_back(std::string("key3"));
    field.push_back(std::string("value"));
    field.push_back(std::string("is_live"));
    std::map<std::string, std::string> map;
    map["key1"]="physical_port";
    std::map<std::string, std::map<std::string, std::string>> out;

    al_db db;
    if( db.select(field, map, [&out](int argc, char** argv)->void{
                out[argv[0]][argv[1]]=argv[2];
                out[argv[0]]["is_live"]=argv[3];
            })){
        return -1;
    }

    std::string sql("delete from hwaddr");
    if( db.del_with_sql(sql)){
        fprintf(stderr, "Clear hwaddr failed\n");
        return -1;
    }

    for(auto  ite=out.begin(); ite != out.end(); ite++){
        std::map<std::string, std::string>& d=ite->second;
        if( d["is_live"].compare("0") == 0 ){
            continue;
        }
        if(fetch_nic_hwaddr(d["name"].c_str(), fbuf) < 0){
            pclose(fp);
            return -11;
        }else{
            std::string sql="insert into hwaddr(ifname, hwaddr) values(\""+ d["name"] +"\",\""+fbuf+"\")";
            if(db.insert_with_sql(sql)){
                fprintf(stderr, "Insert into hw address failed\n");
                return -1;
            }
        }
#if 1
        if(bind_nic_to_driver(d["driver"].c_str(), d["devid"].c_str(), d["vendor"].c_str(), d["device"].c_str(), d["old_driver"].c_str())<0){
       
        }
#else
        std::cout<<"Drvier:"<<d["driver"]<<" devid:"<<d["devid"]<<" vendor:"<<d["vendor"]<<" device:"<<d["device"]<<" old_driver"<<d["old_driver"]<<std::endl;
#endif
    }
    return 0;

}

static char* get_slot_prt(char* ptr){

    while(  *ptr ){
        if( *ptr++ == ':' ){
            break;
        }
    }

    if( ! *ptr ){
        return NULL;
    }

    while(  *ptr++ == ' '  );

    if( ! *ptr ){
        return NULL;
    }

    memcpy(nic[ncouter].devid, "0000:", 5);
    memcpy(&nic[ncouter].devid[5], ptr, 7);
    nic[ncouter].devid[12]='\0';

    return nic[ncouter].devid;
}

static char* get_keyword(char* ptr){

    char* cptr=NULL;
    
    if( memcmp(ptr, K_VENDOR, strlen(K_VENDOR)) == 0 ){
        cptr = nic[ncouter].vendor;
    }else if( memcmp(ptr, K_DEVICE, strlen(K_DEVICE)) == 0 ){
        cptr = nic[ncouter].device;
    }else if( memcmp(ptr, K_DRIVER, strlen(K_DRIVER)) == 0 ){
        cptr = nic[ncouter].odriver;
    }else if( memcmp(ptr, K_CLASS, strlen(K_CLASS)) == 0 ){
        cptr = nic[ncouter].tclass;
        memset(&nic[ncouter].odriver, '\0', 16);
    }else{
        return NULL;
    }
    
    while(  *ptr ){
        if( *ptr++ == ':' ){
            break;
        }
    }

    if( ! *ptr ){
        return NULL;
    }

    while(  *ptr++ == ' '  );

    if( ! *ptr ){
        return NULL;
    }

    while( *ptr != '\n' ){
        *cptr++=*ptr++;
    }
    *cptr='\0';

    return ptr;
}

static void output_nic_info(void){
    struct nic_info* nptr=nic;
    int i=0;
    for(i=0; i<ncouter; i++){
        fprintf(stderr, "Dev id:%s vendor:%s device:%s old driver:%s class:%s\n", nptr->devid, nptr->vendor, nptr->device, nptr->odriver, nptr->tclass);
        nptr++;
    }
}

static int bind_nic_to_driver(const char* driver, const char* devid, const char* vendor, const char* device, const char* odriver){

    const char* dfile_format = "/sys/bus/pci/drivers/%s/new_id";
    const char* ffile_format = "/sys/bus/pci/drivers/%s/bind";
    const char* ufile_format = "/sys/bus/pci/drivers/%s/unbind";
    
    char   file_path[1024];
    FILE* fp;
    int rc;

    sprintf(file_path, ufile_format, odriver);
    fp = fopen(file_path, "a");
    if( fp  == NULL){
        return -1;
    }

    rc = fprintf(fp, "%s", devid);
    if( rc < 0 ){
        fclose(fp);
        return -1;
    }

    fclose(fp);
    
    sprintf(file_path, dfile_format, driver);
    fp=fopen(file_path, "w");
    if(fp == NULL){
        return -1;
    }

    rc = fprintf(fp, "%s %s", vendor, device);
    if( rc < 0 ){
        fclose(fp);
        return -1;
    }

    fclose(fp);

    fprintf(stdout, "New id:%s driver:%s data:%s %s\n", file_path, driver, vendor, device);

    sprintf(file_path, ffile_format, driver);
    fp = fopen(file_path, "a");
    if( fp  == NULL){
        return -1;
    }

    rc = fprintf(fp, "%s", devid);
    if( rc < 0 ){
        fclose(fp);
        return -1;
    }

    fclose(fp);

    fprintf(stdout, "Bind:%s, devid:%s\n", file_path, devid);

    return 0;
}

static int  gen_initial_conf(){

    int64_t i, idv, vlanid, sshv, apiv, apim, sshp, apip, sshm;
    char* ptr;
    FILE* fp;
    int key;
    struct nic_info* nptr;
    char keystr[128];
    char valstr[128];
    int rc;

    if( (rc = al_already_initial() ) == 0 ){
        fprintf(stdout, "System already initial\n");
        return 0;
    }else if( rc < 0 ){
        fprintf(stdout, "Initial failed\n");
        return -1;
    }

    int kniid;
    al_db db;    
    
    i=0;
    nptr=nic;
    vlanid = 0;
    
    int knivport;
    //add vlan 0, only kni 
    rc  = al_db::newid();
    if( rc < 0 ){
        fprintf(stderr, "Get global key failed\n");
        return -1;
    }

    int cport;
    for(i=0; i<ncouter; i++){
        rc  = al_db::newid();
        if( rc < 0 ){
            fprintf(stderr, "Get global key failed\n");
            return -1;
        }
             
        char tid[32];
        char buf[32];

        sprintf(tid, "%d", rc);
        {
            std::map<std::string, std::string> map;
            map["key1"]="physical_port";
            map["key2"]=tid;
            map["key3"]="driver";
            map["value"]="igb_uio";
            if(db.insert(map)){
                return -1;
            }
        }

        {
            std::map<std::string, std::string> map;
            map["key1"]="physical_port";
            map["key2"]=tid;
            map["key3"]="devid";
            map["value"]=nptr->devid;
            if(db.insert(map)){
                return -1;
            }
        }

        {
            std::map<std::string, std::string> map;
            map["key1"]="physical_port";
            map["key2"]=tid;
            map["key3"]="device";
            map["value"]=nptr->device;
            if(db.insert(map)){
                return -1;
            }
        }

        {
            std::map<std::string, std::string> map;
            map["key1"]="physical_port";
            map["key2"]=tid;
            map["key3"]="vendor";
            map["value"]=nptr->vendor;
            if(db.insert(map)){
                return -1;
            }
        }

        {
            std::map<std::string, std::string> map;
            map["key1"]="physical_port";
            map["key2"]=tid;
            map["key3"]="old_driver";
            map["value"]=nptr->odriver;
            if(db.insert(map)){
                return -1;
            }
        }

        {
            std::map<std::string, std::string> map;
            map["key1"]="physical_port";
            map["key2"]=tid;
            map["key3"]="name";
            map["value"]=nptr->nic_name;
            if(db.insert(map)){
                return -1;
            }
        }
#if 0
        {
            std::map<std::string, std::string> map;
            map["key1"]="physical_port";
            map["key2"]=tid;
            map["key3"]="hwaddr";
            map["value"]=nptr->hwaddr;
            if(db.insert(map)){
                return -1;
            }
        }
#endif
        nptr++;
    }

    return 0;
}

static int read_nic_name(const char* path, char name[64])
{
    DIR * dir;
    struct dirent * ptr;
    int i;
    
    dir = opendir(path);
    if( dir == NULL ){
        fprintf(stderr, "Cann't open path:%s - %s\n", path, strerror(errno));
        return -1;
    }
    
    while((ptr = readdir(dir)) != NULL)
    {
        if( (memcmp(ptr->d_name, "..", 2) == 0) || (memcmp(ptr->d_name, ".", 1) == 0) ){
            continue;
        }else{
            sprintf(name, "%s", ptr->d_name);
            break;
        }
    }
    closedir(dir);

    return 0;
}

static int fetch_nic_hwaddr(const char* iface, char* hwaddr){

    int fd;
    struct ifreq ifr;
    unsigned char *mac;

    memset(&ifr, 0, sizeof(ifr));

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if( fd == -1 ){
        return -1;
    }

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name , iface , IFNAMSIZ-1);

    if (0 == ioctl(fd, SIOCGIFHWADDR, &ifr)) {
        mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;

        //display mac address
        sprintf( hwaddr, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X" , mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        close(fd);
        return 0;
    }

    close(fd);
    return -1;
}
#if 0
static int read_nic_ip(const char* name, int vlanid, int id){
    
    struct ifaddrs *ifaddr, *ifa;
    int family, s, s1, n;
    struct in_addr netaddr;
    char keystr[128], *ptr;
    char host[NI_MAXHOST], netmask[NI_MAXHOST], netaddr_s[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) 
    {
        fprintf(stderr, "Getifaddrs failed, msg:%s\n", strerror(errno));
        return 0;
    }

    n=0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) 
    {
        if (ifa->ifa_addr == NULL)
            continue;  

        if((ifa->ifa_addr->sa_family==AF_INET) && (strcmp(ifa->ifa_name, name)==0))
        {
            s=getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            s1=getnameinfo(ifa->ifa_netmask,sizeof(struct sockaddr_in), netmask, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if ((s != 0) || (s1 != 0))
            {
                fprintf(stderr, "getnameinfo() failed: %s\n", gai_strerror(s));
                goto Exit;
            }
            n++;

            al_db db;
            char tid[32];
            char buf[32];
            sprintf( tid, "%d", id);

            int64_t ipid = al_db::newid();
            if( ipid < 0 ){
                fprintf(stderr, "Get global key failed\n");
                return -1;
            }


            {
                std::map<std::string, std::string> map;
                map["key1"]="vlan";
                map["key2"]=tid;
                map["key3"]="ip";
                sprintf(buf, "ip_%d", ipid);
                map["key4"]=buf;
                map["key5"]="address";
                map["value"]=host;
                if(db.insert(map)){
                    return -1;
                }
            }
        
            {
                std::map<std::string, std::string> map;
                map["key1"]="vlan";
                map["key2"]=tid;
                map["key3"]="ip";
                sprintf(buf, "ip_%d", ipid);
                map["key4"]=buf;
                map["key5"]="netmask";
                map["value"]=netmask;
                if(db.insert(map)){
                    return -1;
                }
            }

            {
                std::map<std::string, std::string> map;
                map["key1"]="vlan";
                map["key2"]=tid;
                map["key3"]="ip";
                sprintf(buf, "ip_%d", ipid);
                map["key4"]=buf;
                map["key5"]="iptype";
                map["value"]="ipv4";
                if(db.insert(map)){
                    return -1;
                }
            }

            netaddr.s_addr = ((struct  sockaddr_in*)ifa->ifa_addr)->sin_addr.s_addr & ((struct  sockaddr_in*)ifa->ifa_netmask)->sin_addr.s_addr;

            int rid = al_db::newid();
            if( rid < 0 ){
                fprintf(stderr, "Get global key failed\n");
                return -1;
            }else{
                char tid[32];
                sprintf(tid, "%d", rid);

                {
                    std::map<std::string, std::string> map;
                    map["key1"]="route";
                    map["key2"]="direct";
                    map["key3"]=tid;
                    map["key4"]="dstip";
                    map["value"]=std::string(inet_ntoa(netaddr));
                    if(db.insert(map)){
                        return -1;
                    }
                }

                {
                    std::map<std::string, std::string> map;
                    map["key1"]="route";
                    map["key2"]="direct";
                    map["key3"]=tid;
                    map["key4"]="netmask";
                    map["value"]=netmask;
                    if(db.insert(map)){
                        return -1;
                    }
                }

                {
                    std::map<std::string, std::string> map;
                    map["key1"]="route";
                    map["key2"]="direct";
                    map["key3"]=tid;
                    map["key4"]="iptype";
                    map["value"]="ipv4";
                    if(db.insert(map)){
                        return -1;
                    }
                }

                {
                    std::map<std::string, std::string> map;
                    map["key1"]="route";
                    map["key2"]="direct";
                    map["key3"]=tid;
                    map["key4"]="vlan_id";
                    sprintf(buf, "%d", id);
                    map["value"]=buf;
                    if(db.insert(map)){
                        return -1;
                    }
                }
            }
        }
    }

Exit:
    freeifaddrs(ifaddr);
    return n;
}
#endif
static int al_already_initial(){
    return al_db_initial("/usr/local/aware", 1);
}

