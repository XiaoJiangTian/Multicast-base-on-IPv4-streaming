#ifndef PROTO_H__
#define PROTO_H__

#include <site_type.h>
//client和server端共用的ip和port
#define MULTICAST_GROUP  "224.2.2.2"
#define PORT              "1989"

//频道信息
#define CHANNEL_NUM       100
#define LIST_CHANEL_ID    0
#define MIN_CHANEL_ID     1
#define MAX_CHANEL_ID     MIN_CHANEL_ID+CHANNEL_NUM-1

//频道信息结构体信息
#define MSG_CHANNEL_MAX  (65536-20-8)  //减去的分别是ip包和udp包的包头长度
#define MAX_DATA         (MSG_CHANNEL_MAX-sizeof(chnid_t))

//节目单结构体信息
#define MSG_LIST_MAX     (65535-20-8)
#define MAX_CHANNEL_INFO_ST (MSG_LIST_MAX-sizeof(chnid_t))

//频道信息的结构体
struct  msg_channel_st
{
    chnid_t chnid;
    uint8_t data[1];//变长数组
}__attribute__((packed)); //取消字节对其操作

//每一个频道的具体信息结构体
struct msg_channel_info_st
{
    chnid_t chnid;
    uint16_t len_channel_info;
    uint8_t desc[1];//介绍信息
}__attribute__((packed));

//节目单信息的结构体
struct msg_list_st
{
    chnid_t chnid;
    struct msg_channel_info_st chanel_list[1];//节目单的具体介绍信息，变长数组
}__attribute__((packed));


#endif