#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <linux/if_ether.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/file.h>
#include "igd_share.h"
#include "host_type.h"
#include "nlk_ipc.h"
#include "ipc_msg.h"
#include "igd_md5.h"
#include "linux_list.h"
#include "igd_host.h"
#include "igd_cloud.h"
#include "igd_system.h"
#include "igd_qos.h"
#include "igd_plug.h"
#include "igd_advert.h"

#define CE_DBG_FILE   "/tmp/ce_dbg"
#define CE_ERR_FILE   "/tmp/ce_err"

#ifdef FLASH_4_RAM_32
#define CE_DBG(fmt,args...) do{}while(0)
#else
#define CE_DBG(fmt,args...) \
	igd_log(CE_DBG_FILE, "DBG:[%05d],%s():"fmt, __LINE__, __FUNCTION__, ##args)
#endif

#define CE_ERR(fmt,args...) \
	igd_log(CE_ERR_FILE, "ERR:[%05d],%s():"fmt, __LINE__, __FUNCTION__, ##args)

#define CLOUD_UPLOAD_PWD  "cqm#yg$ysd%ss@oy"

#define DNS_UPLOAD_FILE "/tmp/dns_upload_file"
#define DNS_UPLOAD_GZ_FILE DNS_UPLOAD_FILE".gz"
#define DNS_URL_FILE   "/tmp/dns_url"

char *cloud_read_file(char *file, int *file_len)
{
	char *buf = NULL;
	int fd = -1, len = 0;
	struct stat statbuf;

	if ((fd = open(file, O_RDONLY)) < 0) {
		CE_ERR("open fail, %s,%s\n", file, strerror(errno));
		goto err;
	}

	if (fstat(fd, &statbuf) < 0) {
		CE_ERR("fstat fail, %s,%s\n", file, strerror(errno));
		goto err;
	}

	len = statbuf.st_size;
	buf = (char *)malloc(len + 1);
	if (!buf) {
		CE_ERR("malloc fail, len:%d,%s\n", len, strerror(errno));
		goto err;
	}

	memset(buf, 0, len + 1);
	lseek(fd, 0, SEEK_SET);
	if (read(fd, buf, len) != len) {
		CE_ERR("read fail, len:%d,%s\n", len, strerror(errno));
		goto err;
	}

	close(fd);
	*file_len = len;
	return buf;
err:
	*file_len = 0;
	if (fd > 0)
		close(fd);
	if (buf)
		free(buf);
	return NULL;
}

char *cloud_wget_file(char *url, char *file, int *file_len)
{
	char *buf = NULL;
	char cmd[256] = {0,};

	snprintf(cmd, sizeof(cmd) - 1, "wget -q %s -O %s -T 300", url, file);
	if (system(cmd))
		goto err;

	buf = cloud_read_file(file, file_len);
	if (!buf || !file_len)
		goto err;

	return buf;
err:
	*file_len = 0;
	snprintf(cmd, sizeof(cmd) - 1, "rm -rf %s", file);
	system(cmd);
	if (buf)
		free(buf);
	return NULL;
}

int cloud_upload_connect(const char *url, const char *port)
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
		CE_DBG("connect fail, %s\n", strerror(errno));
		close(sockfd);
		sockfd = -1;
	}
	freeaddrinfo(iplist);
	return sockfd;
}

static int cloud_upload_send(char *dns, char *port, char *url, char *data, int data_len)
{
	char *buf;
	int i, sock, len, ret;

	for (i = 0; i < 3; i++) {
		sock = cloud_upload_connect(dns, port ? port : "80");
		if (sock > 0)
			break;
		sleep(1);
	}
	if (i >= 3) {
		CE_DBG("connect fail, %s,%s\n", dns, url);
		return -1;
	}

	buf = malloc(data_len + 512);
	if (!buf) {
		CE_ERR("malloc fail, %d\n", data_len);
		goto err;
	}
	memset(buf, 0, data_len + 512);

	len = snprintf(buf, 512, 
		"POST %s HTTP/1.1\r\n"
		"User-Agent: wiair\r\n"
		"Accept: */*\r\n"
		"Host: %s\r\n"
		"Connection: close\r\n"
		"Content-Type: application/octet-stream\r\n"
		"Content-Length: %d\r\n\r\n",
		url, dns, data_len);
	CE_DBG("len:%d, data_len:%d\n", len, data_len);
	if (data_len > 0) {
		memcpy(buf + len, data, data_len);
		len += data_len;
	}

	i = 0;
	while (i < len) {
		ret = send(sock, buf + i, len - i, 0);
		if (ret <= 0) {
			CE_DBG("send fail, %s\n", strerror(errno));
			goto err;
		}
		i += ret;
	}

err:
	if (sock > 0)
		close(sock);
	if (buf)
		free(buf);
	return 0;
}

static long cloud_file_size(char *file)
{
	int fd;
	struct stat st;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		return -1;
	if (fstat(fd, &st) < 0) {
		CE_ERR("fstat fail, %s,%s\n", file, strerror(errno));
		st.st_size = -1;
	}
	close(fd);
	return (long)st.st_size;
}

static int ce_unlock(int fd, char *name)
{
	char file[128];

	snprintf(file, sizeof(file),
		"/tmp/ce_lock.%s", name);

	flock(fd, LOCK_UN);
	close(fd);
	remove(file);
	return 0;
}

static int ce_lock(char *name)
{
	int fd = 0;
	char file[128];

	snprintf(file, sizeof(file),
		"/tmp/ce_lock.%s", name);
	fd = open(file, O_CREAT | O_WRONLY, 0777);
	if (fd < 0)
		return -1;
	if (flock(fd, LOCK_EX | LOCK_NB)) {
		close(fd);
		return -2;
	}
	return fd;
}

int n_md5(char *file, void *md5)
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

#define CLOUD_COMMON "-------------above is cloud common function-----------------------"

