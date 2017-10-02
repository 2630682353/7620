#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <netinet/ip_icmp.h>
#include <errno.h>
#include <time.h>
#include <sys/timeb.h>
#include <signal.h>
#include "getuisdk.h"
#include "command.h"
#include "nlk_ipc.h"
#include "ipc_msg.h"
#include "module.h"
#include "igd_interface.h"
#include "igd_vpn.h"

#ifdef FLASH_4_RAM_32
#define GL_DEBUG(fmt,args...) do{}while(0)
#else
#define GL_DEBUG(fmt,args...) do{igd_log(GL_LOG_PATH, fmt, ##args);}while(0)
#endif

#define VPN_IP_MX 4
#define PPTP_PPPD "pppd-pptp"
#define GL_PRODUCT_ID_WIAIR "0001"

#define GL_LOG_PATH "/tmp/gl_log"
#define PPPD_LOG_FILE "/tmp/pptp_pppd.log"
#define PPPD_ZIP_FILE "/tmp/pptp_pppd.log.gz"

struct vpn_cfg {
	unsigned short enable;
	unsigned short debug;
	char user[IGD_NAME_LEN];
	char pass[IGD_NAME_LEN];
	struct in_addr ips[VPN_IP_MX];
};

enum GL_MSG_CODE {
	GL_MSG_REGISTER_COD = 201,
	GL_MSG_VPN_CTRL_COD,
	GL_MSG_VPN_LOG_COD,
};

static int havecid;
static int registerd;
static unsigned long ctltimestamp;
static char logserver[IGD_NAME_LEN];
static char logdir[IGD_NAME_LEN];
static char logport[IGD_NAME_LEN];
static char callid[IGD_NAME_LEN * 2];
static char ctlmsgid[IGD_NAME_LEN * 2];
static struct vpn_cfg vcfg;
static struct GETUI_CMD_SESSION_RSP cysession;
static pthread_mutex_t gl_mutex;

extern int get_sysflag(unsigned char bit);

static int get_jsonval(char*value, int len, char*key, char *msg)
{
	char *p;
	char *pstart;
	char *pend;
	
	if ((p = strstr(msg, key)) == NULL)
		return -1;
	if ((p += strlen(key)) == NULL)
		return -1;
	if ((p = strchr(p, '"')) == NULL)
		return -1;
	if ((pstart = p + 1) == NULL)
		return -1;
	if ((pend = strchr(pstart, '"')) == NULL)
		return -1;
	memset(value, 0x0, len);
	if (!__arr_strcpy_end((unsigned char *)value, (unsigned char *)pstart, len, *pend))
		return -1;
	return 0;
}

