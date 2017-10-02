#ifndef __NLK_MSG_H__
#define __NLK_MSG_H__

#include <asm/types.h>
#include <linux/netlink.h>

struct nlk_msg_handler {
	struct sockaddr_nl src_addr;
	struct sockaddr_nl dst_addr;
	int sock_fd;
	unsigned int seq;
};

enum NLK_RET_ERR {
	NLK_ERR_FAIL = -301,
	NLK_ERR_NOMEM = -302,
	NLK_ERR_DATE = -303,
	NLK_ERR_INIT = -304,
	NLK_ERR_SEND = -305,
	NLK_ERR_RCV = -306,
	NLK_ERR_DATEPUT = -307,
	NLK_ERR_DUMP = -308,
};

extern int nlk_head_send(unsigned short type, void *header, unsigned int h_len);
extern int nlk_data_send(unsigned short type, void *header, unsigned int h_len,
									void *body_buf, unsigned int b_len);
extern int nlk_head_send_recv(unsigned short type, void *header, unsigned int h_len,
									void *rcv_buf, unsigned int r_len);
extern int nlk_data_send_recv(unsigned short type, void *header, unsigned int h_len,
					void *body_buf, unsigned int b_len, void *rcv_buf, unsigned int r_len);
extern int nlk_dump_from_kernel(unsigned short msgid, void *header,
										unsigned int h_len, void *recv_buf, int max_nr);
extern int nlk_event_init(struct nlk_msg_handler *h, int grp);
extern int nlk_event_recv(struct nlk_msg_handler *h, void *buf, int buf_len);
extern int nlk_event_send(int grp, void *buf, int buf_len);

#endif //__NLK_MSG_H__
