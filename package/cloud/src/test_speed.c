#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include<sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/timeb.h>
#include <unistd.h>
#include "igd_share.h"
#include "nlk_ipc.h"
#include "igd_lib.h"
#include "igd_qos.h"

static char *tspeed_result_file = NULL;

#define TSPEED_DBG(fmt,args...)  printf(fmt, ##args);
#define TSPEED_FILE  tspeed_result_file
#define TSPEED_TIME  128

static void ts_log(const char *fmt, ...)
{
	int fd;
	FILE *fp;
	va_list ap;

	fp = fopen(TSPEED_FILE, "a+");
	if (fp == NULL)
		return;

	fd = fileno(fp);
	if (!flock(fd, LOCK_EX)) {
		va_start(ap, fmt);
		vfprintf(fp, fmt, ap);
		va_end(ap);
	}
	flock(fd, LOCK_UN);
	fclose(fp);
}

static void tspeed_get(char *file, char *flag, int (*fun)(void *, int))
{
	FILE *fp = NULL;
	char buf[2048];
	int fd = 0, i = 0, len = strlen(flag);
	char *p = NULL;

	fp = fopen(file, "r");
	if (fp == NULL)
		return;
	fd = fileno(fp);
	if (!flock(fd, LOCK_EX)) {
		while (1) {
			memset(buf, 0, sizeof(buf));
			if (!fgets(buf, sizeof(buf) - 1, fp))
				break;
			if (len && !memcmp(buf, flag, len)) {
				if ((buf[len] != '=') && (buf[len] != ':'))
					continue;
				p = buf + strlen(buf) - 1;
				while ((p > buf) && ((*p == '\r') || 
					(*p == '\n') || (*p == ' ') || (*p == '\0'))) {
					*p = '\0';
					p--;
				}
				if (fun(buf + len + 1, i)) {
					exit(0);
				}
			}
			i++;
		}
	}
	flock(fd, LOCK_UN);
	fclose(fp);
}

static int ts_kill(void *data, int delay_time)
{
	pid_t pid = 0;

	pid = atoi((char *)data);
	TSPEED_DBG("kill pid:%d\n", pid);
	if ((pid > 0) && (pid != getpid())) {
		kill(pid, SIGTERM);
	}
	return 0;
}

int ts_url2ip(struct in_addr *addr, int nr, const char *url)
{
	struct addrinfo *ailist, *aip;
	struct addrinfo hint;
	struct sockaddr_in *sinp = NULL;
	int err, i = 0;

	hint.ai_flags = AI_CANONNAME;
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = IPPROTO_TCP;
	hint.ai_addrlen = 0;
	hint.ai_canonname = NULL;
	hint.ai_addr = NULL;
	hint.ai_next = NULL;

	if ((err = getaddrinfo(url, NULL, &hint, &ailist)) != 0) {
		TSPEED_DBG("getaddrinfo error: %s\n", gai_strerror(err));
		return 0;
	}

	for (aip = ailist; aip != NULL && i < nr; aip = aip->ai_next) {
		sinp = (struct sockaddr_in *)aip->ai_addr;
		addr[i++] = sinp->sin_addr;
	}
	freeaddrinfo(ailist);
	return i;
}

#define TS_FORK() do {\
	pid_t pid = 0;\
	pid = fork();\
	if (pid < 0) {\
		ts_log(TSF_ERR"=fork\n");\
		return 0;\
	} else if (pid > 0) {\
		ts_log(TSF_PID"=%d\n", pid);\
		return 0;\
	}\
	signal(SIGTERM, SIG_DFL);\
	signal(SIGKILL, SIG_DFL);\
	alarm(60);\
}while(0)

static int ts_ping(void *url, int delay_time)
{
	int i = 0, sock;
	struct in_addr ip;
	struct timeb tb, newtb;
	struct sockaddr_in addr;

	TSPEED_DBG("ping:%s\n", (char *)url);
	TS_FORK();
	usleep(delay_time*10000);

	if (ts_url2ip(&ip, 1, url) <= 0) {
		ts_log(TSF_PING"=%d,%s\n", 0, url);
		return -1;
	}
	for (i = 0; i < 3; i++) {
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
			continue;
		addr.sin_family = AF_INET;
		addr.sin_addr = ip; 
		addr.sin_port = htons(80);
		ftime(&tb);
		newtb = tb;
		if (!connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr))) {
			ftime(&newtb);
		}
		close(sock);
		ts_log(TSF_PING"=%d,%s\n", \
			(newtb.time - tb.time)*1000 + newtb.millitm - tb.millitm, url);
		usleep(500000);
	}
	return 1;
}