unsigned short cal_chksum(unsigned short *addr, int len)
{      
	int nleft = len;
	int sum=0;
	unsigned short *w = addr;
	unsigned short answer = 0;

	while(nleft > 1) {      
		sum += *w++;
		nleft -= 2;
	}
	if( nleft == 1) {
		*(unsigned char *)(&answer) = *(unsigned char *)w;
		sum += answer;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer =~ sum;
	return answer;
}

int pack(unsigned char *data, int nr)
{ 
	int psize;
	struct icmp *icmp;
	struct timeval *tval;

	icmp = (struct icmp*)data;
	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;
	icmp->icmp_cksum = 0;
	icmp->icmp_seq = nr;
	icmp->icmp_id = getpid();
	psize = 8 + 56;
	tval = (struct timeval *)icmp->icmp_data;
	gettimeofday(tval, NULL);
	icmp->icmp_cksum = cal_chksum((unsigned short *)icmp, psize);
	return psize;
}

static int gl_test_pptp_speed(struct vpn_cfg *cfg)
{
	int sockfd;
	fd_set rset;
	char retstr[128];
	struct sockaddr_in daddr, raddr;
	struct protoent *protocol;
	struct timeval stm, otm, ntm;
	unsigned int delay = 0, cur = 0;
	int len, i, j, k = 0;
	
	if (!cfg->ips[1].s_addr)
		goto out;
	if((protocol = getprotobyname("icmp")) == NULL)
		goto out;
	if((sockfd = socket(AF_INET, SOCK_RAW, protocol->p_proto)) < 0)
		goto out;
	for (i = 0; i < VPN_IP_MX; i++) {
		if (!cfg->ips[i].s_addr)
			continue;
		memset(&daddr, 0x0, sizeof(daddr));
		daddr.sin_family = AF_INET;
		daddr.sin_addr = cfg->ips[i];
		for (j = 0; j < 3; j++) {
			FD_ZERO(&rset);
			FD_SET(sockfd, &rset);
			stm.tv_sec = 2;
			stm.tv_usec = 0;
			memset(retstr, 0x0, sizeof(retstr));
			len = pack((unsigned char *)retstr, (i * 3) + j);
			gettimeofday(&otm, NULL);
			ntm = otm;
			if (sendto(sockfd, retstr, len, 0,
					(struct sockaddr *)&daddr,
					(socklen_t)sizeof(daddr)) < 0) {
				ntm.tv_sec += 2;
			} else {
				if (select(sockfd + 1, &rset, NULL, NULL, &stm) > 0) {
					memset(retstr, 0x0, sizeof(retstr));
					if (recvfrom(sockfd, retstr, sizeof(retstr), 0,
								(struct sockaddr *)&raddr,
								(socklen_t *)&len) < 0) {
						GL_DEBUG("recvfrom ping package error\n");
						ntm.tv_sec += 2;
					} else if (daddr.sin_addr.s_addr != raddr.sin_addr.s_addr) {
						GL_DEBUG("recvfrom ping package error ip %s\n", inet_ntoa(raddr.sin_addr));
						ntm.tv_sec += 2;
					} else {
						gettimeofday(&ntm, NULL);
					}
				} else {
					GL_DEBUG("recv ping package timeout in 2s, ip %s\n", inet_ntoa(cfg->ips[i]));
					ntm.tv_sec += 2;
				}
			}
			cur += ((ntm.tv_sec - otm.tv_sec) * 1000000 + ntm.tv_usec - otm.tv_usec);
		}
		if (cur > 0 && (!delay || delay > cur)) {
			delay = cur;
			k = i;
		}
		cur = 0;
	}
	close(sockfd);
out:
	GL_DEBUG("use ip %s\n", inet_ntoa(cfg->ips[k]));
	return k;
}

static int gl_parser_vpn_param(char *msg, struct vpn_cfg *cfg)
{	
	int nr = 0;
	char *p, *tmp;
	char retstr[128];
	
	if (!get_jsonval(retstr, sizeof(retstr), "\"open\":", msg))
		cfg->enable = atoi(retstr);
	else
		cfg->enable = 1;
	if (cfg->enable == 0)
		return 0;
	if (get_jsonval(retstr, sizeof(retstr), "\"user\":", msg))
		return -1;
	arr_strcpy(cfg->user, retstr);
	if (get_jsonval(retstr, sizeof(retstr), "\"pass\":", msg))
		return -1;
	arr_strcpy(cfg->pass, retstr);
	if (!get_jsonval(retstr, sizeof(retstr), "\"debug\":", msg))
		cfg->debug = atoi(retstr);
	else
		GL_DEBUG("debug str is error [%s]\n", retstr);
	p = msg;
	loop_for_each(0, VPN_IP_MX) {
		if (get_jsonval(retstr, sizeof(retstr), "\"ip\":", p))
			break;
		inet_aton(retstr, &cfg->ips[nr]);
		tmp = strstr(p, "\"ip\":");
		GL_DEBUG("get ip %s\n", inet_ntoa(cfg->ips[nr]));
		p = tmp + 5;
		nr++;
	} loop_for_each_end();
	if (!nr) {
		GL_DEBUG("get 0 server ip addr\n");
		return -1;
	}
	return 0;
}

static void gl_vpn_restart(struct vpn_cfg *cfg)
{
	int k = 0;
	char cmd[256];
	char retstr[64];
	struct sys_msg_ipcp ipcp;
	
	snprintf(retstr, sizeof(retstr), "killall %s", PPTP_PPPD);
	system(retstr);
	memset(&ipcp, 0x0, sizeof(ipcp));
	ipcp.pppd_type = PPPD_TYPE_PPTP_CLIENT;
	mu_msg(VPN_MOD_PPPD_IPDOWN, &ipcp, sizeof(ipcp), NULL, 0);
	if (!cfg->enable)
		return;
	sleep(30);
	remove(PPPD_LOG_FILE);
	remove(PPPD_ZIP_FILE);
	memset(retstr, 0x0, sizeof(retstr));
	if (cfg->debug)
		snprintf(retstr, sizeof(retstr), "debug logfile %s ", PPPD_LOG_FILE);
	k = gl_test_pptp_speed(cfg);
	sprintf(cmd, "%s file "
			"/etc/ppp/options.pptp "
			"ifname ppp-pptp "
			"authfailnum 3 "
			"pptp_server %s "
			"user %s "
			"password %s "
			"pppd_type %d "
			"%s&",
			PPTP_PPPD,
			inet_ntoa(cfg->ips[k]),
			cfg->user,
			cfg->pass,
			PPPD_TYPE_PPTP_CLIENT,
			retstr);
	snprintf(retstr, sizeof(retstr), "killall -kill %s", PPTP_PPPD);
	system(retstr);
	system(cmd);
}

static void gl_send_msg_ack(struct GETUI_CMD_RECEIVE_P2P_MESSAGE *msg)
{
	struct GETUI_CMD_RECEIVE_P2P_RSP ack;
	memset(&ack, 0x0, sizeof(ack));
	ack.cid = msg->cid;
	ack.taskid = msg->taskid;
	ack.messageid = msg->messageid;
	ack.actionid = 10001;
	ack.timestamp = (unsigned long)time(NULL);
	getui_lib_send_receive_p2p_rsp(&ack);
	return;
}

static void gl_recv_msg_hander(struct GETUI_CMD_RECEIVE_P2P_MESSAGE *msg)
{
	int len = 0;
	char *p = NULL;
	char retstr[128];
	struct vpn_cfg cfg;
	
	gl_send_msg_ack(msg);
	GL_DEBUG("recv vpn param msg [%s], len %d\n", (char *)msg->payload, msg->payload_len);
	if (get_jsonval(retstr, sizeof(retstr), "\"code\":", (char *)msg->payload)) {
		GL_DEBUG("parser vpn msg code error [%s]\n", (char *)msg->payload);
		return;
	}
	switch (atoi(retstr)) {
	case GL_MSG_VPN_CTRL_COD:
		if (!strcmp(msg->messageid, ctlmsgid)) {
			GL_DEBUG("recv old msg, msg %s, time %lu, vcfg msg [%s], time %lu\n",
					msg->messageid, msg->timestamp, ctlmsgid, ctltimestamp);
			break;
		}
		if (get_jsonval(retstr, sizeof(retstr), "\"timestamp\":", (char *)msg->payload)) {
			GL_DEBUG("parser vpn msg timestamp error [%s]\n", (char *)msg->payload);
			return;
		}
		if (ctltimestamp >= atol(retstr)) {
			GL_DEBUG("recv vpn over time ctrl msg, [%s]\n", (char *)msg->payload);
			break;
		}
		ctltimestamp = atol(retstr);
		arr_strcpy(ctlmsgid, msg->messageid);
		if (gl_parser_vpn_param((char *)msg->payload, &cfg)) {
			GL_DEBUG("parser vpn param error, [%s]\n", (char *)msg->payload);
			return;
		}
		pthread_mutex_lock(&gl_mutex);
		memcpy(&vcfg, &cfg, sizeof(vcfg));
		pthread_mutex_unlock(&gl_mutex);
		gl_vpn_restart(&cfg);
		break;
	case GL_MSG_REGISTER_COD:
		if (registerd)
			break;
		if (get_jsonval(retstr, sizeof(retstr), "\"result\":", (char *)msg->payload)) {
			GL_DEBUG("parser register reply msg result error [%s]\n", (char *)msg->payload);
			return;
		}
		if (atoi(retstr)) {
			GL_DEBUG("register error[%s]\n", (char *)msg->payload);
			return;
		}
		if (!get_jsonval(retstr, sizeof(retstr), "\"logserver\":", (char *)msg->payload)) {
			len = __arr_strcpy_end((unsigned char *)logserver,
									(unsigned char *)retstr,
									sizeof(logserver), ':');
			if (len < 7 && len > 15) {
				GL_DEBUG("get logserver %s error, tmp %s\n", retstr, logserver);
				goto error;
			}
			p = strchr(retstr, ':');
			if (!p) {
				GL_DEBUG("get logserver port %s error\n", retstr);
				goto error;
			}
			p++;
			len = __arr_strcpy_end((unsigned char *)logport,
									(unsigned char *)p,
									sizeof(logport), '/');
			if (len < 2 && len > 5) {
				GL_DEBUG("get logport %s error,\n", retstr);
				goto error;
			}
			p = strchr(retstr, '/');
			if (!p) {
				GL_DEBUG("get logserver dir %s error\n", retstr);
				goto error;
			}
			arr_strcpy(logdir, p);
			registerd = 1;
			break;
		}
	error:
		registerd = 1;
		memset(logserver, 0x0, sizeof(logserver));
		break;
	default:
		break;
	}
	return;
}

static void gl_callback(int commandid, void *command)
{
	char str[32];
	struct vpn_info info;
	struct GETUI_CMD_SESSION_RSP *rsp;
	struct GETUI_CMD_RECEIVE_P2P_MESSAGE *msg;
	
	GL_DEBUG("###############callback commandid= %d\n", commandid);
	switch (commandid) {
	case GETUI_CMD_LOGIN_SESSION_ID:
		rsp = (struct GETUI_CMD_SESSION_RSP *)command;
		if (cysession.session == rsp->session) {
			GL_DEBUG("recv session sameid  %llu, cur is %llu\n", rsp->session, cysession.session);
			break;
		}
		cysession.session = rsp->session;
		snprintf(str, sizeof(str), "vpnid=%llu", rsp->session);
		mtd_set_val(str);
		GL_DEBUG("session id %llu\n", rsp->session);
		break;
	case GETUI_CMD_CID_ID:
		pthread_mutex_lock(&gl_mutex);
		arr_strcpy(callid, (char *)command);
		pthread_mutex_unlock(&gl_mutex);
		memset(&info, 0x0, sizeof(info));
		arr_strcpy(info.callid, (char *)command);
		GL_DEBUG("cid %s\n", (char *)command);
		mu_msg(VPN_MOD_SET_CALLID, &info, sizeof(info), NULL, 0);
		havecid = 1;
		break;
	case GETUI_CMD_RECEIVE_P2P_MESSAGE_ID:
		msg = (struct GETUI_CMD_RECEIVE_P2P_MESSAGE *)command;
		if (strcmp(msg->cid, callid)) {
			GL_DEBUG("recv error msg cid [%s], our cid is [%s]\n", msg->cid, callid);
			break;
		}
		gl_recv_msg_hander(msg);
		break;
	default:
		break;
	}
	getui_lib_command_release(commandid, command);
	GL_DEBUG("###############callback commandid= %d return\n", commandid);
}

int gl_logserver_connect(const char *url, const char *port)
{
	int ov = 65535;
	int sockfd = -1;
	struct addrinfo hints, *iplist, *ip;

	bzero(&hints,sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(url, port, &hints, &iplist))
		return -1;
	for (ip = iplist; ip != NULL; ip = ip->ai_next) {
		sockfd = socket(ip->ai_family, ip->ai_socktype, ip->ai_protocol);
		if (sockfd < 0)
			continue;
		if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (void *)&ov, sizeof(ov)))
			return -1;
		if (!connect(sockfd, ip->ai_addr, ip->ai_addrlen))
			break;
		close(sockfd);
		sockfd = -1;
	}
	freeaddrinfo(iplist);
	return sockfd;
}

