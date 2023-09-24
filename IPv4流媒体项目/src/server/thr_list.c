#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "../include/proto.h"
#include "thr_list.h"
#include "server_conf.h"
#include "medialib.h"
#include<arpa/inet.h>

#define BUFSIZE 1024
static pthread_t tid_list;
static int nr_list_end;
static struct medialib_des_st  *list_end;
static char buf[BUFSIZE];
static void*thr_list(void *p)
{
    int ret;
    int i; 
    int totalsize;
    int size;
    struct msg_channel_info_st* entryp; //部分信息
    struct msg_list_st* entlistp; //全部信息

    totalsize = sizeof(chnid_t);//先包含本身的chnid大小
    for(i=0;i<nr_list_end;i++)//总的节目单个数
    {
        totalsize += sizeof(struct msg_channel_info_st) + strlen(list_end[i].desc);//变长，即基本长度加上，描述内容的长度
    }
    entlistp = malloc(totalsize);//根据具体desc创建足够大的内存空间存放节目单结构体的数据
    if(entlistp ==NULL)
    {
        syslog(LOG_ERR,"malloc():%s",strerror(errno));
        exit(1);
    }

    entlistp->chnid = LIST_CHANEL_ID;//节目单对应的id
    entryp = entlistp->chanel_list;//具体的节目单的描述信息
    syslog(LOG_DEBUG,"nr_list_entn:%d\n",nr_list_end);//节目单条数
    for(int i=0;i<nr_list_end;i++)
    {
        size = sizeof(struct msg_channel_info_st) + strlen(list_end[i].desc);//要和申请时的size一样，否则移动时就会出现地址对不齐的情况
        //size不确定，要根据具体内容desc来确定每个频道的size，所以第二项要用strlen

        entryp->chnid = list_end[i].chid; //各个节目的chnid
        entryp->len_channel_info = htons(size); //各个节目的长度
        strcpy(entryp->desc,list_end[i].desc);
        entryp = (void *)(((char *)entryp) + size);//使其偏移到下一个节目单的位置，这里存放的节目单结构体数组的起始位置
        syslog(LOG_DEBUG,"entry len:%d\n",entryp->len_channel_info);
    }
    while (1)
    {
        inet_ntop(AF_INET,&sendder.sin_addr.s_addr,buf,BUFSIZE);
        syslog(LOG_INFO,"thr_list snaddr:%s\n",buf);
        ret = sendto(sockfd,entlistp,totalsize,0,(void *)&sendder,sizeof(sendder)); //包括频道id0，和各个频道的信息，用的频道结构体指针指向的第一块，这里chnid标识为0表示是第一个单线程频道线程的工作
        syslog(LOG_DEBUG,"sent content len:%d\n",entlistp->chanel_list->len_channel_info);//这里是频道描述信息的长度
        if(ret<0)
        {
            syslog(LOG_WARNING,"sendto(serversd,enlistp...:%s)",strerror(errno));

        }
        else
        {
            syslog(LOG_DEBUG,"sendto(serversd,enlistp...):success");

        }
        sleep(1); //发送节目单
    }
    
}

int thr_list_create(struct medialib_des_st *listp,int nr_ent)
{
    int err;
    list_end = listp;
    nr_list_end = nr_ent;
    syslog(LOG_DEBUG,"list content: chnid:%d,desc:%s\n",listp->chid,listp->desc);
    err = pthread_create(&tid_list,NULL,thr_list,NULL);
    if(err)
    {
        syslog(LOG_ERR,"pthread_create():%s",strerror(errno));
        return -1;
    }
    return 0;
}

int thr_list_destory(void)
{
    pthread_cancel(tid_list);
    pthread_join(tid_list,NULL);
    return 0;
}