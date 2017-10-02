#include <sys/types.h>          /* See NOTES */
#include<stdio.h>
#include<stdlib.h>
 #include <sys/types.h>
       #include <sys/socket.h>
       #include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
//#include <net/if.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdarg.h> 
#include <errno.h> 
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#if 1
#include"protocol.h"
#include "server.h"
#include <linux/if.h>
#include "nlk_ipc.h"
#include "ioos_uci.h"
#include "log.h"
#include "net.h"
#endif
static char *httphead="%s HTTP/1.1\r\nAccept-Language:zh-CN\r\nUser-Agent:Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; Trident/5.0)\r\nContent-Type: text/html; charset=UTF-8\r\nConnection:Keep-Alive\r\n"
"Content-Length: %d\r\n"
"Host: %s\r\n\r\n";
#define FWINFO "/tmp/fwinfo.txt"
#define FWINFOMSG "/tmp/fwinfomsg.txt"
struct host_info *dump_host_alive(int *nr);
//struct conn_info *dump_host_conn(struct in_addr addr, int *nr);
struct app_conn_info *dump_host_app(struct in_addr addr, int *nr);
#if 0
int tcp_connect(const char *host,const char *serv){
	int sockfd, n;
	struct addrinfo hints,*res,*ressave;
	bzero(&hints,sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if((n = getaddrinfo(host,serv,&hints,&res)) != 0 ){
		return -1;
	}
	ressave = res;
	do{
		sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
		if(sockfd < 0)continue;
		if(connect(sockfd,res->ai_addr,res->ai_addrlen) == 0)break;
		close(sockfd);
	}while((res = res->ai_next) != NULL);
	if(res == NULL){
		return  -1;
	}
	freeaddrinfo(ressave);
	return sockfd;
}
#endif
static int curl_post(char *send, char *rbuf,int rbuf_len,char*ip,char *port)
{
	//struct sockaddr_in addr;
	int sock_fd;
	//int sock_fd, addrlen;
	//int i =0;
	sock_fd = tcp_connect(ip,port);
	printf("sockfd = %d\n",sock_fd);
	if(sock_fd < 0){
		return -1;
	}
	int n;
	char buftmp[1024];
	memset(buftmp,0,sizeof(buftmp));
	sprintf(buftmp,httphead,send,rbuf_len,ip);
	n=write(sock_fd,buftmp,strlen(buftmp));
	printf("%s\n",buftmp);
	if(n < 0){
		printf("error");
		close(sock_fd);
		return -1;
	}
	n=write(sock_fd,rbuf,rbuf_len);
	printf("%s\n",rbuf);
	printf("write end\n");
	read(sock_fd,buftmp,1024);
	printf("buftmp=%s\n",buftmp);
	close(sock_fd);
	return 0;
}
#if 1
struct json_object* set_response(void){
	struct json_object* response;
	struct json_object* obj;
	response = json_object_new_object();
	int nr;
	int i;
	char mac[64];
	get_val("/tmp/ioosmac", "mac",mac,64);
	obj= json_object_new_string(mac);
	json_object_object_add(response, "mac",obj);
	struct host_info *hostinfo = dump_host_alive(&nr);
	struct json_object* terminals;
	terminals = json_object_new_array();
	printf("nr=%d\n",nr);
	if(nr > 0){
		for(i=0; i< nr; i++){
		struct json_object* terminal;
		terminal = json_object_new_object();
		struct host_info *p = &hostinfo[i];
		char macbuf[32];
		sprintf(macbuf,"%02X:%02X:%02X:%02X:%02X:%02X", p->mac[0], p->mac[1], \
                                        p->mac[2], p->mac[3], p->mac[4], p->mac[5]);
		obj= json_object_new_string(macbuf);
		json_object_object_add(terminal, "mac",obj);
		obj= json_object_new_int(p->vender);
		json_object_object_add(terminal, "vender",obj);
		obj= json_object_new_int(p->os_type);
		json_object_object_add(terminal, "os_type",obj);
		obj= json_object_new_string(p->name);
		json_object_object_add(terminal, "name",obj);
		struct json_object* apps;
		apps = json_object_new_array();
		int connr;
		struct app_conn_info *conn;
		conn = dump_host_app(p->addr, &connr);
		printf("CONNR=%d\n",connr);
		int j;
		for(j=0;j<connr;j++){
			struct json_object* app;
			app = json_object_new_object();
			int appid;
			struct app_conn_info *q = &conn[j];
			appid = q->appid;
			obj= json_object_new_int(appid);
			json_object_object_add(app, "id",obj);
			json_object_array_add(apps,app);
			
		}
		json_object_object_add(terminal, "apps",apps);
		json_object_array_add(terminals,terminal);
		if(conn){
			free(conn);
		}
		}
	}
	if(hostinfo)free(hostinfo);
	json_object_object_add(response, "terminals",terminals);
	return response;
}
#endif
int main(int argc,char**argv){
#if 1
	if(argc <= 2){
		dm_daemon();
		sleep(100);
	}
	do_cmd("ioos_setmac.sh");
#endif
	while(1){
#if 1
    	struct json_object* response; 
	response = set_response();
	char *text;
    	text=json_object_to_json_string(response);
	if(text == NULL){
		printf("text == NULL\n");
		sleep(60);
		continue;
	}
#endif
	char buf[1024];
	sprintf(buf,"{\"mac\":\"AA:BB:CC:DD:EE:FF\",\"terminals\":[{\"mac\":\"74:BA:AB:EE:EE:EE\",\"os_type\": 1,\"vender\": 1; \"name\":\"aaaaa\", \"apps\":[{\"id\": 333}]}]}");
//	curl_post("POST /index/home/api/reprot", buf,strlen(buf),"www.simicloud.com","80");
#if 1
	curl_post("POST /index/home/api/report", text,strlen(text),"www.simicloud.com","80");
	if(response){
    		json_object_put(response);
		response = NULL;
	}
#endif
	if(argc <= 2){
		sleep(600);
	}else{
		sleep(60);
	}
	}    
	return 0;
}