static int gl_send_log(void)
{
	int nr, len;
	int fd, sockfd;
	char cmd[256];
	char data[4096];
	struct stat st;
	
	if (!logserver[0])
		goto err;
	snprintf(cmd, sizeof(cmd), "gzip %s", PPPD_LOG_FILE);
	system(cmd);
	fd = open(PPPD_ZIP_FILE, O_RDONLY);
	if (fd < 0 || fstat(fd, &st))
		goto err;
	memset(cmd, 0x0, sizeof(cmd));
	nr = snprintf(cmd, sizeof(cmd),
				"--========7d4a6d158c9\r\n"
				"Content-Disposition: form-data;name=\"file\";filename=\"%s.gz\"\r\n"
				"Content-Type: application/octet-stream\r\n\r\n", callid);
	memset(data, 0x0, sizeof(data));
	len = snprintf(data, 1024,
				"POST %s HTTP/1.1\r\n"
				"User-Agent: wiair\r\n"
				"Accept: */*\r\n"
				"Host: %s\r\n"
				"Connection: Keep-Alive\r\n"
				"Charsert: UTF-8\r\n"
				"Content-Type: multipart/form-data; boundary=========7d4a6d158c9\r\n"
				"Content-Length: %d\r\n\r\n%s",
				logdir, logserver, (int)st.st_size + nr + 27, cmd);
	nr = igd_safe_read(fd, (unsigned char *)data + len, (int)st.st_size);
	if (nr != (int)st.st_size)
		goto err;
	strcpy(data + len + nr, "\r\n--========7d4a6d158c9--\r\n");
	sockfd = gl_logserver_connect(logserver, logport);
	if (sockfd <= 0)
		goto err;
	if (write(sockfd, data, len + nr + 27) < 0)
		GL_DEBUG("send error file error\n");
err:
	if (sockfd > 0)
		close(sockfd);
	if (fd > 0)
		close(fd);
	remove(PPPD_LOG_FILE);
	remove(PPPD_ZIP_FILE);
	return 0;
}

