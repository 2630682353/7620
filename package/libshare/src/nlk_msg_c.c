#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <assert.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/if_ether.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include "igd_share.h"
#include "nlk_ipc.h"
#include "nlk_msg_c.h"

#if 0
#define NLK_UK_ERROR(fmt,args...) do{}while(0)
#else
#define NLK_LOG_PATH  "/tmp/nlk_log"
#define NLK_UK_ERROR(fmt,args...) do{nlk_log(NLK_LOG_PATH, "[ERR:%05d]:"fmt, __LINE__, ##args);}while(0)
#endif

#define NLK_UK_ASSERT(exp) do{if(!(exp)){NLK_UK_ERROR("ASSERT "#exp" ERROR\n");exit(-1);}}while(0)
#define NLK_GOTO_ERR(v,r)   do{if(v){ret = (r);goto err;}}while(0)

#define NETLINK_USERMSG  25
#define NETLINK_BROADCAST  26

#ifndef NLMSG_HDRLEN
#define NLMSG_HDRLEN NLMSG_LENGTH(0)
#endif

void nlk_log(char *file, const char *fmt, ...)
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

int nlk_uk_init(struct nlk_msg_handler *h, __u32 type,__u32 src_grp, __u32 dst_grp, pid_t pid)
{
	int size = 0;

	NLK_UK_ASSERT(h);

	memset(h, 0, sizeof(struct nlk_msg_handler));
	h->sock_fd = socket(PF_NETLINK, SOCK_RAW, type);
	if (h->sock_fd < 0) {
		NLK_UK_ERROR("create socket failed, %s\n", strerror(errno));
		return -1;
	}

	h->src_addr.nl_family = AF_NETLINK;
	h->src_addr.nl_pid =(int)getpid();
	h->src_addr.nl_groups = src_grp;

	if (bind(h->sock_fd, (struct sockaddr * )&h->src_addr, sizeof(h->src_addr)) < 0) {
		close(h->sock_fd);
		NLK_UK_ERROR("bind socket failed, %d,%s\n", h->sock_fd, strerror(errno));
		h->sock_fd = -1;
		return -1;
	}

	h->dst_addr.nl_family = AF_NETLINK;
	h->dst_addr.nl_pid = pid;
	h->dst_addr.nl_groups = dst_grp; 
	h->seq = 0;

	size = 65535;
	setsockopt(h->sock_fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
	size = 65535;
	setsockopt(h->sock_fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
	return 0;
}

struct nlmsghdr *nlk_uk_alloc(struct nlk_msg_handler *h, __u16 msg_type, __u32 len)
{
	struct nlmsghdr *nlh = NULL;

	NLK_UK_ASSERT(h);

	nlh = (struct nlmsghdr *)malloc(len);
	if (nlh == NULL) {
		NLK_UK_ERROR("nlh malloc failed, len:%d\n", len);
		return NULL;
	}

	memset((char *)nlh, 0, len);
	nlh->nlmsg_len = len;
	nlh->nlmsg_pid = h->src_addr.nl_pid;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_type = msg_type;
	h->seq++;
	nlh->nlmsg_seq = h->seq;

	return nlh;
}

int nlk_uk_data_push(struct nlmsghdr *msg, int *offset, void *data, int len)
{
	NLK_UK_ASSERT(msg);
	NLK_UK_ASSERT(offset);
	NLK_UK_ASSERT(data);

	if (*offset >= NLMSG_PAYLOAD(msg,0) || \
			len > (NLMSG_PAYLOAD(msg,0) - *offset)) {
		NLK_UK_ERROR("data wrong:%d, %d, %d, %d\n", *offset, msg->nlmsg_len, NLMSG_HDRLEN, len);
		return -1;
	}

	memcpy(NLMSG_DATA(msg) + *offset, data, len);
	(*offset) += len;

	return 0;
}

int nlk_uk_data_pop(struct nlmsghdr *msg, int *offset, void *data, int len)
{
	NLK_UK_ASSERT(msg);
	NLK_UK_ASSERT(offset);
	NLK_UK_ASSERT(data);

	if (*offset >= NLMSG_PAYLOAD(msg,0) || \
			len > (NLMSG_PAYLOAD(msg,0) - *offset)) {
		NLK_UK_ERROR("data wrong:%d, %d, %d, %d\n", *offset, msg->nlmsg_len, NLMSG_HDRLEN, len);
		return -1;
	}

	memcpy(data, NLMSG_DATA(msg) + *offset, len);
	(*offset) += len;
	return 0;
}

int nlk_uk_send(const struct nlk_msg_handler *h, struct nlmsghdr *msg)
{
	NLK_UK_ASSERT(h);
	NLK_UK_ASSERT(msg);

	int ret = 0;
	ret = sendto(h->sock_fd, (void *)msg, msg->nlmsg_len, 0,\
					(struct sockaddr *)&h->dst_addr, sizeof(struct sockaddr_nl));
	if (ret <= 0) {
		NLK_UK_ERROR("sendto ret:%d, error:%s,pid:%d msgid:%d, sock:%d\n", ret, strerror(errno), getpid(), msg->nlmsg_type, h->sock_fd);
		return -1;
	}

	return ret;
}

int nlk_uk_recv(const struct nlk_msg_handler *h, void *buf, int buf_len)
{
	ssize_t recv_len; 
	socklen_t addrlen;
	struct nlmsghdr *nlh;

	NLK_UK_ASSERT(h);
	NLK_UK_ASSERT(buf);

	if (buf_len < NLMSG_HDRLEN){
		NLK_UK_ERROR("buf_len:%d < NLMSG_HDRLEN\n", buf_len);
		return -1;
	}

	addrlen = sizeof(h->dst_addr);
	recv_len = recvfrom(h->sock_fd, buf, buf_len, 0, (struct sockaddr *)&h->dst_addr, &addrlen);
	if (recv_len < NLMSG_HDRLEN) {
		NLK_UK_ERROR("rcvd_len(%d) < NLMSG_HDRLEN(%d)\n", recv_len, NLMSG_HDRLEN);
		return recv_len;
	}

	if (addrlen != sizeof(h->dst_addr)) {
		NLK_UK_ERROR("addrlen != sizeof(h->dst_addr)\n");
		return -1;
	}

	nlh = (struct nlmsghdr *)buf;
	if (nlh->nlmsg_flags & MSG_TRUNC || nlh->nlmsg_len != recv_len) {
		NLK_UK_ERROR("wrong data, %d, %d, %d\n", nlh->nlmsg_flags, nlh->nlmsg_len, recv_len);
		return -1;
	}

	if (nlh->nlmsg_seq != h->seq) {
		NLK_UK_ERROR("nlh->nlmsg_seq=%d, h->sep=%d\n", nlh->nlmsg_seq, h->seq);
		return -1;
	}

	return recv_len;
}

int nlk_uk_date_put(struct nlmsghdr *nlh, char *rcv_buf, __u32 r_len, __u32 *save_len)
{
	int offset = 0;
	__u32 date_len = 0;

	if ((!nlh) || (!rcv_buf) || (!r_len) || (!save_len)) {
		NLK_UK_ERROR("input err\n");
		return -1;
	}

	date_len = NLMSG_PAYLOAD(nlh,0);

	if (date_len > r_len - *save_len) {
		NLK_UK_ERROR("len err, %d, %d, %d\n", date_len, r_len, *save_len);
		return -1;
	}

	if (0 != nlk_uk_data_pop(nlh, &offset, &rcv_buf[*save_len], date_len)) {
		return -1;
	}
	*save_len += date_len;
	return 0;
}


/****************************************
input:
	pid:  destination process id;
	msg_type:  message type;
	header:  message header;
	h_len:  message header len;
	body_buf:  real data;
	b_len:  real data len;
	rcv_buf:  donot null, date form kernel input it;
	r_len:  rcv_buf  size;
return:
	0:success;
	<0; fail;	
****************************************/
int nlk_uk_swap (
	pid_t pid,
	__u16 msg_type,
	void *header, __u32 h_len,
	void *body_buf, __u32 b_len,
	void *rcv_buf, __u32 r_len) 
{
	struct nlk_msg_handler h;
	struct nlmsghdr *nlh = NULL;
	int ret = -1, offset = 0, rcv_len = 0;
	__u32 save_len = 0;

	ret = nlk_uk_init(&h, NETLINK_USERMSG, 0, 0, pid);
	NLK_GOTO_ERR(ret, NLK_ERR_INIT);

	nlh = nlk_uk_alloc(&h, msg_type, NLMSG_SPACE(h_len + b_len));
	NLK_GOTO_ERR(!nlh, NLK_ERR_NOMEM);

	ret = nlk_uk_data_push(nlh, &offset, header, h_len);
	NLK_GOTO_ERR(ret, NLK_ERR_DATE);

	if (b_len > 0) {
		ret = nlk_uk_data_push(nlh, &offset, body_buf, b_len);
		NLK_GOTO_ERR(ret, NLK_ERR_DATE);
	}

	ret = nlk_uk_send(&h, nlh);
	NLK_GOTO_ERR(ret == -1, NLK_ERR_SEND);

	if (nlh) {
		free(nlh);
		nlh = NULL;
	}
	nlh = (struct nlmsghdr *)malloc(NLK_MSG_BUF_MAX);
	NLK_GOTO_ERR(!nlh, NLK_ERR_NOMEM);

	while(1) {
		memset(nlh, 0, NLK_MSG_BUF_MAX);
		rcv_len = nlk_uk_recv(&h, nlh, NLK_MSG_BUF_MAX);

		NLK_GOTO_ERR(rcv_len <= 0, NLK_ERR_RCV);

		if (!(nlh->nlmsg_flags & NLM_F_ACK)) {
			NLK_UK_ERROR("nlmsg_flags & NLM_F_ACK\n");
			NLK_GOTO_ERR(1, NLK_ERR_DATE);
		}

		offset = 0;
		if (nlh->nlmsg_type == NLK_MSG_END) {
			if (0 != nlk_uk_data_pop(nlh, &offset, &ret, sizeof(ret))) {
				NLK_GOTO_ERR(1, NLK_ERR_DATE);
			}
			break;
		} else if (nlh->nlmsg_type == NLK_MSG_RET) {
			ret = nlk_uk_date_put(nlh, rcv_buf, r_len, &save_len);
			NLK_GOTO_ERR(ret != 0, NLK_ERR_DATEPUT);
		} else {
			NLK_UK_ERROR("msg type(%d) err\n", nlh->nlmsg_type);
			NLK_GOTO_ERR(1, NLK_ERR_FAIL);
		}
	}
err:
	close(h.sock_fd);
	if (nlh)
		free(nlh);
	return ret;
}

int nlk_head_send(unsigned short type, void *header, unsigned int h_len)
{
	return nlk_uk_swap(0, type, header, h_len, NULL, 0, NULL, 0);
}

int nlk_data_send(unsigned short type, void *header, unsigned int h_len,
						void *body_buf, unsigned int b_len)
{
	return nlk_uk_swap(0, type, header, h_len, body_buf, b_len, NULL, 0);
}

int nlk_head_send_recv(unsigned short type, void *header, unsigned int h_len,
				void *rcv_buf, unsigned int r_len)
{
	return nlk_uk_swap(0, type, header, h_len, NULL, 0, rcv_buf, r_len);
}

int nlk_data_send_recv(unsigned short type, void *header, unsigned int h_len,
					void *body_buf, unsigned int b_len, void *rcv_buf, unsigned int r_len)
{
	return nlk_uk_swap(0, type, header, h_len, body_buf, b_len, rcv_buf, r_len);
}

int nlk_dump_from_kernel(unsigned short msgid, void *header,
			unsigned int h_len, void *recv_buf, int max_nr)
{
	int nr = 0, loop = 0, dump_nr = 0;
	struct nlk_msg_comm *nlk = NULL;
	char *dst = NULL;
	unsigned int dst_len = 0;

	if ((!header) || (!recv_buf))
		return NLK_ERR_DATE;

	nlk = (struct nlk_msg_comm *)malloc(h_len);
	if (nlk == NULL)
		return NLK_ERR_NOMEM;

keep_dump:
	dst = recv_buf;
	memcpy(nlk, header, h_len);

	nlk->index = dump_nr;
	dst += nlk->obj_len*dump_nr;
	dst_len = (max_nr - dump_nr)*nlk->obj_len;

	if (nlk->obj_nr > (max_nr - dump_nr))
		nlk->obj_nr = max_nr - dump_nr;

	nr = nlk_head_send_recv(msgid, nlk, h_len, dst, dst_len);
	if (nr > 0) {
		dump_nr += nr;
	} else {
		goto dump_over;
	}
	loop++;

	if ((nr < nlk->obj_nr) || (max_nr == dump_nr)) {
		goto dump_over;
	} else if (max_nr < dump_nr) {
		dump_nr = NLK_ERR_DUMP;
		goto dump_over;
	}

	if ((nr > 0) && (loop < 10000))
		goto keep_dump;

dump_over:
	if (nlk)
		free(nlk);
	return dump_nr;
}

int nlk_event_init(struct nlk_msg_handler *h, int grp)
{
	return nlk_uk_init(h, NETLINK_BROADCAST, grp, 0, 0);
}

int nlk_event_recv(struct nlk_msg_handler *h, void *buf, int buf_len)
{
	int len = 0;
	char recv_buf[8192] = {0,};
	ssize_t recv_len; 
	socklen_t addrlen;
	struct nlmsghdr *nlh;

	if (!buf || !buf_len || !h) {
		NLK_UK_ERROR("input is err, %p, %d, %p\n", buf, buf_len, h);
		return -1;
	}

	addrlen = sizeof(h->dst_addr);
	recv_len = recvfrom(h->sock_fd, recv_buf, sizeof(recv_buf),
			MSG_DONTWAIT, (struct sockaddr *)&h->dst_addr, &addrlen);
	if (recv_len < 0) {
		NLK_UK_ERROR("rcvd_len < 0, %s\n", strerror(errno));
		return recv_len;
	}

	if (addrlen != sizeof(h->dst_addr)) {
		NLK_UK_ERROR("addrlen != sizeof(h->dst_addr)\n");
		return -1;
	}

	nlh = (void *)recv_buf;
	if (buf_len <= 0 || nlh->nlmsg_len != recv_len) {
		NLK_UK_ERROR("wrong data, %d, %d, %d\n", buf_len, nlh->nlmsg_len, recv_len);
		return -1;
	}

	len = (recv_len > buf_len) ? buf_len : recv_len;
	memcpy(buf, NLMSG_DATA(nlh), len);
	return len;
}

int nlk_event_send(int grp, void *buf, int buf_len)
{
	struct nlk_msg_comm nlk;

	memset(&nlk, 0, sizeof(nlk));
	nlk.gid = grp;
	nlk.obj_len = buf_len;

	return nlk_data_send(NLK_MSG_BROADCAST, &nlk, sizeof(nlk), buf, buf_len);
}
