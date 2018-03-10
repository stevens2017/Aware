#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "alpha.h"
#include "al_common.h"
#include "al_pktdump.h"
#include "al_dev.h"
#include "al_log.h"
#include "al_to_api.h"
#include "al_dev.h"

#define WIRESHARK_MAJOR_VERSION  2
#define WIRESHARK_MINOR_VERSION  4

#define MAX_PACKAGE_LEN   65535

struct{
    int fd;
    uint8_t lock;
    off_t   offset;
    time_t start;
    time_t duration;
}dump_fd[AL_PORT_END][RTE_MAX_ETHPORTS];

static int al_port_cap_init( uint8_t portid, int type);

int al_init_dump(void){
   int i, k;
   for(k=0; k<AL_PORT_END; k++){
        for(i=0; i < RTE_MAX_ETHPORTS; i++){
	    dump_fd[k][i].fd=-1;
	    dump_fd[k][i].lock=0;
        }
   }
    return 0;
}

void al_set_pcap_duration(uint8_t id, uint8_t type, int duration){
    dump_fd[type][id].duration=duration;
    dump_fd[type][id].start=g_timeval.tv_sec;
}

void al_output(uint8_t portid, int type, struct rte_mbuf** buf, int cnt){

    int i;
    struct pcap_pkthdr hdr;
    struct rte_mbuf* mbuf;

   if( g_timeval.tv_sec - dump_fd[type][portid].start > dump_fd[type][portid].duration ){
       if(unlikely(dump_fd[type][portid].fd != -1  )){
           close(dump_fd[type][portid].fd);
           dump_fd[type][portid].fd=-1;
           al_finished_pcap(portid, type);
       }
       return;
   }
    
    if( !cnt ){
        return;
    }

    while(!CAS(&dump_fd[type][portid].lock, 0, 1));
    if( unlikely(dump_fd[type][portid].fd == -1)){
        if(al_port_cap_init(portid, type)){
            dump_fd[type][portid].lock=0;
            dump_fd[type][portid].fd=-1;
            LOG(LOG_ERROR, LAYOUT_CONF, "Open file portid %d type %d failed", portid, type);
            return;
        }else{
            LOG(LOG_ERROR, LAYOUT_CONF, "Open file portid %d type %d success", portid, type);
	}
    }

#if 0
    LOG(LOG_ERROR, LAYOUT_CONF, " Write pkg to %d type %d", portid, type);
#endif

    for( i=0; i<cnt; i++){
        
        mbuf=buf[i];
        
        hdr.ts_sec=g_timeval.tv_sec;
        hdr.ts_usec=g_timeval.tv_nsec/1000;

        hdr.len=rte_pktmbuf_pkt_len(mbuf);
        hdr.caplen=hdr.len > MAX_PACKAGE_LEN ? MAX_PACKAGE_LEN : hdr.len;

        
        if(write(dump_fd[type][portid].fd, (uint8_t*)&hdr, sizeof(struct pcap_pkthdr)) != sizeof(struct pcap_pkthdr) ){
            dump_fd[type][portid].lock=0;
            return;   
        }
       
        while(mbuf){
            if(write(dump_fd[type][portid].fd, rte_pktmbuf_mtod(mbuf, uint8_t*), rte_pktmbuf_data_len(mbuf)) != rte_pktmbuf_data_len(mbuf) ){
                break;
            }else{
                mbuf=mbuf->next;
            }
        }
        dump_fd[type][portid].lock=0;
    }
}

static int al_port_cap_init( uint8_t portid, int type){
    
    int rc=0;
    char buf[1024];

    struct pcap_file_header header={
        .magic=0xA1B2C3D4,
        .version_major=2,
        .version_minor=4,
        .thiszone=0,
        .sigfigs=0,
        .snaplen=65535,
        .linktype=1
    };
    
    dump_fd[type][portid].offset=0;
 
    sprintf(buf, "/tmp/%s-%u.pcap", type == AL_PORT_TYPE_KNI ? "kni" : (type==AL_PORT_TYPE_VPORT ? "vport" : "physic"), portid);
    if( (dump_fd[type][portid].fd = open(buf, O_WRONLY|O_CREAT|O_TRUNC, 0755)) == -1 ){
        LOG(LOG_FETAL, LAYOUT_CONF, "Open file:%s, %s", buf, strerror(errno));
        dump_fd[type][portid].fd=-1;
        rc =-1;
    }else{
        LOG(LOG_FETAL, LAYOUT_CONF, "Open file:%s", buf);
        if( write(dump_fd[type][portid].fd, (uint8_t*)&header, sizeof(struct pcap_file_header)) != sizeof(struct pcap_file_header) ){
            close(dump_fd[type][portid].fd);
            dump_fd[type][portid].fd=-1;
            rc=-1;
        }else{
            dump_fd[type][portid].offset=sizeof(header);
        }
        dump_fd[type][portid].lock=0;
    }

    return rc;
}

