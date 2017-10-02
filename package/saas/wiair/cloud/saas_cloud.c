#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <stdarg.h>
#include "saas_cloud.h"
#include "linux_list.h"

#define CC_LOG_FILE   "/tmp/cc_dbg"
#define CC_ERR_FILE   "/tmp/cc_err"

#ifdef FLASH_4_RAM_32
#define CC_DBG(fmt,args...) do {}while(0)
#define CC_ERR(fmt,args...) do {}while(0)
#else
#define CC_DBG(fmt,args...) do { \
	cloud_log(CC_LOG_FILE, "DBG:[%05d],%s:"fmt, \
		__LINE__, __FUNCTION__, ##args);\
} while(0)
#define CC_ERR(fmt,args...) do { \
	cloud_log(CC_ERR_FILE, "ERR:[%05d],%s:"fmt, \
		__LINE__, __FUNCTION__, ##args);\
} while(0)
#endif

#define CC_CRC  111190
#define CC_SERVER_DNS  "login.wiair.com"
#define CC_SERVER_PORT  8510
#define CC_SERVER_ADDR_NUM  5
#define CC_BUF_MAX  60*1024
#define CC_CGI_MAX 30
#define CC_UPGRADE_FILE   FW_CLOUD_FILE
#define CC_CP_MAX   5

/*CCF: cloud client flag*/
enum CLOUD_CLIENT_FLAG {
	CCF_HARDWARE = 0,
	CCF_RESET,
	CCF_UPGRADE,

	CCF_MAX, //must last
};

struct cc_cp_list {
	pid_t pid;
	unsigned int userid;
};

struct cloud_client_info {
	int sock;
	unsigned int key;
	unsigned int devid;
	unsigned int userid;
	int len;
	unsigned char *data;
	unsigned char cgi_num;
	unsigned char ctime;
	unsigned long rtime;
	pid_t upgrade_pid;
	unsigned long flag[BITS_TO_LONGS(CCF_MAX)];
	struct list_head cgi;
	char *up_md5;
	char *up_url;
	struct cc_cp_list cp[CC_CP_MAX];
};

struct cc_cgi_list {
	struct list_head list;
	int sock;
	long time;
	char *cgi;
	unsigned int userid;
};

static void cloud_log(char *file, const char *fmt, ...)
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
	if (ftell(fp) > 300*1024)
		snprintf(bakfile, sizeof(bakfile), "%s.bak", file);
	fclose(fp);
	if (bakfile[0])
		rename(file, bakfile);
}

static long cc_uptime(void)
{
	struct sysinfo info;

	if (sysinfo(&info))
		return 0;

	return info.uptime;
}

int cc_get_devid(unsigned int *devid)
{
	return 0;
}

int cc_set_devid(unsigned int devid)
{
	return 0;
}

unsigned int cc_get_usrid(void)
{
	return 0;
}

int cc_set_usrid(unsigned int usrid)
{
	return 0;
}

int cc_read_mac(unsigned char *mac)
{

	return 0;
}

static int cc_url2ip(struct in_addr *addr, int nr, const char *url)
{
	struct addrinfo *ailist, *aip;
	struct addrinfo hint;
	struct sockaddr_in *sinp = NULL;
	int i = 0;

	hint.ai_flags = AI_CANONNAME;
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = IPPROTO_TCP;
	hint.ai_addrlen = 0;
	hint.ai_canonname = NULL;
	hint.ai_addr = NULL;
	hint.ai_next = NULL;

	if (getaddrinfo(url, NULL, &hint, &ailist)) {
		CC_DBG("fail\n");
		return 0;
	}

	for (aip = ailist; aip != NULL && i < nr; aip = aip->ai_next) {
		sinp = (struct sockaddr_in *)aip->ai_addr;
		addr[i++] = sinp->sin_addr;
	}
	freeaddrinfo(ailist);
	return i;
}

static int cc_connect(struct in_addr ip, unsigned short port)
{
	int sock;
	struct sockaddr_in addr;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		CC_ERR("fail, %s\n", strerror(errno));
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ip.s_addr; 
	addr.sin_port = htons(port);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0) {
		close(sock);
		CC_DBG("fail, %s\n", strerror(errno));
		return -1;
	}

	CC_DBG("succ,%d,%d\n", sock, port);
	return sock;
}

