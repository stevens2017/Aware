#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <asm/types.h>
#include <sys/socket.h>
#include "alpha.h"
#include "al_router.h"
#include "al_log.h"
#include "al_to_api.h"
#include "al_to_main.h"
#include "al_ip.h"

#define BUFFER_SIZE (1024*10)
#define ROUTE_DEBUG 0

typedef struct nl_req_s nl_req_t;  

struct nl_req_s {
  struct nlmsghdr hdr;
  struct rtgenmsg gen;
};

static void rtnl_dump_route(struct nlmsghdr *nlh)
{
    struct  rtmsg *route_entry;
    struct  rtattr *route_attribute;

    int     route_attribute_len = 0;
    uint32_t           route_netmask = 0;
    ipcs_t               route_mask;
    uint8_t*            route_nexthop=NULL;
    uint8_t*            route_dest=NULL;
    uint32_t           oif=0, oifid;

#if ROUTE_DEBUG
    char    destination_address[128];
    char    gateway_address[128];
#endif
    uint8_t af;

    route_entry = (struct rtmsg *) NLMSG_DATA(nlh);
    if (route_entry->rtm_table != RT_TABLE_MAIN)
        return;
    else{
        af=0xFF;
    }

    route_netmask = route_entry->rtm_dst_len;
    route_attribute = (struct rtattr *) RTM_RTA(route_entry);
    /* Get the route atttibutes len */
    route_attribute_len = RTM_PAYLOAD(nlh);
    /* Loop through all attributes */
    for ( ;RTA_OK(route_attribute, route_attribute_len); \
          route_attribute = RTA_NEXT(route_attribute, route_attribute_len))
    {
        af=((route_entry->rtm_family == AF_INET) || (route_entry->rtm_family == AF_INET)) ? route_entry->rtm_family  : af;
        /* Get the destination address */
        if (route_attribute->rta_type == RTA_DST)
        {
#if ROUTE_DEBUG
            inet_ntop(route_entry->rtm_family, RTA_DATA(route_attribute), destination_address, sizeof(destination_address));
#endif
            route_dest=RTA_DATA(route_attribute);
        }else if(route_attribute->rta_type == RTA_OIF){
            oif=*(uint32_t*)RTA_DATA(route_attribute);
        }else{
		/* Get the gateway (Next hop) */
		if (route_attribute->rta_type == RTA_GATEWAY)
		{
#if ROUTE_DEBUG                    
			inet_ntop(route_entry->rtm_family, RTA_DATA(route_attribute), gateway_address, sizeof(gateway_address));
#endif
                        route_nexthop=RTA_DATA(route_attribute);
		}
       }
    }

    if(al_get_nic_id(oif, &oifid)){
        LOG(LOG_INFO, LAYOUT_IP, "Get  if %u  failed, ", oif);
        return;
    }


    if( af != 0xFF ){
        
        if( af == AF_INET ){
            route_mask.ipv4=tomask_ipv4(route_netmask);
        }else if( af == AF_INET6){
            route_mask.ipv6=tomask_ipv6(route_netmask);
        }else{
            return;
	}

        if( al_router_insert(0, route_dest, (uint8_t*)&route_mask, route_nexthop,  af,  route_nexthop != NULL ? GATEWAY_TABL : DYNAMIC_TABL, oifid)){
#if ROUTE_DEBUG
            LOG(LOG_EMERG, LAYOUT_IP, "route to destination --> %s/%d  and gateway %s failed", destination_address, route_netmask, gateway_address);
#else
            LOG(LOG_EMERG, LAYOUT_IP, "route to destination  insert failed");
#endif
        }

#if ROUTE_DEBUG
        LOG(LOG_INFO, LAYOUT_IP, "route to destination --> %s/%d  and gateway %s", destination_address, route_netmask,  gateway_address);
#endif
    }
}

