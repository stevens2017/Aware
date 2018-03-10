#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include "al_ether.h"

#define ETHER_ADDR_LEN    6

int get_mac_addr(char *ifname, char *mac)
{
    int fd, rtn;
    struct ifreq ifr;
    
    if( !ifname || !mac ) {
        return -1;
    }
    fd = socket(AF_INET, SOCK_DGRAM, 0 );
    if ( fd < 0 ) {
        perror("socket");
           return -1;
    }
    ifr.ifr_addr.sa_family = AF_INET;    
    strncpy(ifr.ifr_name, (const char *)ifname, IFNAMSIZ - 1 );

    if ( (rtn = ioctl(fd, SIOCGIFHWADDR, &ifr) ) == 0 )
        memcpy(    mac, (unsigned char *)ifr.ifr_hwaddr.sa_data, 6);
    close(fd);
    return rtn;
}

int set_mac_addr(const char *ifname, const char *mac)
{
    int fd, rtn;
    struct ifreq ifr;
    unsigned char cbuf[16];

    if( !ifname || !mac ) {
        return -1;
    }
    
    fd = socket(AF_INET, SOCK_DGRAM, 0 );
    if ( fd < 0 ) {
        perror("socket");
        return -1;
    }
    ifr.ifr_addr.sa_family = ARPHRD_ETHER;
    strncpy(ifr.ifr_name, (const char *)ifname, IFNAMSIZ - 1 );

    if(!ether_atoe(mac, cbuf)){
        return -1;
    }
    
    memcpy((unsigned char *)ifr.ifr_hwaddr.sa_data, cbuf, 6);
    
    if ( (rtn = ioctl(fd, SIOCSIFHWADDR, &ifr) ) != 0 ){
        perror("SIOCSIFHWADDR");
    }
    
    close(fd);
    return rtn;
}

int if_updown(const char *ifname, int flag)
{
    int fd, rtn;
    struct ifreq ifr;        

    if (!ifname) {
        return -1;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0 );
    if ( fd < 0 ) {
        perror("socket");
        return -1;
    }
    
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, (const char *)ifname, IFNAMSIZ - 1 );

    if ( (rtn = ioctl(fd, SIOCGIFFLAGS, &ifr) ) == 0 ) {
        if ( flag == AL_IF_DOWN )
            ifr.ifr_flags &= ~IFF_UP;
        else if ( flag == AL_IF_UP ) 
            ifr.ifr_flags |= IFF_UP;
        
    }

    if ( (rtn = ioctl(fd, SIOCSIFFLAGS, &ifr) ) != 0) {
        perror("SIOCSIFFLAGS");
    }

    close(fd);

    return rtn;
}

/*
 * Convert Ethernet address string representation to binary data
 * @param    a    string in xx:xx:xx:xx:xx:xx notation
 * @param    e    binary data
 * @return    TRUE if conversion was successful and FALSE otherwise
 */
int ether_atoe(const char *a, unsigned char *e)
{
    char *c = (char *) a;
    int i = 0;

    memset(e, 0, ETHER_ADDR_LEN);
    for (;;) {
        e[i++] = (unsigned char) strtoul(c, &c, 16);
        if (!*c++ || i == ETHER_ADDR_LEN)
            break;
    }
    return (i == ETHER_ADDR_LEN);
}


/*
 * Convert Ethernet address binary data to string representation
 * @param    e    binary data
 * @param    a    string in xx:xx:xx:xx:xx:xx notation
 * @return    a
 */
char * ether_etoa(const unsigned char *e, char *a)
{
    char *c = a;
    int i;

    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        if (i)
            *c++ = ':';
        c += sprintf(c, "%02X", e[i] & 0xff);
    }
    return a;
}

