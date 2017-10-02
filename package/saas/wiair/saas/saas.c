#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "linux_list.h"
#include "saas_lib.h"

struct saas_proc_listen {
	struct list_head list;
	char *name;
	int min_mem;
	int max_mem;
};

struct saas_info {
	int cpu;
	int mem;
	int connect;
	int wifinum;
	struct list_head proc;
};

#define SEIT_CPU      "CPU"
#define SEIT_MEM      "MEM"
#define SEIT_PROC     "PROC"
#define SEIT_CONNECT  "CONNECT"
#define SEIT_WIFINUM  "WIFINUM"

#define SAAS_ERR_FILE "/tmp/saas_err"
#define SAAS_UPLOAD_PWD  "cqm#yg$ysd%ss@oy"

struct saas_err_info {
	struct list_head list;
	int time;
	char *flag;
	char *value;
	char *info;
};

static struct saas_info saas;
static int saas_errlist_num = 0;
static struct list_head saas_errlist = LIST_HEAD_INIT(saas_errlist);

extern int saas_aes(const char *path,
	unsigned char *out, int len, unsigned char *pwd);

static int saas_time(void)
{
	struct sysinfo info;

	if (sysinfo(&info))
		return 0;

	return (int)info.uptime;
}

static int saas_add_listen(char *buf)
{
	char name[512] = {0,};
	int min = 0, max = 0;
	struct saas_proc_listen *proc;

	if (3 != sscanf(buf, "%512[^,],%d,%d\n", name, &min, &max)) {
		SAAS_DBG("sscanf fail, %s", buf);
		return -1;
	}
	list_for_each_entry(proc, &saas.proc, list) {
		if (!strcmp(name, proc->name)) {
			SAAS_DBG("exist, %s", buf);
			return -1;
		}
	}
	proc = malloc(sizeof(*proc));
	if (!proc) {
		SAAS_DBG("malloc fail, %s", buf);
		return -1;
	}
	memset(proc, 0, sizeof(*proc));
	proc->name = strdup(name);
	if (!proc->name) {
		free(proc);
		SAAS_DBG("strdup fail, %s", buf);
		return -1;
	}
	proc->min_mem = min;
	proc->max_mem = max;
	list_add(&proc->list, &saas.proc);
	SAAS_DBG("PROC:%s,%d,%d\n",
		proc->name, proc->min_mem, proc->max_mem);
	return 0;
}

static int saas_conf(char *file)
{
	FILE *fp = NULL;
	char buf[2048];

	memset(&saas, 0, sizeof(saas));
	INIT_LIST_HEAD(&saas.proc);

	fp = fopen(file, "rb");
	if (!fp) {
		SAAS_DBG("fopen fail, %s, %s\n", file, strerror(errno));
		return -1;
	}

	while (1) {
		memset(buf, 0, sizeof(buf));
		if (!fgets(buf, sizeof(buf) - 1, fp))
			break;
		if (!memcmp(buf,
			SEIT_CPU":", strlen(SEIT_CPU) + 1)) {
			saas.cpu = atoi(buf + 4);
			SAAS_DBG("CPU:%d\n", saas.cpu);
		} else if (!memcmp(buf,
			SEIT_MEM":", strlen(SEIT_MEM) + 1)) {
			saas.mem = atoi(buf + 4);
			SAAS_DBG("MEM:%d\n", saas.mem);
		} else if (!memcmp(buf,
			SEIT_PROC":", strlen(SEIT_PROC) + 1)) {
			saas_add_listen(buf + 5);
		} else if (!memcmp(buf,
			SEIT_CONNECT":", strlen(SEIT_CONNECT) + 1)) {
			saas.connect = atoi(buf + 8);
			SAAS_DBG("CONNECT:%d\n", saas.connect);
		} else if (!memcmp(buf,
			SEIT_WIFINUM":", strlen(SEIT_WIFINUM) + 1)) {
			saas.wifinum = atoi(buf + 8);
			SAAS_DBG("WIFINUM:%d\n", saas.wifinum);
		}
	}
	fclose(fp);
	return 0;
}

