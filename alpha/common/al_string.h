#ifndef _AL_STRING_H_
#define _AL_STRING_H_

typedef struct al_string al_string_t;

struct al_string{
    uint8_t* data;
	int      len;
};

#endif /*_AL_STRING_H_*/
