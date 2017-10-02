#ifndef __IPC_COMM_H__
#define __IPC_COMM_H__

#include <stdio.h>

struct ipc_header {
	int msg;
	int len;
	int flag;
	int reply_len;
	void *reply_buf;
};

typedef int ipc_proc(struct ipc_header *msg);

#define IPC_RET_ERR   -1119
#define IPC_HEADER_LEN   (sizeof(struct ipc_header))
#define IPC_TOTAL_LEN(len)   (len + IPC_HEADER_LEN)
#define IPC_DATA(msg)   ((void*)(((char*)msg) + IPC_TOTAL_LEN(0)))

extern int ipc_data_push(struct ipc_header *msg, int *offset, void *data, int len);
extern int ipc_data_pop(struct ipc_header *msg, int *offset, void *data, int len);
extern int ipc_only_send(char *path, int msg, void *send_buf, int send_len);
extern int ipc_send(char *path, int msg, void *send_buf, int send_len, void *recv_buf, int recv_len);
extern int ipc_server_init(char *path);
extern int ipc_server_close(char *path);
extern int ipc_server_accept(int sock, ipc_proc *proc);
extern int ipc_reply(struct ipc_header *msg, void *data, int len);

#endif //__IPC_COMM_H__
