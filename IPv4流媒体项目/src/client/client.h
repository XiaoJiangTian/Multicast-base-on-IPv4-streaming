#ifndef CLIENT_H__
#define CLIENT_H__

#define DEFAULT_PLAYERCMD "/usr/bin/mpg123 - > /dev/null" //除了运行mpg123，其他内容重定向不显示
//mgp123后面加 - 代表播放从标准输入来的数据
struct client_conf_st
{
    char *recevport;
    char *multigroup;
    char *player_cmd;
};

extern struct client_conf_st client_conf;//使用extern关键声明，扩展了当前全局变量的作用域，可以在其他.c文件中使用

#endif