static int cc_setsock(int sock, int time)
{
	int ov = 0;
	socklen_t ovlen = sizeof(ov);
	struct timeval timeout;

	ov = 65535;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void *)&ov, ovlen))
		CC_ERR("rcvbuf fail, %s\n", strerror(errno));
	ov = 65535;
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void *)&ov, ovlen))
		CC_ERR("sndbuf fail, %s\n", strerror(errno));
	ov = 1;
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *)&ov, ovlen))
		CC_ERR("nodelay fail, %s\n", strerror(errno));

	timeout.tv_sec = time;
	timeout.tv_usec = 0;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
		CC_ERR("rcvtimeo fail, %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

static void cc_setkeep(int sock)
{
	int ov = 0;
	socklen_t ovlen = sizeof(ov);

	ov = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,(void *)&ov, ovlen))	  
		CC_ERR("keepalive fail, %s\n", strerror(errno));
	ov = 30;
	if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&ov, ovlen))
		CC_ERR("keepidle fail, %s\n", strerror(errno));
	ov = 50;
	if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&ov, ovlen))
		CC_ERR("keepintvl fail, %s\n", strerror(errno));
	ov = 2;
	if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, (void *)&ov, ovlen))
		CC_ERR("keepcnt fail, %s\n", strerror(errno));
}

static int cc_recv_len(
	int sock, unsigned char *buf, int len)
{
	int r, l = 0;

	while (l < len) {
		r = recv(sock, buf + l, len - l, 0);
		if (r <= 0) {
			CC_DBG("%s\n", strerror(errno));
			break;
		}
		l += r;
	}
	return l;
}

static int cc_recv(
	int sock, unsigned char **buf, int *len)
{
	unsigned char d[4], *t;
	int l = sizeof(d);
	unsigned short order;

	l = cc_recv_len(sock, d, l);
	if (l != sizeof(d)) {
		CC_DBG("head,%d\n", l);
		return -1;
	}

	l = CC_PULL2(d, 0);
	if (l < 4 || l > CC_BUF_MAX) {
		CC_ERR("len,%d\n", l);
		return -1;
	}

	t = malloc(l + 1);
	if (!t) {
		CC_ERR("malloc,%d\n", l);
		return -1;
	}

	order = CC_PULL2(d, 2);
	if (order != CSO_ACK_CONNECT_ONLINE)
		CC_DBG("%d,%d\n", order, l);

	t[l] = 0;
	l -= 4;

	if (cc_recv_len(sock, t + 4, l) != l) {
		free(t);
		CC_ERR("info,%d\n", l);
		return -1;
	}
	*buf = t;
	*len = l + 4;
	memcpy(t, d, sizeof(d));
	return 0;
}

static int cc_send(
	int sock, unsigned char *buf, int len)
{
	int r, l = 0;
	unsigned short order = CC_PULL2(buf, 2);

	if (order != CSO_REQ_CONNECT_ONLINE)
		CC_DBG("%d,%d\n", order, len);

	while (l < len) {
		r =  send(sock, buf + l, len - l, 0);
		if (r <= 0) {
			CC_DBG("%s\n", strerror(errno));
			return -1;
		}
		l += r;
	}
	return 0;
}

static int cc_cpuinfo(char *data, int data_len)
{
	FILE *fp = NULL;
	char buf[256] = {0,}, *ptr = NULL;
	unsigned short len = 0;

	fp = fopen("/proc/cpuinfo", "r");
	if (!fp)
		return 0;

	memset(data, 0, data_len);
	memset(buf, 0, sizeof(buf));
	while (fgets(buf, sizeof(buf) - 1, fp)) {
		if ((!memcmp(buf, "system type", 11)) || \
			  (!memcmp(buf, "cpu model", 9))) {
			ptr = strchr(buf, ':');
			ptr = ptr ? ptr + 2 : NULL;
			while (ptr) {
				if ((len >= data_len) || (*ptr == 0))
					break;
				if ((*ptr == '\n') || (*ptr == '\r'))
					*ptr = ' ';
				data[len] = *ptr;
				len++;
				ptr++;
			}
		}
		memset(buf, 0, sizeof(buf));
	}
	fclose(fp);
	return len;
}

