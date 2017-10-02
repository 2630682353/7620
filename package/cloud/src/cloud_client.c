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
#include "igd_share.h"
#include "igd_cloud.h"
#include "nlk_ipc.h"
#include "ipc_msg.h"
#include "linux_list.h"
#include "igd_system.h"
#include "igd_md5.h"
#include "igd_host.h"
#include "igd_wifi.h"
#include "igd_advert.h"
#include "igd_interface.h"
#include "igd_plug.h"
#include "image.h"
#include "aes.h"

#define CC_LOG_FILE   "/tmp/cc_dbg"
#define CC_ERR_FILE   "/tmp/cc_err"

#ifdef FLASH_4_RAM_32
#define CC_DBG(fmt,args...) do {}while(0)
#else
#define CC_DBG(fmt,args...) do { \
	igd_log(CC_LOG_FILE, "DBG:[%05ld,%05d],%s:"fmt, \
		sys_uptime(), __LINE__, __FUNCTION__, ##args);\
} while(0)
#endif

#define CC_ERR(fmt,args...) do { \
	igd_log(CC_ERR_FILE, "ERR:[%05ld,%05d],%s:"fmt, \
		sys_uptime(), __LINE__, __FUNCTION__, ##args);\
} while(0)

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

struct cp_file_info {
	int fd;
	char md5[33];
	char file[2048];
	char upfile[2048];
	unsigned int fileid;
	unsigned long long size;
	unsigned long long rcv_size;
};

struct cloud_private_info {
	int sock;
	int len;
	struct in_addr addr;
	unsigned short port;
	unsigned int key;
	unsigned int usrid;
	unsigned int id;
	unsigned char *data;
	struct cp_file_info cpfi;
};

extern int set_hw_id(unsigned int hwid);
extern int get_sysflag(unsigned char bit);
extern int set_sysflag(unsigned char bit, unsigned char fg);
extern int get_flashid(unsigned char *fid);
extern int set_flashid(unsigned char *fid);

int cc_devid(unsigned int *devid)
{
	int ret;
	char hwid[32];

	memset(hwid, 0, sizeof(hwid));
	ret = mtd_get_val("hw_id", hwid, sizeof(hwid));
	if (ret == -2) {
		return -1;
	} else if (ret < 0) {
		*devid = 0;
	} else {
		*devid = (unsigned int)atoll(hwid);
	}
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

static void cc_settime(unsigned int time)
{
	struct timeval tv, now;

	gettimeofday(&now, NULL);

	tv.tv_sec = time;
	tv.tv_usec = 0;

	if (tv.tv_sec != now.tv_sec) {
		settimeofday(&tv, NULL);
		//CC_DBG("%ld,%ld\n", tv.tv_sec, now.tv_sec);
	}
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
	ccl->time = sys_uptime();
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
		if ((ccl->time + 3*60) < sys_uptime()) {
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

static int cc_fw_check(char *file)
{
	struct image_header iheader;
	int len, fd, ret = -1;
	char *retstr, firmname[32];

	retstr = read_firmware("VENDOR");
	if (!retstr)
		return -1;

	len = snprintf(firmname, sizeof(firmname), "%s_", retstr);
	retstr = read_firmware("PRODUCT");
	if (!retstr)
		return -1;

	snprintf(&firmname[len], sizeof(firmname) - len, "%s", retstr);
	fd = open(file, O_RDONLY);
	if (fd < 0)
		return -1;
	len = read(fd, (void *)&iheader, sizeof(iheader));
	if (len != sizeof(iheader))
		goto error;

	if (strncmp((char *)iheader.ih_name, firmname, 15)) {
		CC_ERR("mod err [%s],[%s]\n", iheader.ih_name, firmname);
		goto error;
	}
	ret = 0;

error:
	CC_DBG("[%s],[%s]\n", iheader.ih_name, firmname);
	close(fd);
	return ret;
}

static int cc_sysupdate(char *url, char *md5)
{
	char tmp[512], *d = NULL;
	int fd = 0, l, r = -1;
	struct stat st;
	unsigned char hexmd5[32];

	remove(CC_UPGRADE_FILE);
	snprintf(tmp, sizeof(tmp), 
		"wget -q %s -O %s -T 300", url, CC_UPGRADE_FILE);
	if (system(tmp))
		goto err;

	fd = open(CC_UPGRADE_FILE, O_RDONLY);
	if (fd < 0)
		goto err;

	if (fstat(fd, &st) < 0)
		goto err;
	l = (int)st.st_size;

	d = malloc(l);
	if (!d) 
		goto err;

	if (read(fd, d, l) != l)
		goto err;

	get_md5_numbers((unsigned char *)d, hexmd5, l);
	for (l = 0; l < 16; l++)
		sprintf((char *)&tmp[l*2], "%02x", hexmd5[l]);

	if (strncasecmp(md5, tmp, 32)) {
		CC_ERR("URL:[%s], MD5ERR:\n%s\n%s\n", url,  md5, tmp);
		goto err;
	}
	CC_DBG("MD5:\n%s\n%s\n", md5, tmp);

	if (cc_fw_check(CC_UPGRADE_FILE)) {
		r = 0; // mod err exit
		goto err;
	}

	if (mu_msg(SYSTME_MOD_CLOUD_UPDATE, NULL, 0, NULL, 0))
		system("sysupgrade "CC_UPGRADE_FILE);

	r = 0;
err:
	if (fd > 0)
		close(fd);
	if (d)
		free(d);
	return r;
}

static pid_t cc_upgrade_fork(char *url, char *md5)
{
	int i = 0;
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		CC_ERR("upgrade fork fail, %m\n");
	} else if (pid > 0) {
		CC_DBG("upgrade fork succ, pid:%d\n", pid);
	} else {
		CC_DBG("start\n");
		while (sys_uptime() < 3*60) {
			CC_DBG("sleep 30\n");
			sleep(30);
		}

#ifdef FLASH_4_RAM_32
		system("up_ready.sh");
#endif
		for (i = 0; i < 3; i++) {
			if (!cc_sysupdate(url, md5))
				break;
			CC_DBG("sleep 60\n");
			sleep(60);
		}
		sleep(30);

#ifdef FLASH_4_RAM_32
		system("reboot");
#endif
		exit(0);
	}
	return pid;
}

static int cc_flashid(unsigned int *devid, int check)
{
	int ret = 0, i = 0;
	unsigned char uid[20];
	unsigned char md5[18], old_md5[18];

	memset(uid, 0, sizeof(uid));
	if (dump_flash_uid(uid) < 0) {
		CC_ERR("dump fail\n");
		memset(uid, 0, sizeof(uid));
	}

	memcpy(&uid[8], devid, sizeof(*devid));
	memcpy(&uid[12], "SFDSEM", 6);

	memset(md5, 0, sizeof(md5));
	get_md5_numbers(uid, md5, sizeof(uid));

	if (check) {
		ret = get_flashid(old_md5);
		if (ret && *devid) {
			set_flashid(md5);
		} else if ((*devid != 0)
			&& (ret == 0) && memcmp(md5, old_md5, 16)) {
			CC_ERR("reset\n");
			for (i = 0; i < 16; i++)
				CC_ERR("%02X", md5[i]);
			CC_ERR("\n");
			for (i = 0; i < 16; i++)
				CC_ERR("%02X", old_md5[i]);
			CC_ERR("\n");

			*devid = 0;
			set_hw_id(0);
		}
		return 0;
	} else {
		return set_flashid(md5);
	}
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

unsigned long long cc_ntohll(unsigned long long val)
{
	return (((unsigned long long)(ntohl(val)) << 32)) | ntohl(val >> 32);;
}

unsigned long long cc_htonll(unsigned long long val)
{
	return (((unsigned long long)(htonl(val)) << 32)) | htonl(val >> 32);
}

static int cc_md5(char *file, void *md5)
{
	int fd, size, len;
	unsigned char buf[4096] = {0};
	oemMD5_CTX context;
	struct stat st;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		return -1;

	if (fstat(fd, &st) < 0)
		goto err;

	size = (int)st.st_size;
	if (size < 0)
		goto err;

	oemMD5Init(&context);
	while (size > 0) {
		len = igd_safe_read(fd, buf, sizeof(buf));
		if (len == sizeof(buf) || len == size) {
			oemMD5Update(&context, buf, len);
			size -= len;
		} else {
			goto err;
		}
	}
	close(fd);
	oemMD5Final(md5, &context);
	return 0;
err:
	close(fd);
	return -1;
}

int cc_read(int fd, void *dst, int len)
{
	int r;
	unsigned char *d = dst;

	while (len > 0) {
		r = read(fd, d, len);
		if (r > 0) {
			len -= r;
			d += r;
		} else if (r < 0) {
			if (errno == EINTR
				|| errno == EAGAIN
				|| errno == EWOULDBLOCK) 
				continue;
			break;
		} else {
			break;
		}
	}
	return (int)(d - (unsigned char *)dst);
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
	cc_settime(CC_PULL4(cci->data, 4));
	return CSO_XX;
}

static int cc_link(struct cloud_client_info *cci)
{
	int nr, i, sock = 0, bit[2];
	struct in_addr addr[CC_SERVER_ADDR_NUM];

	bit[0] = ICFT_ONLINE;
	bit[1] = NLK_ACTION_DEL;
	mu_msg(IGD_CLOUD_FLAG, bit, sizeof(bit), NULL, 0);

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
	set_hw_id(cci->devid);
	cc_flashid(&cci->devid, 0);

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
	mu_msg(IGD_CLOUD_IP, &ip, sizeof(ip), NULL, 0);

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
	int i = 0, l, uid;
	char *ptr, tmp[64];
	unsigned char buf[128] = {0,};
	struct iface_conf wan_info;

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

	l = CC_MSG_RCONF(RCONF_FLAG_VER, tmp, sizeof(tmp));
	CC_DBG("%d,%s\n", l, tmp);
	l = (l < 0) ? 0 : l;
	CC_PUSH1(buf, i, l);
	i += 1;
	if (l > 0) {
		CC_PUSH_LEN(buf, i, tmp, l);
		i += l;
	}

	uid = 1;
	memset(&wan_info, 0, sizeof(wan_info));
	if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &wan_info, sizeof(wan_info)) ||
		(wan_info.mode != MODE_PPPOE)) {
		wan_info.pppoe.user[0] = '\0';
	}
	wan_info.pppoe.user[sizeof(wan_info.pppoe.user) - 1] = '\0';
	l = strlen(wan_info.pppoe.user);
	CC_PUSH1(buf, i, l);
	i += 1;
	if (l > 0) {
		CC_PUSH_LEN(buf, i, wan_info.pppoe.user, l);
		i += l;
		CC_DBG("%d,%s\n", l, wan_info.pppoe.user);
	}

	CC_PUSH2(buf, 0, i);
	if (cc_send(cci->sock, buf, i))
		return CSO_LINK;
	return CSO_ACK_ROUTER_KEEP;
}

static int cc_keep_ack(struct cloud_client_info *cci)
{
	int len = 0, bit[2];
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
		set_sysflag(SYSFLAG_RESET, 0);
		set_sysflag(SYSFLAG_RECOVER, 0);
		igd_clear_bit(CCF_RESET, cci->flag);
	}

	bit[0] = ICFT_ONLINE;
	bit[1] = NLK_ACTION_ADD;
	mu_msg(IGD_CLOUD_FLAG, bit, sizeof(bit), NULL, 0);

	cci->key = CC_PULL4(buf, 6);
	cci->userid = CC_PULL4(buf, 10);
	cc_settime(CC_PULL4(buf, 14));
	set_usrid(cci->userid);

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
	set_usrid(cci->userid);
	return CSO_XX;
}

