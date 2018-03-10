#include "alpha.h"
#include "al_common.h"
#include "al_session.h"
#include "al_to_api.h"
#include "al_arp.h"

int al_mainloop(void* UNUSED(args)){

    struct thread_info* th=&g_thread_info[rte_lcore_id()];
    g_lcoreid=th->lcoreid;

    if(session_thread_init() != 0){
        fprintf(stderr, "session_thread_init failed\n");
        return -1;
    }

    th->started=1;
    
    return th->proc(th);
}

