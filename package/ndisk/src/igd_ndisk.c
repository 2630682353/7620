#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/vfs.h>
#include "nlk_ipc.h"
#include "ipc_msg.h"
#include "linux_list.h"
#include "igd_ndisk.h"
#include "igd_ndisk_db.h"

#define NDISK_PARTITIONS   "/proc/partitions"
#define NDISK_MOUNTS   "/proc/mounts"
#define NDISK_MOUNTS_DIR   "/tmp/ndisk"

extern int nlk_uk_init(struct nlk_msg_handler *h, __u32 type,__u32 src_grp, __u32 dst_grp, pid_t pid);
static struct list_head ndisk_head = LIST_HEAD_INIT(ndisk_head);

struct ndisk_info {
	struct list_head list;
	unsigned char type;
	int major;
	int minor;
	char *devname;
	char *mount_point;
	unsigned long long total; //KB
	unsigned long long used;
};

int n_fgets(
	char *file, int max_line,
	int (*fun)(char *, void *), void *data)
{
	int ret = 0;
	FILE *fp = NULL;
	char *buf = NULL;

	fp = fopen(file, "rb");
	if (!fp)
		return -1;

	buf = malloc(max_line);
	if (!buf) {
		fclose(fp);
		return -2;
	}

	while (1) {
		memset(buf, 0, max_line);
		if (!fgets(buf, max_line - 1, fp))
			break;
		ret = fun(buf, data);
		if (ret)
			break;
	}

	fclose(fp);
	free(buf);
	return ret;
}

struct ndisk_info *ndisk_find(char *devname)
{
	struct ndisk_info *ni;

	list_for_each_entry(ni, &ndisk_head, list) {
		if (!strcmp(devname, ni->devname))
			return ni;
	}
	return NULL;
}

struct ndisk_info *ndisk_add(char *devname)
{
	struct ndisk_info *ni = NULL;

	ni = malloc(sizeof(*ni));
	if (!ni)
		goto err;
	memset(ni, 0, sizeof(*ni));
	ni->devname = strdup(devname);
	if (!ni->devname)
		goto err;
	list_add(&ni->list, &ndisk_head);
	return ni;
err:
	if (ni)
		free(ni);
	return NULL;
}

void ndisk_del(struct ndisk_info *ni)
{
	if (ni->devname)
		free(ni->devname);
	if (ni->mount_point)
		free(ni->mount_point);
	list_del(&ni->list);
	free(ni);
}

static int ndisk_mount_find(char *buf, void *data)
{
	char *ptr;

	ptr = strchr(buf, ' ');
	if (!ptr)
		return 0;
	*ptr = '\0';
	if (strcmp(buf, data))
		return 0;
	buf = ptr + 1;
	ptr = strchr(buf, ' ');
	if (!ptr)
		return 0;
	*ptr = '\0';
	strncpy(data, buf, 511);
	return 1;
}

static int ndisk_mount(char *dev, char *mount_point)
{
	char tmp[512];

	snprintf(tmp, sizeof(tmp),
		"mount -o readahead=64K %s %s", dev, mount_point);

	return system(tmp);
}

