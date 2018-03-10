#include <inttypes.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include "al_ip.h"
#include "al_chksum.h"


#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define SWAP_BYTES_IN_WORD(w) rte_cpu_to_be_16(w)
#else
    #define SWAP_BYTES_IN_WORD(w) (((w) & 0xff) << 8) | (((w) & 0xff00) >> 8)
#endif

#define FOLD_U32T(u)          (((u) >> 16) + ((u) & 0x0000ffffUL))

static uint16_t lwip_standard_chksum(void *dataptr, int len);

static uint32_t chksum(void *dataptr, uint16_t len)
{
  uint16_t *sdataptr = dataptr;
  uint32_t acc;
  
  
  for(acc = 0; len > 1; len -= 2) {
    acc += *sdataptr++;
  }

  /* add up any odd byte */
  if (len == 1) {
    acc += rte_cpu_to_be_16((uint16_t)(*(uint8_t *)dataptr) << 8);
  }

  return acc;

}


uint16_t inet6_chksum_pseudo(struct rte_mbuf *p, uint16_t boff, uint16_t *src, uint16_t *dest, uint8_t proto, uint32_t proto_len)
{
  uint32_t acc;
  struct rte_mbuf *q;
  uint8_t swapped, i;

  acc = 0;
  swapped = 0;
  for(q = p; q != NULL; q = q->next) {    
      acc += chksum(rte_pktmbuf_mtod_offset(q, void*, boff), rte_pktmbuf_data_len(q)-boff);
      boff=0;
    while (acc >> 16) {
      acc = (acc & 0xffff) + (acc >> 16);
    }
    if (rte_pktmbuf_data_len(q) % 2 != 0) {
      swapped = 1 - swapped;
      acc = ((acc & 0xff) << 8) | ((acc & 0xff00) >> 8);
    }
  }

  if (swapped) {
    acc = ((acc & 0xff) << 8) | ((acc & 0xff00) >> 8);
  }
  
  for(i = 0; i < 8; i++) {
    acc += src[i] & 0xffff;
    acc += dest[i] & 0xffff;
    while (acc >> 16) {
      acc = (acc & 0xffff) + (acc >> 16);
    }
  }
  acc += (uint16_t)rte_cpu_to_be_16((uint16_t)proto);
  acc += ((uint16_t *)&proto_len)[0] & 0xffff;
  acc += ((uint16_t *)&proto_len)[1] & 0xffff;

  while (acc >> 16) {
    acc = (acc & 0xffff) + (acc >> 16);
  }
  return ~(acc & 0xffff);
}

/* inet_chksum_pseudo:
 *
 * Calculates the pseudo Internet checksum used by TCP and UDP for a pbuf chain.
 * IP addresses are expected to be in network byte order.
 *
 * @param p chain of pbufs over that a checksum should be calculated (ip data part)
 * @param src source ip address (used for checksum of pseudo header)
 * @param dst destination ip address (used for checksum of pseudo header)
 * @param proto ip protocol (used for checksum of pseudo header)
 * @param proto_len length of the ip data part (used for checksum of pseudo header)
 * @return checksum (as uint16_t) to be saved directly in the protocol header
 */
