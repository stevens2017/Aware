#include "al_ip.h"
#include "al_vserver.h"
#include "al_common.h"
#include "al_to_api.h"
#include "al_to_main.h"
#include "al_mempool.h"
#include "al_group.h"
#include "al_member.h"
#include "al_log.h"
#include "al_errmsg.h"


void al_load_configure(void){

    if( al_filter_load() ){
        LOG(LOG_EMERG, LAYOUT_CONF, "Load filter failed, msg:%s", al_get_errmsg(errno));
        return;
    }

    return ;
}