int ts_connect(const char *host, const char *port)
{
	int sockfd = -1;
	struct addrinfo hints, *iplist = NULL, *ip = NULL;

	bzero(&hints,sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if(getaddrinfo(host, port, &hints, &iplist) != 0 ){
		return -1;
	}

	for (ip = iplist; ip != NULL; ip = ip->ai_next) {
		sockfd = socket(ip->ai_family, ip->ai_socktype, ip->ai_protocol);
		if (sockfd < 0)
			continue;
		if (connect(sockfd, ip->ai_addr, ip->ai_addrlen) == 0)
			break;
		close(sockfd);
	}
	freeaddrinfo(iplist);
	return sockfd;
}

#define TS_HTTP_HEAD  "GET /%s HTTP/1.1\r\n"\
"Accept: text/html, application/xhtml+xml, */*\r\n"\
"Accept-Language: zh-CN\r\n"\
"User-Agent: Mozilla/5.0(compatible; MSIE 9.0; windows NT 6.1; WOW64; Trident/5.0)\r\n"\
"Accept-Encoding: gzip, deflate\r\n"\
"Accept: */*\r\n"\
"Host: %s\r\n"\
"Connection: Keep-Alive\r\n\r\n"

static int ts_down(void *url, int delay_time)
{
	int len = 0, sock = 0, ret = 0;
	char *ptr = NULL, *end = NULL;
	char host[256], port[64], hurl[512], http[2048], newurl[2048];
	char data[32 * 1024];

	TS_FORK();
	usleep(delay_time*100000);
restart:

	if (!memcmp(url, "http://", 7)) {
		ptr = url + 7;
	} else {
		ptr = url;
	}
	end = strchr(ptr, '/');
	if (!end)
		goto err;
	*end = '\0';
	memset(host, 0, sizeof(host));
	strncpy(host, ptr, sizeof(host) - 1);

	memset(port, 0, sizeof(port));
	ptr = end + 1;
	end = strchr(host, ':');
	if (end) {
		*end = '\0';
		strncpy(port, end + 1, sizeof(port) - 1);
	} else {
		strncpy(port, "80", sizeof(port) - 1);
	}
	end = strchr(ptr, '&');
	if (end) {
		*end = '\0';
	}
	memset(hurl, 0, sizeof(hurl));
	strncpy(hurl, ptr, sizeof(hurl) - 1);

	memset(http, 0, sizeof(http));
	snprintf(http, sizeof(http) - 1, TS_HTTP_HEAD, hurl, host);

	TSPEED_DBG("PRE:sock:%d, host:%s:%s/%s\n", sock, host, port, hurl);
	do {
		sock = ts_connect(host, port);
		sleep(1);
	} while (sock < 0);
	TSPEED_DBG("POST:sock:%d, host:%s:%s/%s\n", sock, host, port, hurl);

	if (send(sock, http, strlen(http), 0) <= 0) {
		TSPEED_DBG("send http fail\n");
		goto err;
	}

	len = 0;
	while (len < 4096) {
		memset(http, 0, sizeof(http));
		ret = recv(sock, http, sizeof(http) - 1, 0);
		if (ret < 0) {
			goto err;
		}
		len += ret;
		if (strstr(http, "\r\n\r\n")) {
			if ((len > 9) && (http[9] == '3')) {
				ptr = strstr(http, "Location:");
				if (!ptr) {
					goto err;
				}
				end = strchr(ptr, '\r');
				if (!end) {
					goto err;
				}
				memset(newurl, 0, sizeof(newurl));
				snprintf(newurl, sizeof(newurl) - 1, "%.*s",\
					end - ptr - strlen("Location: "), ptr + strlen("Location: "));
				close(sock);
				url = newurl;
				TSPEED_DBG("restart:%s\n", newurl);
				goto restart;
			}
			break;
		}
	}

	while (1) {
		ret = recv(sock, data, sizeof(data), 0);
		if (!ret) 
			break;
		if (ret < 0) {
			goto err;
		}
	}

	if (sock > 0)
		close(sock);
	return 1;
err:
	ts_log(TSF_ERR"=down,%s\n", url);
	if (sock > 0)
		close(sock);
	return -1;
}

int ts_classify(int *speed, int *nr)
{
	int i = 0, total = 0;

	for (i = *nr; i < TSPEED_TIME; i++) {
		if (!speed[i])
			break;
		if (speed[*nr] - speed[i] > 100)
			break;
		total += speed[i];
	}
	*nr = i;
	return total;
}

int ts_calc(int *speed)
{
	int tmp = 0, i, j, nr = 0, total = 0, sp = 0;

	for (i = 0; i < TSPEED_TIME; i++) {
		for (j = i; j < TSPEED_TIME; j++) {
			if (speed[i] < speed[j]) {
				tmp = speed[i];
				speed[i] = speed[j];
				speed[j] = tmp;
			}
		}
	}

	TSPEED_DBG("tspeed:");
	for (i = 0; i < TSPEED_TIME; i++) {
		if (!speed[i])
			break;
		TSPEED_DBG("%d,", speed[i]);
	}
	TSPEED_DBG("\n");

	i = 0;
	while (1) {
		tmp = i;
		total = ts_classify(speed, &i);
		j = i - tmp;
		if (j <= 0)
			break;
		TSPEED_DBG("%d-%d, nr:%d, sp:%d\n", tmp, i, j, total/j);
		if (j > 6) {
			nr = j;
			sp = total/nr;
			break;
		} else if (j > nr) {
			nr = j;
			sp = total/nr;
		}
	}
	i = ((sp*8)/1024);
	j = ((sp*8)%1024);
	if ((j > 512) || (i == 0))
		i++;
	sp = i*128;
	TSPEED_DBG("i:%d, j:%d\n", i, j);
	return sp;
}

static void sig_hdr(int signo)
{
	TSPEED_DBG("recv = %d\n", signo);
	ts_log(TSF_ERR"=abort\n");
	tspeed_get(TSPEED_FILE, TSF_PID, ts_kill);
	exit(0);
}

int main(int argc, char *argv[])
{
	int time, flag = 0, speed[TSPEED_TIME], i = 0;
	char *file = argv[1];
	struct if_statistics st;

	if (fork() != 0)
		exit(0);

	tspeed_result_file = argv[2];
	if (!tspeed_result_file || !file)
	{
		TSPEED_DBG("input err, ./tspeed url_file result file\n");
		return 0;
	}

	if (!strcmp(file, "break")) {
		tspeed_get(TSPEED_FILE, TSF_PID, ts_kill);
		return 0;
	}

	signal(SIGTERM, sig_hdr);
	signal(SIGKILL, sig_hdr);

	tspeed_get(TSPEED_FILE, TSF_PID, ts_kill);
	remove(TSPEED_FILE);

	if (access(file, F_OK)) {
		ts_log(TSF_ERR"=file,%s\n", file);
		return -1;
	}
	system("/usr/bin/qos-start.sh stop");

	ts_log(TSF_PID"=%d\n", getpid());

	tspeed_get(file, TSF_PING, ts_ping);
	sleep(2);
	tspeed_get(file, TSF_DOWN, ts_down);
	sleep(1);

	memset(speed, 0, sizeof(speed));
	time = sys_uptime();
	while (sys_uptime() - time < 36) {
		sleep(1);
		if (get_if_statistics(1, &st)) {
			if (!flag) {
				ts_log(TSF_ERR"=st,get\n");
				flag = 1;
			}
		} else if (st.in.all.speed/1024) {
			speed[i] = st.in.all.speed/1024;
			i++;
			if (i >= TSPEED_TIME)
				break;
		}
		ts_log(TSF_SPEED"=%lu\n", st.in.all.speed);
		TSPEED_DBG("down:%lu(KB/s), up:%lu(KB/s)\n",
			st.in.all.speed/1024, st.out.all.speed/1024);
	}

	tspeed_get(TSPEED_FILE, TSF_PID, ts_kill);
	ts_log(TSF_DOWN"=%d\n", ts_calc(speed));
	system("/usr/bin/qos-start.sh restart");
	return 0;
}
