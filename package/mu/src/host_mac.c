#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "nlk_ipc.h"

#define VENDER_PRINTF(fmt,args...)  do{}while(0)
//#define VENDER_PRINTF(fmt,args...)  do{console_printf("[MAKER:%05d]"fmt, __LINE__, ##args);}while(0)

struct vender_info {
	char *name;
	unsigned short id;
	unsigned short num;
	unsigned char *macs;
	struct vender_info *next;
};

#define VENDER_TXT "/tmp/rconf/vender.txt"
static struct vender_info *macs_list = NULL;

static int read_file_all(char *file, char **buf)
{
	int len, fd;
	struct stat st;
	char *data = NULL;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		goto err;

	if (fstat(fd, &st) < 0)
		goto err;

	len = (int)st.st_size;
	if (len <= 0)
		goto err;

	data = malloc(len);
	if (!data) 
		goto err;

	if (igd_safe_read(fd, (unsigned char *)data, len) != len)
		goto err;

	*buf = data;
	close(fd);
	return len;
err:
	*buf = NULL;
	if (data)
		free(data);
	if (fd > 0)
		close(fd);
	return -1;
}

static void vender_free(struct vender_info *info)
{
	if (!info)
		return;
	vender_free(info->next);
	if (info->name)
		free(info->name);
	if (info->macs)
		free(info->macs);
	free(info);
}

#define MAC_MALLOC(ptr, len) do {\
	ptr = malloc(len);\
	if (!ptr) {\
		VENDER_PRINTF("malloc fail\n");\
		goto err;\
	}\
	memset(ptr, 0, len);\
} while(0)

#define BIT2_CALC(buf) (((unsigned char *)(buf))[0] * 255 + ((unsigned char *)(buf))[1])

int read_vender(void)
{
	int len, i = 0;
	char *buf = NULL;
	struct vender_info *info = NULL, **next;

	vender_free(macs_list);
	next = &macs_list;

	len = read_file_all(VENDER_TXT, &buf);
	if (len < 0) {
		VENDER_PRINTF("read all fail\n");
		goto err;
	}

	while ((len - i) > 8) {
		if (memcmp(&buf[i], "MAC:", 4)) {
			VENDER_PRINTF("start flag err, %c\n", buf[i]);
			goto err;
		}
		i += 4;
		MAC_MALLOC(info, sizeof(*info));
		info->id = BIT2_CALC(&buf[i]);
		i += 2;
		info->num = BIT2_CALC(&buf[i]);
		i += 2;
		info->name = strdup(&buf[i]);
		i += (strlen(&buf[i]) + 1);
		VENDER_PRINTF("id:%d, num:%d, name:%s\n", info->id, info->num, info->name);
		if (buf[i] != '\n' || 
			(len - i - 1 < info->num*3)) {
			VENDER_PRINTF("check err, %c, %d,%d,%d\n",
				buf[i], len, i, info->num*3);
			goto err;
		}
		i += 1;
		if (info->num) {
			MAC_MALLOC(info->macs, info->num*3);
			memcpy(info->macs, &buf[i], info->num*3);
			i += info->num*3;
		}
		*next = info;
		next = &info->next;
	}
	free(buf);
	remove(VENDER_TXT);
	return 0;
err:
	if (buf)
		free(buf);
	if (info)
		vender_free(info);
	return -1;
}

int dbg_vender(void)
{
	int i = 0;
	struct vender_info *info;
	FILE *fp = fopen("/tmp/vender_dbg", "w");

	if (!fp)
		return -1;

	info = macs_list;
	while (info) {
		fprintf(fp, "MAC:%d,%d,%s\n", info->id, info->num, info->name);
		for (i = 0; i < info->num; i++) {
			fprintf(fp, "%02X:%02X:%02X", info->macs[i*3],
				info->macs[i*3 + 1], info->macs[i*3 + 2]);
			fprintf(fp, "%c", (i == info->num - 1) ? '\n' : ',');
		}
		info = info->next;
	}
	fclose(fp);
	return 0;
}

unsigned short get_vender(unsigned char *mac)
{
	int i = 0;
	struct vender_info *info;

	info = macs_list;
	while (info) {
		for (i = 0; i < info->num; i++) {
			if (!memcmp(mac, &info->macs[i*3], 3))
				return info->id;
		}
		info = info->next;
	}
	return 0;
}

char *get_vender_name(unsigned short id, char *hostname)
{
	static char name[32] = {0,};
	struct vender_info *info;

	info = macs_list;
	snprintf(name, sizeof(name) - 1,
		"%s%s%s", "其它", 
		hostname ? "-" : "", 
		hostname ? hostname : "");
	while (info) {
		if (info->id == id) {
			snprintf(name, sizeof(name) - 1,
				"%s%s%s", info->name,
				hostname ? "-" : "", 
				hostname ? hostname : "");
			break;
		}
		info = info->next;
	}
	return name;
}
