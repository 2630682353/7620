#if !defined(_GETUI_H_)
#define _GETUI_H_
/**
* 个推协议封装接口
* 2014-12-9 donglin
* Gexin Interactive Inc.
*
* 主要包含三个接口：
* 1. 协议库初始化
* 2. 发送消息封装
* 3. 接收数据处理
*/

#define RECEIVE_BUF_SIZE 1*1024 + 512

#ifdef __cplusplus
extern "C" {               // 告诉编译器下列代码要以C链接约定的模式进行链接
#endif

#include "gttypes.h"
#include "command.h"

/*重置上一次遗留的数据  断网重连*/
extern void  reset_data(void *handle_buf);
/*
 * 协议库初始化
 * return:
 *   返回接收handle指针
 */
extern void* getui_init(struct GETUI_CONFIG *config);

/*
 * 将消息命令command转换成字节流，保存到buf中
 * command: command.h中定义的命令消息体
 * return:
 *   字节流的长度
*/
extern int getui_command_to_send(BYTE* buf, int len, int command_id, void *command);

/*
 * 将接收到的字节流输入协议库，如果有解析到完整命令，通过GETUI_HANDLER进行回调
 * command: command.h中定义的命令消息体
 * return:
 *   0: succeeded  -1:failed
*/
struct GETUI_COMMAND
{
	int command_id;
	void* command;
} __attribute__ ((packed));

extern int getui_command_to_receive(int command_max_num, void *handle_buf, struct GETUI_COMMAND *commands, BYTE* buf, int len);

/*
* 释放命令消息体内存，需要根据不同的command_id做针对性的释放工作
* command: command.h中定义的命令消息体
* return:
*   0: succeeded  -1:failed
*/
extern int getui_command_release(struct GETUI_COMMAND* command);

extern char * get_version();

#ifdef __cplusplus
}
#endif

#endif