static int ndisk_search(char *buf, void *data)
{
	int major, minor, ret;
	unsigned long long size;
	char tmp[512], dev_path[128];
	struct ndisk_info *ni;

	if (sscanf(buf, " %d %d %llu %[^\n ]",
		&major, &minor, &size, tmp) != 4) {
		return 0;
	}
	if (memcmp(tmp, "sd", 2) 
		&& memcmp(tmp, "mmc", 3))
		return 0;
	if (data && strcmp(tmp, data))
		return 0;

	NDISK_DBG("%d,%d,%llu,%s\n", major, minor, size, tmp);

	ni = ndisk_find(tmp);
	if (!ni) {
		ni = ndisk_add(tmp);
		if (!ni) {
			NDISK_ERR("add fail, %s\n", tmp);
			return 0;
		}
	}
	ni->total = size*1024;
	ni->major = major;
	ni->minor = minor;
	ret = isdigit(ni->devname[strlen(ni->devname) -1 ]) ? 1 : 0;
	if (!memcmp(ni->devname, "sd", 2)) {
		ni->type = ret ? NDISK_USB_PART : NDISK_MMC_MAIN;
	} else {
		ni->type = ret ? NDISK_MMC_PART : NDISK_MMC_MAIN;
	}
	snprintf(dev_path, sizeof(dev_path), "/dev/%s", ni->devname);
	if (access(dev_path, F_OK)) {
		NDISK_DBG("Create Node %s\n", dev_path);
		mknod(dev_path, S_IFBLK | 0600, 
			makedev(ni->major, ni->minor));
	}

	if (ni->type == NDISK_MMC_PART || ni->type == NDISK_USB_PART) {
		memset(tmp, 0, sizeof(tmp));
		strncpy(tmp, dev_path, sizeof(tmp) - 1);

		if (ni->mount_point) {
			free(ni->mount_point);
			ni->mount_point = NULL;
		}

		// tmp size must be >= 512
		ret = n_fgets(NDISK_MOUNTS, 512, ndisk_mount_find, tmp);
		if (ret == 1) {
			ni->mount_point = strdup(tmp);
			NDISK_DBG("mounted, dev:[%s], mount:[%s]\n",
				ni->devname, ni->mount_point);
		} else {
			snprintf(tmp, sizeof(tmp), "%s/%s", NDISK_MOUNTS_DIR, ni->devname);
			ni->mount_point = strdup(tmp);
			if (!ni->mount_point) {
				NDISK_ERR("strdup point fail\n");
				return 0;
			}
			if (access(ni->mount_point, F_OK))
				mkdir(ni->mount_point, 0777);			
			if (ndisk_mount(dev_path, ni->mount_point)) {
				NDISK_ERR("mount fail, %s,%s\n",
					dev_path, ni->mount_point);
				rmdir(ni->mount_point);
				free(ni->mount_point);
				ni->mount_point = NULL;
			}
		}
		if (!ni->mount_point)
			NDISK_ERR("mount point is null\n");
	}
	return 0;
}

#define SAMBA_HEAD_STR \
"[global]\n" \
	"\tnetbios name = OpenWrt\n" \
	"\tdisplay charset = UTF-8\n" \
	"\tserver string = OpenWrt\n" \
	"\tunix charset = UTF-8\n" \
	"\tworkgroup = WORKGROUP\n" \
	"\tdeadtime = 30\n" \
	"\tdomain master = yes\n" \
	"\tencrypt passwords = true\n" \
	"\tenable core files = no\n" \
	"\tguest account = nobody\n" \
	"\tguest ok = yes\n" \
	"\tlocal master = yes\n" \
	"\tload printers = no\n" \
	"\t;map to guest = Bad User\n" \
	"\tmax protocol = SMB2\n" \
	"\tmin receivefile size = 16384\n" \
	"\tnull passwords = yes\n" \
	"\tobey pam restrictions = yes\n" \
	"\tos level = 20\n" \
	"\tpassdb backend = smbpasswd\n" \
	"\tpreferred master = yes\n" \
	"\tprintable = no\n" \
	"\tsecurity = user\n" \
	"\tsmb encrypt = disabled\n" \
	"\tsmb passwd file = /etc/samba/smbpasswd\n" \
	"\tsocket options = TCP_NODELAY IPTOS_LOWDELAY\n" \
	"\tsyslog = 2\n" \
	"\tuse sendfile = yes\n" \
	"\twriteable = yes\n" \
	"\tconfig file = /tmp/smb.conf\n"