static int cc_upgrade(struct cloud_client_info *cci)
{
	int i = 4, l = cci->len;
	unsigned char type, act, len;
	char *now_ver, ver[32], url[256], md5[128];

	if (l < 9) {
		CC_ERR("%d\n", l);
		goto end;
	}

	type = CC_PULL1(cci->data, i);
	i += 1;
	if (!type)
		CC_DBG("type is 0\n");

	act = CC_PULL1(cci->data, i);
	i += 1;

	//ver
	len = CC_PULL1(cci->data, i);
	i += 1;
	l -= 7;
	if (l < len) {
		CC_ERR("%d,%d\n", l, len);
		goto end;
	} else if (len > 0) {
		memset(ver, 0, sizeof(ver));
		CC_PULL_LEN(cci->data, i, ver, N_MIN(len, sizeof(ver) - 1));
		i += len;
		l -= len;
	}

	//url
	if (l < 1) {
		CC_ERR("%d\n", l);
		goto end;
	}
	len = CC_PULL1(cci->data, i);
	i += 1;
	l -= 1;
	if (l < len) {
		CC_ERR("%d,%d\n", l, len);
		goto end;
	} else if (len > 0) {
		memset(url, 0, sizeof(url));
		CC_PULL_LEN(cci->data, i, url, len);
		i += len;
		l -= len;
	}

	//md5
	if (l < 1) {
		CC_ERR("%d\n", l);
		goto end;
	}
	len = CC_PULL1(cci->data, i);
	i += 1;
	if (len > 0) {
		memset(md5, 0, sizeof(md5));
		CC_PULL_LEN(cci->data, i, md5, N_MIN(len, sizeof(md5) - 1));
		i += len;
	}

	now_ver = read_firmware("CURVER");
	CC_DBG("[%s]:%d,%d,[%s],[%s],[%s]\n",
		now_ver ? now_ver : "", type, act, ver, url, md5);

	if (now_ver && !strcmp(ver, now_ver))
		goto end;

	if (cci->upgrade_pid > 0)
		goto end;

	if (act == 3) {
		igd_set_bit(CCF_UPGRADE, cci->flag);
		if (cci->up_md5)
			free(cci->up_md5);
		if (cci->up_url)
			free(cci->up_url);
		cci->up_md5 = strdup(md5);
		cci->up_url = strdup(url);
		goto end;
	}

	cci->upgrade_pid = cc_upgrade_fork(url, md5);

end:
	return CSO_XX;
}