static void gl_register(unsigned char *devmac)
{
	char payload[128];
	char registerid[64];
	char tmpcid[IGD_NAME_LEN * 2];
	struct GETUI_CMD_SEND_P2P_MESSAGE vpn;
	
	snprintf(registerid, sizeof(registerid), "DMSG-P-%lu", (unsigned long)time(NULL));
	snprintf(payload, sizeof(payload),
			"{ \"code\":\"%d\", \"message\":"
			"\"VPN-%lu\", "
			"\"product\":\"%s\", \"mac\":"
			"\"%02X%02X%02X%02X%02X%02X\" }",
			GL_MSG_REGISTER_COD,
			(unsigned long)time(NULL),
			GL_PRODUCT_ID_WIAIR,
			MAC_SPLIT(devmac));
	memset(tmpcid, 0x0, sizeof(tmpcid));
	pthread_mutex_lock(&gl_mutex);
	arr_strcpy(tmpcid, callid);
	pthread_mutex_unlock(&gl_mutex);
	vpn.cid = tmpcid;
	vpn.payload = (BYTE *)payload;
	vpn.timestamp = (unsigned long)time(NULL);
	vpn.taskid = registerid;
	vpn.messageid = registerid;
	vpn.payload_len = strlen((char *)vpn.payload);
	while(1) {
		if (registerd)
			break;
		getui_lib_send_p2p_msg(&vpn);
		sleep(5);
	}
	GL_DEBUG("register success\n");
}

