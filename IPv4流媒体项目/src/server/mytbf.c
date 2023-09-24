#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/time.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<string.h>
#include<pthread.h>


#include"mytbf.h"

static pthread_t tid;

struct mytbf_t
{
    int cps; //速率
    int burst; //上限
    int token; //token值，每次取的数据值
    int pos; //位置
    pthread_mutex_t mut; //锁token
    pthread_cond_t cond; //条件变量进行通知而不是一直查询占用cpu资源
};

struct mytbf_t* job[MAXTBF_NUM];//令牌桶数组
pthread_mutex_t mut_job=PTHREAD_MUTEX_INITIALIZER;//锁令牌桶数组的锁

static int get_free_pos_unlock()
{
    int i;
    for(i=0;i<MAXTBF_NUM;i++)
    {
        if(job[i]==NULL) //指针为null说明该结构体没有被占用，有空位
        {
            return i;
        }
    }
    return -1;
}

static void *thr_fun(void *p)
{
    int i;
    while (1)
    { 
        pthread_mutex_lock(&mut_job);
        for(i=0;i<MAXTBF_NUM;i++)
        {
            if(job[i]!=NULL)
            {
                pthread_mutex_lock(&job[i]->mut);
                job[i]->token += job[i]->cps;
                if(job[i]->token>=job[i]->burst)
                {
                    job[i]->token = job[i]->burst;
                }
                pthread_cond_broadcast(&job[i]->cond);
                pthread_mutex_unlock(&job[i]->mut);
            }
        }
        pthread_mutex_unlock(&mut_job);
        sleep(1);
    }
}

static void module_unload()
{
    int i;
    pthread_cancel(tid);
    pthread_join(tid,NULL); //回收进程资源
    for(i=0;i<MAXTBF_NUM;i++)
    {
        free(job[i]);//释放令牌桶
    }
}

static void module_load()
{
    int ret;
    ret = pthread_create(&tid,NULL,thr_fun,NULL);
    if(ret)
    {
        fprintf(stderr,"pthread_create():%s",strerror(ret));
        exit(1);
    }

    atexit(module_unload);
}

mytbf_t* mytbf_init(int cps,int burst)
{
    int pos;
    struct mytbf_t* me;
    pthread_once_t once;
    pthread_once(&once,module_load);

    me = malloc(sizeof(*me));
    if(me==NULL)
    {
        return NULL;
    }
    //初始化
    me->cps = cps;
    me->burst = burst;
    me->token = 0;
    pthread_mutex_init(&me->mut,NULL);
    pthread_cond_init(&me->cond,NULL);

    pthread_mutex_lock(&mut_job);
    pos = get_free_pos_unlock(); //锁住job数组，去查找空位
    if(pos<0)
    {
        pthread_mutex_unlock(&mut_job);//防止死锁
        free(me);
        return NULL;
    }

    me->pos = pos;
    job[me->pos] = me;
    pthread_mutex_unlock(&mut_job);
    return me;

}

static int min(int a,int b)
{
    if(a>b)
    {
        return b;
    }
    return a;
}

int mytbf_fetchtoken(mytbf_t *ptr,int size)
{
    struct mytbf_t *me = ptr;
    int takenum;
    pthread_mutex_lock(&me->mut);
    while(me->token<=0)//循环阻塞等待通知
    {
        pthread_cond_wait(&me->cond,&me->mut);
    }
    takenum = min(me->token,size);
    me->token -= takenum;
    pthread_mutex_unlock(&me->mut);

    return takenum;
}

int mytbf_returntoken(mytbf_t *ptr,int size)
{
    struct mytbf_t *me = ptr;
    pthread_mutex_lock(&me->mut);
    me->token += size;
    if(me->token>=me->burst)
    {
        me->token=me->burst;
    }
    pthread_cond_broadcast(&me->cond);//通知缺token等待的地方
    pthread_mutex_unlock(&me->mut);

    return 0;
}

int mytbf_destory(mytbf_t *ptr)
{
    struct mytbf_t *me = ptr;
    pthread_mutex_lock(&mut_job);
    job[me->pos] = NULL;
    pthread_mutex_unlock(&mut_job);
    pthread_mutex_destroy(&me->mut);
    pthread_cond_destroy(&me->cond);
    free(me);//用完最后再free
    return 0;
}