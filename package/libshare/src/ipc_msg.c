#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/un.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include "ipc_msg.h"
#include "nlk_ipc.h"

#define IPC_LOG_PATH  "/tmp/ipc_log"
#define IPC_DEBUG(fmt,args...) do{}while(0)
//#define IPC_DEBUG(fmt,args...) do{console_printf("[IPC:%05d]:"fmt, __LINE__, ##args);}while(0)
#define IPC_ERROR(fmt,args...) do{ \
	ipc_log(IPC_LOG_PATH, "[IPC:%05d,%d]:"fmt, __LINE__, getpid(), ##args); \
}while(0)

enum {
	IPCF_NORMAL = 0,
	IPCF_ONLY_SEND,
};

void ipc_log(char *file, const char *fmt, ...)
{
	va_list ap;
	FILE *fp = NULL;
	char bakfile[32] = {0,};

	fp = fopen(file, "a+");
	if (fp == NULL)
		return;
	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
	if (ftell(fp) > 10*1024)
		snprintf(bakfile, sizeof(bakfile), "%s.bak", file);
	fclose(fp);
	if (bakfile[0])
		rename(file, bakfile);
}

int ipc_write(int fd, char *buf, int len)
{
	int wlen = 0, ylen = 0, offset = 0;

	if ((!buf) || (len <= 0)) {
		IPC_ERROR("data err\n");
		return -1;
	}

	wlen = len;
	while((ylen = write(fd, &buf[offset], wlen)) != wlen) {
		if(ylen > 0) {
			offset += ylen;
			wlen -= ylen;
		} else if (errno == EAGAIN || errno == EINTR) {
		  	continue;
		} else {
			IPC_ERROR("ylen:%d, errno:%d, err:%s\n", ylen, errno, strerror(errno));
			return -1;
		}
	}
	return 0;
}

int ipc_read(int fd, char *buf, int len)
{
	int rlen = 0, ylen = 0, offset = 0;

	if ((!buf) || (len <= 0)){
		IPC_ERROR("data err\n");
		return -1;
	}

	rlen = len;
	while ((ylen = read(fd, &buf[offset], rlen)) != rlen) {
		if(ylen > 0) {
			offset += ylen;
			rlen -= ylen;
		} else if (ylen == 0) {
			break;
		} else if (errno == EAGAIN || errno == EINTR) {
		  	continue;
		} else {
			IPC_ERROR("ylen:%d, errno:%d, err:%s\n", ylen, errno, strerror(errno));
			return -1;
		}
	}
	return 0;
}