static int cc_history(struct cloud_client_info *cci)
{
	if (cci->len < 4) {
		CC_ERR("%d\n", cci->len);
		return CSO_LINK;
	}

	mu_msg(IGD_HOST_ADD_HISTORY,
		cci->data + 4, cci->len - 4, NULL, 0);
	return CSO_XX;
}

static int cc_reset(struct cloud_client_info *cci)
{
	CC_DBG("succ\n");
	//set_sysflag(SYSFLAG_RECOVER, 0);
	return CSO_XX;
}

static int cc_custom_url(struct cloud_client_info *cci)
{
	CC_DBG("succ\n");
	return CSO_XX;
}

static int cc_recommand_url(struct cloud_client_info *cci)
{
	CC_DBG("succ\n");
	return CSO_XX;
}

static int cc_visitor_mac(struct cloud_client_info *cci)
{
	CC_DBG("succ\n");
	return CSO_XX;
}

static int cc_ssid_ack(struct cloud_client_info *cci)
{
	CC_DBG("succ\n");
	mu_msg(WIFI_UPLOAD_SUCCESS, NULL, 0, NULL, 0);
	return CSO_XX;
}

static int cc_price(struct cloud_client_info *cci)
{
	CC_DBG("succ\n");
	mu_msg(ADVERT_MOD_UPLOAD_SUCCESS, NULL, 0, NULL, 0);
	return CSO_XX;
}

static int cc_config(struct cloud_client_info *cci)
{
	int i = 0, bit[2];
	char ver[32], url[256], cmd[512];
	unsigned char len;

	if (cci->len < 6) {
		CC_ERR("%d\n", cci->len);
		return CSO_XX;
	}

	CC_DBG("result:%d\n", CC_PULL2(cci->data, 4));

	bit[0] = ICFT_CHECK_RCONF;
	bit[1] = NLK_ACTION_DEL;
	mu_msg(IGD_CLOUD_FLAG, bit, sizeof(bit), NULL, 0);

	if (CC_PULL2(cci->data, 4)) {
		system("cloud_exchange c "RCONF_FLAG_L7" &");
		return CSO_XX;
	}

	len = CC_PULL1(cci->data, 6);
	if (cci->len < (len + 8)) {
		CC_ERR("%d,%d\n", cci->len, len);
		return CSO_XX;
	}

	i = 7;
	memset(ver, 0, sizeof(ver));
	CC_PULL_LEN(cci->data, i, ver, len);
	i += len;

	len = CC_PULL1(cci->data, i);
	i += 1;
	if (cci->len < (len + i)) {
		CC_ERR("%d,%d,%d\n", cci->len, len, i);
		return CSO_XX;
	}
	memset(url, 0, sizeof(url));
	CC_PULL_LEN(cci->data, i, url, len);
	i += len;

	CC_DBG("%d,%d,%s,%s\n", cci->len, i, ver, url);
	snprintf(cmd, sizeof(cmd), "cloud_exchange r \"%s\" &", url);
	CC_DBG("%s\n", cmd);
	system(cmd);
	return CSO_XX;
}

static int cc_config_ver(struct cloud_client_info *cci)
{
	int bit[2];

	bit[0] = ICFT_UP_RCONF_VER;
	bit[1] = NLK_ACTION_DEL;
	if (mu_msg(IGD_CLOUD_FLAG, bit, sizeof(bit), NULL, 0))
		CC_ERR("del fail\n");
	else
		CC_DBG("succ\n");
	return CSO_XX;
}

static int cc_alone(struct cloud_client_info *cci)
{
	int bit[2], i;
	unsigned short result, flag;
	char ver[32], md5[64], url[256], cmd[512];
	unsigned char len;

	if (cci->len < 8) {
		CC_ERR("%d\n", cci->len);
		return CSO_XX;
	}

	result = CC_PULL2(cci->data, 4);
	flag = CC_PULL2(cci->data, 6);

	if (flag >= CCA_MAX) {
		CC_ERR("nonsupport:%d\n", flag);
		return CSO_XX;
	}
	bit[0] = flag;
	bit[1] = NLK_ACTION_DEL;
	mu_msg(IGD_CLOUD_ALONE_CHECK_UP, bit, sizeof(bit), NULL, 0);

	if (result) {
		CC_DBG("result:%d, flag:%d\n", result, flag);
		return CSO_XX;
	}

	i = 8;
	len = CC_PULL1(cci->data, i);
	i += 1;
	if ((len >= sizeof(ver)) || ((len + i + 1) > cci->len)) {
		CC_ERR("%d,%d,%d\n", cci->len, len, i);
		goto err;
	}
	CC_PULL_LEN(cci->data, i, ver, len);
	ver[len] = 0;
	i += len;

	len = CC_PULL1(cci->data, i);
	i += 1;
	if ((len + i + 1) > cci->len) {
		CC_ERR("%d,%d,%d\n", cci->len, len, i);
		goto err;
	}
	CC_PULL_LEN(cci->data, i, url, len);
	url[len] = 0;
	i += len;

	len = CC_PULL1(cci->data, i);
	i += 1;
	if ((len >= sizeof(md5)) || ((len + i) > cci->len)) {
		CC_ERR("%d,%d,%d\n", cci->len, len, i);
		goto err;
	}
	CC_PULL_LEN(cci->data, i, md5, len);
	md5[len] = 0;
	i += len;
	
	snprintf(cmd, sizeof(cmd),
		"cloud_exchange a %d \"%s\" \"%s\" \"%s\" &",
		flag, ver, md5, url);
	CC_DBG("%d\n", flag);
	system(cmd);

err:
	return CSO_XX;
}

static int cc_alone_ver(struct cloud_client_info *cci)
{
	int bit[2];
	unsigned short result, flag;

	if (cci->len < 8) {
		CC_ERR("%d\n", cci->len);
		return CSO_XX;
	}
	result = CC_PULL2(cci->data, 4);
	flag = CC_PULL2(cci->data, 6);
	if (result) {
		CC_DBG("%d, fail\n", flag);
		return CSO_XX;
	}
	bit[0] = flag;
	bit[1] = NLK_ACTION_DEL;
	if (mu_msg(IGD_CLOUD_ALONE_VER, bit, sizeof(bit), NULL, 0))
		CC_ERR("%d, add ver fail\n", flag);
	else
		CC_DBG("%d, succ\n", flag);
	return CSO_XX;
}