static int  proc_loop(int sock, struct sockaddr_nl *addr)
{
    int     received_bytes = 0;
    struct  nlmsghdr *nlh;
    unsigned char    route_netmask = 0;
    ipcs_t                route_mask;
    uint8_t             *route_dest;
    uint8_t             *route_nexthop;
    uint8_t             af;

    #if ROUTE_DEBUG
    struct in_addr route_dest4;
    struct in6_addr route_dest6;
    #endif
    char    gateway_address[128];
    char    destination_address[128];
    
    struct  rtmsg *route_entry; 
    struct  rtattr *route_attribute;
    int     route_attribute_len = 0;
    uint32_t     oif, oifid;
    char    buffer[BUFFER_SIZE];

    bzero(destination_address, sizeof(destination_address));
    bzero(gateway_address, sizeof(gateway_address));

    bzero(buffer, sizeof(buffer));

    while (g_sys_stop)
    {
    
        received_bytes = recv(sock, buffer, sizeof(buffer), 0);
        if (received_bytes < 0){
            LOG(LOG_ERROR, LAYOUT_IP, "Receved failed, %s", strerror(errno));
            return -1;            
        }


        nlh = (struct nlmsghdr *) buffer;
        if (nlh->nlmsg_type == NLMSG_DONE)
            break;

        if (addr->nl_groups == (RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE))
            break;
    }


    for ( ; NLMSG_OK(nlh, received_bytes); \
                    nlh = NLMSG_NEXT(nlh, received_bytes))
    {
        route_entry = (struct rtmsg *) NLMSG_DATA(nlh);

        route_netmask = route_entry->rtm_dst_len;
        route_attribute = (struct rtattr *) RTM_RTA(route_entry);

        route_nexthop=NULL;
        
        route_attribute_len = RTM_PAYLOAD(nlh);
        for ( ; RTA_OK(route_attribute, route_attribute_len); \
            route_attribute = RTA_NEXT(route_attribute, route_attribute_len))
        {
            if (route_attribute->rta_type == RTA_DST)
            {
                inet_ntop(route_entry->rtm_family, RTA_DATA(route_attribute), destination_address, sizeof(destination_address));
                route_dest=(uint8_t*)RTA_DATA(route_attribute);
            }

            if (route_attribute->rta_type == RTA_GATEWAY)
            {
                inet_ntop(route_entry->rtm_family, RTA_DATA(route_attribute), gateway_address, sizeof(gateway_address));
                route_nexthop=(uint8_t*)RTA_DATA(route_attribute);
            }

            if (route_attribute->rta_type == RTA_OIF)
            {
                oif=*(uint32_t*)RTA_DATA(route_attribute);
            }
        }

        LOG(LOG_INFO, LAYOUT_IP, "if:%u------------------> %s %s %d", oif, gateway_address, destination_address, route_netmask);

        if(al_get_nic_id(oif, &oifid)){
            LOG(LOG_INFO, LAYOUT_IP, "Get  if %u  failed, ", oif);
            continue;
        }

        
        af=0xFF;
        if( route_entry->rtm_family == AF_INET ){
            route_mask.ipv4=tomask_ipv4(route_netmask);
            af=AF_INET;
           #if ROUTE_DEBUG
           inet_pton(AF_INET, destination_address, &route_dest4); 
           #endif 
        }else if( route_entry->rtm_family == AF_INET6){
           route_mask.ipv6=tomask_ipv6(route_netmask);
           af=AF_INET6;
           #if ROUTE_DEBUG
           inet_pton(AF_INET6, destination_address, &route_dest6); 
           #endif 
        }else{
           continue; 
	}
        
        /* Now we can dump the routing attributes */
        if(nlh->nlmsg_type == RTM_NEWROUTE ){
            if( al_router_insert(0, route_dest, (uint8_t*)&route_mask, route_nexthop, af,  route_nexthop != NULL ? GATEWAY_TABL : DYNAMIC_TABL, oifid)){
#if ROUTE_DEBUG
                LOG(LOG_EMERG, LAYOUT_IP, "Route to destination --> %s/%d and gateway %s failed", destination_address, route_netmask,  gateway_address);
#else
                LOG(LOG_EMERG, LAYOUT_IP, "Route to destination insert failed");
#endif
            }
        }
        
        if(nlh->nlmsg_type == RTM_DELROUTE){

            if( al_router_delete(route_dest, (uint8_t*)&route_mask, route_entry->rtm_family,  0)){
#if ROUTE_DEBUG
                LOG(LOG_EMERG, LAYOUT_IP, "route to destination --> %s/%d and gateway %s failed", destination_address, route_netmask,  gateway_address);
#else
                LOG(LOG_EMERG, LAYOUT_IP, "route to destination  insert failed");
#endif
            }
        }
    }

    return 0;
}