static int cc_meminfo(char *data, int data_len)
{
	int i = 0;
	long totalram = 0;
	FILE *fp;
	char buf[1024];

	fp = fopen("/proc/meminfo", "rb");
	if (!fp)
		return 0;
	while (1) {
		memset(buf, 0, sizeof(buf));
		if (!fgets(buf, sizeof(buf) - 1, fp))
			break;
		if (memcmp(buf, "MemTotal", 8))
			continue;
		totalram = atoll(buf + 9);
		break;
	}
	fclose(fp);
	if (totalram == 0)
		return 0;
	while (totalram >> i)
		i++;
	if (i >= 10) {
		i = 1 << (i - 10);
	} else {
		return 0;
	}
	memset(data, 0, data_len);
	snprintf(data, data_len - 1, "%dM", i);
	return strlen(data);
}

static void cc_del_cgi(struct cc_cgi_list *ccl)
{
	list_del(&ccl->list);
	close(ccl->sock);
	if (ccl->cgi)
		free(ccl->cgi);
	free(ccl);
}

static void cc_free_cgi(struct cloud_client_info *cci)
{
	struct cc_cgi_list *ccl, *_ccl;

	list_for_each_entry_safe(ccl, _ccl, &cci->cgi, list) {
		cc_del_cgi(ccl);
		cci->cgi_num--;
	}
}

static int cc_add_cgi(
	struct cloud_client_info *cci, int sock, char *cgi)
{
	struct cc_cgi_list *ccl;

	ccl = malloc(sizeof(*ccl));
	if (!ccl) {
		CC_ERR("malloc fail, %d\n", sizeof(*ccl));
		return -1;
	}
	ccl->sock = sock;
	ccl->time = cc_uptime();
	ccl->cgi = strdup(cgi);
	ccl->userid = CC_PULL4(cci->data, 8);
	list_add(&ccl->list, &cci->cgi);
	cci->cgi_num++;
	return 0;
}

static int cc_transfer_ack(
	struct cloud_client_info *cci,
	unsigned int userid, int sock, char *msg, int msg_len)
{
	unsigned char *buf = NULL;
	int len = msg_len + 14;

	buf = malloc(len);
	if (!buf) {
		CC_ERR("malloc fail, %d\n", len);
		goto err;
	}

	CC_PUSH2(buf, 0, len);
	CC_PUSH2(buf, 2, CSO_NTF_ROUTER_TRANSFER);
	CC_PUSH4(buf, 4, cci->key);
	CC_PUSH4(buf, 8, userid);
	CC_PUSH2(buf, 12, msg_len);
	CC_PUSH_LEN(buf, 14, msg, msg_len);

	CC_DBG("%d,%d\n", msg_len, sock);
	if (cc_send(cci->sock, buf, len))
		goto err;

	free(buf);
	return 0;
err:
	if (buf)
		free(buf);
	return -1;
}

static int cc_send_cgi(
	struct cloud_client_info *cci, int len)
{
	int sock = -1, l;
	struct in_addr ip;
	char *buf = NULL, *cgi = (char *)&cci->data[14];

	CC_DBG("%d,[%s]\n", len, cgi);

	inet_aton("127.0.0.1", &ip);
	sock = cc_connect(ip, 80);
	if (sock < 0)
		goto err;

	buf = malloc(len + 200);
	if (!buf) {
		CC_ERR("malloc fail, %d\n", len);
		goto err;
	}

	cc_setsock(sock, 3);

	l = snprintf(buf, len + 200, 
		"%s HTTP/1.1\r\n"
		"Host: 127.0.0.1\r\n"
		"Accept-Encoding: gzip, deflate\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n\r\n", cgi);

	if (cc_send(sock, (unsigned char *)buf, l) < 0) {
		CC_ERR("send fail, %d\n", l);
		goto err;
	}

	if (cc_add_cgi(cci, sock, cgi))
		goto err;

	free(buf);
	return 0;
err:
	if (sock > 0)
		close(sock);
	if (buf)
		free(buf);
	return -1;
}

static int cc_recv_cgi(
	struct cloud_client_info *cci, struct cc_cgi_list *ccl)
{
	char *buf = NULL, *ptr, *msg = NULL;
	int ret, len = 0, head_len = 0, msg_len = 0;