static void gl_init(void)
{
	int uid = 1;
	int flag = 0;
	char idstr[128];
	char vpnidstr[64];
	struct iface_info info;
	struct GETUI_LIB_CONFIG config;
	sigset_t sig;

	sigemptyset(&sig);
	/*sigaddset(&sig, SIGALRM);*/
	sigaddset(&sig, SIGABRT);
	sigaddset(&sig, SIGPIPE);
	sigaddset(&sig, SIGQUIT);
	/*sigaddset(&sig, SIGTERM);*/
	sigaddset(&sig, SIGUSR1);
	sigaddset(&sig, SIGUSR2);
	sigprocmask(SIG_BLOCK, &sig, NULL);
	
	pthread_mutex_init(&gl_mutex, NULL);
	flag = get_sysflag(SYSFLAG_RESET);
	if (flag != 1)
		flag = get_sysflag(SYSFLAG_RECOVER);
	if (flag == 1)
		mtd_set_val("vpnid=0");
	while (1) {
		if (!mu_msg(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &info, sizeof(info)) && info.net)
			break;
		sleep(5);
	}
	memset(&config, 0x0, sizeof(config));
	config.appkey = "sQUdK0h1Zh74AgcVQpwbP8";
	config.appid = "diIXIeqn8R8RHrdT3roKOA";
	config.appsecret = "RGUeup9rvK9TfIXjrrwAr9";
	config.host = "dt.gl.igexin.com";
	config.port = 5224;
	config.log_level = 0;
	snprintf(idstr, sizeof(idstr), "L-%02X%02X%02X%02X%02X%02X-", MAC_SPLIT(info.mac));
	snprintf(idstr + 15, sizeof(idstr) - 15, "%lu", (unsigned long)time(NULL));
	config.registerid = idstr;
	if (!mtd_get_val("vpnid", vpnidstr, sizeof(vpnidstr)))
		cysession.session = atoll(vpnidstr);
	if (getui_lib_init(&config, gl_callback, &cysession)) {
		GL_DEBUG("getui_lib_init error\n");
		return;
	}
	while(1) {
		sleep(5);
		if (havecid > 0 && cysession.session > 0)
			break;
	}
	gl_register(info.mac);
}

