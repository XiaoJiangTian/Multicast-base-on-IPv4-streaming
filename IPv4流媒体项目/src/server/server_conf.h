#ifndef SERVER_CONF_H__
#define SERVER_CONF_H__

#define DEFAULT_FILE_LIB "/var/media"
#define DEFAULT_INTERFACE "ens33"

enum
{
    STATE_DAEMON =1,
    STATE_FRONT
};

struct server_conf_st
{
    char *multi_group;
    char *multi_port;
    char state;
    char *file_lib;
    char *interface;
};

extern int sockfd; 
extern struct sockaddr_in sendder;//多播组信息
extern struct server_conf_st server_conf;


#endif