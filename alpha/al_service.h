#ifndef AL_SERVICE_H
#define AL_SERVICE_H

void al_service(al_fargs *f, session_t* s, struct rte_mbuf* buf, void* hdr, void* nextptr);
void al_process(struct session* s, struct rte_mbuf* buf, void* iphdr, void* tcphdr);
void al_gslb_process(struct rte_mbuf* buf, void* iphdr, void* tcphdr);
#endif
