#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "igd_md5.h"

void igd_log(char *file, const char *fmt, ...)
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
	if (ftell(fp) > 10*1024)
		snprintf(bakfile, sizeof(bakfile), "%s.bak", file);
	fclose(fp);
	if (bakfile[0])
		rename(file, bakfile);
}

int get_devid(unsigned int *devid)
{
	*devid = 1573336; //113018;
	return 0;
}

int set_devid(unsigned int hwid)
{
	return 0;
}

long sys_uptime(void)
{
	struct sysinfo info;

	if (sysinfo(&info))
		return 0;

	return info.uptime;
}

int read_mac(unsigned char *mac)
{
	mac[0] = 0x94;
	mac[1] = 0x8b;
	mac[2] = 0x03;
	mac[3] = 0xFF;
	mac[4] = 0xFF;
	mac[5] = 0x16;
	return 0;
}

char *read_firmware(char *key)
{

	if (!strcmp(key, "CURVER"))
		return "1.27.0";
	else if (!strcmp(key, "VENDOR"))
		return "CY_WiFi";
	else if (!strcmp(key, "PRODUCT"))
		return "I1";
	else
		return NULL;
}

int get_cpuinfo(char *data, int data_len)
{
	strncpy(data, "MT7620", data_len);
	return strlen(data);
}

int get_meminfo(char *data, int data_len)
{
	strncpy(data, "64M", data_len);
	return strlen(data);
}

/* read file , return n bytes read, or -1 if error */
int igd_safe_read(int fd, unsigned char *dst, int len)
{
	int count = 0;
	int ret;

	while (len > 0) {
		ret = read(fd, dst, len);
		if (!ret) 
			break;
		if (ret < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) 
				continue;
			if (errno == EINTR) 
				continue;
			return -1;
		}
		count += ret;
		len -= ret;
		dst += ret;
	}
	return count;
}

int igd_md5sum(char *file, void *md5)
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

