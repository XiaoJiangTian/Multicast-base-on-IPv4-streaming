#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include"client.h"
#include"proto.h"
#include<getopt.h>
#include<proto.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/ip.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<net/if.h>
#include<errno.h>
#include<string.h>

#define BUFFSIZE 1024
//结构体创建加初始化
struct client_conf_st client_conf={
    .recevport = PORT,\
    .multigroup = MULTICAST_GROUP,\
    .player_cmd = DEFAULT_PLAYERCMD
};//默认的客户端的端口，多播组以及音频解析器的信息



static void print_help(void )
{
    printf("-M  --mgroup 指定多播组\n \
            -P  --port   指定接收端口\n \
            -p  --player 指定播放器\n  \
            -H  --help   显示参数帮助\n"); //打印提示信息
}
/*
    设置提示信息
    -M  --mgroup 指定多播组
    -P  --port   指定接收端口
    -p  --player 指定播放器
    -H  --help   显示参数帮助

*/

static ssize_t writen(int fd,const char *data,int len)
{
    int pos = 0;
    int ret = 0;
    while (len>0)
    {
        ret = write(fd,data+pos,len);//向管道写端写数据
        if(ret<0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            perror("write()");
            return -1;
        }
        pos += ret;
        len -= ret;
    }
    return pos;
}