	buf = malloc(CC_BUF_MAX);
	if (!buf) {
		CC_ERR("malloc fail, %d\n", CC_BUF_MAX);
		goto err;
	}

	memset(buf, 0, sizeof(CC_BUF_MAX));
	while (1) {
		ret = recv(ccl->sock, buf + len, CC_BUF_MAX - len - 1, 0);
		if (ret <= 0)
			break;
		len += ret;
		if (!msg) {
			msg = strstr(buf, "\r\n\r\n");
			if (!msg)
				continue;
			msg += 4;
			head_len = (int)(msg - buf);
			ptr = strstr(buf, "Content-Length:");
			if (!ptr) {
				CC_ERR("head err\n");
				goto err;
			}
			ptr += strlen("Content-Length:");
			msg_len = atoi(ptr);
			if (msg_len <= 0) {
				CC_ERR("msg_len:%d\n", msg_len);
				goto err;
			}
			if ((msg_len + head_len) <= len)
				break;
		}
	}
	if ((msg_len <= 0) || 
		((msg_len + head_len) > len)) {
		CC_ERR("%d,%d,%d\n", msg_len, len, head_len);
		goto err;
	}

	if (cc_transfer_ack(cci, 
		ccl->userid, ccl->sock, msg, msg_len)) {
		goto err;
	}

	free(buf);
	return 0;
err:
	if (buf)
		free(buf);
	return -1;
}

static int cc_cgi_fds(
	struct cloud_client_info *cci, int max_fd, fd_set *fds)
{
	struct cc_cgi_list *ccl;

	list_for_each_entry(ccl, &cci->cgi, list) {
		FD_SET(ccl->sock, fds);
		max_fd = N_MAX(max_fd, ccl->sock);
	}
	return max_fd;
}

static void cc_check_cgi(
	struct cloud_client_info *cci, fd_set *fds)
{
	struct cc_cgi_list *ccl, *_ccl;
	char msg[512];

	list_for_each_entry_safe(ccl, _ccl, &cci->cgi, list) {
		if ((ccl->time + 3*60) < cc_uptime()) {
			//timeout
			snprintf(msg, sizeof(msg),
				"{\"errno\":\"104\",\"cmd\":\"%s\"}", ccl->cgi);
			cc_transfer_ack(cci, ccl->userid, -4, msg, strlen(msg));
		} else if (FD_ISSET(ccl->sock, fds)) {
			if (cc_recv_cgi(cci, ccl)) {
				//recv err
				snprintf(msg, sizeof(msg),
					"{\"errno\":\"102\",\"cmd\":\"%s\"}", ccl->cgi);
				cc_transfer_ack(cci, ccl->userid, -2, msg, strlen(msg));
			}
		} else {
			continue;
		}
		cc_del_cgi(ccl);
		cci->cgi_num--;
	}
}

static void cc_timeout(struct cloud_client_info *cci)
{
	long t = cc_uptime();
	unsigned char buf[8];

	if (cci->ctime > 3) {
		close(cci->sock);
		CC_DBG("relink\n");
		return;
	} else if ((cci->rtime
		+ (cci->ctime + 1)*90) > t) {
		return;
	}

	cci->ctime++;
	CC_PUSH2(buf, 0, 8);
	CC_PUSH2(buf, 2, CSO_REQ_CONNECT_ONLINE);
	CC_PUSH4(buf, 4, cci->key);
	cc_send(cci->sock, buf, 8);
}

#define CC_BASE_FUNCITON "----------------------------------"

static int cc_xx(
	int msg, struct cloud_client_info *cci)
{
	if (msg != -1)
		CC_DBG("%d\n", msg);

	if (cci->data) {
		free(cci->data);
		cci->data = NULL;
		cci->len = 0;
	}
	return CSO_WAIT;
}

static int cc_wait(struct cloud_client_info *cci)
{
	if (cci->data) {
		free(cci->data);
		cci->data = NULL;
	}

	if (cc_recv(cci->sock, &cci->data, &cci->len) < 0)
		return CSO_LINK;

	return CC_PULL2(cci->data, 2);
}