static int cc_switch(struct cloud_client_info *cci)
{
	unsigned char st_len;
	struct nlk_switch_config nlk;

	if (cci->len < 6) {
		CC_ERR("%d\n", cci->len);
		return CSO_XX;
	}

	memset(&nlk, 0, sizeof(nlk));
	nlk.flag = CC_PULL1(cci->data, 4);
	st_len = CC_PULL1(cci->data, 5);
	if ((st_len + 6) > cci->len) {
		CC_ERR("%d,%d\n", st_len, cci->len);
		return CSO_XX;
	}
	st_len = N_MIN(st_len, sizeof(nlk.status) - 1);
	if (st_len > 0)
		CC_PULL_LEN(cci->data, 6, nlk.status, st_len);

	nlk.comm.gid = NLKMSG_GRP_SYS;
	nlk.comm.mid = SYS_GRP_MID_SWITCH;
	nlk_event_send(NLKMSG_GRP_SYS, &nlk, sizeof(nlk));
	return CSO_XX;
}

static int cc_tspeed(struct cloud_client_info *cci)
{
	unsigned char act, hl, ul;
	char host[128] = {0,}, url[128] = {0,}, str[256];
	unsigned short port;

	if (cci->len < 11) {
		CC_ERR("%d\n", cci->len);
		return CSO_XX;
	}

	act = CC_PULL1(cci->data, 4);
	if (act != 1) {
		CC_ERR("act err, %d\n", act);
		return CSO_XX;
	}

	port = CC_PULL2(cci->data, 5);
	hl = CC_PULL1(cci->data, 7);
	if (!hl || hl >= sizeof(host)
		|| ((8 + hl) > cci->len)) {
		CC_ERR("hl %d\n", hl);
		return CSO_XX;
	}
	CC_PULL_LEN(cci->data, 8, host, hl);

	ul = CC_PULL1(cci->data, 8 + hl);
	if (!ul || ul >= sizeof(url)
		|| ((9 + hl + ul) > cci->len)) {
		CC_ERR("ul %d\n", ul);
		return CSO_XX;
	}
	CC_PULL_LEN(cci->data, 9 + hl, url, ul);

	snprintf(str, sizeof(str), "cloud_exchange t \"%s\" \"%d\" \"%s\" &",
		host, port, url);
	CC_DBG("%s\n", str);
	system(str);
	return CSO_XX;
}