static int ipc_setsock(int sock, int time)
{
	struct timeval timeout;

	timeout.tv_sec = time;
	timeout.tv_usec = 0;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
		IPC_ERROR("rcvtimeo fail, %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int ipc_data_push(struct ipc_header *msg, int *offset, void *data, int len)
{
	if ((!msg) || (!offset) || (!data)) {
		IPC_ERROR("data err\n");
		return -1;
	}

	if (*offset >= msg->len || \
			(len + *offset) > msg->len) {
		IPC_ERROR("data wrong:%d, %d, %d\n", *offset, msg->len, len);
		return -1;
	}

	memcpy(IPC_DATA(msg) + *offset, data, len);
	(*offset) += len;
	return 0;
}

int ipc_data_pop(struct ipc_header *msg, int *offset, void *data, int len)
{
	if ((!msg) || (!offset) || (!data)) {
		IPC_ERROR("data err\n");
		return -1;
	}

	if (*offset >= msg->len || \
			(len + *offset) > msg->len) {
		IPC_ERROR("data wrong:%d, %d, %d\n", *offset, msg->len, len);
		return -1;
	}

	memcpy(data, IPC_DATA(msg) + *offset, len);
	(*offset) += len;
	return 0;
}

//ipc client
static int ipc_client_init(char *path)
{
	int sock = 0, len = 0, ret = 0;
	struct sockaddr_un addr;

	if ((!path) || (!strlen(path))) {
		IPC_ERROR("path is err\n");
		return -1;
	}

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		IPC_ERROR("create socket fail, errno:%d, %s\n", errno, strerror(errno));
		return -1;
	}

	memset (&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);
	len = sizeof(addr.sun_family) + sizeof(addr.sun_path);

	ret = connect(sock, (struct sockaddr *)&addr, len);
	if (ret < 0) {
		close(sock);
		IPC_ERROR("connect error:%d, path:%s\n", ret, path);
		return -1;
	} else {
		IPC_DEBUG("connect OK,socket=%d\n", sock);
	}
	return sock;
}

int ipc_only_send(char *path, int msg, void *send_buf, int send_len)
{
	struct ipc_header *hdr;
	int ret = IPC_RET_ERR, sock = 0;
	int offset = 0;

	if ((!path) || (!strlen(path))) {
		IPC_ERROR("path is err\n");
		return ret;
	}

	if (!send_buf)
		send_len = 0;

	sock = ipc_client_init(path);
	if (sock < 0)
		return ret;

	hdr = (struct ipc_header *)malloc(IPC_TOTAL_LEN(send_len));
	if (hdr == NULL) {
		IPC_ERROR("malloc fail\n");
		goto err;
	}

	hdr->msg = msg;
	hdr->len = send_len;
	hdr->flag = IPCF_ONLY_SEND;
	hdr->reply_len = 0;
	hdr->reply_buf = NULL;

	if (send_buf && (send_len > 0) && \
			(ipc_data_push(hdr, &offset, send_buf, send_len) < 0)) {
		goto err;
	}
	if (ipc_write(sock, (char *)hdr, IPC_TOTAL_LEN(send_len)) < 0) {
		IPC_ERROR("write hdr err\n");
		goto err;
	}
	ret = 0;
err:
	if (hdr)
		free(hdr);
	close(sock);
	return ret;
}

int ipc_send(char *path, int msg, void *send_buf, int send_len, void *recv_buf, int recv_len)
{
	struct ipc_header *hdr;
	int ret = IPC_RET_ERR, sock = 0;
	int offset = 0;

	if ((!path) || (!strlen(path))) {
		IPC_ERROR("path is err\n");
		return ret;
	}

	if (!send_buf)
		send_len = 0;
	if (!recv_buf)
		recv_len = 0;

	sock = ipc_client_init(path);
	if (sock < 0)
		return ret;

	ipc_setsock(sock, 3);

	hdr = (struct ipc_header *)malloc(IPC_TOTAL_LEN(send_len));
	if (hdr == NULL) {
		IPC_ERROR("malloc fail\n");
		goto err;
	}

	hdr->msg = msg;
	hdr->len = send_len;
	hdr->flag = IPCF_NORMAL;
	hdr->reply_len = recv_len;
	hdr->reply_buf = NULL;

	if (send_buf && (send_len > 0) && \
			(ipc_data_push(hdr, &offset, send_buf, send_len) < 0)) {
		goto err;
	}

	if (ipc_write(sock, (char *)hdr, IPC_TOTAL_LEN(send_len)) < 0) {
		IPC_ERROR("write hdr err\n");
		goto err;
	}

	free(hdr);
	hdr = NULL;

	hdr = (struct ipc_header *)malloc(IPC_TOTAL_LEN(recv_len));
	if (hdr == NULL) {
		IPC_ERROR("malloc fail\n");
		goto err;
	}
	memset(hdr, 0, IPC_TOTAL_LEN(recv_len));

	if (ipc_read(sock, (char *)hdr, IPC_TOTAL_LEN(recv_len)) < 0) {
		IPC_ERROR("read hdr err, 0x%X, %d\n", msg, send_len);
		goto err;
	}

	offset = 0;
	if (recv_buf && (recv_len > 0) && \
			(ipc_data_pop(hdr, &offset, recv_buf, recv_len) < 0)) {
		goto err;
	}

	ret = hdr->msg;
err:
	if (hdr)
		free(hdr);
	close(sock);
	return ret;
}

//ipc server
static void sig_hander(int signo)
{
	IPC_ERROR("recv signal, signal number = %d\n", signo);
	exit(0);
}

int ipc_server_init(char *path)
{
	int len = 0, sock = 0;
	sigset_t sig;
	struct sockaddr_un addr;

	if (!path)
		return IPC_RET_ERR;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		IPC_ERROR("create socket fail, %s\n", strerror(errno));
		return IPC_RET_ERR;
	}

	if (fcntl(sock, F_SETFD, FD_CLOEXEC) < 0) {
		IPC_ERROR("set FD_CLOEXEC fail, %s\n", strerror(errno));
	}

	unlink(path);
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);
	len = sizeof(addr.sun_family) + strlen(addr.sun_path);

	if (bind(sock, (struct sockaddr *)&addr, len) < 0) {
		IPC_ERROR("bind socket fail, %s\n", strerror(errno));
		close(sock);
		return IPC_RET_ERR;
	}

	if (listen(sock, 5) < 0) {
		IPC_ERROR("listen on socket fail, %s\n", strerror(errno));
		close(sock);
		return IPC_RET_ERR;
	}

	sigemptyset(&sig);
	/*sigaddset(&sig, SIGALRM);*/
	sigaddset(&sig, SIGABRT);
	sigaddset(&sig, SIGPIPE);
	sigaddset(&sig, SIGQUIT);
	/*sigaddset(&sig, SIGTERM);*/
	sigaddset(&sig, SIGUSR1);
	sigaddset(&sig, SIGUSR2);
	sigprocmask(SIG_BLOCK, &sig, NULL);
	signal(SIGBUS, sig_hander);
	signal(SIGHUP, sig_hander);
	signal(SIGILL, sig_hander);

	IPC_DEBUG("ipc server success, path:%s, sock:%d\n", path, sock);
	return sock;
}

