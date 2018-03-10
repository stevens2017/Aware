#ifndef __AL_BUF_H__
#define __AL_BUF_H__

typedef struct buf_st{
  uint8_t* start;
  uint8_t* pos;
  uint8_t* last;
  uint8_t* end;
}al_buf_t;


#define al_buf_len(b) ((b)->last-(b)->pos)
#define al_buf_rest_len(b) ((b)->end-(b)->last)

#define al_init_buf(b, buf, len)                       \
           (b)->start=(b)->pos=(b)->last=(buf);         \
           (b)->end=(b)->start+len
#endif /*__AL_BUF_H__*/