static int cc_transfer(struct cloud_client_info *cci)
{
	unsigned int userid;
	unsigned short msg_len;
	char msg[512], *cgi;

	if (cci->len < 14) {
		CC_ERR("%d\n", cci->len);
		goto err;
	}
	msg_len = CC_PULL2(cci->data, 12);
	if ((msg_len + 14) != cci->len) {
		CC_ERR("%d,%d\n", cci->len, msg_len);
		goto err;
	}

	cgi = (char *)&cci->data[14];
	userid = CC_PULL4(cci->data, 8);

	if (cci->cgi_num > CC_CGI_MAX) {
		//too fast
		snprintf(msg, sizeof(msg),
			"{\"errno\":\"103\",\"cmd\":\"%s\"}", cgi);
		cc_transfer_ack(cci, userid, -3, msg, strlen(msg));
		goto err;
	}

	if (cc_send_cgi(cci, msg_len)) {
		//send err
		snprintf(msg, sizeof(msg),
			"{\"errno\":\"101\",\"cmd\":\"%s\"}", cgi);
		cc_transfer_ack(cci, userid, -1, msg, strlen(msg));
		goto err;
	}
err:
	return CSO_XX;
}

static int cc_online_req(struct cloud_client_info *cci)
{
	unsigned char buf[8];

	CC_PUSH2(buf, 0, 4);
	CC_PUSH2(buf, 2, CSO_ACK_CONNECT_ONLINE);
	if (cc_send(cci->sock, buf, 4) < 0)
		return CSO_LINK;
	return CSO_XX;
}

static int cc_online_ack(struct cloud_client_info *cci)
{
	if (cci->len < 8) {
		CC_ERR("%d\n", cci->len);
		return CSO_LINK;
	}
	return CSO_XX;
}

static int cc_link(struct cloud_client_info *cci)
{
	int nr, i, sock = 0;
	struct in_addr addr[CC_SERVER_ADDR_NUM];

	if (cci->sock > 0) {
		close(cci->sock);
		cci->sock = -1;
	}
	cc_xx(0, cci);
	cc_free_cgi(cci);

	nr = cc_url2ip(addr, CC_SERVER_ADDR_NUM, CC_SERVER_DNS);
	if (nr <= 0)
		goto err;

	for (i = 0; i < nr; i++) {
		sock = cc_connect(addr[i], CC_SERVER_PORT);
		if (sock < 0) {
			sleep(10);
			continue;
		}
		if (cc_setsock(sock, 60))
			goto err;
		cci->sock = sock;
		return CSO_NTF_KEY;
	}

err:
	if (sock > 0)
		close(sock);
	sleep(30);
	return CSO_LINK;
}

static int cc_key(struct cloud_client_info *cci)
{
	int len = 0;
	unsigned char *buf = NULL;

	if (cc_recv(cci->sock, &buf, &len) < 0)
		goto err;
	if ((len < 8) || (CC_PULL2(buf, 2) != CSO_NTF_KEY)) {
		CC_ERR("%d,%d\n", len, CC_PULL2(buf, 2));
		goto err;
	}

	cci->key = CC_PULL4(buf, 4)^CC_CRC;
	free(buf);
	if (cci->devid)
		return CSO_REQ_ROUTER_LOGIN;
	return CSO_REQ_ROUTER_FIRST;
err:
	if (buf)
		free(buf);
	sleep(30);
	return CSO_LINK;
}

static int cc_register(struct cloud_client_info *cci)
{
	unsigned char buf[32], mac[6];

	CC_PUSH2(buf, 0, 21);
	CC_PUSH2(buf, 2, CSO_REQ_ROUTER_FIRST);
	CC_PUSH4(buf, 4, cci->key);
	CC_PUSH1(buf, 8, 12); //mac len

	if (cc_read_mac(mac)) {
		CC_ERR("get mac err\n");
		sleep(120);
		return CSO_LINK;
	}
	snprintf((char *)&buf[9], 13, NF2_MAC, MAC_SPLIT(mac));
	if (cc_send(cci->sock, buf, 21))
		return CSO_LINK;
	return CSO_ACK_ROUTER_FIRST;
}

