#ifndef __AL_DEAMON_H__
#define __AL_DEAMON_H__

#define ACT_SHUTDOWN 0
#define ACT_RELOAD   1
#define ACT_START    2
#define ACT_STOP     3

#define START_FRONT   0
#define START_BACKEND 1

int main_loop(int front, int act, const char* wdir, const char* locker);

#endif /*__AL_DEAMON_H__*/

