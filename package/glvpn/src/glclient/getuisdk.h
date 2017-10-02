#if !defined(_GETUI_SDK_H_)
#define _GETUI_SDK_H_

#ifdef __cplusplus
extern "C" {               // 告诉编译器下列代码要以C链接约定的模式进行链接
#endif

#include "gttypes.h"
//初始化配置信息
struct GETUI_LIB_CONFIG
{
	char* appkey;
	char* appid;
	char* appsecret;

	char* registerid;
	char* deviceid;
	char* host;
	unsigned short port;
	unsigned short log_level;
	char* mac;
	char* ssid;
} __attribute__ ((packed));

#define GETUI_CMD_CID_ID 10002
//cid回调命令
struct GETUI_LIB_CID_COMMAND
{
	char * cid;
} __attribute__ ((packed));

#define GETUI_CMD_PUSHMESSAGE_ID 10001
struct GETUI_LIB_PUSHMESSAGE_COMMAND
{
	char* appkey;
	char* appid;
	char* messageid;
	char* taskid;
	// 透传消息
	char* payload;


} __attribute__ ((packed));
//发送消息反馈回调，目前主要用于发送消息失败的回调 消息发送失败actionid=20000
#define GETUI_CMD_SENDMESSAGE_FEEDBACK_ID 1003
struct GETUI_CMD_SENDMESSAGE_FEEDBACK
{
	char* cmdid;
	char* cid;
	char* appid;
	char* taskid;
	char* actionid;
	char* result;
	unsigned long timestamp;
}__attribute__((packed));

//下行消息命令
#define GETUI_CMD_RECEIVE_P2P_MESSAGE_ID 1004
struct GETUI_CMD_RECEIVE_P2P_MESSAGE
{
	unsigned long timestamp;
	char* cid;
	char* taskid;
	char* messageid;
	//char* payload;
	short payload_len;
	BYTE* payload;
}__attribute__((packed));

//发送上行消息 响应回执命令
#define GETUI_CMD_SEND_P2P_RSP_ID 1005
struct GETUI_CMD_SEND_P2P_RSP
{
	unsigned long timestamp;
	char* cid;
	char* taskid;
	char* messageid;
	short actionid;
}__attribute__((packed));

//session返回指令
#define GETUI_CMD_LOGIN_SESSION_ID 1006
struct GETUI_CMD_SESSION_RSP
{
	unsigned long long session;
}__attribute__((packed));

struct GETUI_CMD_SENDMESSAGE
{
	char* cmdid;
	char* taskid;
	char* payload;
}__attribute__((packed));
//上行消息
struct GETUI_CMD_SEND_P2P_MESSAGE
{
	unsigned long timestamp;
	char* cid;
	char* taskid;
	char* messageid;
	//char* payload;
	short payload_len;
	BYTE* payload;
}__attribute__((packed));

//接收到下行消息的回执命令
struct GETUI_CMD_RECEIVE_P2P_RSP
{
	unsigned long timestamp;
	char* cid;
	char* taskid;
	char* messageid;
	short actionid;
}__attribute__((packed));

// bindapp
#define CMD_BINDAPP_ID 12
struct CMD_BINDAPP
{
	char* cmdid;
	char* cid;
	char* appid;
	char* type;
}__attribute__((packed));

// bindapp响应
#define CMD_BINDAPP_RSP_ID 112
struct CMD_BINDAPP_RSP
{
	char* cmdid;
	char* cid;
	char* appid;
	char* type;
	char* result;
}__attribute__((packed));

// pushmessage
#define CMD_PUSHMESSAGE_ID 113
struct CMD_ACTION
{
	short actionid; // 动作ID
	char* type; // 动作类型
	char* next; // 下一条动作
}__attribute__((packed));

struct CMD_PUSHMESSAGE
{
	char* cmdid;
	char* appkey;
	char* appid;
	char* messageid;
	char* taskid;
	char* condition; // 暂不支持
	// 动作链信息
	short action_num;
	struct CMD_ACTION* action_chain;
	// 透传消息
	char* payload;
}__attribute__((packed));

// pushmessage_feedback
#define CMD_PUSHMESSAGE_FEEDBACK_ID 13
struct CMD_PUSHMESSAGE_FEEDBACK
{
	char* cmdid;
	char* appkey;
	char* appid;
	char* cid;
	char* messageid;
	char* taskid;
	char* actionid;
	char* result;
	unsigned long timestamp;
}__attribute__((packed));

#define CMD_SENDMESSAGE_ID 14
struct CMD_SENDMESSAGE
{
	char* cmdid;
	char* cid;
	char* appid;
	char* taskid;
	char* payload;
}__attribute__((packed));

#define CMD_SENDMESSAGE_FEEDBACK_ID 114
struct CMD_SENDMESSAGE_FEEDBACK
{
	char* cmdid;
	char* cid;
	char* appid;
	char* taskid;
	char* actionid;
	char* result;
	unsigned long timestamp;
}__attribute__((packed));

// received回执
#define CMD_RECEIVED_ID 10
struct CMD_RECEIVED
{
	char* cmdid;
	char* cid;
}__attribute__((packed));

// received回执响应
#define CMD_RECEIVED_RSP_ID 110
struct CMD_RECEIVED_RSP
{
	char* cmdid;
	char* cid;
}__attribute__((packed));

typedef void (*GETUI_CALLBACK)(int commandid, void *command);
//getui_session没有传0
extern int getui_lib_init(struct GETUI_LIB_CONFIG *config, GETUI_CALLBACK cb, struct GETUI_CMD_SESSION_RSP *getui_session);
//getui_session没有传0
extern int getui_reset_host(struct GETUI_LIB_CONFIG *config, GETUI_CALLBACK cb, struct GETUI_CMD_SESSION_RSP *getui_session);

extern void getui_lib_command_release(int commandid, void *command);

extern void getui_lib_sendmsg(struct GETUI_CMD_SENDMESSAGE * sendmsg);

extern char * getui_get_version();

extern void getui_lib_send_p2p_msg(struct GETUI_CMD_SEND_P2P_MESSAGE *send_p2p_msg);

extern void getui_lib_send_receive_p2p_rsp(struct GETUI_CMD_RECEIVE_P2P_RSP* receive_p2p_rsp);

#ifdef __cplusplus
}
#endif

#endif
