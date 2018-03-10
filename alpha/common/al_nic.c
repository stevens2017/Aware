#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<net/if.h>
#include<netinet/in.h>
#include<sys/ioctl.h>
#include<errno.h>
#include<unistd.h>
#include<net/route.h>
#include <ifaddrs.h>
#include <bits/sockaddr.h>
#include <asm/types.h>
#include <linux/rtnetlink.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netdb.h>
#include "al_nic.h"

static int al_nic_set_gw( int sockfd, in_addr_t getway );

int al_nic_set_mac(char *ifname, char *addr)
{
    struct ifreq ifr;
    int sock;
    
    sock = socket(AF_INET,SOCK_DGRAM,0);
    if( sock == -1 ){
        return -1;
    }
    
    strcpy( ifr.ifr_name, ifname );
    ifr.ifr_addr.sa_family = AF_UNIX;
    memcpy(ifr.ifr_hwaddr.sa_data,addr,6);

    if (ioctl( sock, SIOCSIFHWADDR, &ifr ) < 0){
        return -1;
    }
    
    shutdown(sock,SHUT_RDWR);
    
    return 0;
}

int al_nic_get_mac(char *ifname, char *addr)
{
    struct ifreq ifr;
    int sock;
    
    sock = socket(AF_INET,SOCK_DGRAM,0);
    if( sock == -1 ){
        return -1;
    }

    strcpy( ifr.ifr_name, ifname );
    ifr.ifr_addr.sa_family = AF_INET;

    if (ioctl( sock, SIOCGIFHWADDR, &ifr ) < 0){
        return -1;
    }
    
    memcpy(addr,ifr.ifr_hwaddr.sa_data,6);

    shutdown(sock,SHUT_RDWR);
    return 0;
}

void al_nic_set_ip(const char *ifname, in_addr_t ip, in_addr_t mask, in_addr_t gip){
    
    struct ifreq ifr;
    struct rtentry rt;
    struct sockaddr_in sa;
    
    int sock;
    sock = socket(AF_INET,SOCK_DGRAM,0);
    if( sock == -1 ){
        fprintf(stderr, "Set ip %s\n", strerror(errno));
        return;
    }
    
    memset(&sa, 0, sizeof(sa));
    memset(&rt, 0, sizeof(rt));

    strcpy( ifr.ifr_name, ifname );
    ifr.ifr_addr.sa_family = AF_INET;

    if (ioctl( sock, SIOCGIFADDR, &ifr ) < 0){
        fprintf(stderr, "Get nic addr %s\n", strerror(errno));
        close(sock);
        return;
    }
        
    ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr = ip;
    if (ioctl( sock, SIOCSIFADDR, &ifr ) < 0){
        fprintf(stderr, "Set nic addr %s", strerror(errno));
        close(sock);
        return;
    }
        
    al_nic_set_gw(sock, gip);
    shutdown(sock,SHUT_RDWR);
}

