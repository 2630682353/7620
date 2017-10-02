#ifndef _IGD_MU_COMMON_
#define _IGD_MU_COMMON_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <assert.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/if_ether.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include "ioos_uci.h"
#include "uci_fn.h"
#include "linux_list.h"
#include "nlk_ipc.h"
#include "ipc_msg.h"
#include "module.h"

struct schedule_entry {
	struct list_head list;
	struct timeval tv;
	struct timeval tv_bak;
	void (*cb)(void *data);
	void *data;
};

#define TVLESS(a,b) ((a).tv_sec == (b).tv_sec ? \
				((a).tv_usec < (b).tv_usec) : \
				((a).tv_sec < (b).tv_sec))
#define TVLESSEQ(a,b) ((a).tv_sec == (b).tv_sec ? \
				((a).tv_usec <= (b).tv_usec) : \
				((a).tv_sec <= (b).tv_sec))

struct igd_dns {
	int nr;
	struct in_addr addr[DNS_IP_MX];
};

#ifndef NIPQUAD
#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0],\
	((unsigned char *)&addr)[1],\
	((unsigned char *)&addr)[2],\
	((unsigned char *)&addr)[3]
#endif

extern struct list_head event_head;
extern module_t MODULE[MODULE_MX];
extern int mu_call(MSG_ID mgsid, void *data, int len, void *rbuf, int rlen);
extern int exe_default(void *cmdline);
extern int exec_cmd(const char * fmt,...);
extern void init_scheduler (void);
extern struct timeval *process_schedule(struct timeval *ptv);
extern int dschedule(struct schedule_entry *event);
extern struct schedule_entry *schedule (struct timeval tv, void (*cb)(void *data), void *data);

#endif
