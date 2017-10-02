#ifndef __IGD_STORAGE_H__
#define __IGD_STORAGE_H__

#define IPC_PATH_NDISK   "/tmp/ipc_path_ndisk"
#define NDISK_MAX  32

enum {
	NDISK_MMC_MAIN = 0,
	NDISK_MMC_PART,
	NDISK_USB_MAIN,
	NDISK_USB_PART,
};

enum {
	NDISK_UEVENT_ACTION = 0,
	NDISK_UEVENT_SUBSYSTEM,
	NDISK_UEVENT_DEVNAME,
	NDISK_UEVENT_MAX,
};

enum {
	NDFI_ALL = 0,
	NDFI_AUDIO,
	NDFI_VIDEO,
	NDFI_PIC,
	NDFI_TEXT,
	NDFI_PACK,
	NDFI_MAX,
};

enum {
	IGD_NDISK_DUMP = 1,
	IGD_NDISK_FILE_INFO,
};

struct ndisk_info_dump {
	unsigned char type;
	char devname[64];
	char mount_point[128];
	unsigned long long total; //bytes
	unsigned long long used; //bytes
};

struct ndisk_file_info {
	unsigned long long nr;
	unsigned long long size;
};


#define ndisk_msg(mgsid, data, len, rbuf, rlen) \
	ipc_send(IPC_PATH_NDISK, mgsid, data, len, rbuf, rlen)

#if 0
#define NDISK_DBG(fmt, args...) \
	igd_log("/tmp/ndisk_dbg", "%s,%d:"fmt, __FUNCTION__, __LINE__, ##args)
#define NDISK_ERR(fmt, args...) \
	igd_log("/tmp/ndisk_err", "%s,%d:"fmt, __FUNCTION__, __LINE__, ##args)
#else
#define NDISK_DBG(fmt, args...) \
	printf("%s,%d:"fmt, __FUNCTION__, __LINE__, ##args)
#define NDISK_ERR(fmt, args...) \
	printf("%s,%d:"fmt, __FUNCTION__, __LINE__, ##args)
#endif

#endif