int main(int argc, char const *argv[])
{
    int pipefd[2];
    int sockfd;
    int ret;
    int channel_num;
    pid_t pid;
    struct ip_mreqn mreq;
    struct sockaddr_in client,server,r_st;
    socklen_t server_len,r_st_len;
    ssize_t len;

    //解析命令行参数，是否更改组播地址、端口或音频解析器等
    char c;
    int index =1;
    struct option argarray[] = {{"port",1,NULL,'P'},\
                                {"mgroup",1,NULL,'M'},\
                                {"player",1,NULL,'p'},\
                                {"help",1,NULL,'H'},\
                                {NULL,0,NULL,0}};
    /*
        初始化
        级别：默认值，配置文件，环境变量，命令行参数（从左到右越来越高）
    */
    while(1)
    {
        c = getopt_long(argc,argv,"P:M:p:H",argarray ,&index);//分析命令行传参
        if(c<0)
        {
            break;
        }
        switch (c)
        {
        case 'P':
            client_conf.recevport = optarg;
            break;
        case 'M':
            client_conf.multigroup = optarg;
            break;
        case 'p':
            client_conf.player_cmd = optarg;
            break;
        case 'H':
            print_help();
            break;
        default:
            abort();
            break;
        }
    }

    //创建套接字，加入多播组
    sockfd = socket(AF_INET,SOCK_DGRAM,0);//创建socket进行通信
    if(sockfd<0)
    {
        perror("fail to socket!\n");
        exit(1);
    }

    //多播组地址，客户端ip以及index的填充,设置多播组和端口复用。绑定客户端信息
    inet_pton(AF_INET,client_conf.multigroup,&mreq.imr_multiaddr);
    inet_pton(AF_INET,"0.0.0.0",&mreq.imr_address);
    mreq.imr_ifindex = if_nametoindex("ens33");
    if(setsockopt(sockfd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq))<0)//设置该socket加入多播组
    {
        perror("fail to setsockopt1");
        exit(1);
    }

    int reuse=1;
    if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))<0) //设置复用端口
    {
        perror("fail to setsockopt2");
        exit(1);
    }

    client.sin_family = AF_INET;
    client.sin_port = htons(atoi(client_conf.recevport));//字节序和类型的转换
    inet_pton(AF_INET,"0.0.0.0",&client.sin_addr.s_addr);
    if(bind(sockfd,(struct sockaddr *)&client,sizeof(client))<0)
    {
        perror("fail to bind\n");
        exit(1);
    }
    
    //管道用于父子进程间传送接收到的音频信息，并给解码器解析
    if(pipe(pipefd)<0)
    {
        perror("fail to pipe!\n");
        exit(1);
    }

    //创建子进程
    pid = fork();
    if(pid<0)
    {
        perror("fail to fork!");
        exit(1);
    }
    if(pid == 0 )
    {
        close(sockfd);//子进程不负责网络通信，不写管道，关闭不需要的描述符，并且将管道的读重定向的标准输入，作为mpg123的输入
        close(pipefd[1]);
        dup2(pipefd[0],0);//将标准输入指向管道读端，mpg123会读取标准输入fd的内容并进行解析
        if(pipefd[0]>0)
        {
            close(pipefd[0]);
        }
        execl("/bin/sh","sh","-c",client_conf.player_cmd,NULL); //调用mpg123对传入的数据进行解析\
        //sh -c 后面跟字符串，意思为将此字符串作为一个完整的命令来执行，可扩大sudo的作用域，作用于整个命令
        //list 一个一个单词传入
        //vector 直接传入字符数组且不需加null结束
        perror("execl error!\n");
        exit(1);
        //子进程从管道读端读数据并且解析音频数据
    }
    else{
        //父进程接收数据并发送给子进程
        
        //收节目单
        struct msg_list_st *list_info;
        list_info = malloc(MSG_LIST_MAX);
        if(list_info==NULL)
        {
            perror("fail to malloc");
            exit(1);
        }
        server_len = sizeof(server);
        while (1)
        {
            len = recvfrom(sockfd,list_info,MSG_LIST_MAX,0,(struct sockaddr*)&server,&server_len);
            if(len<sizeof(struct msg_list_st))
            {
                fprintf(stderr,"message is to small!\n");
                continue;
            }
            if(list_info->chnid !=LIST_CHANEL_ID)
            {
                fprintf(stderr,"chnid is not match\n");
                continue;
            }
            break;
        }
        //测试，打印服务器ip和端口号
        char buf_1[BUFFSIZE];
        inet_ntop(AF_INET,&server.sin_addr.s_addr,buf_1,BUFFSIZE);
        printf("from ip:%s port:%d\n",buf_1,ntohs(server.sin_port));
  
        //打印节目单并且 选择频道,使用一个频道信息结构体来接收相应的数据，每次移动该结构体的长度到下个结构体，这是一个变长结构体，所以只能通过其结构体内的元素len_channel_info来确定
        struct msg_channel_info_st *channel_info;
        for(channel_info = list_info->chanel_list;(char *)channel_info<(((char *)list_info)+len);channel_info = (void *)(((char *) channel_info) + ntohs(channel_info->len_channel_info)))//转换为void *防止类型不匹配
        {
            printf("channel:%d,content:%s\n",channel_info->chnid,channel_info->desc); //打印出节目单id和对应内容描述
        }
        free(list_info);//节目单在打印显示后将内存释放
        
        //选择频道
        puts("choice the channel:\n");
        while (1)
        {
            ret = scanf("%d",&channel_num);
            if(ret!=1)
            {
                exit(1);
            }
            break;//正常输入为1,就直接break
        }
        
        //收对应的频道包，发送给子进程
        struct msg_channel_st * channel_st;
        channel_st = malloc(MSG_CHANNEL_MAX);
        if(channel_st ==NULL)
        {
            perror("fail to malloc!");
            exit(1);
        }
        r_st_len = sizeof(r_st);
        while (1)
        {
            len = recvfrom(sockfd,channel_st,MSG_CHANNEL_MAX,0,(struct sockaddr *)&r_st,&r_st_len);
            if( r_st.sin_addr.s_addr != server.sin_addr.s_addr \
            || r_st.sin_port != server.sin_port)
            {
                fprintf(stderr,"different server ip or port\n");
                continue;
            }
            if(len<sizeof(struct msg_channel_st))//这里对结构体进行sizeof即，变长数组的最短时刻的数据量
            {
                fprintf(stderr,"to small info!\n");
                continue;
            }
            //可控制流速，不一定要传来就写入管道，而是达到一定量再写入
            if(channel_st->chnid==channel_num)//接收对应频道的内容
            {
                fprintf(stdout,"channel info from chnid:%d\n",channel_st->chnid);
                if(writen(pipefd[1],channel_st->data,len-sizeof(chnid_t))<0)//len-sizeof(chnid_t)
                {
                    exit(1);
                }
                //重置复用数据
            }
        }
        free(channel_st);//释放资源
        close(sockfd);//关闭套接字
        exit(0);   
    }
    exit(0);
}
