#ifndef _COMLIB_H
#define _COMLIB_H

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
#include <libgen.h>

#define TVLESS(a,b) ((a).tv_sec == (b).tv_sec ? \
				((a).tv_usec < (b).tv_usec) : \
				((a).tv_sec < (b).tv_sec))
#define TVLESSEQ(a,b) ((a).tv_sec == (b).tv_sec ? \
				((a).tv_usec <= (b).tv_usec) : \
				((a).tv_sec <= (b).tv_sec))
				
#define arr_strcpy(dst,src) do{strncpy(dst,src,sizeof(dst)-1);dst[sizeof(dst)-1]=0;}while(0)


int exe_default(void *cmdline);
int exec_cmd(const char * fmt,...);
int do_daemon(void);
int handler_sig(void);

#endif
