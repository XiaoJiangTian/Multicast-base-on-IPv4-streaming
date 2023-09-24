#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<syslog.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/ip.h>


#include"thr_channel.h"
#include"../include/proto.h"
#include"server_conf.h"


struct thr_channel_ent_st
{
    chnid_t chnid;
    pthread_t tid;
};

static int tid_nextpos=0;
struct thr_channel_ent_st thr_channel[CHANNEL_NUM];//结构体数组，数组包含了频道id和线程id


static void *thr_channel_snder(void *ptr)
{
    int len;
    struct msg_channel_st* sbufp; //音频具体内容
    struct medialib_des_st *ent = ptr;
    sbufp = malloc(MSG_CHANNEL_MAX);
    if(sbufp==NULL)
    {
        syslog(LOG_ERR,"malloc()%s",strerror(errno));
        exit(1);
    }
    sbufp->chnid = ent->chid;//确定接收的chnid
    while(1)
    {
        len = mlib_readchannel(ent->chid,sbufp->data,MAX_DATA);//参数意义：频道，读到那，读内容， 返回值为真正读到的
        
        if(sendto(sockfd,sbufp,len+sizeof(chnid_t),0,(struct sockaddr*)&sendder,sizeof(sendder))<0)//这里sendder中填充的就是多播组的相应
        {
            syslog(LOG_ERR,"thr_channel(%d):sendto:%s",ent->chid,strerror(errno));

        }
        else
        {
            syslog(LOG_DEBUG,"thr_channel(%d):sendto successed",ent->chid);//debug信息会显示在终端
        }
        sched_yield();//出让调度器
    }
    pthread_exit(NULL);
}


int channel_create(struct medialib_des_st *ptr)
{
    int err;
    err = pthread_create(&thr_channel[tid_nextpos].tid,NULL,thr_channel_snder,ptr);//传入ptr确定id
    {
        if(err)
        {
            syslog(LOG_WARNING,"pthread_create():%s",strerror(errno));
            return -err;
        }
    }
    thr_channel[tid_nextpos].chnid = ptr->chid; //对频道结构体的chnid进行填充
    tid_nextpos++;
    return 0;
}


int channel_destory(struct medialib_des_st *ptr) //销毁某个频道
{
    int i;
    for(i=0;i<CHANNEL_NUM;i++)
    {
        if(thr_channel[i].chnid == ptr->chid)
        {
            if(pthread_cancel(thr_channel[i].tid)<0)
            {
                syslog(LOG_ERR,"pthread_cancel():%s",strerror(errno));
                return -ESRCH;
            }
            pthread_join(thr_channel[i].tid,NULL);
            thr_channel[i].chnid = -1;
            return 0;
        }
    }
}

int channel_destoryall(void)
{
    int i;
    for(i=0;i<CHANNEL_NUM;i++)
    {
        if(thr_channel[i].tid>0)
        {
            if(pthread_cancel(thr_channel[i].tid)<0)
            {
                syslog(LOG_ERR,"pthread_cancel():the thread of channel %d",thr_channel[i].chnid);
                return -ESRCH;
            }
            pthread_join(thr_channel[i].tid,NULL);//阻塞等待线程结束并回收资源
            thr_channel[i].chnid=-1;
        }
    }
}