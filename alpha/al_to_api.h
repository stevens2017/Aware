#ifndef _AL_TO_API_H_
#define _AL_TO_API_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct al_group_conf{
    uint8_t schd;
}al_group_conf_t;
    
enum{
    AL_PORT_TYPE_KNI=0,
    AL_PORT_TYPE_VPORT,
    AL_PORT_TYPE_PHYSICAL,
    AL_PORT_END
};
    
typedef enum rtype{
    POLICY_TABL=0,
    DIRECT_TABL,
    STATIC_TABL,
    DYNAMIC_TABL,
    GATEWAY_TABL,
    MAX_ROUTER
}rtype_t;

enum{
    AL_LLB=0,
    AL_SIMPLE_LB,
    AL_TCP_PROXY,
    AL_HTTP_PROXY,
    AL_GSLB,
    AL_GSLB_DOMAIN,
    AL_DHCP,
    AL_PROC_END
};

enum{
    AL_FLT_PKG=0,
    AL_FLT_NAT,
    AL_FLT_LLB,
    AL_FLT_SIMPLE_TRANS,
    AL_FLT_GSLB,
    AL_FLT_TCP_PROXY,
    AL_FLT_HTTP_PROXY,
    AL_FLT_END
};

enum{
    IP_ASS_STATIC=0,
    IP_ASS_DHCP=1,
    IP_ASS_VIP=2,
    IP_ASS_FLOAT=3,
    IP_ASS_END
};

typedef struct al_addr{
    union{
      struct{
          uint8_t len;
          uint8_t family;
      }addr;
      uint8_t caddr[0];
    }data;
}__attribute__((packed, aligned(1)))al_addr_t;

typedef struct al_addr4{
    union{
      struct{
          uint8_t len;
          uint8_t family;
          union{
              uint8_t caddr[4];
              uint32_t naddr;
          }addr;
      }addr;
      uint8_t caddr[0];
    }data;
}__attribute__((packed, aligned(1)))al_addr4_t;

typedef struct al_addr6{
    union{
      struct{
          uint8_t len;
          uint8_t family;
          union{
              uint8_t caddr[16];
              __uint128_t naddr;
          }addr;
      }addr;
      uint8_t caddr[0];
    }data;
}__attribute__((packed, aligned(1)))al_addr6_t;

typedef struct al_addr6 al_address_buf;

#define al_addr_len(d)  (d).data.addr.len
#define al_addr_af(d)  (d).data.addr.family
#define al_addr_addr(d)    (d).data.addr.addr.naddr
#define al_addr_addr_ptr(d) ((d).data.addr.addr.caddr)

#define al_p_addr_len(d)  (d)->data.addr.len
#define al_p_addr_af(d)  (d)->data.addr.family
#define al_p_addr_addr(d)    (d)->data.addr.addr.naddr
#define al_p_addr_addr_ptr(d) ((d)->data.addr.addr.caddr)
    
typedef uint32_t  uid_t;

typedef struct{
    uint8_t    iptype;
    uint8_t    proto;
    uint8_t    is_gslb;
    uint32_t   ip4;
    uint32_t   msk4;    
    __uint128_t  ip6;
    __uint128_t  msk6;

    al_addr4_t addr4;
    al_addr4_t mask4;
    al_addr6_t addr6;
    al_addr6_t mask6;    

    uid_t        id;
    uid_t        gid;
    uint16_t   port;
    uint8_t     sched;
    uint8_t     filter_type;
    uid_t        next_filter;

    uint8_t* dmptr;
    uint8_t   dmlen;
    
}al_api_filter;

typedef struct al_member_conf{
    uint16_t weight;
    uint16_t port;
    uint32_t mid;
    union{
        al_addr_t    addr;
        al_addr4_t   addr4;
        al_addr6_t   addr6;
    }addr;
}al_member_conf_t;
    
    /*ip*/
    extern int al_vlan_add_ipaddr(uint16_t vlanid,  const char* ip, const char* mask, uint8_t iptype, uint8_t ipsrc);

    /*group*/
    const char* al_group_alg(uint8_t alg);

    /*member*/
    int al_member_delete(uint32_t fid, uint32_t gid, uint32_t mid);
    int al_member_add(uint32_t fid, uint32_t gid, al_member_conf_t* conf);
    
    void al_dump_member_api(FILE* fp);
    void al_dump_filter_api(FILE* fp);
    
    /*vserver*/
    extern int al_vserver_add(uint16_t id, uint8_t service_type, uint8_t iptype, uint32_t* start, uint32_t* end, uint16_t port_start, uint16_t port_end, uint16_t pool, const char* domain);
    extern void al_dump_vserver(FILE* fp);
    /* return code -1, means object not found, 0 success*/
    extern int al_vserver_del(uint16_t vsid);

    /*type*/
    int al_filter_insert(al_api_filter* vs);
    int al_filter_del(al_api_filter* vs);
    const int8_t al_filter_type(const char* type);
    const char* al_filter_type_descr(uint8_t type);

    /*mac*/
    extern void al_output_mac(FILE* fp);
    /*arp*/
    extern void al_output_arp(FILE* fp);

    /*status*/
    void al_output_status(FILE* fp);

    /*geoip*/
    int al_geoip_insert(uint32_t id, uint8_t* addr, uint8_t* prefix, uint8_t iptype);    
    int al_geoip_delete(uint8_t* subnet, uint8_t* prefix, uint8_t iptype);
    int al_geoip_dump(FILE* fp);

    /*dns*/
    int al_dns_insert(uint8_t* d, int dlen, uint32_t gid, uint32_t  dnsid);
    int al_dns_delete(uint8_t* d, int dlen);
    int al_dns_dump(FILE* fp);
    
    /*session*/
    void al_output_session(void);

    /*route*/
    int al_router_insert(uint16_t id, uint8_t* subnet, uint8_t* prefix, uint8_t* nexthop, uint8_t iptype, uint8_t rtype, uint32_t oif);
    int al_router_delete(uint8_t* subnet, uint8_t* prefix, uint8_t iptype, uint8_t type);
    int al_router_rtype(const char* name);
    int al_dump_router(FILE* fp);

    int al_group_insert(uint32_t fid, uint32_t gid, uint8_t sched);
    void al_group_dump(FILE* fp);

    /*pcap*/
    int al_dev_internal_id(int id);
    void al_pcap_set(int id, int duration, int type);

    /*ip*/
    int al_del_ip(uint8_t ifindex, uint64_t ipid, uint32_t portid);
    int  al_add_ip(uint8_t ifindex, uint64_t ipid, uint32_t portid, al_addr_t* addr, al_addr_t* maddr, uint8_t ipclass);
    void al_dump_vport_ip(FILE* fp);
#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /*_AL_TO_API_H_*/