static int cc_register_ack(struct cloud_client_info *cci)
{
	int len = 0;
	unsigned char *buf = NULL;

	if (cc_recv(cci->sock, &buf, &len) < 0)
		goto err;
	if ((len < 11) || 
		(CC_PULL2(buf, 2) != CSO_ACK_ROUTER_FIRST)) {
		CC_ERR("%d,%d\n", len, CC_PULL2(buf, 2));
		goto err;
	}
	if (CC_PULL2(buf, 4)) {
		CC_ERR("%d\n", CC_PULL2(buf, 4));
		goto err;
	}
	cci->devid = CC_PULL4(buf, 6);
	if (!cci->devid) {
		CC_ERR("%u\n", cci->devid);
		goto err;
	}
	if (CC_PULL1(buf, 10)) {
		igd_set_bit(CCF_HARDWARE, cci->flag);
		CC_DBG("hardware\n");
	}

	CC_DBG("%d\n", cci->devid);
	cc_set_devid(cci->devid);

	free(buf);
	return CSO_NTF_ROUTER_REDIRECT;
err:
	if (buf)
		free(buf);
	sleep(30);
	return CSO_LINK;
}

static int cc_login(struct cloud_client_info *cci)
{
	unsigned char buf[32];

	CC_PUSH2(buf, 0, 12);
	CC_PUSH2(buf, 2, CSO_REQ_ROUTER_LOGIN);
	CC_PUSH4(buf, 4, cci->key);
	CC_PUSH4(buf, 8, cci->devid);

	if (cc_send(cci->sock, buf, 12))
		return CSO_LINK;
	return CSO_ACK_ROUTER_LOGIN;
}

static int cc_login_ack(struct cloud_client_info *cci)
{
	int len = 0;
	unsigned char *buf = NULL;
	struct in_addr ip;

	if (cc_recv(cci->sock, &buf, &len) < 0)
		goto err;
	if ((len < 10) || 
		(CC_PULL2(buf, 2) != CSO_ACK_ROUTER_LOGIN)) {
		CC_ERR("%d,%d\n", len, CC_PULL2(buf, 2));
		goto err;
	}

	if (CC_PULL2(buf, 4)) {
		CC_ERR("%d\n", CC_PULL2(buf, 4));
		cci->devid = 0;
		goto err;
	}
	ip.s_addr = *(unsigned int *)&buf[6];
	CC_DBG("%s\n", inet_ntoa(ip));

	free(buf);
	return CSO_NTF_ROUTER_REDIRECT;
err:
	if (buf)
		free(buf);
	sleep(30);
	return CSO_LINK;
}

static int cc_redirect(struct cloud_client_info *cci)
{
	int len = 0, i;
	unsigned char *buf = NULL;
	struct in_addr addr;
	unsigned short port;

	if (cc_recv(cci->sock, &buf, &len) < 0)
		goto err;
	if ((len < 14) || 
		(CC_PULL2(buf, 2) != CSO_NTF_ROUTER_REDIRECT)) {
		CC_ERR("%d,%d\n", len, CC_PULL2(buf, 2));
		goto err;
	}
	close(cci->sock);

	addr.s_addr = htonl(CC_PULL4(buf, 4));
	port = CC_PULL2(buf, 8);
	cci->key = CC_PULL4(buf, 10);
	free(buf);
	buf = NULL;

	i = 3;
	while (i > 0) {
		cci->sock = cc_connect(addr, port);
		if (cci->sock < 0) {
			i--;
			sleep(10);
			continue;
		}
		if (cc_setsock(cci->sock, 10))
			goto err;
		cc_setkeep(cci->sock);
		return CSO_REQ_ROUTER_KEEP;
	}
err:
	if (buf)
		free(buf);
	if (cci->sock > 0)
		close(cci->sock);
	sleep(30);
	return CSO_LINK;
}