int ipc_server_accept(int sock, ipc_proc *proc)
{
	socklen_t len = 0;
	struct sockaddr addr;
	int read_sock = 0;
	struct ipc_header hdr;
	struct ipc_header *phdr = NULL;
	struct ipc_header *reply_phdr = NULL;
	int ret = IPC_RET_ERR;

	read_sock = accept(sock, &addr, &len);
	if(read_sock < 0) {
		IPC_ERROR("accept fail, sock:%d\n", sock);
		return ret;
	}

	memset(&hdr, 0, sizeof(hdr));
	if(ipc_read(read_sock, (char *)&hdr, sizeof(hdr)) < 0) {
		IPC_ERROR("read header err\n");
		goto err;
	}

	phdr = (struct ipc_header *)malloc(IPC_TOTAL_LEN(hdr.len));
	if (phdr == NULL) {
		IPC_ERROR("malloc fail\n");
		goto err;
	}

	memcpy(phdr, &hdr, IPC_HEADER_LEN);
	if ((phdr->len > 0) && \
			(ipc_read(read_sock, (char *)IPC_DATA(phdr), phdr->len) < 0)) {
		goto err;
	}

	reply_phdr = (struct ipc_header *)malloc(IPC_TOTAL_LEN(phdr->reply_len));
	if (reply_phdr == NULL) {
		IPC_ERROR("malloc fail\n");
		goto err;
	}
	memset(reply_phdr, 0, IPC_TOTAL_LEN(phdr->reply_len));
	phdr->reply_buf = reply_phdr;
	reply_phdr->len = phdr->reply_len;

	if (proc) {
		reply_phdr->msg = proc(phdr);
	} else {
		IPC_ERROR("ipc_hander_s.proc is null\n");
		goto err;
	}

	if (hdr.flag != IPCF_ONLY_SEND) {
		if(ipc_write(read_sock, (char *)reply_phdr, IPC_TOTAL_LEN(reply_phdr->len)) < 0) {
			IPC_ERROR("write header err, 0x%X, %d\n", hdr.msg, hdr.len);
			goto err;
		}
	} else {
		IPC_ERROR("flag err, 0x%X, %d\n", hdr.msg, hdr.len);
	}
	ret = 0;
err:
	if (phdr)
		free(phdr);
	if (reply_phdr)
		free(reply_phdr);
	close(read_sock);
	return ret;
}

int ipc_reply(struct ipc_header *msg, void *data, int len)
{
	int offset = 0;
	struct ipc_header *reply = msg->reply_buf;

	if ((!data) || (len <= 0) || (!reply)) {
		IPC_ERROR("data err, %p,%p,%d\n", data, reply, len);
		return -1;
	}

	if (ipc_data_push(reply, &offset, data, len) < 0)
		return -1;
	return 0;
}
