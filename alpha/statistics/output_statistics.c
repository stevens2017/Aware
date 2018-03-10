#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "statistics/statistics.h"

int main(int argc, char* argv[]){

   int rc = statistics_open(1);
   if( rc != 0 ){
   	  return -1;
   }

   printf("New Connect     :%"PRIu64"\n", g_stat->new_conn);
   printf("Close Connect   :%"PRIu64"\n", g_stat->close_conn);
   printf("Input buf	   :%"PRIu64"\n", g_stat->in_mbuf);
   printf("Output buf      :%"PRIu64"\n", g_stat->out_mbuf);

   printf("-----------------------proxy L4----------------------\n");
   printf("Refuse data	           :%"PRIu64"\n", g_stat->mbuf_refuse_by_proxy);
   printf("Gen segment failed      :%"PRIu64"\n", g_stat->segment_create_failed);

   statistics_close();

   return 0;
}
