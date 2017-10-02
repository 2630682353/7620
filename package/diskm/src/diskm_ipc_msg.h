#ifndef __IPC_COMM_H__
#define __IPC_COMM_H__

#include <stdio.h>

struct disk_ipc_header {
	int msg;
	int len;
	union {
		int flag; //use for send user assign send type
		int response; //use for send to request for response
	}direction;
};
enum {
	IPCF_NORMAL = 0,
	IPCF_ONLY_SEND,
};


#define DISKM_IPC_RET_ERR   -110
#define DISKM_IPC_HEADER_LEN   (sizeof(struct disk_ipc_header))
#define DISKM_IPC_TOTAL_LEN(len)   (len + DISKM_IPC_HEADER_LEN)
#define DISKM_IPC_DATA(msg)   ((void*)(((char*)msg) + DISKM_IPC_TOTAL_LEN(0)))

extern int diskm_ipc_data_push(struct disk_ipc_header *msg, int *offset, void *data, int len);
extern int diskm_ipc_data_pop(struct disk_ipc_header *msg, int *offset, void *data, int len);
extern int diskm_ipc_only_send(char *path, int msg, void *send_buf, int send_len);
extern int diskm_ipc_send(char *path, int msg, void *send_buf, int send_len, void *recv_buf, int recv_len);
extern int diskm_ipc_server_init(char *path);
int diskm_ipc_client_init(char *path);
int diskm_ipc_read(int fd, char *buf, int len);
int diskm_ipc_write(int fd, char *buf, int len);

#endif //__IPC_COMM_H__