struct rconf_info {
	char flag[32];
	int new_ver;
	int old_ver;
	char info[256];
	char name[32];
	char nosave;
};

static void rconf_init(struct rconf_info *cci)
{
	memset(cci, 0, sizeof(*cci)*CCT_MAX);
	arr_strcpy(cci[CCT_L7].flag, RCONF_FLAG_L7);
	arr_strcpy(cci[CCT_L7].name, "l7.tar.gz");
	cci[CCT_L7].nosave = 1;

	arr_strcpy(cci[CCT_UA].flag, RCONF_FLAG_UA);
	arr_strcpy(cci[CCT_UA].name, "UA.txt");

	arr_strcpy(cci[CCT_DOMAIN].flag, RCONF_FLAG_DOMAIN);
	arr_strcpy(cci[CCT_DOMAIN].name, "domain.txt");

	arr_strcpy(cci[CCT_WHITE].flag, RCONF_FLAG_WHITE);
	arr_strcpy(cci[CCT_WHITE].name, "white.txt");

	arr_strcpy(cci[CCT_VENDER].flag, RCONF_FLAG_VENDER);
	arr_strcpy(cci[CCT_VENDER].name, "vender.txt");

	arr_strcpy(cci[CCT_TSPEED].flag, RCONF_FLAG_TSPEED);
	arr_strcpy(cci[CCT_TSPEED].name, "tspeed.txt");

	arr_strcpy(cci[CCT_STUDY_URL].flag, RCONF_FLAG_STUDY_URL);
	arr_strcpy(cci[CCT_STUDY_URL].name, "study_url.txt");

	arr_strcpy(cci[CCT_VPN_DNS].flag, RCONF_FLAG_VPNDNS);
	arr_strcpy(cci[CCT_VPN_DNS].name, "vpndns.txt");

	arr_strcpy(cci[CCT_ERRLOG].flag, RCONF_FLAG_ERRLOG);
	arr_strcpy(cci[CCT_URLLOG].flag, RCONF_FLAG_URLLOG);
	arr_strcpy(cci[CCT_URLSAFE].flag, RCONF_FLAG_URLSAFE);
	arr_strcpy(cci[CCT_URLSTUDY].flag, RCONF_FLAG_URLSTUDY);
	arr_strcpy(cci[CCT_URLHOST].flag, RCONF_FLAG_URLHOST);
	arr_strcpy(cci[CCT_URLREDIRECT].flag, RCONF_FLAG_URLREDIRECT);
	arr_strcpy(cci[CCT_CHECKUP].flag, RCONF_FLAG_CHECKUP);
	arr_strcpy(cci[CCT_VPN_SERVEER].flag, RCONF_FLAG_VPNSERVER);
}

static int rconf_read(
	char *file, char *ver, int ver_len,
	struct rconf_info *cci, int new)
{
	FILE *fp;
	char buff[512], *ptr;
	int i = 0, len = 0, config_ver = 0;

	fp = fopen(file, "rb");
	if (!fp) {
		CE_DBG("fopen fail, %s\n", file);
		return -1;
	}

	while (1) {
		memset(buff, 0, sizeof(buff));
		if (!fgets(buff, sizeof(buff) - 1, fp))
			break;
		len = strlen(buff);
		while ((len > 0) && 
			((buff[len - 1] == '\r')
			|| (buff[len - 1] == '\n'))) {
			buff[len - 1] = '\0';
			len--;
		}

		if (len <= 0) {
			CE_ERR("str len err, [%s]\n", buff);
			break;
		}

		if (!memcmp(buff, "VER:", 4)) {
			strncpy(ver, buff + 4, ver_len - 1);
			CE_DBG("config ver:%s\n", ver);
			continue;
		}

		for (i = 0; i < CCT_MAX; i++) {
			len = strlen(cci[i].flag);
			if (memcmp(buff, cci[i].flag, len))
				continue;
			config_ver = atoi(buff + len);
			if (config_ver < 0) {
				config_ver = 0;
				CE_ERR("ver err, [%s]:[%d]\n", cci[i].flag, config_ver);
			}
			if (new) {
				cci[i].new_ver = config_ver;
				ptr = strchr(buff + len, ',');
				if (ptr) {
					arr_strcpy(cci[i].info, ptr + 1);
				} else {
					CE_ERR("info err, %s\n", buff);
				}
				CE_DBG("new ver:[%s],[%d],[%s]\n", cci[i].flag, config_ver, cci[i].info);
			} else {
				cci[i].old_ver = config_ver;
				CE_DBG("old ver:[%s],[%d]\n", cci[i].flag, config_ver);
			}
			break;
		}
	}
	fclose(fp);
	return 0;
}

static int rconf_down(struct rconf_info *cci, int save)
{
	int i, buf_len;
	unsigned char hexmd5[64];
	char *buf, cmd[256], strmd5[64], *url, *md5;

	if (!cci->name[0])
		return 0;

	md5 = cci->info;
	url = strchr(cci->info, ',');
	if (!url) {
		CE_ERR("info err, %s, %s\n", cci->flag, cci->info);
		return -1;
	}
	*url = '\0';
	url++;

	if (access(RCONF_DIR_NEW, F_OK)) {
		if (mkdir(RCONF_DIR_NEW, 0777)) {
			CE_ERR("mkdir [%s] fail, %s\n", RCONF_DIR_NEW, strerror(errno));
			return -1;
		}
	}

	snprintf(cmd, sizeof(cmd), "%s/%s", RCONF_DIR_NEW, cci->name);
	for (i = 0; i < RCONF_RETRY_NUM; i++) {
		remove(cmd);
		buf = cloud_wget_file(url, cmd, &buf_len);
		if (buf)
			break;
		sleep(30);
	}

	if (!buf) {
		remove(cmd);
		CE_DBG("wget fail, %d\n", cci->flag);
		return -1;
	}

	get_md5_numbers((unsigned char *)buf, hexmd5, buf_len);
	for (i = 0; i < 16; i++)
		sprintf(&strmd5[i*2], "%02X", hexmd5[i]);

	if (strncasecmp(strmd5, md5, 32)) {
		free(buf);
		remove(cmd);
		CE_ERR("URL:[%s], MD5ERR:\n[%s],[%s]\n", url, strmd5, md5);
		return -1;
	} else {
		CE_DBG("MD5:\n[%s]\n[%s]\n", strmd5, md5);
	}
	free(buf);

	snprintf(cmd, sizeof(cmd), "mv %s/%s %s/%s",
		RCONF_DIR_NEW, cci->name, RCONF_DIR_TMP, cci->name);
	system(cmd);

#ifndef FLASH_4_RAM_32
	if (save) {
		snprintf(cmd, sizeof(cmd), "cp -rf %s/%s %s/%s",
			RCONF_DIR_TMP, cci->name, RCONF_DIR, cci->name);
		system(cmd);
	}
#endif

	if (*(url - 1) == '\0')
		*(url - 1) = ',';
	return 0;
}

