#ifndef MEDIALIB_H__
#define MEDIALIB_H__

#include<site_type.h>

struct medialib_des_st
{
    chnid_t chid;
    char  *desc;
    int len;
};

int mlib_getchannel_list(struct medialib_des_st **,int *);//结构体指针数组，去存放相应的节目单信息
int mlib_freechannel_list(struct medialib_des_st *);
ssize_t mlib_readchannel(chnid_t,void *,size_t);


#endif