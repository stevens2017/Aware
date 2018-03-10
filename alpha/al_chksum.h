#ifndef AL_CHKSUM_H
#define AL_CHKSUM_H

uint16_t inet_chksum(void *dataptr, uint16_t len);
uint16_t inet_chksum_pseudo(struct rte_mbuf *p, uint16_t boff, uint32_t src, uint32_t dest, uint8_t proto, uint16_t proto_len);
uint16_t inet6_chksum_pseudo(struct rte_mbuf *p, uint16_t boff, uint16_t *src, uint16_t *dest, uint8_t proto, uint32_t proto_len);

#endif
