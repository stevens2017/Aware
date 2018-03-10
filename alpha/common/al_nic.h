#ifndef __AL_NIC_H__
#define __AL_NIC_H__

int al_nic_set_mac(char *ifname, char *addr);
int al_nic_get_mac(char *ifname, char *addr);
int al_nic_up_and_running(char *ifname);
void al_nic_set_ip(const char *ifname, in_addr_t ip, in_addr_t mask, in_addr_t gip);
int al_opt_ip(uint8_t index, uint8_t is_add, uint8_t iptype, const char* ip, uint8_t prefix);
void al_ip_show(void);
#endif /*__AL_NIC_H__*/

