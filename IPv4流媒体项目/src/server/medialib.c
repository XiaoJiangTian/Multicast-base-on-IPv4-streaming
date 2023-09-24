#include<stdio.h>
#include<stdlib.h>
#include<glob.h>
#include<syslog.h>
#include<string.h>
#include<unistd.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>


#include"server_conf.h"
#include"../include/proto.h"
#include"mytbf.h"
#include"medialib.h"


#define DEBUG//可以用ifdef来设置调试信息
#define PATHSIZE 1024
#define LINEBUFFSIZE 1024
#define MP3_BITRATE 64*1024*2 //与音乐的播放速率有关


struct channel_allinfo_st
{
    chnid_t chnid;
    char *desc;
    glob_t mp3glob;//用于解析目录文件
    int pos;//用来定位是那个频道
    int fd;//用来指示音频文件在那个位置
    off_t offset;//读取数据的偏移量
    mytbf_t *tbf;//流量控制的令牌桶
    //struct medialib_des_st entry;
};

struct channel_allinfo_st channel[MAX_CHANEL_ID+1];

static struct channel_allinfo_st * path2entry(const char *dir)
{
    static chnid_t curr_id = MIN_CHANEL_ID;
    struct channel_allinfo_st*me;
    char linebuf[LINEBUFFSIZE];
    FILE *fp;
    char pathall[PATHSIZE] = {'\0'};//初始化为空，'\0'代表结束字符，ascii为0.代表将字符数组初始化为空
    
    strcat(pathall,dir);
    strcat(pathall,"/desc.txt");
    fp = fopen(pathall,"r");
    syslog(LOG_INFO,"fopen():channel dir:%s",dir);
    if(fp==NULL)
    {
        syslog(LOG_INFO,"%s is not a channel dir(cannot find desc.txt)",dir);//log_info也会在终端显示
        return NULL;
    }
    if(fgets(linebuf,LINEBUFFSIZE,fp)==NULL)
    {
        syslog(LOG_INFO,"%s is not a channel dir(cannot get the content of desc.txt)",dir);
        return NULL;//不正确就返回null，前面会处理
    }
    fclose(fp);//使用完毕，关闭防止内存泄漏
    me = malloc(sizeof(*me));
    if(me==NULL)
    {
        syslog(LOG_ERR,"cannot malloc():%s",strerror(errno));
        return NULL;
    }

    //创建令牌桶
    me->tbf = mytbf_init(MP3_BITRATE/8,MP3_BITRATE/8*5);
    if(me->tbf==NULL)
    {
        syslog(LOG_ERR,"fail to mytbf_init ");
        return NULL;
    }
    me->desc = strdup(linebuf);//获得一份linebuf的副本，防止后序对desc操作会影响原来的linebuf；
    
    
    //解析mp3文件
    strncpy(pathall,dir,PATHSIZE);
    strncat(pathall,"/*.mp3",PATHSIZE);
    if(glob(pathall,0,NULL,&me->mp3glob))//解析数据放在glob_t类型的mp3glob中，方便在其他模块调用
    {
        curr_id++;//标识当前的频道号并且对应相关的目录
        syslog(LOG_ERR,"glob errno,can not find the mp3 file");
        free(me);//防止内存泄漏
        return NULL;
    }
    me->pos = 0;
    me->offset = 0;
    me->fd = open(me->mp3glob.gl_pathv[me->pos],O_RDONLY);
    if(me->fd<0)
    {
        syslog(LOG_WARNING,"%s open() failed",me->mp3glob.gl_pathv[me->pos]);//对应相应的mp3文件路径
        return NULL;
    }
    me->chnid = curr_id;
    curr_id++;
    return me;
}

int mlib_getchannel_list(struct medialib_des_st **result,int *resnum)//结构体指针数组，去存放相应的节目单信息
{
    struct channel_allinfo_st* allinfo;
    struct medialib_des_st* someinfo;
    int num=0;
    int i;
    char path[PATHSIZE];
    glob_t glob_res;

    for(i=0;i<MAX_CHANEL_ID+1;i++)
    {
        channel[i].chnid = -1;
    }
    snprintf(path,PATHSIZE,"%s/*",server_conf.file_lib);
    if(glob(path,0,NULL,&glob_res))
    {
        return -1;
    }
    someinfo = malloc(sizeof(struct medialib_des_st) * glob_res.gl_pathc); //开辟解析到的节目单结构体数目（gl_pathc）大的空间
    if(someinfo==NULL)
    {
        syslog(LOG_ERR,"malloc():%s",strerror(errno));
        exit(1);
    }
    for(i=0;i<glob_res.gl_pathc;i++)
    {
        allinfo = path2entry(glob_res.gl_pathv[i]);
        if(allinfo!=NULL)
        {//成功解析文件
            syslog(LOG_ERR,"path2entry()return :%d %s.",allinfo->chnid,allinfo->desc);
            memcpy(channel+allinfo->chnid,allinfo,sizeof(*allinfo));//这里不是变长，存放desc的是指针，不是变长数组
            someinfo[num].chid = allinfo->chnid;
            someinfo[num].desc = allinfo->desc;
            num++;
        }
    }//经过次过程，所有节目单的内容都被填充好
    *result = realloc(someinfo,sizeof(struct medialib_des_st) * num);//节目单的信息
    if(result  ==NULL)
    {
        syslog(LOG_ERR,"realloc()failed.");
    }
    *resnum = num;// 解析到媒体库的个数
    return 0;
}


//在第一首歌播放完后，pos++，用fd指向下一首歌曲
static int open_next(chnid_t chnid)
{
    int i;
    for(i=0;i<channel[chnid].mp3glob.gl_pathc;i++)
    {
        channel[chnid].pos++;
        if(channel[chnid].pos == channel[chnid].mp3glob.gl_pathc)
        {
            channel[chnid].pos = 0;
            break;
        }

        close(channel[chnid].fd);
        channel[chnid].fd = open(channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],O_RDWR);
        if(channel[chnid].fd<0)//打开不了就打开下一首
        {
            syslog(LOG_WARNING,"open(%s)%s",channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],strerror(errno));
        }
        else//跳转成功
        {
            channel[chnid].offset = 0;
            return 0;
        }
    }
    syslog(LOG_ERR,"None of mp3 in channel %d is avalible ",chnid);

}

int mlib_freechannel_list(struct medialib_des_st *ptr)
{
    free(ptr);
    return 0;
}

ssize_t mlib_readchannel(chnid_t chnid,void *buf,size_t size)
{
    int tbfsize;
    int len;
    tbfsize = mytbf_fetchtoken(channel[chnid].tbf,size); //从令牌桶中取token，取的是token现有值和size的最小值
    while(1)
    {
        len = pread(channel[chnid].fd,buf,tbfsize,channel[chnid].offset);//在获取频道后置位的信息在这里其作用，例如音频文件的文件描述符，偏移量等等
        if(len<0)
        {
            syslog(LOG_WARNING,"media file %s pread():%s",channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],strerror(errno));
            open_next(chnid);
        }
        else if(len==0)
        {
            syslog(LOG_DEBUG,"media file %s is over",channel[chnid].mp3glob.gl_pathv[channel[chnid].pos]);
            open_next(chnid);
        }
        else
        {
            channel[chnid].offset += len;
            break; //只有正常读到数据才会退出
        }
    }
    if(tbfsize-len>0)
    {
        mytbf_returntoken(channel[chnid].tbf,tbfsize-len);
    }
    return len;
}