static int cc_plug_info(struct cloud_client_info *cci)
{
	int i, l, act;
	struct plug_cache_info pci;

	mu_msg(IGD_PLUG_STOP_REQUEST, NULL, 0, NULL, 0);
	if (cci->len < 5) {
		CC_ERR("%d\n", cci->len);
		return CSO_XX;
	}

	memset(&pci, 0, sizeof(pci));
	act = (int)CC_PULL1(cci->data, 4);
	i = 5;
	CC_DBG("act: %d\n", act);
	if (act == 2) { // close all plug
		mu_msg(IGD_PLUG_KILL_ALL, NULL, 0, NULL, 0);
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
		mu_msg(IGD_PLUG_KILL, pci.plug_name, l + 1, NULL, 0);
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

	mu_msg(IGD_PLUG_CACHE, &pci, sizeof(pci), NULL, 0);
	return CSO_XX;
}

#define CP_CALL_FUNCITON "----------------------------------"

#define CP_CRC 131427
#define CP_FILE_SIGN   ".cpup"
#define CP_PATH_PREFIX   "/data/"

enum {
	CPE_FAIL = 10001,
	CPE_INPUT,
	CPE_MALLOC,
	CPE_EXIST,
	CPE_NONEXIST,
	CPE_FULL,
	CPE_NOLOGIN,
	CPE_NOSUPPORT,
	CPE_ACCOUNT_NOTREADY,
	CPE_TIMEOUT,
	CPE_FILE,
};

static int cp_file_check(char *file)
{
	if (memcmp(file, CP_PATH_PREFIX, strlen(CP_PATH_PREFIX)))
		return -1;
	if (strstr(file, "/../"))
		return -1;
	return 0;
}

static int cp_connect(struct cloud_private_info *cpi)
{
	int i = 3;

	cpi->id = 1;
	while (i > 0) {
		cpi->sock = cc_connect(cpi->addr, cpi->port);
		if (cpi->sock > 0) {
			cc_setkeep(cpi->sock);
			cc_setsock(cpi->sock, 3);
			return CPO_REQ_ROUTER_KEEP;
		}
		sleep(3);
	}
	CC_DBG("fail, %s\n", strerror(errno));
	return CPO_EXIT;
}

static int cp_keep(struct cloud_private_info *cpi)
{
	unsigned char buf[32];
	unsigned int devid = get_devid();

	if (!devid) {
		CC_ERR("devid err\n");
		return CPO_EXIT;
	}

	CC_PUSH2(buf, 0, 16);
	CC_PUSH2(buf, 2, CPO_REQ_ROUTER_KEEP);
	CC_PUSH4(buf, 4, devid);
	CC_PUSH4(buf, 8, cpi->key^cpi->id);
	CC_PUSH4(buf, 12, cpi->usrid);
	cpi->id++;
	CC_DBG("id:%d, key:0x%X\n", cpi->id, cpi->key);
	return cc_send(cpi->sock, buf, 16) ? \
		CPO_EXIT : CPO_ACK_ROUTER_KEEP;
}

static int cp_keep_ack(struct cloud_private_info *cpi)
{
	int len;
	unsigned char *buf = NULL;
	
	if (cc_recv(cpi->sock, &buf, &len) < 0)
		goto err;
	if ((len < 6) ||
		(CC_PULL2(buf, 2) != CPO_ACK_ROUTER_KEEP)) {
		CC_ERR("%d,%d\n", len, CC_PULL2(buf, 2));
		goto err;
	}
	if (CC_PULL2(buf, 4)) {
		CC_ERR("%d\n", CC_PULL2(buf, 4));
		goto err;
	}
	cpi->key = CC_PULL4(buf, 6)^CP_CRC;
	free(buf);
	return CPO_WAIT;
err:
	if (buf)
		free(buf);
	return CPO_EXIT;
}

static int cp_wait(struct cloud_private_info *cpi, int timeout)
{
	fd_set fds;
	struct timeval tv;

	while (1) {
		if (timeout == 1)
			break;
		if (timeout > 1)
			timeout--;
	
		FD_ZERO(&fds);
		FD_SET(cpi->sock, &fds);

		tv.tv_sec = 1;
		tv.tv_usec = 0;
		if (select(cpi->sock + 1, &fds, NULL, NULL, &tv) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
		}

		if (FD_ISSET(cpi->sock, &fds)) {
			if (cc_recv(cpi->sock, &cpi->data, &cpi->len) < 0)
				break;
			return CC_PULL2(cpi->data, 2);
		}
	}
	return CPO_EXIT;
}

static int cp_free(struct cloud_private_info *cpi)
{
	if (cpi->data) {
		free(cpi->data);
		cpi->data = NULL;
		cpi->len = 0;
	}
	return CPO_WAIT;
}

static int cp_exit(struct cloud_private_info *cpi)
{
	CC_DBG("\n");

	cp_free(cpi);
	if (cpi->sock > 0)
		close(cpi->sock);
	return 0;
}

static int cp_transfer(struct cloud_private_info *cpi)
{
	unsigned short len, order_len, order;

	if (cpi->len < 18) {
		CC_ERR("len:%d\n", cpi->len);
		return CPO_EXIT;
	}

	if (cpi->usrid != CC_PULL4(cpi->data, 8)) {
		CC_ERR("userid:%u, %u\n",
			cpi->usrid, CC_PULL4(cpi->data, 8));
		return CPO_EXIT;
	}

	len = CC_PULL2(cpi->data, 12);
	order_len = CC_PULL2(cpi->data, 14);
	order = CC_PULL2(cpi->data, 16);
	if ((len != order_len) || (cpi->len < (len + 12))) {
		CC_ERR("%d,%d,%d\n", len, order_len, cpi->len);
		return CPO_EXIT;
	}
	CC_DBG("%d,%d\n", order, order_len);
	return order;
}

static int cp_transfer_send(
	struct cloud_private_info *cpi, unsigned char *data, int len)
{
	int ret;
	unsigned char *buf;

	buf = malloc(len + 14);
	if (!buf) {
		CC_ERR("malloc fail\n");
		return -1;
	}
	CC_PUSH2(buf, 0, len + 14);
	CC_PUSH2(buf, 2, CPO_NTF_ROUTER_TRANSFER);
	CC_PUSH4(buf, 4, cpi->key^cpi->id);
	CC_PUSH4(buf, 8, cpi->usrid);
	CC_PUSH2(buf, 12, len);
	CC_PUSH_LEN(buf, 14, data, len);

	cpi->id++;
	CC_DBG("%d,%d\n", CC_PULL2(data, 2), len);
	ret = cc_send(cpi->sock, buf, len + 14);
	free(buf);
	return ret;
}

static int cp_file_s2r_req(struct cloud_private_info *cpi)
{
	unsigned char *buf = &cpi->data[14];
	struct cp_file_info *cpfi = &cpi->cpfi;
	unsigned short order_len = CC_PULL2(buf, 0), i, l;

	if (order_len < 19) {
		CC_ERR("%d\n", order_len);
		return CPO_EXIT;
	}

	if (cpfi->fileid) {
		CC_ERR("fileid is exist, %d,%d\n",
			cpfi->fileid, CC_PULL4(buf, 4));
		return CPO_EXIT;
	}
	cpfi->fileid = CC_PULL4(buf, 4);
	l = CC_PULL2(buf, 8);
	if ((l == 0) || (order_len < (19 + l))) {
		CC_ERR("%d,%d\n", order_len, l);
		return CPO_EXIT;
	}
	CC_PULL_LEN(buf, 10, cpfi->file,
		N_MIN(l, sizeof(cpfi->file) - 1));

	i = 10 + l;
	l = CC_PULL1(buf, i);
	i += 1;
	if ((l == 0) || (order_len < (i + l + 8))) {
		CC_ERR("%d,%d\n", order_len, l);
		return CPO_EXIT;
	}
	CC_PULL_LEN(buf, i, cpfi->md5, N_MIN(32, l));
	i += l;
	cpfi->size = CC_PULL8(buf, i);
	CC_DBG("fileid:%d, size:%llu\n", cpfi->fileid, cpfi->size);
	return CPO_ACK_START_SYNC_TO_ROUTER;
}

static int cp_file_s2r_ack(struct cloud_private_info *cpi)
{
	unsigned char buf[18];
	unsigned short ret = 0;
	struct cp_file_info *cpfi = &cpi->cpfi;

	snprintf(cpfi->upfile, sizeof(cpfi->upfile), "%s%s",
		cpfi->file, CP_FILE_SIGN);
	if (cp_file_check(cpfi->file)) {
		CC_ERR("%s\n", cpfi->file);
		ret = CPE_INPUT;
	} else if (!access(cpfi->file, F_OK)) {
		ret = CPE_EXIST;
	} else {
		cpfi->fd = open(cpfi->upfile, O_WRONLY | O_CREAT, 0777);
		if (cpfi->fd < 0) {
			CC_ERR("%s, %s\n", cpfi->upfile, strerror(errno));
			ret = CPE_FAIL;
		} else {
			cpfi->rcv_size = lseek(cpfi->fd, 0, SEEK_END);
		}
	}
	CC_PUSH2(buf, 0, 18);
	CC_PUSH2(buf, 2, CPO_ACK_START_SYNC_TO_ROUTER);
	CC_PUSH2(buf, 4, ret);
	CC_PUSH4(buf, 6, cpfi->fileid);
	CC_PUSH8(buf, 10, cpfi->rcv_size);
	CC_DBG("ret:%d, %llu, %s\n", ret, cpfi->rcv_size, cpfi->file);
	if (cp_transfer_send(cpi, buf, 18)) {
		CC_ERR("send err\n");
		return CPO_EXIT;
	}
	return ret ? CPO_EXIT : CPO_SERVER_TO_ROUTE;
}

static int cp_sync_file(struct cloud_private_info *cpi)
{
	unsigned char *buf = &cpi->data[14], ack[32];
	unsigned short order_len = CC_PULL2(buf, 0);
	unsigned int len = 0;
	int ret = CPE_FAIL;

	if (order_len < 12) {
		CC_ERR("len err, %d\n", order_len);
		goto err;
	}
	if (cpi->cpfi.fileid != CC_PULL4(buf, 4)) {
		CC_ERR("fileid err, %d\n", CC_PULL4(buf, 4), cpi->cpfi.fileid);
		goto err;
	}
	len = CC_PULL4(buf, 8);
	if ((len + 12) > order_len) {
		CC_ERR("data err, %d,%d\n", order_len, len);
		goto err;
	}
	if (len != igd_safe_write(cpi->cpfi.fd, &buf[12], len)) {
		CC_ERR("write err, %d,%d\n", order_len, len);
		goto err;
	}
	cpi->cpfi.rcv_size += len;
	ret = 0;
err:
	CC_PUSH2(ack, 0, 14);
	CC_PUSH2(ack, 2, CPO_ACK_SYNC_FILE);
	CC_PUSH2(ack, 4, ret);
	CC_PUSH4(ack, 6, cpi->cpfi.fileid);
	CC_PUSH4(ack, 10, len);
	if (cp_transfer_send(cpi, ack, 14)) {
		CC_ERR("send err\n");
		return CPE_FAIL;
	}
	return ret;
}

static int cp_sync_over(struct cloud_private_info *cpi)
{
	int ret = CPE_FAIL, i;
	unsigned char *buf = &cpi->data[14], str[64], md5[32];
	unsigned short order_len = CC_PULL2(buf, 0);

	if (order_len < 8) {
		CC_ERR("len err, %d\n", order_len);
		goto err;
	}
	if (cpi->cpfi.fileid != CC_PULL4(buf, 4)) {
		CC_ERR("fileid err, %d\n", CC_PULL4(buf, 4), cpi->cpfi.fileid);
		goto err;
	}
	if (cpi->cpfi.size != cpi->cpfi.rcv_size) {
		CC_ERR("size: %llu,%llu\n", cpi->cpfi.size, cpi->cpfi.rcv_size);
		goto err;
	}
	if (cc_md5(cpi->cpfi.upfile, md5)) {
		CC_ERR("md5 fail: %s\n", cpi->cpfi.upfile);
		goto err;
	}
	for (i = 0; i < 16; i++)
		snprintf((char *)&str[i*2], sizeof(str) - i*2, "%02X", md5[i]);
	if (strncasecmp(cpi->cpfi.md5, (char *)str, 32)) {
		CC_ERR("md5 err:\n%s\n%s\n", cpi->cpfi.md5, str);
		goto err;
	}
	if (cpi->cpfi.fd > 0) {
		close(cpi->cpfi.fd);
		cpi->cpfi.fd = -1;
	}
	CC_DBG("finish\n");
	rename(cpi->cpfi.upfile, cpi->cpfi.file);
	ret = 0;
err:
	if (cpi->cpfi.fd > 0) {
		close(cpi->cpfi.fd);
		cpi->cpfi.fd = -1;
	}
	remove(cpi->cpfi.upfile);
	CC_PUSH2(str, 0, 10);
	CC_PUSH2(str, 2, CPO_ACK_SYNC_OVER);
	CC_PUSH2(str, 4, ret);
	CC_PUSH4(str, 6, cpi->cpfi.fileid);
	if (cp_transfer_send(cpi, str, 10)) {
		CC_ERR("send err\n");
		return CPE_FAIL;
	}
	return ret;
}

static int cp_file_s2r(struct cloud_private_info *cpi)
{
	int order;

	while (1) {
		cp_free(cpi);
		order = cp_wait(cpi, 60);
		CC_DBG("order:%d, len:%d\n", order, cpi->len);
		if (order == CPO_EXIT) {
			break;
		} else if (order == CPO_NTF_ROUTER_TRANSFER) {
			order = cp_transfer(cpi);
			CC_DBG("order:%d\n", order);
			if (order == CPO_NTF_SYNC_FILE) {
				if (cp_sync_file(cpi))
					break;
			} else if (order == CPO_NTF_SYNC_OVER) {
				if (cp_sync_over(cpi))
					break;
				memset(&cpi->cpfi, 0, sizeof(cpi->cpfi));
				return CPO_FREE;
			} else {
				break;
			}
		} else if (order == CPO_ACK_ROUTER_TRANSFER) {
			;
		} else {
			CC_DBG("order err:%d\n", order);
		}
	}
	return CPO_EXIT;
}

static int cp_file_r2s_req(struct cloud_private_info *cpi)
{
	unsigned char *buf = &cpi->data[14];
	struct cp_file_info *cpfi = &cpi->cpfi;
	unsigned short order_len = CC_PULL2(buf, 0), l;

	if (order_len < 18) {
		CC_ERR("%d\n", order_len);
		return CPO_EXIT;
	}
	if (cpfi->fileid) {
		CC_ERR("fileid is exist, %d,%d\n",
			cpfi->fileid, CC_PULL4(buf, 4));
		return CPO_EXIT;
	}
	cpfi->fileid = CC_PULL4(buf, 4);
	l = CC_PULL2(buf, 8);
	if ((l == 0) || (order_len < (18 + l))) {
		CC_ERR("%d,%d\n", order_len, l);
		return CPO_EXIT;
	}
	CC_PULL_LEN(buf, 10, cpfi->file,
		N_MIN(l, sizeof(cpfi->file) - 1));
	cpfi->rcv_size = CC_PULL8(buf, 10 + l);
	CC_DBG("file:%s, size:%llu\n", cpfi->file, cpfi->rcv_size);
	return CPO_ACK_START_SYNC_FROM_ROUTER;
}

static int cp_file_r2s_ack(struct cloud_private_info *cpi)
{
	int i;
	unsigned char buf[64], md5[16];
	unsigned short ret = 0;
	struct stat st;
	struct cp_file_info *cpfi = &cpi->cpfi;

	cpfi->fd = open(cpfi->file, O_RDONLY);
	if (cpfi->fd < 0) {
		CC_ERR("%s, %s\n", cpfi->upfile, strerror(errno));
		ret = CPE_NONEXIST;
	} else if (cp_file_check(cpfi->file)) {
		CC_ERR("%s\n", cpfi->file);
		ret = CPE_INPUT;
	} else if (fstat(cpfi->fd, &st) < 0) {
		ret = CPE_FAIL;
		CC_ERR("fstat %s\n", strerror(errno));
	} else if (lseek(cpfi->fd, cpfi->rcv_size, SEEK_SET) < 0) {
		ret = CPE_FAIL;
		CC_ERR("lseek %s\n", strerror(errno));
	} else if (cc_md5(cpfi->file, md5) < 0) {
		ret = CPE_FAIL;
		CC_ERR("md5 fail\n");
	} else {
		for (i = 0; i < 16; i++) {
			snprintf(&cpfi->md5[i*2],
				sizeof(cpfi->md5) - i*2, "%02X", md5[i]);
		}
		cpfi->size = (unsigned long long)st.st_size;
	}

	CC_PUSH2(buf, 0, 51);
	CC_PUSH2(buf, 2, CPO_ACK_START_SYNC_FROM_ROUTER);
	CC_PUSH2(buf, 4, ret);
	CC_PUSH4(buf, 6, cpfi->fileid);
	CC_PUSH1(buf, 10, 32);
	CC_PUSH_LEN(buf, 11, cpfi->md5, 32);
	CC_PUSH8(buf, 43, cpfi->size);
	CC_DBG("ret:%d, %llu,%llu, %s\n", ret,
		cpfi->size, cpfi->rcv_size, cpfi->file);
	if (cp_transfer_send(cpi, buf, 51)) {
		CC_ERR("send err\n");
		return CPO_EXIT;
	}
	return ret ? CPO_EXIT : CPO_ROUTE_TO_SERVER;
}

#define CP_SEND_MX  60*1024
static int cp_file_send(struct cloud_private_info *cpi)
{
	unsigned char *buf = NULL;
	struct cp_file_info *cpfi = &cpi->cpfi;
	int size = 0;

	if (cpfi->rcv_size <= cpfi->size) {
		size = N_MIN(cpfi->size - cpfi->rcv_size, CP_SEND_MX);
	} else {
		CC_ERR("size err:%llu,%llu\n", cpfi->size, cpfi->rcv_size);
		return CPO_EXIT;
	}

	buf = malloc(CP_SEND_MX + 12);
	if (!buf) {
		CC_ERR("malloc fail\n");
		return CPO_EXIT;
	}
	CC_DBG("size:%d\n", size);
	size = cc_read(cpfi->fd, &buf[12], CP_SEND_MX);
	CC_DBG("read:%d\n", size);
	if (size) {
		CC_PUSH2(buf, 0, size + 12);
		CC_PUSH2(buf, 2, CPO_NTF_SYNC_FILE);
		CC_PUSH4(buf, 4, cpfi->fileid);
		CC_PUSH4(buf, 8, size);
		if (cp_transfer_send(cpi, buf, size + 12)) {
			CC_ERR("fail, %d\n", CPO_NTF_SYNC_FILE);
			free(buf);
			return CPO_EXIT;
		}
		cpfi->rcv_size += size;
		free(buf);
		return CPO_ACK_SYNC_FILE;
	} else {
		CC_PUSH2(buf, 0, 8);
		CC_PUSH2(buf, 2, CPO_NTF_SYNC_OVER);
		CC_PUSH4(buf, 4, cpfi->fileid);
		if (cp_transfer_send(cpi, buf, 8)) {
			CC_ERR("fail, %d\n", CPO_NTF_SYNC_OVER);
			free(buf);
			return CPO_EXIT;
		}
		free(buf);
		return CPO_ACK_SYNC_OVER;
	}
}

static int cp_send_ack(struct cloud_private_info *cpi)
{
	unsigned char *buf = &cpi->data[14];
	unsigned short order_len = CC_PULL2(buf, 0);

	if (order_len < 14) {
		CC_ERR("%d\n", order_len);
		return -1;
	}
	if (CC_PULL2(buf, 4)) {
		CC_ERR("%d\n", CC_PULL2(buf, 4));
		return -1;
	}
	if (CC_PULL4(buf, 6) != cpi->cpfi.fileid) {
		CC_ERR("%d,%d\n", CC_PULL4(buf, 6), cpi->cpfi.fileid);
		return -1;
	}
	return 0;
}

static int cp_send_over_ack(struct cloud_private_info *cpi)
{
	unsigned char *buf = &cpi->data[14];
	unsigned short order_len = CC_PULL2(buf, 0);

	if (order_len < 10) {
		CC_ERR("%d\n", order_len);
		return -1;
	}
	if (CC_PULL2(buf, 4)) {
		CC_ERR("%d\n", CC_PULL2(buf, 4));
		return -1;
	}
	if (CC_PULL4(buf, 6) != cpi->cpfi.fileid) {
		CC_ERR("%d,%d\n", CC_PULL4(buf, 6), cpi->cpfi.fileid);
		return -1;
	}
	return 0;
}

static int cp_file_r2s(struct cloud_private_info *cpi)
{
	int order = 0, wait_order = 0, send = 1;

	while (1) {
		if (send) {
			wait_order = cp_file_send(cpi);
			if (wait_order == CPO_EXIT)
				break;
		}
		send = 1;
		cp_free(cpi);
		order = cp_wait(cpi, 60);
		CC_DBG("order:%d, len:%d\n", order, cpi->len);
		if (order == CPO_EXIT) {
			break;
		} else if (order == CPO_NTF_ROUTER_TRANSFER) {
			order = cp_transfer(cpi);
			CC_DBG("order:%d, %d\n", order, wait_order);
			if (wait_order != order) {
				break;
			} else if (order == CPO_ACK_SYNC_FILE) {
				if (cp_send_ack(cpi))
					break;
			} else if (order == CPO_ACK_SYNC_OVER) {
				if (cp_send_over_ack(cpi))
					break;
				if (cpi->cpfi.fd > 0)
					close(cpi->cpfi.fd);
				memset(&cpi->cpfi, 0, sizeof(cpi->cpfi));
				return CPO_FREE;
			} else {
				break;
			}
		} else if (order == CPO_ACK_ROUTER_TRANSFER) {
			send = 0;
		} else {
			CC_ERR("order err:%d\n", order);
			break;
		}
	}
	return CPO_EXIT;
}

static int cp_run(struct cloud_private_info *cpi)
{
	int msg;

	CC_DBG("%s:%d, usrid:%u, key:%X\n",
		inet_ntoa(cpi->addr), cpi->port,
		cpi->usrid, cpi->key);

	msg = cp_connect(cpi);
	while (1) {
		switch (msg) {
		case CPO_WAIT:
			msg = cp_wait(cpi, 60);
			break;
		case CPO_FREE:
			msg = cp_free(cpi);
			break;
		case CPO_REQ_ROUTER_KEEP:
			msg = cp_keep(cpi);
			break;
		case CPO_ACK_ROUTER_KEEP:
			msg = cp_keep_ack(cpi);
			break;
		case CPO_NTF_ROUTER_TRANSFER:
			msg = cp_transfer(cpi);
			break;
		case CPO_REQ_START_SYNC_TO_ROUTER:
			msg = cp_file_s2r_req(cpi);
			break;
		case CPO_ACK_START_SYNC_TO_ROUTER:
			msg = cp_file_s2r_ack(cpi);
			break;
		case CPO_SERVER_TO_ROUTE:
			msg = cp_file_s2r(cpi);
			break;
		case CPO_REQ_START_SYNC_FROM_ROUTER:
			msg = cp_file_r2s_req(cpi);
			break;
		case CPO_ACK_START_SYNC_FROM_ROUTER:
			msg = cp_file_r2s_ack(cpi);
			break;
		case CPO_ROUTE_TO_SERVER:
			msg = cp_file_r2s(cpi);
			break;
		case CPO_EXIT:
			return cp_exit(cpi);
		default:
			CC_ERR("msg nonsupport, %d\n", msg);
			msg = cp_free(cpi);
			break;
		}
	}
	return 0;
}

static struct cc_cp_list *cc_cp_add(
	struct cloud_client_info *cci, unsigned int userid)
{
	int i = 0, j = 0;

	if (!userid)
		return NULL;

	for (j = 0; j < CC_CP_MAX; j++) {
		if (!cci->cp[j].userid)
			break;
	}
	if (j >= CC_CP_MAX)
		return NULL;
	for (i = 0; i < CC_CP_MAX; i++) {
		if (cci->cp[i].userid == userid)
			return NULL;
	}
	cci->cp[j].userid = userid;
	return &cci->cp[j];
}

static void cc_cp_del(
	struct cloud_client_info *cci, pid_t pid)
{
	int i = 0;

	for (i = 0; i < CC_CP_MAX; i++) {
		if (cci->cp[i].pid != pid)
			continue;
		CC_DBG("%d,%d\n", cci->cp[i].userid, cci->cp[i].pid);
		cci->cp[i].pid = 0;
		cci->cp[i].userid = 0;
		break;
	}
}

static int cp_redirect(struct cloud_client_info *cci)
{
	pid_t pid;
	struct cloud_private_info cpi;
	struct cc_cp_list *cccp;

	if (cci->len < 18) {
		CC_ERR("%d\n", cci->len);
		return CSO_XX;
	}
	memset(&cpi, 0, sizeof(cpi));
	cpi.addr.s_addr = htonl(CC_PULL4(cci->data, 4));
	cpi.port = CC_PULL2(cci->data, 8);
	cpi.key = CC_PULL4(cci->data, 10)^CP_CRC;
	cpi.usrid = CC_PULL4(cci->data, 14);
	cpi.id = 1;

	cccp = cc_cp_add(cci, cpi.usrid);
	if (!cccp) {
		CC_DBG("cccp fail:%d\n", cpi.usrid);
		return CSO_XX;
	}

	pid = fork();
	if (pid < 0) {
		cccp->userid = 0;
		CC_ERR("fork fail, %s\n", strerror(errno));
		return CSO_XX;
	} else if (pid > 0) {
		cccp->pid = pid;
		CC_DBG("%d,%d\n", pid, cpi.usrid);
		return CSO_XX;
	} else {
		exit(cp_run(&cpi));
	}
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
	case CSO_NTF_ROUTER_VERSION:
		return cc_upgrade(cci);
	case CSO_ACK_ROUTER_RECORD:
		return cc_history(cci);
	case CSO_ACK_ROUTER_RESET:
		return cc_reset(cci);
	case CSO_ACK_SITE_CUSTOM:
		return cc_custom_url(cci);
	case CSO_ACK_SITE_RECOMMAND:
		return cc_recommand_url(cci);
	case CSO_ACK_VISITOR_MAC:
		return cc_visitor_mac(cci);
	case CSO_ACK_ROUTER_SSID:
		return cc_ssid_ack(cci);
	case CSO_ACK_ROUTER_PRICE:
		return cc_price(cci);
	case CSO_ACK_ROUTER_CONFIG:
		return cc_config(cci);
	case CSO_ACK_CONFIG_VER:
		return cc_config_ver(cci);
	case CSO_ACK_AREA_CONFIG:
		return cc_alone(cci);
	case CSO_ACK_AREA_VER:
		return cc_alone_ver(cci);
	case CSO_NTF_SWITCH_STATUS:
		return cc_switch(cci);
	case CSO_NTF_PPPOE_ACTION:
		return cc_tspeed(cci);
	case CSO_NTF_PLUGIN_INFO:
		return cc_plug_info(cci);
	case CPO_NTF_ROUTER_REDIRECT:
		return cp_redirect(cci);
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

static void cc_cache(struct cloud_client_info *cci)
{
	int i, len;
	unsigned char *buf;

	buf = malloc(CC_BUF_MAX);
	if (!buf) {
		CC_ERR("malloc fail,%d\n", CC_BUF_MAX);
		return;
	}
	for (i = 0; i < 5; i++) {
		len = CC_MSG_DUMP(buf, CC_BUF_MAX);
		if (len < 8 || len > CC_BUF_MAX)
			break;
		if (len != CC_PULL2(buf, 0)) {
			CC_ERR("cache err, %d, %d, %d\n",
				len, CC_PULL2(buf, 0), CC_PULL2(buf, 2));
			break;
		}
		CC_PUSH4(buf, 4, cci->key);
		cc_send(cci->sock, buf, len);
	}
	free(buf);
}

static void cc_child(struct cloud_client_info *cci)
{
	pid_t pid;
	int status;

	pid = waitpid(-1, &status, WNOHANG);
	if (pid <= 0)
		return;
	cc_cp_del(cci, pid);

	if (cci->upgrade_pid == pid) {
		CC_DBG("upgrade exit, %d\n", pid);
		cci->upgrade_pid = -1;
	}
}

static void cc_check_upgrade(struct cloud_client_info *cci)
{
	int nr;
	struct host_info *hi;
	long now;
	static long time = 0;

	if (!igd_test_bit(CCF_UPGRADE, cci->flag))
		return;

	if (cci->upgrade_pid > 0)
		return;

	now = sys_uptime();
	if (now > (time + 2*60))
		time = now;
	else
		return;

	hi = dump_host_alive(&nr);
	if (hi) {
		free(hi);
		CC_DBG("nr:%d\n", nr);
		return;
	}
	CC_DBG("no host upgrade\n");

	cci->upgrade_pid = cc_upgrade_fork(cci->up_url, cci->up_md5);
	if (cci->upgrade_pid > 0) {
		igd_clear_bit(CCF_UPGRADE, cci->flag);
		if (cci->up_md5) {
			free(cci->up_md5);
			cci->up_md5 = NULL;
		}
		if (cci->up_url) {
			free(cci->up_url);
			cci->up_url = NULL;
		}
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
		if (!cc_devid(&cci->devid))
			break;
		CC_ERR("read devid fail\n");
		sleep(6);
	}

	if (get_sysflag(SYSFLAG_RECOVER) == 1) {
		cci->userid = 2;
		igd_set_bit(CCF_RESET, cci->flag);
		igd_log("/tmp/reset_log", "Software reset\n");
	} else if (get_sysflag(SYSFLAG_RESET) == 1) {
		cci->userid = 1;
		igd_set_bit(CCF_RESET, cci->flag);
		igd_log("/tmp/reset_log", "Hardware reset\n");
	} else {
		cci->userid = get_usrid();
	}
	cc_flashid(&cci->devid, 1);

	CC_DBG("%u,%u\n", cci->devid, cci->userid);
}

#ifdef WIAIR_SDK
static void check_live(void *data)
{
	struct nlk_msg_comm nlk;
	unsigned char buf[AES_BLOCK_SIZE] = {0x11, 0x22, 0x33, 0x44, };
	unsigned char out[AES_BLOCK_SIZE];
	aes_encrypt_ctx en_ctx[1];
	uint32_t rand;
	struct timespec now;
	char pwd[16] = {0xfc, 0xec, 0x03, 0xcc,
	       	 	0x5c, 0x77, 0xde, 0x75,
			0x80, 0x35, 0x79, 0x7c};

	clock_gettime(CLOCK_MONOTONIC, &now);
	rand = (uint32_t)lrand48();
	memcpy(&buf[4], &rand, sizeof(rand));
	*((uint64_t *)&buf[8]) = now.tv_sec;

	memset(&nlk, 0, sizeof(nlk));
	nlk.key = 0;
	nlk.action = NLK_ACTION_ADD;

	aes_encrypt_key128(pwd, en_ctx);
	aes_ecb_encrypt(buf, out, AES_BLOCK_SIZE, en_ctx);
	nlk_data_send(NLK_MSG_END-1, &nlk,
		       	sizeof(nlk), out, sizeof(out));
	return ;
}
#endif

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
		cc_cache(&cci);
		cc_child(&cci);
		cc_check_upgrade(&cci);
		cnt++;
#ifdef WIAIR_SDK
		if (!(cnt % 300)) 
			check_live(NULL);
#endif
	}
	return 0;
}