static int cc_keep(struct cloud_client_info *cci)
{
	int i = 0, l;
	char *ptr, tmp[64];
	unsigned char buf[128] = {0,};

	CC_PUSH2(buf, 2, CSO_REQ_ROUTER_KEEP);
	CC_PUSH4(buf, 4, cci->devid);
	CC_PUSH4(buf, 8, cci->userid);
	CC_PUSH4(buf, 12, cci->key);
	i = 16;
	ptr = "x.xx.xx";//read_firmware("CURVER");
	l = ptr ? strlen(ptr) : 0;
	CC_PUSH1(buf, i, l);
	i += 1;
	if (l > 0) {
		CC_DBG("%s\n", ptr);
		CC_PUSH_LEN(buf, i, ptr, l);
		i += l;
	}

	ptr = "TTTTT";//read_firmware("VENDOR");
	l = snprintf(tmp, sizeof(tmp), "%s_", ptr ? ptr : "ERR");
	ptr = "FFFFF";//read_firmware("PRODUCT");
	l += snprintf(tmp + l, sizeof(tmp) - l,	"%s", ptr ? ptr : "ERR");
	CC_PUSH1(buf, i, l);
	i += 1;
	CC_PUSH_LEN(buf, i, tmp, l);
	i += l;
	CC_DBG("%s\n", tmp);

	l = snprintf(tmp, sizeof(tmp), "5000"); //CC_MSG_RCONF(RCONF_FLAG_VER, tmp, sizeof(tmp));
	CC_DBG("%d,%s\n", l, tmp);
	l = (l < 0) ? 0 : l;
	CC_PUSH1(buf, i, l);
	i += 1;
	if (l > 0) {
		CC_PUSH_LEN(buf, i, tmp, l);
		i += l;
	}

	CC_PUSH1(buf, i, 0);
	i += 1;

	CC_PUSH2(buf, 0, i);
	if (cc_send(cci->sock, buf, i))
		return CSO_LINK;
	return CSO_ACK_ROUTER_KEEP;
}

static int cc_keep_ack(struct cloud_client_info *cci)
{
	int len = 0;
	unsigned char *buf = NULL;

	if (cc_recv(cci->sock, &buf, &len) < 0)
		goto err;
	if ((len < 18) || 
		(CC_PULL2(buf, 2) != CSO_ACK_ROUTER_KEEP)) {
		CC_ERR("%d,%d\n", len, CC_PULL2(buf, 2));
		goto err;
	}
	if (CC_PULL2(buf, 4)) {
		CC_ERR("%d\n", CC_PULL2(buf, 4));
		goto err;
	}

	if (igd_test_bit(CCF_RESET, cci->flag)) {
		;//set_sysflag(SYSFLAG_RESET, 0);
		;//set_sysflag(SYSFLAG_RECOVER, 0);
		igd_clear_bit(CCF_RESET, cci->flag);
	}

	cci->key = CC_PULL4(buf, 6);
	cci->userid = CC_PULL4(buf, 10);
	cc_set_usrid(cci->userid);

	free(buf);
	if (igd_test_bit(CCF_HARDWARE, cci->flag))
		return CSO_NTF_ROUTER_HARDWARE;
	return CSO_WAIT;
err:
	if (buf)
		free(buf);
	sleep(30);
	return CSO_LINK;
}

static int cc_hardware_info(struct cloud_client_info *cci)
{
	int i = 0, l;
	char tmp[64];
	unsigned char buf[128] = {0,};

	CC_PUSH2(buf, 2, CSO_NTF_ROUTER_HARDWARE);
	CC_PUSH4(buf, 4, cci->key);
	i = 8;

	l = cc_cpuinfo(tmp, sizeof(tmp));
	CC_DBG("%s\n", tmp);

	CC_PUSH1(buf, i, l);
	i += 1;
	if (l > 0) {
		CC_PUSH_LEN(buf, i, tmp, l);
		i += l;
	}

	l = cc_meminfo(tmp, sizeof(tmp));
	CC_DBG("%s\n", tmp);

	CC_PUSH1(buf, i, l);
	i += 1;
	if (l > 0) {
		CC_PUSH_LEN(buf, i, tmp, l);
		i += l;
	}

	CC_PUSH2(buf, 0, i);
	if (cc_send(cci->sock, buf, i) < 0)
		return CSO_LINK;
	return CSO_WAIT;
}

static int cc_bind(struct cloud_client_info *cci)
{
	if (cci->len < 8) {
		CC_ERR("%d\n", cci->len);
		return CSO_LINK;
	}
	cci->userid = CC_PULL4(cci->data, 4);
	CC_DBG("%d\n", cci->userid);
	cc_set_usrid(cci->userid);
	return CSO_XX;
}

