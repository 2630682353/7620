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
#include "linux_list.h"
#include "libcom.h"
#include "cloud_client.h"

#define CC_CRC  111190
#define CC_SERVER_DNS  "login.wiair.com"
#define CC_SERVER_PORT  8510
#define CC_SERVER_ADDR_NUM  5
#define CC_BUF_MAX  60*1024

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

int cc_send(
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

static void cc_timeout(struct cloud_client_info *cci)
{
	long t = sys_uptime();
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

static int cc_pull_data(
	struct cloud_client_info *cci, int i, void *data, int len)
{
	int l;

	if (i >= cci->len)
		return -1;

	l = CC_PULL1(cci->data, i);
	i += 1;
	if (!l || (l >= len)
		|| ((l + i) > cci->len)) {
		return -1;
	}
	CC_PULL_LEN(cci->data, i, data, l);
	return l + 1;
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

	if (read_mac(mac)) {
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
	set_devid(cci->devid);

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
	ptr = read_firmware("CURVER");
	l = ptr ? strlen(ptr) : 0;
	CC_PUSH1(buf, i, l);
	i += 1;
	if (l > 0) {
		CC_DBG("%s\n", ptr);
		CC_PUSH_LEN(buf, i, ptr, l);
		i += l;
	}

	ptr = read_firmware("VENDOR");
	l = snprintf(tmp, sizeof(tmp), "%s_", ptr ? ptr : "ERR");
	ptr = read_firmware("PRODUCT");
	l += snprintf(tmp + l, sizeof(tmp) - l,	"%s", ptr ? ptr : "ERR");
	CC_PUSH1(buf, i, l);
	i += 1;
	CC_PUSH_LEN(buf, i, tmp, l);
	i += l;
	CC_DBG("%s\n", tmp);

	strcpy(tmp, "0");
	l = strlen(tmp);
	CC_DBG("%d,%s\n", l, tmp);
	l = (l < 0) ? 0 : l;
	CC_PUSH1(buf, i, l);
	i += 1;
	if (l > 0) {
		CC_PUSH_LEN(buf, i, tmp, l);
		i += l;
	}

	l = 0;
	CC_PUSH1(buf, i, l);
	i += 1;
	if (l > 0) {
		//CC_PUSH_LEN(buf, i, wan_info.pppoe.user, l);
		//i += l;
		//CC_DBG("%d,%s\n", l, wan_info.pppoe.user);
	}

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
		igd_clear_bit(CCF_RESET, cci->flag);
	}

	cci->key = CC_PULL4(buf, 6);
	cci->userid = CC_PULL4(buf, 10);

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

	l = get_cpuinfo(tmp, sizeof(tmp));
	CC_DBG("%s\n", tmp);

	CC_PUSH1(buf, i, l);
	i += 1;
	if (l > 0) {
		CC_PUSH_LEN(buf, i, tmp, l);
		i += l;
	}

	l = get_meminfo(tmp, sizeof(tmp));
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
	return CSO_XX;
}

extern unsigned char plug_request_flag;
static int cc_plug_info(struct cloud_client_info *cci)
{
	int i, l, act;
	struct plug_cache_info pci;

	plug_request_flag = 0;
	if (cci->len < 5) {
		CC_ERR("%d\n", cci->len);
		return CSO_XX;
	}

	memset(&pci, 0, sizeof(pci));
	act = (int)CC_PULL1(cci->data, 4);
	i = 5;
	CC_DBG("act: %d\n", act);
	if (act == 2) { // close all plug
		cc_plug_stop(NULL);
		return CSO_XX;
	}

	l = cc_pull_data(cci, i, pci.plug_name, sizeof(pci.plug_name));
	if (l < 0) {
		CC_ERR("plug name err\n");
		return CSO_XX;
	}
	i += l;
	CC_DBG("plug_name: %s\n", pci.plug_name);

	if (act == 0) { // close plug one
		cc_plug_stop(&pci);
		return CSO_XX;
	} else if (act != 1) {
		CC_ERR("act err, %d\n", act);
		return CSO_XX;
	}

	l = cc_pull_data(cci, i, pci.url, sizeof(pci.url));
	if (l < 0) {
		CC_ERR("plug url err\n");
		return CSO_XX;
	}

	i += l;
	CC_DBG("url: %s\n", pci.url);

	l = cc_pull_data(cci, i, pci.md5, sizeof(pci.md5));
	if (l < 0) {
		CC_ERR("plug md5 err\n");
		return CSO_XX;
	}

	i += l;
	CC_DBG("md5: %s\n", pci.md5);

	cc_plug_start(&pci);
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
	case CSO_NTF_PLUGIN_INFO:
		return cc_plug_info(cci);
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
	cci->rtime = sys_uptime();
}

static void cc_child(struct cloud_client_info *cci)
{
	pid_t pid;
	int status;

	pid = waitpid(-1, &status, WNOHANG);
	if (pid <= 0)
		return;
	cc_plug_wait(pid, status, cci);
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

	while(1) {
		if (!get_devid(&cci->devid))
			break;
		CC_ERR("read devid fail\n");
		sleep(6);
	}
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

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		if (select(max_fd + 1, &fds, NULL, NULL, &tv) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
		}

		if (FD_ISSET(cci.sock, &fds))
			cc_mesg(CSO_WAIT, &cci);

		cc_timeout(&cci);
		cc_child(&cci);
		cc_plug_loop(&cci);
		cnt++;
	}
	return 0;
}