#define NDISK_SAMBA_CONF  "/etc/samba/smb.conf"
static int igd_ndisk_samba(void)
{
	FILE *fp = NULL;
	struct ndisk_info *ni;
	unsigned char restart = 0;

	system("/etc/init.d/samba stop");
	usleep(200000);

	fp = fopen(NDISK_SAMBA_CONF, "wb");
	if (!fp) {
		NDISK_ERR("fopen fail, %s\n", strerror(errno));
		return -1;
	}
	fprintf(fp, "%s", SAMBA_HEAD_STR);

	list_for_each_entry(ni, &ndisk_head, list) {
		if (ni->type == NDISK_USB_PART) {
			fprintf(fp, "[Usb%d_Vol%d]\n", ni->major, ni->minor);
		} else if (ni->type == NDISK_MMC_PART) {
			fprintf(fp, "[Sd%d_Vol%d]\n", ni->major, ni->minor);
		} else {
			continue;
		}
		fprintf(fp,
			"\tpath = %s\n"
			"\tread only = no\n"
			"\tvalid users = admin\n"
			"\tcreate mask = 0700\n"
			"\tdirectory mask = 0700\n", ni->mount_point);
		restart = 1;
	}
	fclose(fp);
	if (restart)
		system("/etc/init.d/samba restart &");
	return 0;
}

static int igd_ndisk_add(char *devname)
{
	n_fgets(NDISK_PARTITIONS, 512, ndisk_search, devname);
	igd_ndisk_samba();
	return 0;
}

static int igd_ndisk_del(char *devname)
{
	char tmp[512];

	struct ndisk_info *ni;

	ni = ndisk_find(devname);
	if (!ni)
		return -1;

	system("/etc/init.d/samba stop &");
	usleep(200000);

	if (ni->mount_point) {
		snprintf(tmp, sizeof(tmp), "umount %s", ni->mount_point);
		if (system(tmp))
			NDISK_ERR("umount fail, %s\n", ni->mount_point);
		rmdir(ni->mount_point);
	}

	snprintf(tmp, sizeof(tmp), "/dev/%s", ni->devname);
	if (!access(tmp, F_OK)) {
		NDISK_DBG("Remove Node %s\n", tmp);
		remove(tmp);
	}
	ndisk_del(ni);
	igd_ndisk_samba();
	return 0;
}

static int igd_ndisk_dump(struct ndisk_info_dump *dump, int dump_len)
{
	int nr = 0;
	struct ndisk_info *ni;
	struct statfs st;

	if (!dump || sizeof(*dump)*NDISK_MAX != dump_len)
		return -1;

	list_for_each_entry(ni, &ndisk_head, list) {
		dump[nr].type = ni->type;
		dump[nr].total = ni->total;
		arr_strcpy(dump[nr].devname, ni->devname);
		if (!ni->mount_point) {
			dump[nr].mount_point[0] = '\0';
			dump[nr].used = dump[nr].total;
		} else {
			arr_strcpy(dump[nr].mount_point, ni->mount_point);
			if (!statfs(ni->mount_point, &st) && (st.f_blocks > 0)) {
				dump[nr].total = (unsigned long long)st.f_blocks * 
					(unsigned long long)st.f_bsize;
				dump[nr].used  = (unsigned long long)(st.f_blocks - st.f_bfree) * 
					(unsigned long long)st.f_bsize;
				NDISK_DBG("fs:0x%lX\n", st.f_type);
			}
		}
		nr++;
		if (nr >= NDISK_MAX)
			break;
	}
	return nr;
}

static int igd_ndisk_file_info(
	char *mount_point, struct ndisk_file_info *info, int len)
{
	if (len != sizeof(*info)*NDFI_MAX) {
		NDISK_ERR("input err\n");
		return -1;
	}
	return ndisk_db_file_info(mount_point, info, len);
}

int ndisk_ipc_msg(int msgid, void *data, int len, void *rbuf, int rlen)
{
	switch (msgid) {
	case IGD_NDISK_DUMP:
		return igd_ndisk_dump(rbuf, rlen);
	case IGD_NDISK_FILE_INFO:
		return igd_ndisk_file_info(data, rbuf, rlen);
	default:
		return -1;
	}
	return 0;
}

int ndisk_ipc(struct ipc_header *msg)
{
	char *in = msg->len ? IPC_DATA(msg) : NULL;
	char *out = msg->reply_len ? IPC_DATA(msg->reply_buf) : NULL;

	NDISK_DBG("%d, %p, %d, %p, %d\n", msg->msg, in, msg->len, out, msg->reply_len);
	return ndisk_ipc_msg(msg->msg, in, msg->len, out, msg->reply_len);
}