static int cc_deal_msg(
	int msg, struct cloud_client_info *cci)
{
	switch (msg) {
	case CSO_WAIT:
		return cc_wait(cci);
	case CSO_NTF_ROUTER_TRANSFER:
		return cc_transfer(cci);
	case CSO_REQ_CONNECT_ONLINE:
		return cc_online_req(cci);
	case CSO_ACK_CONNECT_ONLINE:
		return cc_online_ack(cci);
	case CSO_LINK:
		return cc_link(cci);
	case CSO_NTF_KEY:
		return cc_key(cci);
	case CSO_REQ_ROUTER_FIRST:
		return cc_register(cci);
	case CSO_ACK_ROUTER_FIRST:
		return cc_register_ack(cci);
	case CSO_REQ_ROUTER_LOGIN:
		return cc_login(cci);
	case CSO_ACK_ROUTER_LOGIN:
		return cc_login_ack(cci);
	case CSO_NTF_ROUTER_REDIRECT:
		return cc_redirect(cci);
	case CSO_REQ_ROUTER_KEEP:
		return cc_keep(cci);
	case CSO_ACK_ROUTER_KEEP:
		return cc_keep_ack(cci);
	case CSO_NTF_ROUTER_HARDWARE:
		return cc_hardware_info(cci);
	case CSO_NTF_ROUTER_BIND:
		return cc_bind(cci);
	default:
		return cc_xx(msg, cci);
	}
	return CSO_WAIT;
}

static void cc_mesg(
	int msg, struct cloud_client_info *cci)
{
	while (1) {
		msg = cc_deal_msg(msg, cci);
		if (msg == CSO_WAIT)
			break;
	}
	cci->ctime = 0;
	cci->rtime = cc_uptime();
}

static void cc_child(struct cloud_client_info *cci)
{
	pid_t pid;
	int status;

	pid = waitpid(-1, &status, WNOHANG);
	if (pid <= 0)
		return;

	if (cci->upgrade_pid == pid) {
		CC_DBG("upgrade exit, %d\n", pid);
		cci->upgrade_pid = -1;
	}
}

static void cc_exit(int signo)
{
	CC_ERR("%d\n", signo);
	exit(0);
}

static void cc_sig(void)
{
	sigset_t sig;

	sigemptyset(&sig);
	sigaddset(&sig, SIGABRT);
	sigaddset(&sig, SIGPIPE);
	sigaddset(&sig, SIGQUIT);
	sigaddset(&sig, SIGUSR1);
	sigaddset(&sig, SIGUSR2);
	sigaddset(&sig, SIGHUP);
	sigprocmask(SIG_BLOCK, &sig, NULL);

	signal(SIGBUS, cc_exit);
	signal(SIGINT, cc_exit);
	signal(SIGTERM, cc_exit);
	signal(SIGFPE, cc_exit);
	signal(SIGSEGV, cc_exit);
	signal(SIGKILL, cc_exit);
}

static void cc_init(struct cloud_client_info *cci)
{
	memset(cci, 0, sizeof(*cci));
	INIT_LIST_HEAD(&cci->cgi);

	while(1) {
		if (!cc_get_devid(&cci->devid))
			break;
		CC_ERR("read devid fail\n");
		sleep(6);
	}

	cci->userid = cc_get_usrid();

	CC_DBG("%u,%u\n", cci->devid, cci->userid);
}

int main(int argc, char *argv[])
{
	fd_set fds;
	int max_fd = 0;
	struct timeval tv;
	struct cloud_client_info cci;
	int cnt = 0;

	cc_sig();
	cc_init(&cci);
	cc_mesg(CSO_LINK, &cci);

	while (1) {
		max_fd = 0;
		FD_ZERO(&fds);
		FD_SET(cci.sock, &fds);
		max_fd = N_MAX(max_fd, cci.sock);
		max_fd = cc_cgi_fds(&cci, max_fd, &fds);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		if (select(max_fd + 1, &fds, NULL, NULL, &tv) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
		}

		if (FD_ISSET(cci.sock, &fds))
			cc_mesg(CSO_WAIT, &cci);

		cc_check_cgi(&cci, &fds);
		cc_timeout(&cci);
		cc_child(&cci);
		cnt++;
	}
	return 0;
}