static int rconf_save(
	char *file, char *ver, struct rconf_info *cci)
{
	int i;
	FILE *fp;

	fp = fopen(file, "wb");
	if (!fp) {
		CE_DBG("fopen fail, %s\n", file);
		return -1;
	}

	fprintf(fp, "VER:%s\n", ver);
	for (i = 0; i < CCT_MAX; i++) {
		fprintf(fp, "%s%d", cci[i].flag, cci[i].old_ver);
		if (i >= CCT_ERRLOG || cci[i].nosave) {
			fprintf(fp, ",%s\n", cci[i].info);
		} else {
			fprintf(fp, "\n");
		}
	}

	fclose(fp);
	return 0;
}

static int rconf_run(char *url)
{
	int i, flag, bit[2], first;
	char old_ver[32], new_ver[32], cmd[256];
	struct rconf_info cci[CCT_MAX];
	struct nlk_cloud_config nlk;

	if (!url) {
		CE_ERR("input err\n");
		return -1;
	}

	CE_DBG("url:%s\n", url);

	while (sys_uptime() < 120)
		sleep(10);

	bit[0] = ICFT_UP_FIRST_RCONF;
	bit[1] = NLK_ACTION_DUMP;
	first = mu_msg(IGD_CLOUD_FLAG, bit, sizeof(bit), NULL, 0);
	first = (first <= 0) ? 1 : 0;

	memset(&nlk, 0, sizeof(nlk));
	memset(old_ver, 0, sizeof(old_ver));
	memset(new_ver, 0, sizeof(new_ver));
	rconf_init(cci);

	rconf_read(RCONF_CHECK,
			old_ver, sizeof(old_ver), cci, 0);

	snprintf(cmd, sizeof(cmd) - 1, 
		"wget -q %s -O %s -T 300", url, RCONF_CHECK_TMP);
	for (i = 0; i < RCONF_RETRY_NUM; i++) {
		remove(RCONF_CHECK_TMP);
		system(cmd);
		if (!rconf_read(RCONF_CHECK_TMP,
				new_ver, sizeof(new_ver), cci, 1)) {
			break;
		}
		sleep(30);
	}

	if (i >= RCONF_RETRY_NUM || !new_ver[0]) {
		remove(RCONF_CHECK_TMP);
		CE_ERR("wget fail, %s\n", cmd);
		return -1;
	}

	flag = 0;
	for (i = 0; i < CCT_MAX; i++) {
		if (!cci[i].new_ver)
			continue;
		else if (cci[i].nosave && first)
			CE_DBG("first down nosave\n");
		else if (cci[i].old_ver == cci[i].new_ver)
			continue;
		CE_DBG("UPDATE:[%s],[%d],[%d]\n",
			cci[i].flag, cci[i].old_ver, cci[i].new_ver);
		if (rconf_down(&cci[i], !cci[i].nosave)) {
			flag = 1;
			continue;
		}
		nlk.ver[i] = 1;
		cci[i].old_ver = cci[i].new_ver;
	}

	rconf_save(RCONF_CHECK, flag ? old_ver : new_ver, cci);
	remove(RCONF_CHECK_TMP);

	nlk.comm.gid = NLKMSG_GRP_SYS;
	nlk.comm.mid = SYS_GRP_MID_CONF;
	nlk_event_send(NLKMSG_GRP_SYS, &nlk, sizeof(nlk));

	if (flag)
		return -1;

	bit[0] = ICFT_UP_RCONF_VER;
	bit[1] = NLK_ACTION_ADD;
	mu_msg(IGD_CLOUD_FLAG, bit, sizeof(bit), NULL, 0);

	bit[0] = ICFT_UP_FIRST_RCONF;
	bit[1] = NLK_ACTION_ADD;
	mu_msg(IGD_CLOUD_FLAG, bit, sizeof(bit), NULL, 0);
	return 0;
}

static int rconf_flag(char *flag)
{
	int i, j, f = 0;
	struct rconf_info cci[CCT_MAX];
	struct nlk_cloud_config nlk;

	CE_DBG("flag:%s\n", flag);

	memset(&nlk, 0, sizeof(nlk));
	rconf_init(cci);
	for (i = 0; i < CCT_MAX; i++) {
		if (strncmp(cci[i].flag, flag, sizeof(cci[i].flag)))
			continue;
		if (CC_MSG_RCONF_INFO(cci[i].flag, cci[i].info, sizeof(cci[i].info)) < 0) {
			CE_DBG("get flag fail, %s\n", cci[i].flag);
			continue;
		}
		if (!strchr(cci[i].info, ',')) {
			CE_DBG("flag err, %s\n", cci[i].flag);
			continue;
		}
		for (j = 0; j < 3; j++) {
			if (!rconf_down(&cci[i], 0)) {
				f = 1;
				nlk.ver[i] = 1;
				break;
			}
			sleep(10);
		}
	}

	if (!f)
		return 0;

	nlk.comm.gid = NLKMSG_GRP_SYS;
	nlk.comm.mid = SYS_GRP_MID_CONF;
	nlk_event_send(NLKMSG_GRP_SYS, &nlk, sizeof(nlk));
	return 0;
}