void gl_nlcall(struct nlk_msg_handler *nlh)
{	
	char buf[4096] = {0,};
	struct nlk_msg_comm *comm;
	struct nlk_sys_msg *nlk;
	struct vpn_cfg cfg;

	if (nlk_event_recv(nlh, buf, sizeof(buf)) <= 0)
		return;
	comm = (struct nlk_msg_comm *)buf;
	switch (comm->gid) {
	case NLKMSG_GRP_IF:
		if (comm->mid != IF_GRP_MID_IPCGE)
			return;
		nlk = (struct nlk_sys_msg *)buf;
		if (!nlk->msg.ipcp.uid)
			return;
		if (nlk->comm.action != NLK_ACTION_ADD)
			return;
		pthread_mutex_lock(&gl_mutex);
		memcpy(&cfg, &vcfg, sizeof(cfg));
		pthread_mutex_unlock(&gl_mutex);
		gl_vpn_restart(&cfg);
        break;
	case NLKMSG_GRP_LOG:
		if (comm->mid != LOG_GRP_MID_PPTP)
			return;
		gl_send_log();
		break;
	default:
		break;
	}
	return;
}

int main(int argc, char *argv[])
{
	fd_set fds;
	int grp, max_fd;
	struct nlk_msg_handler nlh;

	gl_init();
	grp = 1 << (NLKMSG_GRP_IF - 1);
	grp |= 1 << (NLKMSG_GRP_LOG - 1);
	nlk_event_init(&nlh, grp);
	for (;;) {
		FD_ZERO(&fds);
		IGD_FD_SET(nlh.sock_fd, &fds);
		if (select(max_fd + 1, &fds, NULL, NULL, NULL) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
		}
		if (FD_ISSET(nlh.sock_fd, &fds))
			gl_nlcall(&nlh);
	}
	return 0;
}
