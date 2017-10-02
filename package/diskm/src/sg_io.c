#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <scsi/sg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory.h>
#include <unistd.h>

#define DISKCK_DBG(fmt, arg...)                           \
        (fprintf(stderr,"(%s,%d)"fmt,            \
                 __FUNCTION__, __LINE__, ##arg))

#if !defined(BLKGETSIZE64)
#define BLKGETSIZE64           _IOR(0x12,114,size_t)
#endif

#define SG_DISK_VENDOR		8
#define SG_DISK_VENDOR_LEN	8
#define SG_DISK_PRODUCT		16
#define SG_DISK_PRODUCT_LEN	16
#define SG_DISK_VERSION		32
#define SG_DISK_VERSION_LEN	4


void show_vendor(struct sg_io_hdr * hdr) {
	unsigned char * buffer = hdr->dxferp;
	int i;
	
	printf("Vendor :");
	for (i=8; i<16; ++i) {
		putchar(buffer[i]);
	}
	putchar('\n');
}

void show_product(struct sg_io_hdr * hdr) {
	unsigned char * buffer = hdr->dxferp;
	int i;
	
	printf("product :");
	for (i=16; i<32; ++i) {
		putchar(buffer[i]);
	}
	putchar('\n');
}

void show_product_rev(struct sg_io_hdr * hdr) {
	unsigned char * buffer = hdr->dxferp;
	int i;
	printf("Version:");
	for (i=32; i<36; ++i) {
		putchar(buffer[i]);
	}
	putchar('\n');
}
int sg_get_disk_vendor_serical(char *devname, char *vendor, char *serical)
{
	struct sg_io_hdr io_hdr;
	unsigned char cdb[6] = {0};
	unsigned char sense[32] = {0};
	unsigned char buff[65535] = {0};
	char *ptr;
	int dev_fd;

	if(!devname || !vendor || !serical){
		return -1;
	}

	dev_fd= open(devname, O_RDWR | O_NONBLOCK);
	if (dev_fd < 0 && errno == EROFS)
		dev_fd = open(devname, O_RDONLY | O_NONBLOCK);
	if (dev_fd<0){
		DISKCK_DBG("Open %s Failed:%s", devname, strerror(errno));
		return -1; 
	}
	cdb[0]=0x12;
	cdb[4]=36;
	memset(&io_hdr,0,sizeof(struct sg_io_hdr));
	io_hdr.interface_id='S';
	io_hdr.cmd_len=sizeof(cdb);
	io_hdr.mx_sb_len=sizeof(sense);
	io_hdr.dxfer_len=64;
	io_hdr.dxferp=buff;
	io_hdr.cmdp=cdb;
	io_hdr.sbp=sense;
	io_hdr.timeout=20000;
	io_hdr.dxfer_direction=SG_DXFER_FROM_DEV;
	
	if(ioctl(dev_fd,SG_IO,&io_hdr)<0){
		DISKCK_DBG("ioctl error:%s!\n", strerror(errno));		
		close(dev_fd);
		return -1;
	}
	ptr=(char*)io_hdr.dxferp;
	strncpy(vendor, ptr+SG_DISK_VENDOR, SG_DISK_VENDOR_LEN);
	vendor[SG_DISK_VERSION] = '\0';
	strncpy(serical, ptr+SG_DISK_PRODUCT, SG_DISK_PRODUCT_LEN);
	serical[SG_DISK_PRODUCT_LEN] = '\0';	 

	close(dev_fd);

	return 0;
}

int sg_get_disk_space(char *devname, unsigned long long *capacity)
{
	unsigned char sense_b[32] = {0};
	unsigned char rcap_buff[8] = {0};
	unsigned char cmd[] = {0x25, 0, 0, 0 , 0, 0};
	struct sg_io_hdr io_hdr;
	unsigned int lastblock, blocksize;
	unsigned long long disk_cap;
	int dev_fd;

	if(!devname){
		return -1;
	}

	dev_fd= open(devname, O_RDWR | O_NONBLOCK);
	if (dev_fd < 0 && errno == EROFS)
		dev_fd = open(devname, O_RDONLY | O_NONBLOCK);
	if (dev_fd<0){
		DISKCK_DBG("Open %s Failed:%s", devname, strerror(errno));
		return -1; 
	}

	memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = sizeof(cmd);
	io_hdr.dxferp = rcap_buff;
	io_hdr.dxfer_len = 8;
	io_hdr.mx_sb_len = sizeof(sense_b);
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.cmdp = cmd;
	io_hdr.sbp = sense_b;
	io_hdr.timeout = 20000;

	if(ioctl(dev_fd,SG_IO,&io_hdr)<0){
		DISKCK_DBG("ioctl error:%s!\n", strerror(errno));		
		close(dev_fd);
		return -1;
	}

	/* Address of last disk block */
	lastblock =  ((rcap_buff[0]<<24)|(rcap_buff[1]<<16)|
	(rcap_buff[2]<<8)|(rcap_buff[3]));

	/* Block size */
	blocksize =  ((rcap_buff[4]<<24)|(rcap_buff[5]<<16)|
	(rcap_buff[6]<<8)|(rcap_buff[7]));

	/* Calculate disk capacity */
	disk_cap  = (lastblock+1);
	disk_cap *= blocksize;
	DISKCK_DBG("Disk Capacity = %llu Bytes\n", disk_cap);

	*capacity = disk_cap;
	close(dev_fd);

	return 0;
}
int ioctl_get_disk_space(char *devname, unsigned long long *capacity)
{
	int dev_fd;
	unsigned long long bytes = 0;

	if(!devname){
		return -1;
	}

	dev_fd= open(devname, O_RDWR | O_NONBLOCK);
	if (dev_fd < 0 && errno == EROFS)
		dev_fd = open(devname, O_RDONLY | O_NONBLOCK);
	if (dev_fd<0){
		DISKCK_DBG("Open %s Failed:%s", devname, strerror(errno));
		return -1; 
	}
	if (ioctl(dev_fd, BLKGETSIZE64, &bytes) != 0) {
		DISKCK_DBG("ioctl error:%s!\n", strerror(errno));	
		close(dev_fd);
		return -1;		
	}
			
	DISKCK_DBG("Disk Capacity = %llu Bytes\n", bytes);

	*capacity = bytes;
	close(dev_fd);

	return 0;	
}

/*
* action :
* 1: start
* 0: stop
*/
int sg_sleep_disk(char *devname, int action)
{
	unsigned char sense_b[32] = {0};
	unsigned char cmd[] = {0x1b, 1, 0, 0 , 1, 0};
	struct sg_io_hdr io_hdr;
	char dev[64] = {0};

	int dev_fd;

	if(!devname){
		return -1;
	}
	if(strstr(devname, "/dev") == NULL){
		sprintf(dev, "/dev/%s", devname);
	}else{
		strcpy(dev, devname);
	}
	dev_fd= open(dev, O_RDWR | O_NONBLOCK);
	if (dev_fd < 0 && errno == EROFS)
		dev_fd = open(dev, O_RDONLY | O_NONBLOCK);
	if (dev_fd<0){
		DISKCK_DBG("Open %s Failed:%s", dev, strerror(errno));
		return -1; 
	}
	if(action == 1){
		cmd[4] = 32;
	}else{
		cmd[4] = 48;
	}

	memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = sizeof(cmd);
	io_hdr.mx_sb_len = sizeof(sense_b);
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.cmdp = cmd;
	io_hdr.sbp = sense_b;
	io_hdr.timeout = 20000;

	if(ioctl(dev_fd,SG_IO,&io_hdr)<0){
		DISKCK_DBG("ioctl error:%s!\n", strerror(errno));		
		close(dev_fd);
		return -1;
	}

	if((io_hdr.info & SG_INFO_OK_MASK) == SG_INFO_OK){
		DISKCK_DBG("%s %s Successful...\n", action==1?"Start":"Stop", devname);
	}

	close(dev_fd);

	return 0;
}

