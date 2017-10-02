#include "saas_lib.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#define SAAS_UPLOAD_PWD  "cqm#yg$ysd%ss@oy"

#define MAX_SIMPLE_TIMER 					16
typedef int (*func)(void *data, int len);
typedef struct {
	func cb; 
	int dst_utc;
	void *data;
	int len;
	char is_used;
}stimer_t;

static stimer_t timers[MAX_SIMPLE_TIMER];
static int get_free_index()
{
	int i;
	for (i = 0; i < MAX_SIMPLE_TIMER; i++) {
		if (!timers[i].is_used) {
			return i;
		}
	}
	return -1;
}

int simple_timer_add(int timeout_sec, func _cb, void *d, int dlen)
{
	int i;
	if (!_cb) {
		return -1;
	}

	for (i = 0; i < MAX_SIMPLE_TIMER; i++) {
		if (timers[i].cb == _cb) {
			break;
		}
	}
	
	if (i == MAX_SIMPLE_TIMER) {
		i = get_free_index();
	}
	
	if (i >= 0) {
		timers[i].cb = _cb;
		timers[i].data = d;
		timers[i].len = dlen;
		timers[i].is_used = 1; 
		timers[i].dst_utc = time(NULL) + timeout_sec;
		return 0;
	}else {
		return -1;
	}
}
void simple_timer_sched()
{
	int i;
	time_t now = time(NULL);
	for (i = 0; i < MAX_SIMPLE_TIMER; i++) {
		if (timers[i].is_used && now >= timers[i].dst_utc) {
			timers[i].is_used = 0;
			if (timers[i].cb) {
				timers[i].cb(timers[i].data, timers[i].len);
			}
		}
	}
}
static int saas_url(char *url, int ul, char *addr)
{
	int fd, i, l, len;
	char file[64], tmp[128];

	strcpy(file, "/tmp/taesXXXXXX");
	fd = mkstemp(file);
	if (fd < 0) {
		SAAS_DBG("mkstemp fail, %s\n", strerror(errno));
		return -1;
	}
	snprintf(tmp, sizeof(tmp), "%lu", time(NULL));
	write(fd, tmp, strlen(tmp));
	close(fd);

	len = saas_aes(file, (unsigned char *)tmp,
		sizeof(tmp), (unsigned char *)SAAS_UPLOAD_PWD);

	l = snprintf(url, ul, "%s?t=", addr);
	for (i = 0; i < len; i++) {
		l += snprintf(url + l, ul - l, "%02X", (unsigned char)(tmp[i]));
	}
	remove(file);
	return 0;
}

int saas_connect(const char *url, const char *port)
{
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
		if (!connect(sockfd, ip->ai_addr, ip->ai_addrlen))
			break;
		SAAS_DBG("connect fail, %s\n", strerror(errno));
		close(sockfd);
		sockfd = -1;
	}
	freeaddrinfo(iplist);
	return sockfd;
}

int saas_send(char *file)
{
	struct stat st;
	int len, hl, dl, sock = 0, i = 3;
	char gzfile[128], *buf = NULL, *http, url[128] = {0,};
	char httphead[512] = {0,};

	snprintf(gzfile, sizeof(gzfile), "gzip %s", file);
	if (system(gzfile)) {
		SAAS_DBG("gzip fail\n");
		goto err;
	}
	snprintf(gzfile, sizeof(gzfile), "%s.gz", file);
	if (stat(gzfile, &st)) {
		SAAS_DBG("stat fail\n");
		goto err;
	}
	len = st.st_size + 1024;
	buf = malloc(len);
	if (!buf) {
		SAAS_DBG("malloc fail\n");
		goto err;
	}
	memset(buf, 0, len);

	if (saas_url(url, sizeof(url), "/SS")) {
		SAAS_DBG("url fail\n");
		goto err;
	}

	dl = saas_aes(gzfile, (unsigned char *)(buf + 512),
		len - 512, (unsigned char *)SAAS_UPLOAD_PWD);
	if (dl <= 0) {
		SAAS_DBG("aes fail, %d\n", len);
		goto err;
	}
	hl = snprintf(httphead, sizeof(httphead),
		"POST %s HTTP/1.1\r\n"
		"User-Agent: wiair\r\n"
		"Accept: */*\r\n"
		"Host: http.wiair.com\r\n"
		"Connection: close\r\n"
		"Content-Type: application/octet-stream\r\n"
		"Content-Length: %d\r\n\r\n",
		url, dl);
	http = buf + (512 - hl);
	memcpy(http, httphead, hl);

	i = 3;
	while (i-- > 0) {
		sock = saas_connect("http.wiair.com", "80");
		if (sock > 0)
			break;
		sleep(3);
	}
	if (sock <= 0)
		goto err;

	i = 0;
	while (i < (hl + dl)) {
		len = send(sock, http + i, (hl + dl) - i, 0);
		if (len <= 0) {
			SAAS_DBG("send fail, %s\n", strerror(errno));
			goto err;
		}
		i += len;
	}

	char rep[1024] = {0};
	SAAS_DBG("wait rep...\n");
	read(sock, rep, sizeof(rep));
	SAAS_DBG("rep=:\n%s\n", rep);
	
	free(buf);
	close(sock);
	remove(gzfile);
	remove(file);
	return 0;
err:
	if (buf)
		free(buf);
	if (sock > 0)
		close(sock);
	remove(gzfile);
	remove(file);
	return -1;
}











