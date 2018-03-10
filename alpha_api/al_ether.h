#ifndef AL_ETHER_H
#define AL_ETHER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define AL_IF_UP    1
#define AL_IF_DOWN    0

int get_mac_addr(char *ifname, char *mac);
int set_mac_addr(const char *ifname, const char *mac);
int if_updown(const char *ifname, int flag);
int ether_atoe(const char *a, unsigned char *e);
char * ether_etoa(const unsigned char *e, char *a);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif

