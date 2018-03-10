#ifndef _AL_TO_MAIN_H_
#define _AL_TO_MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    struct al_group_conf ;
    struct al_member_conf;

    typedef int (*group_cb)(uint32_t fid, uint32_t gid, struct al_group_conf* conf);
    
    int  al_write_kni_nic_conf(uint32_t kid, char* ifname, uint8_t* hwaddr);
    int16_t al_get_port_id(int domain, int bud, int devid, int function);
    int al_get_nic_id(uint32_t ifindex,  uint32_t* ifid);
    
   /*
     *  opt 1: init db, 0 not
     */
    int al_db_initial(const char* dir, int opt);
    int al_db_close();

    /*load all module*/
    int al_ip_load(void);
    int al_member_load(void);
    int al_filter_load(void);
    int al_ip_load(void);
    int al_route_load(void);
    int al_group_info(uint32_t fid, uint32_t gid, group_cb f);
    int al_mac_load(void);

    /*pcap*/
    void al_pcap_set(int id, int duration, int type);

    typedef int (*al_group_load_member_handler)(void *, struct al_member_conf *);
    
    int al_group_load_member(uint32_t gid, void* mc, al_group_load_member_handler f);
    int al_api_group_exist(uint32_t gid, uint8_t* sched);
#ifdef __cplusplus
}
#endif /* __cplusplus */
    
#endif /*_AL_TO_MAIN_H_*/