static struct saas_err_info *
	saas_err_add(char *flag, char *exinfo, const char *fmt, ...)
{
	va_list ap;
	struct saas_err_info *info;

	if (saas_errlist_num > 500) {
		SAAS_DBG("errlist full\n");
		return NULL;
	}

	info = malloc(sizeof(*info));
	if (!info) {
		SAAS_DBG("malloc fail\n");
		return NULL;
	}
	memset(info, 0, sizeof(*info));
	va_start(ap, fmt);
	vasprintf(&info->value, fmt, ap);
	va_end(ap);
	if (!info->value || !info->value[0]) {
		SAAS_DBG("value fail\n");
		goto err;
	}

	info->time = saas_time();
	info->flag = flag;
	if (exinfo)
		info->info = strdup(exinfo);
	list_add(&info->list, &saas_errlist);
	saas_errlist_num++;
	return info;
err:
	if (!info)
		return NULL;
	if (info->info)
		free(info->info);
	if (info->value)
		free(info->value);
	free(info);
	return NULL;
}

#define CPU_ERR_TIME 5
static int saas_cpu_err(int add, char *ext, int cpu)
{
	static int num = 0;
	static int time[CPU_ERR_TIME] = {0,};
	static int cpuused[CPU_ERR_TIME] = {0,};
	static char *extinfo[CPU_ERR_TIME] = {NULL,};
	int i;
	struct saas_err_info *info;

	if (add) {
		time[num] = saas_time();
		cpuused[num] = cpu;
		extinfo[num] = strdup(ext);
		if (!extinfo[num])
			return 0;
		num++;
		if (num < CPU_ERR_TIME)
			return 0;
	} else {
		for (i = 0; i < CPU_ERR_TIME; i++) {
			cpuused[i] = 0;
			if (extinfo[i])
				free(extinfo[i]);
			extinfo[i] = NULL;
		}
		num = 0;
		return 0;
	}
	for (i = 0; i < CPU_ERR_TIME; i++) {
		if (!extinfo[i])
			continue;
		info = saas_err_add(SEIT_CPU, extinfo[i], "%d", cpuused[i]);
		free(extinfo[i]);
		extinfo[i] = NULL;
		cpuused[i] = 0;
		if (info)
			info->time = time[i];
		time[i] = 0;
	}
	num = 0;
	return 0;
}

static int saas_proc_cb(struct saas_proc_info *info)
{
	int rate, i, j, len;
	char buf[1024];
	struct saas_proc_list *list[3], *l, *p;
	struct saas_proc_listen *proc;

	rate = (info->used_mem*100)/(info->used_mem + info->free_mem);
	if (rate >= saas.mem) {
		len = 0;
		memset(list, 0, sizeof(list));
		for (i = 0; i < 3; i++) {
			list_for_each_entry(l, &info->proc, list) {
				if (!list[i]) {
					list[i] = l;
					continue;
				}
				for (j = 0; j < i; j++) {
					if (list[j] == l)
						break;
				}
				if (j < i)
					continue;
				if (list[i]->mem < l->mem)
					list[i] = l;
			}
			len += snprintf(buf + len, sizeof(buf) - len
				, "%s:%d;", list[i]->name, list[i]->mem);
		}
		SAAS_DBG("%s:%d;%s\n", SEIT_MEM, info->free_mem, buf);
		saas_err_add(SEIT_MEM, buf, "%d", info->free_mem);
	}

	if (info->cpuused >= saas.cpu) {
		len = 0;
		memset(list, 0, sizeof(list));
		for (i = 0; i < 3; i++) {
			list_for_each_entry(l, &info->proc, list) {
				if (!list[i]) {
					list[i] = l;
					continue;
				}
				for (j = 0; j < i; j++) {
					if (list[j] == l)
						break;
				}
				if (j < i)
					continue;
				if (list[i]->cpu <= l->cpu)
					list[i] = l;
			}
			len += snprintf(buf + len, sizeof(buf) - len
				, "%s:%d;", list[i]->name, list[i]->cpu);
		}
		SAAS_DBG("%s:%d;%s\n", SEIT_CPU, info->cpuused, buf);
		saas_cpu_err(1, buf, info->cpuused);
	} else {
		saas_cpu_err(0, NULL, 0);
	}

	list_for_each_entry(proc, &saas.proc, list) {
		p = NULL;
		list_for_each_entry(l, &info->proc, list) {
			//SAAS_DBG("xxx:%s  pid:%d  mem:%d  cpu:%d\n",
				//l->name, l->pid, l->mem, l->cpu);
			if (!strcmp(proc->name, l->name)) {
				p = l;
				if (p->mem > 0)
					break;
			}
		}
		if (!p) {
			SAAS_DBG("%s:%s;%s\n", SEIT_PROC, proc->name, "-1,0;");
			saas_err_add(SEIT_PROC, "-1,0;", "%s", proc->name);
		} else if (((proc->min_mem != -1) && (p->mem <= proc->min_mem))
			|| ((proc->min_mem != -1) && (p->mem >= proc->max_mem))) {
			snprintf(buf, sizeof(buf), "%d,%d;", p->pid, p->mem);
			SAAS_DBG("%s:%s;%s\n", SEIT_PROC, proc->name, buf);
			saas_err_add(SEIT_PROC, buf, "%s", proc->name);
		}
	}
	return 0;
}