int ndisk_uevent_msg(struct nlk_msg_handler *nlk)
{
	int len, i;
	socklen_t addrlen;
	char buf[4096] = {0,}, *msg[NDISK_UEVENT_MAX] = {NULL};

	addrlen = sizeof(nlk->dst_addr);
	len = recvfrom(nlk->sock_fd, buf, sizeof(buf) - 1,
			MSG_DONTWAIT, (struct sockaddr *)&nlk->dst_addr, &addrlen);
	if (len < 0) {
		NDISK_ERR("len < 0, %s\n", strerror(errno));
		return len;
	}
	if(len < 32 || len > (sizeof(buf) - 1)) {
		NDISK_ERR("recvmsg err, %s\n", strerror(errno));
		return -1;
	}
	for (i = 0; i < len; i++) {
		if (buf[i] != '\0')
			continue;
		if (!memcmp(&buf[i + 1], "ACTION=", 7))
			msg[NDISK_UEVENT_ACTION] = &buf[i + 8];
		else if (!memcmp(&buf[i + 1], "SUBSYSTEM=", 10))
			msg[NDISK_UEVENT_SUBSYSTEM] = &buf[i + 11];
		else if (!memcmp(&buf[i + 1], "DEVNAME=", 8))
			msg[NDISK_UEVENT_DEVNAME] = &buf[i + 9];
	}
	if (!msg[NDISK_UEVENT_ACTION] ||
		!msg[NDISK_UEVENT_SUBSYSTEM] ||
		!msg[NDISK_UEVENT_DEVNAME]) {
		return 0;
	}
	if (strcmp(msg[NDISK_UEVENT_SUBSYSTEM], "block"))
		return 0;

	NDISK_DBG("[%s],[%s],[%s]\n", msg[NDISK_UEVENT_ACTION],
		msg[NDISK_UEVENT_SUBSYSTEM], msg[NDISK_UEVENT_DEVNAME]);

	if (!strcmp(msg[NDISK_UEVENT_ACTION], "add")) {
		igd_ndisk_add(msg[NDISK_UEVENT_DEVNAME]);
	} else if (!strcmp(msg[NDISK_UEVENT_ACTION], "remove")) {
		igd_ndisk_del(msg[NDISK_UEVENT_DEVNAME]);
	} else {
		NDISK_DBG("ACTION:%s, %s\n",
			msg[NDISK_UEVENT_ACTION], msg[NDISK_UEVENT_DEVNAME]);
	}
	return 0;
}

int ndisk_init(void)
{
	system("echo -e \"admin\nadmin\" | smbpasswd -a admin -s &");

	if (access(NDISK_MOUNTS_DIR, F_OK)) {
		mkdir(NDISK_MOUNTS_DIR, 0777);
		system("mount -t tmpfs -o size=64k tmpfs "NDISK_MOUNTS_DIR);
	}

	n_fgets(NDISK_PARTITIONS, 512, ndisk_search, NULL);
	igd_ndisk_samba();
	return 0;
}

int main(int argc, char *argv[])
{
	fd_set fds;
	struct timeval tv;
	int ipc_fd, max_fd;
	struct nlk_msg_handler nlk;

	if (nlk_uk_init(&nlk, NETLINK_KOBJECT_UEVENT, 1, 0, 0)) {
		NDISK_ERR("uevent init err\n");
		return -1;
	}
	ipc_fd = ipc_server_init(IPC_PATH_NDISK);
	if (ipc_fd < 0) {
		NDISK_ERR("ipc init err\n");
		return -1;
	}
	ndisk_init();

	for (;;) {
		max_fd = 0;
		FD_ZERO(&fds);
		IGD_FD_SET(ipc_fd, &fds);
		IGD_FD_SET(nlk.sock_fd, &fds);

		tv.tv_sec = 2;
		tv.tv_usec = 0;
		if (select(max_fd + 1, &fds, NULL, NULL, &tv) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
		}

		if (FD_ISSET(nlk.sock_fd, &fds))
			ndisk_uevent_msg(&nlk);

		if (FD_ISSET(ipc_fd, &fds))
			ipc_server_accept(ipc_fd, ndisk_ipc);
	}
	return 0;
}