int  al_sync_route(void) {
  int fd;
  struct sockaddr_nl local;
  struct sockaddr_nl kernel;
 
  struct msghdr rtnl_msg;
  struct iovec io;

  nl_req_t req;
  char reply[BUFFER_SIZE];

  pid_t pid = getpid();
  int end = 0;

  fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if( fd < 0 ){
      return -1;
  }

  memset(&local, 0, sizeof(local));
  local.nl_family = AF_NETLINK;
  local.nl_pid = pid;
  local.nl_groups = RTMGRP_IPV4_ROUTE|RTMGRP_IPV6_ROUTE;

  if (bind(fd, (struct sockaddr *) &local, sizeof(local)) < 0){
      close(fd);
      LOG(LOG_EMERG, LAYOUT_IP, "cannot bind, are you root ? if yes, check netlink/rtnetlink kernel support");
      return -1;
    }


  memset(&rtnl_msg, 0, sizeof(rtnl_msg));
  memset(&kernel, 0, sizeof(kernel));
  memset(&req, 0, sizeof(req));

  kernel.nl_family = AF_NETLINK;
  kernel.nl_groups = 0;

  req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
  req.hdr.nlmsg_type = RTM_GETROUTE;
  req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP; 
  req.hdr.nlmsg_seq = 1;
  req.hdr.nlmsg_pid = pid;
  req.gen.rtgen_family = AF_PACKET;

  io.iov_base = &req;
  io.iov_len = req.hdr.nlmsg_len;
  rtnl_msg.msg_iov = &io;
  rtnl_msg.msg_iovlen = 1;
  rtnl_msg.msg_name = &kernel;
  rtnl_msg.msg_namelen = sizeof(kernel);

  sendmsg(fd, (struct msghdr *) &rtnl_msg, 0);

  while (!end){
      int len;
      struct nlmsghdr *msg_ptr;

      struct msghdr rtnl_reply;
      struct iovec io_reply;

      memset(&io_reply, 0, sizeof(io_reply));
      memset(&rtnl_reply, 0, sizeof(rtnl_reply));

      io.iov_base = reply;
      io.iov_len = BUFFER_SIZE;
      rtnl_reply.msg_iov = &io;
      rtnl_reply.msg_iovlen = 1;
      rtnl_reply.msg_name = &kernel;
      rtnl_reply.msg_namelen = sizeof(kernel);

      len = recvmsg(fd, &rtnl_reply, 0); /* read as much data as fits in the receive buffer */
      if (len){
	  for (msg_ptr = (struct nlmsghdr *) reply; NLMSG_OK(msg_ptr, len); msg_ptr = NLMSG_NEXT(msg_ptr, len)){
	      switch(msg_ptr->nlmsg_type){
		case 3:		/* this is the special meaning NLMSG_DONE message we asked for by using NLM_F_DUMP flag */
		  end++;
		  break;
		case 24:
		  rtnl_dump_route(msg_ptr);
		  break;
		default:
		  break;
		}
	    }
      }
  }

  close(fd);

  return 0;
}

int al_watch_route(void)
{
    int sock = -1;
    int i;
    struct sockaddr_nl addr;

    bzero (&addr, sizeof(addr));

    if ((sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0){
        LOG(LOG_EMERG, LAYOUT_IP, "Create sync route fd failed");
        return -1;
    }

    addr.nl_family = AF_NETLINK;
    addr.nl_groups = (RTMGRP_IPV4_ROUTE|RTMGRP_IPV6_ROUTE);

    if(bind(sock,(struct sockaddr *)&addr,sizeof(addr)) < 0){
        LOG(LOG_EMERG, LAYOUT_IP, "Bind sync route fd failed");
        return -1;
    }

    for ( i=0; i < RTE_MAX_LCORE; i++){
        if(g_thread_info[i].used){
            g_thread_info[i].start_lp=1;
        }
    }

    while (g_sys_stop){
        proc_loop(sock, &addr);
    }  

    close(sock);

    return 0;
}