uint16_t inet_chksum_pseudo(struct rte_mbuf *p, uint16_t boff, uint32_t src, uint32_t dest, uint8_t proto, uint16_t proto_len)
{
  uint32_t acc;
  uint32_t addr;
  int dlen, offset=boff;
  struct rte_mbuf *q;
  uint8_t swapped;

  acc = 0;
  swapped = 0;
  dlen = 0;
  /* iterate through all pbuf in chain */
  for(q = p; q != NULL; q = q->next) {
      dlen += rte_pktmbuf_data_len(q)-boff;
      fprintf(stderr, "Buf:%p,  len:%d\n", q, rte_pktmbuf_data_len(q));
      acc += lwip_standard_chksum(rte_pktmbuf_mtod_offset(q, void*, boff), rte_pktmbuf_data_len(q)-boff);
    /*LWIP_DEBUGF(INET_DEBUG, ("inet_chksum_pseudo(): unwrapped lwip_chksum()=%"X32_F" \n", acc));*/
    /* just executing this next line is probably faster that the if statement needed
       to check whether we really need to execute it, and does no harm */
    acc = FOLD_U32T(acc);
    if ((rte_pktmbuf_data_len(q) - boff) % 2  !=  0 ) {
      swapped = 1 - swapped;
      acc = SWAP_BYTES_IN_WORD(acc);
    }
    boff=0;
    /*LWIP_DEBUGF(INET_DEBUG, ("inet_chksum_pseudo(): wrapped lwip_chksum()=%"X32_F" \n", acc));*/
  }

  fprintf(stderr, "Offset %d Chksum total bytes %d total %d\n", offset, dlen, proto_len);

  if (swapped) {
    acc = SWAP_BYTES_IN_WORD(acc);
  }
  addr = src;
  acc += (addr & 0xffffUL);
  acc += ((addr >> 16) & 0xffffUL);
  addr = dest;
  acc += (addr & 0xffffUL);
  acc += ((addr >> 16) & 0xffffUL);
  acc += (uint32_t)rte_cpu_to_be_16((uint16_t)proto);
  acc += (uint32_t)rte_cpu_to_be_16(proto_len);

  /* Fold 32-bit sum to 16 bits
     calling this twice is propably faster than if statements... */
  acc = FOLD_U32T(acc);
  acc = FOLD_U32T(acc);
//  LWIP_DEBUGF(INET_DEBUG, ("inet_chksum_pseudo(): pbuf chain lwip_chksum()=%"X32_F"\n", acc));
  return (uint16_t)~(acc & 0xffffUL);
}


/*
 * Curt McDowell
 * Broadcom Corp.
 * csm@broadcom.com
 *
 * IP checksum two bytes at a time with support for
 * unaligned buffer.
 * Works for len up to and including 0x20000.
 * by Curt McDowell, Broadcom Corp. 12/08/2005
 *
 * @param dataptr points to start of data to be summed at any boundary
 * @param len length of data to be summed
 * @return host order (!) lwip checksum (non-inverted Internet sum) 
 */

static uint16_t lwip_standard_chksum(void *dataptr, int len)
{
  uint8_t *pb = (uint8_t *)dataptr;
  uint16_t *ps, t = 0;
  uint32_t  sum = 0;
  int odd = ((uintptr_t)pb & 1);

  /* Get aligned to uint16_t */
  if (odd && len > 0) {
    ((uint8_t *)&t)[1] = *pb++;
    len--;
  }

  /* Add the bulk of the data */
  ps = (uint16_t *)(void *)pb;
  while (len > 1) {
    sum += *ps++;
    len -= 2;
  }

  /* Consume left-over byte, if any */
  if (len > 0) {
    ((uint8_t *)&t)[0] = *(uint8_t *)ps;
  }

  /* Add end bytes */
  sum += t;

  /* Fold 32-bit sum to 16 bits
     calling this twice is propably faster than if statements... */
  sum = FOLD_U32T(sum);
  sum = FOLD_U32T(sum);

  /* Swap if alignment was odd */
  if (odd) {
    sum = SWAP_BYTES_IN_WORD(sum);
  }

  return (uint16_t)sum;
}

/* inet_chksum:
 *
 * Calculates the Internet checksum over a portion of memory. Used primarily for IP
 * and ICMP.
 *
 * @param dataptr start of the buffer to calculate the checksum (no alignment needed)
 * @param len length of the buffer to calculate the checksum
 * @return checksum (as uint16_t) to be saved directly in the protocol header
 */

uint16_t inet_chksum(void *dataptr, uint16_t len)
{
  return ~lwip_standard_chksum(dataptr, len);
}