static int saas_check_connect(void)
{
	int connect;

	if (!saas.wifinum)
		return 0;
	connect = saas_connect_num();
	if (connect <= saas.connect)
		return 0;
	SAAS_DBG("connect:%d\n", connect);
	saas_err_add(SEIT_CONNECT, NULL, "%d", connect);
	return 0;
}

static int saas_check_wifinum(void)
{
	int num;

	if (!saas.wifinum)
		return 0;
	num = saas_wifi_num();
	if (num <= saas.wifinum)
		return 0;
	SAAS_DBG("wifinum:%d\n", num);
	saas_err_add(SEIT_WIFINUM, NULL, "%d", num);
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

static int saas_send(char *file)
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

	free(buf);
	close(sock);
	remove(gzfile);
	remove(SAAS_ERR_FILE);
	return 0;
err:
	if (buf)
		free(buf);
	if (sock > 0)
		close(sock);
	remove(gzfile);
	remove(SAAS_ERR_FILE);
	return -1;
}

static int saas_upload(void)
{
	FILE *fp;
	int time;
	struct saas_err_info *err, *_err;
	struct saas_sys_info sys;

	if (list_empty(&saas_errlist)) {
		SAAS_DBG("list empty\n");
		return 0;
	}

	fp = fopen(SAAS_ERR_FILE, "a+");
	if (!fp) {
		SAAS_DBG("fopen fail\n");
		return -1;
	}
	memset(&sys, 0, sizeof(sys));
	if (saas_sysinfo(&sys)) {
		SAAS_DBG("sysinfo fail\n");
		fclose(fp);
		return -1;
	}

	fprintf(fp, "%u\1", sys.devid);
	fprintf(fp, "%02X%02X%02X%02X%02X%02X\1",
		sys.mac[0], sys.mac[1], sys.mac[2],
		sys.mac[3], sys.mac[4], sys.mac[5]);
	fprintf(fp, "%s\1", sys.model);
	fprintf(fp, "%s\1", sys.ver);
	fprintf(fp, "%d\1", sys.Revision);
	fprintf(fp, "%s\1", sys.date);
	fprintf(fp, "%d\1", sys.toltal_mem);

	time = saas_time();
	fprintf(fp, "%d", time);
	list_for_each_entry(err, &saas_errlist, list) {
		fprintf(fp, "\1%s:%s,%d;%s",
			err->flag, err->value, time - err->time, err->info ? : "");
	}
	fprintf(fp, "\2\2\2\2\n");
	fclose(fp);

	if (saas_send(SAAS_ERR_FILE)) {
		SAAS_DBG("send fail\n");
		return -1;
	}

	list_for_each_entry_safe(err, _err, &saas_errlist, list) {
		saas_errlist_num--;
		list_del(&err->list);
		if (err->info)
			free(err->info);
		if (err->value)
			free(err->value);
		free(err);
	}
	return 0;
}

static void saas_waitpid(void)
{
	int status, pid = 1;

	while (pid > 0)
		pid = waitpid(-1, &status, WNOHANG);
}

int main(int argc, char *argv[])
{
	int t = saas_time()%(30*6) + 1;

	SAAS_DBG("time:%d\n", t);
	
	if (saas_conf(argv[1]))
		return -1;

	while (1) {
		sleep(60);
		saas_check_proc(saas.mem, saas.cpu, saas_proc_cb);
		saas_check_connect();
		saas_check_wifinum();
		saas_waitpid();

		t--;
		if ((t <= 0) && !saas_upload())
			t = 30*6;
	}
	return 0;
}
