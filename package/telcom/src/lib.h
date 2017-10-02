#ifndef __TELCOM_LIB_H__
#define __TELCOM_LIB_H__

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <error.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
//#include <sys/sysinfo.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <stddef.h>
#include <errno.h>
#include <time.h>
#include <stddef.h>
#include <assert.h>

#include <openssl/ssl.h>
#include <openssl/ec.h>
#include <openssl/aes.h>

#include <jansson.h>

#include "linux_list.h"

#define CFG_ENABLE_WIAIR_API	1
#define CFG_ENABLE_AES	1

#if CFG_ENABLE_WIAIR_API
#include "nlk_ipc.h"
#endif /*CFG_ENABLE_WIAIR_API*/

#define DEBUG2(fmt, args...) do{fprintf(stderr, "DEBUG:%s:%d "fmt, __FUNCTION__, __LINE__,##args);}while(0)
#define DEBUG_S(fmt, args...) do{fprintf(stderr, "SERVER:%s:%d "fmt, __FUNCTION__, __LINE__,##args);fprintf(stderr,"\n");}while(0)

#define DEBUG(fmt, args...) do{fprintf(stderr, "DEBUG:%s:%d "fmt, __FUNCTION__, __LINE__,##args);fprintf(stderr,"\n");}while(0)
#define ERROR(fmt, args...) do{fprintf(stderr,"\n");fprintf(stderr, "ERROR:%s:%d "fmt, __FUNCTION__, __LINE__,##args);}while(0)

#define ERROR_S(fmt, args...) do{fprintf(stderr,"\n");fprintf(stderr, "SERVER_ERROR:%s:%d "fmt, __FUNCTION__, __LINE__,##args);}while(0)

#define DEBUG_TEST(fmt,args...) \
	do{fprintf(stderr, "DEBUG:%s:%d "fmt, __FUNCTION__, __LINE__,##args);fprintf(stderr,"\n");}while(0);\
	igd_log("/tmp/telcom", "[%d]DBG:%s[%d]:"fmt, sys_uptime(), __FUNCTION__, __LINE__, ##args);
/*
#define ERROR(fmt,args...) \
	do{fprintf(stderr, "ERROR:%s:%d "fmt, __FUNCTION__, __LINE__,##args);fprintf(stderr,"\n");}while(0);\
	igd_log("/tmp/telcom", "[%d]ERROR:%s[%d]:"fmt, sys_uptime(), __FUNCTION__, __LINE__, ##args);
*/



#define PORT 32768

#define ECDH_SIZE	14 /*112bit*/
//#define ECDH_STATIC_SECRET "9b87cebb4b481d296e4564bb7268"
#define ECDH_STATIC_SECRET "9b87cebb4b481d296e4564bb72680000"

extern u_int64_t jiffies;

struct wifitimer{
	int enable;

	struct{
		int start;
		int end;
	}week;

	struct{
		int hour;
		int minute;
	};
};

#define BUFFER_SIZE	4096
struct buffer{
	struct list_head list;

	int wait_ack;
	json_t * json; /* cached */

	int len;
	int offset;
	char data[BUFFER_SIZE];
};

struct client_timer{
	struct list_head list;

	int type;
	int status;
	u_int64_t last;
};

/*
 * client
 */
struct client{
	struct list_head list;

	char mac[6];

	int fd;

	int seq;

	struct{
		EC_KEY * key;
		unsigned char my_pub_key[ECDH_SIZE + 2]; /* 16B=128b, my pub key */
		unsigned char peer_pub_key[ECDH_SIZE + 2]; /* peer's pub key */
		unsigned char secret[ECDH_SIZE + 2]; /* shared secret for aes-cbc */
		//这个key怎么治有16位了
	}ecdh;

	struct{
		DH *k1;
		
		BIGNUM *p;
		BIGNUM *g;
		BIGNUM *pub_key;	/* g^x */
		BIGNUM *priv_key;	/* x */

		/*
		EC_KEY * key;
		unsigned char my_pub_key[ECDH_SIZE + 2]; // 16B=128b, my pub key 
		unsigned char peer_pub_key[ECDH_SIZE + 2]; // peer's pub key 
		unsigned char secret[ECDH_SIZE + 2]; // shared secret for aes-cbc 
		*/
	}dh;

	struct client_timer * main_line;

	struct list_head timers;

	struct list_head snd_queue;
	struct list_head rcv_queue;
	struct list_head wait_ack_queue; //TODO

	struct list_head hosts; //TODO
};

/*
 * server & it' clients
 */
struct server{
	int fd;

	struct list_head clients;
};
void print_json(json_t * json);
char * oct2string(unsigned char * oct, int len);
void string2oct(char * str, unsigned char * oct, int len);

EC_KEY *ecdh_pub_key(unsigned char * pub_key);
unsigned char *ecdh_shared_secret(EC_KEY *ecdh, unsigned char *peer_pub_key, unsigned char * shared_secret);

int _aes_encrypt(char * key, int key_len, char * in, int in_len, char * out);
int _aes_decrypt(char * key, int key_len, char * in, int in_len, char * out);

char * mac2str(char * mac);
char * str2mac(char * str);
char * get_mac(char * ifname);

char * json_str_get(json_t * json, char * key);
int json_parse_head(json_t * json, char ** type, int * seq, char ** mac);
int json_is_ack(json_t * json);

struct buffer * buffer_new(void);
void buffer_free(struct buffer * b);

int client_socket_init(void);
int server_socket_init(void);

struct client_timer * client_timer_new(void);
void client_timer_init(struct client_timer * t, int type, int status, u_int64_t last);
void client_timer_free(struct client_timer * t);
struct client_timer * client_timer_find_by_type(struct client * c, int type);

struct client * client_new(void (*init)(struct client *));
void client_free(struct client *c);
void client_del(struct client *c);
int client_recv(struct client * c);
int client_send(struct client * c);

json_t * json_header(char * type, int seq, char * mac);
int _client_send_ack(struct client * c, int seq, int encrypt);

int client_queue_send(struct client * c, char * buffer, int len, json_t * json, int wait_ack);
int client_queue_json(struct client * c, json_t * json, int wait_ack, int encrypt);

void server_init(struct server * s);
int client_recv_server(struct client * c);
int client_send_server(struct client * c);
int _client_send_ack_jiami(struct client * c, int seq, int encrypt);

#endif /*__TELCOM_LIB_H__*/
