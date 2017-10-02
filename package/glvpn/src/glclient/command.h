#if !defined(_COMMAND_H_)
#define _COMMAND_H_

#ifdef __cplusplus
// 告诉编译器下列代码要以C链接约定的模式进行链接
extern "C" {
#endif

#include "gttypes.h"

/////////////////////////////解析库//////////////////////////////
//初始化配置
struct GETUI_CONFIG
{
	// 是否协议压缩。1：压缩 0：未加密
	int is_compress;
	// 是否协议加密。1：加密 0：未加密
	// TODO:目前暂不支持加密
	int is_encrypt;
	// RC4密钥信息（暂不支持）
	int rc4key_len;
	BYTE* rc4key;
	// 平台标识。 3：WP 4：Android 5：iOS 6：wifi模块
	BYTE platform;
  // =1开 》=3关闭
	int log_level;
} __attribute__ ((packed));

//数据包存储
struct CMD_HANDLE_BUF
{
	BYTE * remain_buf;
	int remain_buf_pos;
	int remain_buf_len;
	int offset;//上次的偏移数据用于包的丢弃用
	int package_len;//用于横跨多个包数据的丢弃用；
	struct GETUI_CONFIG *config;
}__attribute__((packed));

// 注册
#define CMD_REG_ID 1
struct CMD_REG
{
	char* imsi;
	char* imei;
	char* registerid;
	char* appid;
} __attribute__ ((packed));

// 注册响应
#define CMD_REG_RSP_ID 101
struct CMD_REG_RSP
{
	unsigned long long session;
	char* registerid;
} __attribute__ ((packed));

// 登录
#define CMD_LOGIN_ID 2
struct CMD_LOGIN
{
	unsigned long long session;
	BYTE login_type;
	char* mac;
	char* ssid;
} __attribute__ ((packed));

// 登录响应
#define CMD_LOGIN_RSP_ID 102
struct CMD_LOGIN_RSP
{
	BYTE result;
	char *error_msg;
	unsigned long long session;
} __attribute__ ((packed));

// 心跳（无命令数据）
#define CMD_HEARTBEAT_ID 3

// 心跳响应（无命令数据）
#define CMD_HEARTBEAT_RSP_ID 103

// 重定向地址
struct CMD_REDIRECT_ADDR
{
	char* address; // 地址，<ip/domain+port>
	long timeout; // 地址有效期
} __attribute__ ((packed));

//重定向
#define CMD_P2P_REDIRECT_RSP_ID 117
struct CMD_P2P_REDIRECT_RSP
{
	int delay;//重定向延时
	char *idc_ioc;
	char *idc_config;
	short addr_num;   //地址列表数量
	struct CMD_REDIRECT_ADDR* addr_arr;  //地址列表
}__attribute__((packed));

#define CMD_RECEIVE_LOCAL_P2P_ID 118
#define CMD_RECEIVE_P2P_ID 115

//下行透传消息
struct CMD_RECEIVE_P2P
{
	unsigned long timestamp;
	char* cid;
	char* taskid;
	char* messageid;
	//char* payload;
	short payload_len;
	BYTE* payload;
}__attribute__((packed));

//下行消息回执
#define CMD_RECEIVE_P2P_RSP_ID 15
struct CMD_RECEIVE_P2P_RSP
{
	unsigned long timestamp;
	char* cid;
	char* taskid;
	char* messageid;
	short actionid;
}__attribute__((packed));

//上行消息得到服务器的响应命令
#define CMD_SEND_P2P_RSP_ID 116
struct CMD_SEND_P2P_RSP
{
	unsigned long timestamp;
	char* cid;
	char* taskid;
	char* messageid;
	short actionid;
}__attribute__((packed));

//上行消息
#define CMD_SEND_P2P_ID 16
struct CMD_SEND_P2P
{
	unsigned long timestamp;
	char* cid;
	char* taskid;
	char* messageid;
	//char* payload;
	short payload_len;
	BYTE* payload;
}__attribute__((packed));

#ifdef __cplusplus
}
#endif
#endif