#define RCONF_UPDATE_ALL  1
#define RCONF_UPDATE_FLAG  2

static int rconf_update(char *url, int flag)
{
	pid_t pid = getpid();
	unsigned char time = 66;

	while (time-- > 0) {
		if (!mu_msg(IGD_CLOUD_RCONF_FLAG,
				&pid, sizeof(pid), NULL, 0)) {
			break;
		}
		CE_DBG("is running\n");
		sleep(6);
	}

	if (time <= 0) {
		CE_ERR("is busying, %s\n", url);
		return 0;
	}

	if (flag == RCONF_UPDATE_ALL)
		rconf_run(url);
	else if (flag == RCONF_UPDATE_FLAG)
		rconf_flag(url);

	pid = 0;
	mu_msg(IGD_CLOUD_RCONF_FLAG,
		&pid, sizeof(pid), NULL, 0);
	return 0;
}

static int read_alone(int flag, char *val, int val_len)
{
	int fd, ret;
	char file[64];

	snprintf(file, sizeof(file), "%s/%d:", ALONE_DIR, flag);
	fd = open(file, O_RDONLY);
	if (fd < 0)
		return 0;
	ret = read(fd, val, val_len - 1);
	if (ret < 0)
		goto err;
	val[ret] = 0;
err:
	close(fd);
	return (ret < 0) ? 0 : ret;
}

static int write_alone(int flag, char *ver, int ver_len)
{
	FILE *fp;
	char file[64];

	snprintf(file, sizeof(file), "%s/%d", ALONE_DIR, flag);
	fp = fopen(file, "wb");
	if (!fp) {
		CE_ERR("fopen fail\n");
		return -1;
	}
	fprintf(fp, "%s", ver);
	fclose(fp);
	return 0;
}

static void msg_alone(int flag)
{
	struct nlk_alone_config nlk;

	nlk.comm.gid = NLKMSG_GRP_SYS;
	nlk.comm.mid = SYS_GRP_MID_ALONE;
	nlk.flag = flag;
	nlk_event_send(NLKMSG_GRP_SYS, &nlk, sizeof(nlk));
}

static int alone_update(char *argv[])
{
	int flag, i, bit[2];
	char old_ver[32], ver[32];
	struct rconf_info ri;

	if (!argv[2] || !argv[3] 
		|| !argv[4] || !argv[5]) {
		CE_ERR("input err\n");
		return -1;
	}
	flag = atoi(argv[2]);
	arr_strcpy(ver, argv[3]);

	if (strcmp(ver, "0")
		&& read_alone(flag, old_ver, sizeof(old_ver))
		&& !strcmp(old_ver, ver)) {
		CE_DBG("%d,%s,%s, same\n", flag, old_ver, ver);
		return 0;
	}

	bit[0] = flag;
	bit[1] = NLK_ACTION_DUMP;
	if (mu_msg(IGD_CLOUD_ALONE_LOCK, bit, sizeof(bit), NULL, 0)) {
		CE_DBG("%d, lock\n", flag);
		return 0;
	}

	bit[0] = flag;
	bit[1] = NLK_ACTION_ADD;
	if (mu_msg(IGD_CLOUD_ALONE_LOCK, bit, sizeof(bit), NULL, 0)) {
		CE_DBG("%d, add lock fail\n", flag);
		goto out;
	}

	snprintf(ri.flag, sizeof(ri.flag), "alone_%d", flag);

	switch (flag) {
	case CCA_JS:
		arr_strcpy(ri.name, "advert.txt");
		break;
	case CCA_P_L7:
		arr_strcpy(ri.name, "p_l7.tar.gz");
		break;
	case CCA_SYSTEM:
		arr_strcpy(ri.name, "system.txt");
		break;
	default:
		CE_ERR("nonsupport:%d\n", flag);
		goto out;
	}

	for (i = 0; i < 3; i++) {
		snprintf(ri.info, sizeof(ri.info), "%s,%s", argv[4], argv[5]);

		if (rconf_down(&ri, 0)) {
			sleep(10);
			continue;
		}

		if (strcmp(ver, "0") &&
			!write_alone(flag, ver, sizeof(ver))
			&& (flag != 3)) {
			bit[0] = flag;
			bit[1] = NLK_ACTION_ADD;
			if (mu_msg(IGD_CLOUD_ALONE_VER, bit, sizeof(bit), NULL, 0))
				CE_DBG("%d, add ver fail\n", flag);
		}
		msg_alone(flag);
		break;
	}

out:
	bit[0] = flag;
	bit[1] = NLK_ACTION_DEL;
	if (mu_msg(IGD_CLOUD_ALONE_LOCK, bit, sizeof(bit), NULL, 0)) {
		CE_DBG("%d, del lock fail\n", flag);
		return -1;
	}
	return 0;
}

#define CLOUD_RCONF_FUNCTION "-------------above is rconf function-----------------------"

#define ERRLOG_URL   "/Defect?f=%d"
#define ERRLOG_TMP   "/tmp/err_file"
#define ERRLOG_TMP_GZIP   ERRLOG_TMP".gz"
#define ERRLOG_LEN_MAX   (300*1024)

struct errlog_list {
	char id;
	char del;
	char *name;
};

