#ifndef THR_CHANNEL_H__
#define THR_CHANNEL_H__

#include "medialib.h"

int channel_create(struct medialib_des_st *);
int channel_destory(struct medialib_des_st *);
int channel_destoryall(void);


#endif
