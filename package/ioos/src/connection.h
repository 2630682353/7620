#ifndef _http_connection_h_
#define _http_connection_h_
#include <arpa/inet.h>
#include <stdarg.h> 
#include <errno.h> 
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
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
#include <linux/netlink.h>
#include <linux/if_ether.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include "common.h"
#include "json.h"
#include "server.h"
#include "linux_list.h"

typedef struct v_list{
    char*key;
    char*value;
    struct v_list *next;
} v_list_t;

enum {
	CON_STATE_READ = 0,
	CON_STATE_WRITE,
	CON_STATE_ERROR,
	CON_STATE_CLOSE,
};

typedef struct request{
	char *ptr;
	int used;
	int size;
}request_t;

#define BUFFER_PR_SIZE 4096
#define BUFFER_PR_SIZE_MAX 8192

typedef struct connection{
	struct list_head node;
	char state;    
	int fd;
	int num;
	request_t req;
	request_t res;
	char *rhead;
	int rhead_len;
	int rsize;
	int function;
	char *csp;
	int gzip;
	int login;
	v_list_t head;
	char *ip_from;
	char *ranged_req;
	struct json_object* response; 
	void (*read)(struct connection*);
	void (*write)(struct connection*);
	void (*handle)(struct connection*);
	void (*free)(struct connection*);
	//ioos for cgi
	int sock;
	char *request;
	char *encoding; // Accept-Encoding
	char *http_ver; // HTTP/1.1
	char *http_ctype; // default: application/json
	char *http_head;
	char *http_body;
	int http_head_len;
	int http_body_len;
}connection_t;

extern connection_t list;
extern char* con_value_get(connection_t *con,char*key);
extern void connections_init(connection_t *list);
extern int connection_is_set(connection_t *con);
extern connection_t* connections_search(connection_t *list,int fd);
extern void connections_del(connection_t *list,connection_t*con);
extern int  connections_add(connection_t *list,int fd);
#endif
