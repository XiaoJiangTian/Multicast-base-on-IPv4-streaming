#include<stdio.h>
#include<stdlib.h>
#include<unistd.h> 
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<syslog.h>
#include<string.h>
#include<errno.h>
#include<signal.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<arpa/inet.h>
#include<net/if.h>



#include"../include/proto.h"
#include"medialib.h"
#include"thr_channel.h"
#include"server_conf.h"
#include"thr_list.h"

//全局变量声明
int sockfd;
struct sockaddr_in sendder;
static struct medialib_des_st *list_info;
//初始化服务器端的配置信息
struct server_conf_st server_conf={.multi_group=MULTICAST_GROUP,\
                                   .multi_port=PORT,\
                                   .file_lib=DEFAULT_FILE_LIB,\
                                   .interface=DEFAULT_INTERFACE,\
                                   .state=STATE_DAEMON
                                    };

/*
-M  指定多播组
-P  指定port
-F  指定前台运行
-D  指定媒体库，即音乐流的库
-I  指定网络设备
-H  显示帮助
*/
static void print_help()
{
    printf("-M   指定多播组\n \
            -P   指定接收端口\n \
            -F   指定前台运行\n  \
            -D   指定媒体库\n \
            -I   指定网络设备\n \
            -H   显示参数帮助\n"); //打印提示信息    
}

static int  socket_init()
{
    struct ip_mreqn sock_info;
    struct sockaddr_in server;
    sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd<0)
    {
        syslog(LOG_ERR,"socket():%s",strerror(errno));
        exit(1);
    }
    inet_pton(AF_INET,server_conf.multi_group,&sock_info.imr_multiaddr);
    inet_pton(AF_INET,"0.0.0.0",&sock_info.imr_address);
    sock_info.imr_ifindex = if_nametoindex(server_conf.interface);
    if(setsockopt(sockfd,IPPROTO_IP,IP_MULTICAST_IF,&sock_info,sizeof(sock_info))<0) //设置为多播组
    {
        syslog(LOG_ERR,"setsockopt():%s",strerror(errno));
        exit(1);
    }  
    //绑定服务器端口信息，以免client出现ip端口不匹配
    // inet_pton(AF_INET,"192.168.184.3",&server.sin_addr.s_addr);
    // server.sin_family=AF_INET;
    // server.sin_port = htons(atoi(server_conf.multi_port));
    // if(bind(sockfd,(struct sockaddr *)&server,sizeof(server))!=0) //不能再bind了，这里服务器和客户端bind相同就无效了
    // {
    //     syslog(LOG_ERR,"bind():%s",strerror(errno));
    //     exit(1);
    // }
    //设置发到的结构体的信息，组播地址，端口号等
    sendder.sin_family = AF_INET;
    sendder.sin_port = htons(atoi(server_conf.multi_port));
    inet_pton(AF_INET,server_conf.multi_group,&sendder.sin_addr.s_addr);
    return 0; 
}


static void daemon_exit(int s)
{
    thr_list_destory();
    channel_destoryall();
    mlib_freechannel_list(list_info);
    
    syslog(LOG_WARNING,"signal-%d caught.exit now",s);
    closelog();
    exit(0);
}

static int daemonize(void)
{
    pid_t pid;
    int fd;
    pid = fork();
    if(pid<0)
    {
        //perror("fail to fork!\n");
        syslog(LOG_ERR,"fork():%s",strerror(errno));
        //用系统日志来等级错误信息，第一个参数是错误的级别,不要加\n会被当做日志记录
        return -1;
    }
    if(pid>0)
    {
        exit(0);
    }
    fd = open("/dev/null",O_RDWR);
    if(fd<0)
    {
        //perror("fail to open!\n");
        syslog(LOG_WARNING,"open():%s",strerror(errno));
        return -2;
    }
    dup2(fd,0);
    dup2(fd,1);
    dup2(fd,2);//重定向标准输入输出报错的文件描述符到空设备上
    if(fd>2)
    {
        close(fd); //关闭原来的文件描述符，防止内存泄漏
    }
    setsid();//将当前进程设置为守护进程
    chdir("/");//改变守护进程的工作目录
    umask(0);//修改权限掩码为全0
    return 0; 
}

int main(int argc, char *const argv[])
{
    int  c;

    int listsize;
    int err;
    int i;
    /*命令行参数分析*/
    struct sigaction sigset;
    sigset.sa_handler = daemon_exit; //该函数有一个默认参数，传入的是信号所对应的信号id
    //对信号集的处理要的都是信号集的指针,防止重入现象的发生 
    sigemptyset(&sigset.sa_mask);
    sigaddset(&sigset.sa_mask,SIGINT);
    sigaddset(&sigset.sa_mask,SIGQUIT);
    sigaddset(&sigset.sa_mask,SIGTERM);
    sigaction(SIGINT,&sigset,NULL);
    sigaction(SIGQUIT,&sigset,NULL);
    sigaction(SIGTERM,&sigset,NULL);


    openlog("netradio",LOG_PID | LOG_PERROR,LOG_DAEMON);//创建系统日志，接收守护进程的日志信息
    while(1)//这里必须要有输入，不指定前后台指定其他的也行
    {
        c = getopt(argc,argv,"M:P:D:I:HF");
        if(c<0)
        {
            //fprintf(stderr,"wrong input!\n");
            break;
        }
        switch(c)
        {
            case 'M':
                server_conf.multi_group = optarg;
                break;
            case 'P':
                server_conf.multi_port = optarg;
                break;
            case 'F':
                server_conf.state = STATE_FRONT;
                break;
            case 'D':
                server_conf.file_lib = optarg;
                break;
            case 'I':
                server_conf.interface = optarg;
                break;

            case 'H':
                print_help();
                exit(0);//打印帮助退出让其重新尝试
                break;
            default:
                abort();
                break;
        }
    }
    /*设置守护进程（对子进程设置setsid）*/
    if(server_conf.state==STATE_DAEMON)//变为后台守护进程
    {
        if(daemonize()!=0)
        {
            exit(1);
        }
    }
    else if(server_conf.state == STATE_FRONT)//前台程序
    {
        //do something
    }
    else
    {
        //fprintf(stderr,"error state!\n");
        syslog(LOG_ERR,"wrongs state!");
        exit(1);
    }


    /*socket初始化*/
    socket_init();


    /*获取频道信息*/
    err = mlib_getchannel_list(&list_info,&listsize); //将所有节目的节目单信息都放在list_info，因为传入的是二级指针
    if(err)
    {
        syslog(LOG_ERR,"mlib_getchnlist():%s",strerror(errno));
        exit(1);
    }


    /*创建节目单单线程,通过socket发送*/
    err = thr_list_create(list_info,listsize);//输入的是节目单结构体的地址和节目单的数量
    if(err)//不为0则创建出错
    {
        exit(1);
    }

    /*创建各个频道多线程*/
    for(i=0;i<listsize;i++)
    {
        err = channel_create(list_info+i);//为每个频道创建一个线程去传送数据
        if(err)
        {
            fprintf(stderr,"thr_channel_create():%s\n",strerror(-err));
            exit(1);
        }
    }
    syslog(LOG_DEBUG,"%d is created!",i);//记录一个调试信息
    while (1)
    {
        pause();
    }
    return 0;
}