static int al_nic_set_gw( int sockfd, in_addr_t getway ){
    
    struct sockaddr_in *dst, *gw, *mask;
    struct rtentry route;

    memset(&route,0,sizeof(struct rtentry));

    dst = (struct sockaddr_in *)(&(route.rt_dst));
    gw = (struct sockaddr_in *)(&(route.rt_gateway));
    mask = (struct sockaddr_in *)(&(route.rt_genmask));

    dst->sin_family = AF_INET;
    gw->sin_family = AF_INET;
    mask->sin_family = AF_INET;

    dst->sin_addr.s_addr = 0;
    gw->sin_addr.s_addr = 0;
    mask->sin_addr.s_addr = 0;
    route.rt_flags = RTF_UP | RTF_GATEWAY;

    ioctl(sockfd,SIOCDELRT,&route);

    dst->sin_addr.s_addr = 0;
    gw->sin_addr.s_addr = getway;
    mask->sin_addr.s_addr = 0;
    route.rt_metric = 1;
    route.rt_flags = RTF_UP | RTF_GATEWAY;

    ioctl(sockfd,SIOCDELRT,&route);

    if( ioctl(sockfd,SIOCADDRT,&route) == -1 )
    {
        fprintf( stderr,"Adding default route: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int al_nic_up_and_running(char *ifname)
{
    struct ifreq ifr;

    int sock;
    sock = socket(AF_INET,SOCK_DGRAM,0);
    if( sock == -1 ){
        fprintf(stderr, "Set ip %s\n", strerror(errno));
        return -1;
    }

    strcpy(ifr.ifr_name, ifname);
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        fprintf(stderr, "%s: unknown interface: %s\n", ifname, strerror(errno));
        close(sock);
        return (-1);
    }

    strcpy(ifr.ifr_name, ifname);
    
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
    if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
        fprintf(stderr, "Set up and running status failed, %s", strerror(errno));
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}

int al_opt_ip(uint8_t index, uint8_t is_add, uint8_t iptype, const char* ip, uint8_t prefix)
{

    struct {
        struct nlmsghdr nl;
        struct ifaddrmsg rt;
        char buf[8192];
    } req;

    int fd;
    struct sockaddr_nl la;
    struct sockaddr_nl pa;
    struct msghdr msg;
    struct iovec iov;
    int rtn;
    uint8_t off=(iptype==AF_INET)?4:16;

    int ifal;
    struct rtattr *rtap;

    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if( fd < 0 ){
        return -1;
    }

    bzero(&la, sizeof(la));
    la.nl_family = AF_NETLINK;
    la.nl_pid = getpid();
    bind(fd, (struct sockaddr*) &la, sizeof(la));


    bzero(&req, sizeof(req));
    ifal = sizeof(struct ifaddrmsg);
    rtap = (struct rtattr *) req.buf;
    rtap->rta_type = IFA_ADDRESS;
    rtap->rta_len = sizeof(struct rtattr) + off;
    inet_pton(iptype, ip,
              ((char *)rtap) + sizeof(struct rtattr));
    ifal += rtap->rta_len;

    rtap = (struct rtattr *) (((char *)rtap)+ rtap->rta_len);
    rtap->rta_type = IFA_LOCAL;
    rtap->rta_len = sizeof(struct rtattr) + off;
    inet_pton(iptype, ip,
              ((char *)rtap) + sizeof(struct rtattr));
    ifal += rtap->rta_len;

    req.nl.nlmsg_len = NLMSG_LENGTH(ifal);
    req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_APPEND;
    req.nl.nlmsg_type = is_add ? RTM_NEWADDR :RTM_DELADDR;

    req.rt.ifa_family = iptype;
    req.rt.ifa_prefixlen = prefix; /*hardcoded*/
    req.rt.ifa_flags = IFA_F_PERMANENT;
    req.rt.ifa_scope = RT_SCOPE_HOST;
    req.rt.ifa_index = index;//if_nametoindex(ifname);

    bzero(&pa, sizeof(pa));
    pa.nl_family = AF_NETLINK;

    bzero(&msg, sizeof(msg));
    msg.msg_name = (void *) &pa;
    msg.msg_namelen = sizeof(pa);

    iov.iov_base = (void *) &req.nl;
    iov.iov_len = req.nl.nlmsg_len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    rtn = sendmsg(fd, &msg, 0);
    if( rtn < 0 ){
        return -1;
    }

    close(fd);
    return 0;
}

void al_ip_show(void){
  struct ifaddrs *ifap, *ifa;
  struct sockaddr_in6 *sa6;
    struct sockaddr_in *sa;
  char addr[INET6_ADDRSTRLEN];
  char* ptr;

  getifaddrs (&ifap);
  for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
    memset(addr, '\0', INET6_ADDRSTRLEN);
    if (ifa->ifa_addr->sa_family==AF_INET6) {
      sa6 = (struct sockaddr_in6 *) ifa->ifa_addr;
      getnameinfo((struct sockaddr*)sa6, sizeof(struct sockaddr_in6), addr,
                  INET6_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST);
      ptr=strstr(addr, "%");
      if(ptr){
          *ptr='\0';
      }      
      printf("Interface: %s\tAddress: %s\n", ifa->ifa_name, addr);
    }else if (ifa->ifa_addr->sa_family==AF_INET) {
      sa = (struct sockaddr_in*) ifa->ifa_addr;
      getnameinfo((struct sockaddr*)sa, sizeof(struct sockaddr_in), addr,
                  INET6_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST);
      printf("Interface: %s\tAddress: %s\n", ifa->ifa_name, addr);
    }
  }

  freeifaddrs(ifap);
}