static int errlog_send(
	int id, char *name, char *dns, unsigned int devid,
	char *ver, char *vendor, char *product, char *date, char *svn)
{
	FILE *fp;
	time_t now;
	struct tm *tm;
	char url[128], cmd[32], *out = NULL;
	int len = 0;

	remove(ERRLOG_TMP);
	fp = fopen(ERRLOG_TMP, "w");
	if (!fp) {
		CE_ERR("fopen fail, %s,%s\n", ERRLOG_TMP, strerror(errno));
		return -1;
	}
	time(&now);
	tm = localtime(&now);
	if (!tm) {
		CE_ERR("tm null, %s\n", strerror(errno));
		goto err;
	}
	fprintf(fp, "---%u,%s_%s,%s,%s,%s,%d%02d%02d-%02d%02d%02d\n",
		devid, vendor, product, ver, svn, date, tm->tm_year + 1900,
		tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

	out = cloud_read_file(name, &len);
	if (!out) {
		CE_ERR("read fail, %s\n", name);
		goto err;
	}
	fprintf(fp, "%s\n", out);
	free(out);
	out = NULL;
	fclose(fp);
	fp = NULL;

	remove(ERRLOG_TMP_GZIP);
	snprintf(cmd, sizeof(cmd), "gzip %s", ERRLOG_TMP);
	if (system(cmd)) {
		CE_ERR("gzip fail, %s\n", cmd);
		goto err;
	}

	len = ERRLOG_LEN_MAX;
	out = malloc(len);
	if (!out) {
		CE_ERR("malloc fail\n");
		goto err;
	}

	len = igd_aes_encrypt(ERRLOG_TMP_GZIP, (unsigned char *)out,
		len, (unsigned char *)CLOUD_UPLOAD_PWD);
	if (len <= 0) {
		CE_ERR("encrypt fail, %d\n", len);
		goto err;
	}

	snprintf(url, sizeof(url), ERRLOG_URL, id);
	CE_DBG("%d, %s\n", id, name);
	if (cloud_upload_send(dns, NULL, (char *)url, out, len) < 0) {
		CE_DBG("send fail, %s, %s\n", dns, url);
		goto err;
	}

	free(out);
	remove(ERRLOG_TMP);
	remove(ERRLOG_TMP_GZIP);
	return 0;
err:
	if (out)
		free(out);
	if (fp)
		fclose(fp);
	remove(ERRLOG_TMP);
	remove(ERRLOG_TMP_GZIP);
	return -1;
}

static void errlog_del(int del, char *name)
{
	FILE *fp;

	if (del)
		remove(name);
	else {
		fp = fopen(name, "w");
		if (fp)
			fclose(fp);
		else
			CE_ERR("fopen fail, %s,%s\n", name, strerror(errno));
	}
}

/*
1 crash_log
2 host_err
3 cc_err
4 ce_err
5 mu_err
6 dnsmasq_log
7 pppoe_client_log
8 mtd_err
9 ppp_log
10 dbg_log
11 tmp_log
12 kill_log
13 reset_log
*/
static int errlog_upload(void)
{
	int i = 0;
	char *tmp, dns[64];
	char vendor[64], product[64], version[64], date[64], svn[64];
	unsigned int devid = get_devid();

	struct errlog_list el[] = {
		{1, 1, "/etc/crash"},
		{1, 1, "/tmp/kernel_debug"},
		{2, 1, "/tmp/igd_host_err"},
		{3, 1, "/tmp/cc_err"},
		{4, 1, "/tmp/ce_err"},
		{5, 1, "/tmp/mu.err"},
		{6, 0, "/tmp/dnsmasq_log"},
		{7, 0, "/tmp/pppoe_client_log"},
		{8, 0, "/tmp/mtd_err"},
		{9, 1, "/tmp/ppp_log"},
		{10, 1, "/tmp/dbg_log"},
		{11, 1, "/tmp/tmp_log"},
		{12, 1, "/tmp/kill_log"},
		{13, 1, "/tmp/reset_log"},
		{-1, 0, NULL},
	};
	tmp = read_firmware("CURVER");
	if (!tmp) {
		CE_ERR("ver err\n");
		return -1;
	}
	arr_strcpy(version, tmp);

	tmp = read_firmware("VENDOR");
	if (!tmp) {
		CE_ERR("vendor err\n");
		return -1;
	}
	arr_strcpy(vendor, tmp);

	tmp = read_firmware("PRODUCT");
	if (!tmp) {
		CE_ERR("product err\n");
		return -1;
	}
	arr_strcpy(product, tmp);

	tmp = read_firmware("Revision");
	if (!tmp) {
		CE_ERR("Revision err\n");
		return -1;
	}
	arr_strcpy(svn, tmp);

	tmp = read_firmware("DATE");
	if (!tmp) {
		CE_ERR("date err\n");
		return -1;
	}
	arr_strcpy(date, tmp);

	if (!devid) {
		CE_ERR("devid err\n");
		return -1;
	}

	if (CC_MSG_RCONF_INFO(RCONF_FLAG_ERRLOG, dns, sizeof(dns)) < 0) {
		CE_ERR("get dns fail, %s\n", RCONF_FLAG_ERRLOG);
		return -1;
	}

	for (i = 0; el[i].id != -1; i++) {
		if (cloud_file_size(el[i].name) < 3)
			continue;
		if (errlog_send(el[i].id, el[i].name, dns, devid,
			version, vendor, product, date, svn))
			continue;
		errlog_del(el[i].del, el[i].name);
	}
	return 0;
}

#define CLOUD_ERRLOG_FUNCTION "-------------above is errlog function-----------------------"

#define L7_GR_BYTE_MX 	4
#define L7_GR_PKT_MX	4
struct l7_gr_bytes {
	uint8_t dir;
	uint8_t cnt;
	uint16_t len_min;
	uint16_t len_max;
	struct {
		int8_t offset;
		uint8_t len;
		uint8_t value[4];
	}byte[L7_GR_BYTE_MX];
};

struct l7_gather {
	char url[IGD_DNS_LEN];
	uint32_t net;
	uint32_t dip;
	uint16_t dport;
	uint8_t proto;
	struct l7_gr_bytes bytes[L7_GR_PKT_MX];
};

static struct l7_gather *dump_l7_gather(int *nr)
{
	struct nlk_msg_comm nlk;
	struct l7_gather *gr;
	int size;

	if (!nr) 
		return NULL;
	memset(&nlk, 0x0, sizeof(nlk));
	size = sizeof(struct l7_gather) * 20;
	gr = malloc(size);
	*nr = IGD_ERR_NO_MEM;
	if (!gr)
		return NULL;
	nlk.key = 5;
	nlk.action = NLK_ACTION_DUMP;
	nlk.obj_nr = 20;
	nlk.obj_len = sizeof(struct l7_gather);
	*nr = nlk_dump_from_kernel(NLK_MSG_L7, &nlk,
		       		sizeof(nlk), gr, 20);
	if (*nr > 0)
		return gr;
	free(gr);
	return NULL;
}

static int l7_gather_upload(void)
{
	int nr = 0, i, j, k, n, l;
	struct l7_gather *l7;
	unsigned char buf[1024];

	l7 = dump_l7_gather(&nr);
	if (!l7) {
		CE_DBG("dump l7 is NULL\n");
		return -1;
	}

	CE_DBG("dump l7 cnt:%d\n", nr);
	for (i = 0; i < nr; i++) {
		CC_PUSH2(buf, 2, CSO_NTF_ROUTER_CODE);
		n = 8;
		CC_PUSH2(buf, n, 2); // version
		n += 2;
		l = strlen(l7[i].url);
		CC_PUSH1(buf, n, l);
		n += 1;
		CC_PUSH_LEN(buf, n, l7[i].url, l);
		n += l;
		CC_PUSH4(buf, n, l7[i].net);
		n += 4;
		CC_PUSH4(buf, n, l7[i].dip);
		n += 4;
		CC_PUSH2(buf, n, l7[i].dport);
		n += 2;
		CC_PUSH1(buf, n, l7[i].proto);
		n += 1;
		CC_PUSH1(buf, n, L7_GR_PKT_MX);
		n += 1;
		CE_DBG("1:%s,%u,%u,%u,%u\n", l7[i].url,
			l7[i].net, l7[i].dip, l7[i].dport, l7[i].proto);
		for (j = 0; j < L7_GR_PKT_MX; j++) {
			CE_DBG("2:%u,%u,%u,%u\n", 
				l7[i].bytes[j].dir, l7[i].bytes[j].cnt,
				l7[i].bytes[j].len_min, l7[i].bytes[j].len_max);
			CC_PUSH1(buf, n, l7[i].bytes[j].dir);
			n += 1;
			CC_PUSH1(buf, n, l7[i].bytes[j].cnt);
			n += 1;
			CC_PUSH2(buf, n, l7[i].bytes[j].len_min);
			n += 2;
			CC_PUSH2(buf, n, l7[i].bytes[j].len_max);
			n += 2;
			if (l7[i].bytes[j].cnt > L7_GR_BYTE_MX) {
				CE_ERR("cnt err, %d,%d\n", l7[i].bytes[j].cnt, L7_GR_BYTE_MX);
				goto over;
			}
			for (k = 0; k < l7[i].bytes[j].cnt; k++) {
				CE_DBG("3:%u,%u,0x%02X%02X%02X%02X\n", 
					l7[i].bytes[j].byte[k].offset, l7[i].bytes[j].byte[k].len,
					l7[i].bytes[j].byte[k].value[0], l7[i].bytes[j].byte[k].value[1],
					l7[i].bytes[j].byte[k].value[2], l7[i].bytes[j].byte[k].value[3]);
				CC_PUSH1(buf, n, l7[i].bytes[j].byte[k].offset);
				n += 1;
				CC_PUSH1(buf, n, l7[i].bytes[j].byte[k].len);
				n += 1;
				CC_PUSH_LEN(buf, n, l7[i].bytes[j].byte[k].value, 4);
				n += 4;
			}
		}
		CC_PUSH2(buf, 0, n);
		CC_MSG_ADD(buf, n);
	}

over:
	free(l7);
	return 0;
}

#define CLOUD_L7_FUNCTION "-------------above is l7 function-----------------------"

#define URL_HOST_FILE_MAX   300*1024
#define URL_HOST_TMP_FILE   "/tmp/url_host_info"
#define URL_HOST_TAR_FILE   URL_HOST_TMP_FILE".gz"
#define URL_HOST_URL_FILE   "/tmp/url_host_url"

static int url_host_aes_url(char *url, int ul, char *addr)
{
	int fd, i, l, len;
	char file[64], tmp[128];

	strcpy(file, "/tmp/taesXXXXXX");
	fd = mkstemp(file);
	if (fd < 0) {
		CE_ERR("mkstemp fail, %s\n", strerror(errno));
		return -1;
	}
	snprintf(tmp, sizeof(tmp), "%lu", time(NULL));
	write(fd, tmp, strlen(tmp));
	close(fd);

	len = igd_aes_encrypt(file, (unsigned char *)tmp,
		sizeof(tmp), (unsigned char *)CLOUD_UPLOAD_PWD);

	l = snprintf(url, ul, "%s?t=", addr);
	for (i = 0; i < len; i++) {
		l += snprintf(url + l, ul - l, "%02X", (unsigned char)(tmp[i]));
	}
	remove(file);
	return 0;
}

static int url_host_send(char *file, char *addr)
{
	int len;
	char dns[128], *out;
	unsigned char url[512];

	if (CC_MSG_RCONF_INFO(RCONF_FLAG_URLHOST, dns, sizeof(dns)) < 0) {
		CE_ERR("get up url fail, %s\n", RCONF_FLAG_URLHOST);
		return -1;
	}

	len = URL_HOST_FILE_MAX + 100;
	out = malloc(len);
	if (!out) {
		CE_ERR("malloc fail\n");
		return -1;
	}

	CE_DBG("malloc len:%d\n", len);
	len = igd_aes_encrypt(file, (unsigned char *)out,
		len, (unsigned char *)CLOUD_UPLOAD_PWD);
	if (len <= 0) {
		CE_ERR("encrypt fail, %d\n", len);
		goto err;
	}

	memset(url, 0, sizeof(url));
	if (url_host_aes_url((char *)url, sizeof(url), addr))
		goto err;

	CE_DBG("[%s],[%s],%d\n", dns, url, len);
	if (cloud_upload_send(dns, NULL, (char *)url, out, len) < 0) {
		CE_DBG("send fail, %s\n", dns);
		goto err;
	}
err:
	if (out)
		free(out);
	return 0;
}

//devid mac host uri times seconds done cookie type:ua refer ip ispost:post
static int url_host_upload(void)
{
	int nr, i, fd;
	FILE *fp = NULL;
	unsigned int devid = get_devid();
	struct http_host_log *hhl = NULL;
	char cmd[64];
	struct in_addr ip;
	uint8_t is_post = 0;
	unsigned char *tmp;
	unsigned char *host;

	fd = ce_lock("url_host");
	if (fd < 0) {
		CE_DBG("is uping\n");
		goto end;
	}
	if (!devid) {
		CE_DBG("devid err\n");
		goto end;
	}
	hhl = dump_http_log(&nr);
	if (!hhl) {
		CE_DBG("dump null\n");
		goto end;
	}
	fp = fopen(URL_HOST_TMP_FILE, "wb");
	if (!fp) {
		CE_DBG("fopen fail\n");
		goto end;
	}
	mu_msg(IGD_CLOUD_IP, NULL, 0, &ip, sizeof(ip));
	for (i = 0; i < nr; i++) {
		if (ftell(fp) > URL_HOST_FILE_MAX)
			break;
		fprintf(fp, "%u\1", devid);
		fprintf(fp, "%02X%02X%02X%02X%02X%02X\1", MAC_SPLIT(hhl[i].mac));
		host = hhl[i].buf + hhl[i].key_offset[HTTP_KEY_HOST];
		fprintf(fp, "%s\1", host);
		tmp = hhl[i].buf + hhl[i].key_offset[HTTP_KEY_URI];
		fprintf(fp, "%s\1", tmp);
		fprintf(fp, "1\1");
		fprintf(fp, "%ld\1", hhl[i].seconds);
		fprintf(fp, "%d\1", hhl[i].l7_done);
		tmp = hhl[i].buf + hhl[i].key_offset[HTTP_KEY_COOKIE];
		fprintf(fp, "%s\1", tmp);
		tmp = hhl[i].buf + hhl[i].key_offset[HTTP_KEY_UA];
		fprintf(fp, "%d:%s\1", 0, tmp);
		tmp = hhl[i].buf + hhl[i].key_offset[HTTP_KEY_REFERER];
		fprintf(fp, "%s\1", tmp);
		fprintf(fp, "%s\1", inet_ntoa(ip));
		is_post = hhl[i].is_post ? 1 : 0;
		fprintf(fp, "%hhu:%s\n", is_post, is_post ? hhl[i].post : "");
	}
	fclose(fp);
	fp = NULL;

	remove(URL_HOST_TAR_FILE);
	snprintf(cmd, sizeof(cmd), "gzip %s", URL_HOST_TMP_FILE);
	if (system(cmd)) {
		CE_ERR("gzip fail, %s\n", cmd);
	} else {
		url_host_send(URL_HOST_TAR_FILE, "/UH_1");
	}
	remove(URL_HOST_TMP_FILE);
	remove(URL_HOST_TAR_FILE);

end:
	if (fd > 0)
		ce_unlock(fd, "url_host");
	if (hhl)
		free(hhl);
	if (fp)
		fclose(fp);
	return 0;
}

#define CLOUD_URL_FUNCTION "-------------above is l7 function-----------------------"

static int aes_file(char *file)
{
	unsigned char *buf = NULL;
	unsigned char pwd[16] = {0xcc, 0xfc, 0x78, 0x66, 0x35, 0x32, 0x97, 0xfc, 0x78, 0x99};
	#define FILE_SIZE  500*1024

	printf("file:%s\n", file);

	buf = malloc(FILE_SIZE + 1);
	if (!buf) {
		printf("malloc fail, %d\n", FILE_SIZE + 1);
		return -1;
	}

	memset(buf, 0, FILE_SIZE + 1);
	if (igd_aes_dencrypt(file, buf, FILE_SIZE, pwd) <= 0)
		printf("aes dencrypt ua fail\n");
	else
		printf("buf:%s\n", buf);
	free(buf);
	return 0;
}

static int plug_daemon_add(char *name, char *path)
{
	struct plug_daemon_info info;

	memset(&info, 0, sizeof(info));
	arr_strcpy(info.name, name);
	if (path)
		arr_strcpy(info.path, path);
	info.oom = 500;
	return mu_msg(SYSTEM_MOD_DAEMON_PLUG_ADD, &info, sizeof(info), NULL, 0);
}

static int gpio_rdwr(char *flag, int gpio)
{
	int r = -1, v;
	char read = flag[1];

	if (read == 'r') {
		r = read_gpio(gpio, &v);
		printf("read_gpio, ret:%d, val:%d, gpio:%d\n", r, v, gpio);
	} else if (read == 'w') {
		v = !!(flag[2] ? (flag[2] - '0') : 0);
		r = write_gpio(gpio, v);
		printf("write_gpio, ret:%d, val:%d, gpio:%d\n", r, v, gpio);
	} else {
		printf("%c nonsupport\n", read);
	}
	return r;
}

static int test_speed(char *uphost, char *upport, char *upurl)
{
	int time = 50, error = 10001;
	struct tspeed_info info;
	unsigned int devid = get_devid();
	char buf[4096];

	CE_DBG("[%s],[%s],[%s]\n", uphost, upport, upurl);

	mu_msg(QOS_TEST_BREAK, NULL, 0, NULL, 0);
	usleep(500000);
	mu_msg(QOS_TEST_SPEED, NULL, 0, NULL, 0);

	while (time-- > 0) {
		if (mu_msg(QOS_SPEED_SHOW, NULL, 0, &info, sizeof(info))) {
			CE_DBG("mu fail, %X\n", QOS_SPEED_SHOW);
			return -1;
		}
		if (info.flag == 2) {
			error = 0;
			break;
		}
		sleep(1);
	}

	snprintf(buf, sizeof(buf), "%s?devid=%u&down_speed=%u&up_speed=%u&delay=%u&error=%u",
		upurl, devid, info.down, info.up, info.delay, error);

	CE_DBG("%s\n", buf);
	if (cloud_upload_send(uphost, upport, buf, NULL, 0) < 0) {
		CE_DBG("send fail, %s:%s%s\n", uphost, upport, upurl);
		return -1;
	}
	return 0;
}

static int plug_down(void)
{
	int ret, i;
	char cmd[512], file[128];
	unsigned char md5[32];
	struct plug_cache_info pci;
	struct plug_info pi;

	if (access(PLUG_FILE_DIR, F_OK))
		mkdir(PLUG_FILE_DIR, 0777);

	while (1) {
		ret = mu_msg(IGD_PLUG_CACHE, NULL, 0, &pci, sizeof(pci));
		if (ret != 1) {
			CE_DBG("%s", ret ? "dump plug cache fail\n" : "dump over\n");
			system("cloud_exchange z \""PLUG_PROC"\" &");
			break;
		}
		CE_DBG("%d, [%s], [%s], [%s]\n", ret, pci.plug_name, pci.url, pci.md5);
		snprintf(file, sizeof(file),
			"%s/%s", PLUG_FILE_DIR, pci.plug_name);

		for (i = 0; i < 3; i++) {
			snprintf(cmd, sizeof(cmd),
				"wget -q %s -O %s -T 10", pci.url, file);
			if (!system(cmd))
				break;
			CE_DBG("wget fail, [%s]\n", cmd);
			sleep(3);
		}
		if (i >= 3)
			continue;

		if (n_md5(file, md5)) {
			CE_ERR("%s calc md5 fail\n", file);
			continue;
		}
		for (i = 0; i < 16; i++)
			sprintf(&cmd[i*2], "%02X", md5[i]);
		if (strncasecmp(cmd, pci.md5, 32)) {
			CE_ERR("URL:[%s], MD5ERR:\n%s\n%s\n", pci.url, cmd, pci.md5);
			continue;
		}
		arr_strcpy(pi.dir, file);
		arr_strcpy(pi.plug_name, pci.plug_name);
		ret = mu_msg(IGD_PLUG_START, &pi, sizeof(pi), NULL, 0);
		if (ret < 0) {
			CE_ERR("start plug [%s] [%s] fail\n", pi.plug_name, pi.dir);
		} else {
			CE_DBG("start plug [%s] [%s] succ\n", pi.plug_name, pi.dir);
		}
		remove(file);
	}
	rmdir(PLUG_FILE_DIR);
	return 0;
}

int compress_file(void)
{
	char cmd[256] = {0};

	snprintf(cmd, sizeof(cmd), "gzip -f %s", DNS_UPLOAD_FILE);
	if (system(cmd) < 0) {
		remove(DNS_UPLOAD_FILE);
		return -1;
	}
	return 0;
}

int dns_file_upload(void)
{
	int ret = 0;
	int fd = 0;

	fd = ce_lock("dns_file");
	if (fd < 0) {
		CE_DBG("is uping\n");
		goto end;
	}
	if (compress_file())
		goto end;
	ret = url_host_send(DNS_UPLOAD_GZ_FILE, "/DnsLog");
end:
	remove(DNS_UPLOAD_GZ_FILE);
	if (fd > 0)
		ce_unlock(fd, "dns_file");
	return ret;
}

#define CLOUD_AES_FUNCTION "-------------above is aes function-----------------------"

int main(int argc, char *argv[])
{
	if (!argv[1])
		return -1;

	CE_DBG("[%s]\n", argv[1]);

	if (argv[1][0] == 'l') {
		return errlog_upload();
	} else if (argv[1][0] == 'r') {
		return rconf_update(argv[2], RCONF_UPDATE_ALL);
	} else if (argv[1][0] == 'c') {
		return rconf_update(argv[2], RCONF_UPDATE_FLAG);
	} else if (argv[1][0] == '7') {
		return l7_gather_upload();
	} else if (argv[1][0] == 'u') {
		return url_host_upload();
	} else if (argv[1][0] == 'a') {
		return alone_update(argv);
	} else if (argv[1][0] == 'e') {
		return aes_file(argv[2]);
	} else if (argv[1][0] == 'd') {
		return plug_daemon_add(argv[2], (argc >= 4) ? argv[3] : NULL);
	} else if (argv[1][0] == 'z') {
		return mu_msg(SYSTEM_MOD_DAEMON_DEL, argv[2], strlen(argv[2]) + 1, NULL, 0);
	} else if (argv[1][0] == 'g') {
		return gpio_rdwr(argv[1], atoi(argv[2]));
	} else if (argv[1][0] == 't') {
		return test_speed(argv[2], argv[3], argv[4]);
	} else if (argv[1][0] == 'p') {
		return plug_down();
	} else if (argv[1][0] == 'n') {
		return dns_file_upload();
	} else if (argv[1][0] == 'x') {
		return gpio_act(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
	} else {
		return -1;
	}
}
