#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/file.h>
#include <syslog.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include<signal.h>
#include <sys/mount.h>
#include <sys/vfs.h>

#include <arpa/inet.h>
#include <syslog.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include "disk_triger.h"
#include "disk_manager.h"
#include "dmodule.h"
#include "diskm_ipc_msg.h"

int disktriger(int argc, char **argv)
{
	int fd;
	char *buf = NULL;
	struct disk_ipc_header *hdr; 
	int total_len = 0;

	if(argc != 4 && argc != 2){
		DISKCK_DBG("Usage:%s aciton[add[4]/remove[5]] major devname\n", argv[0]);
		return -1;
	}
	if(atoi(argv[1]) != 4 && atoi(argv[1]) !=  5){
		DISKCK_DBG("argv[1] must be 4 or 5\n");
		return -1;
	}

	fd = diskm_ipc_client_init(IPC_PATH_MDISK);
	if(fd < 0){
		DISKCK_DBG("Error\n");
		return -1;
	}
	if(argc == 2){
		int act;
		buf = calloc(1, sizeof(struct disk_ipc_header)+sizeof(int));
		hdr = (struct disk_ipc_header*)buf;	
		act = atoi(argv[1]);
		
		hdr->msg = MSG_DISK_TRIGER;
		hdr->len = sizeof(int);
		hdr->direction.flag = IPCF_ONLY_SEND;
		memcpy(buf+sizeof(struct disk_ipc_header), &act, sizeof(int));
		total_len = sizeof(int)+sizeof(struct disk_ipc_header);
	}else if(argc == 4){
		udev_action event;
		memset(&event, 0, sizeof(udev_action));
		event.action= atoi(argv[1]);
		event.major = atoi(argv[2]);
		strcpy(event.dev, argv[3]);
		buf = calloc(1, sizeof(struct disk_ipc_header)+sizeof(udev_action));
		hdr = (struct disk_ipc_header*)buf;	
		
		hdr->msg = MSG_DISK_UDEV;
		hdr->len = sizeof(udev_action);
		hdr->direction.flag = IPCF_ONLY_SEND;
		memcpy(buf+sizeof(struct disk_ipc_header), &event, sizeof(udev_action));
		total_len = sizeof(udev_action)+sizeof(struct disk_ipc_header);
	}else{
		DISKCK_DBG("Error Argc num\n");
		close(fd);
		return -1;
	}
	
	diskm_ipc_write(fd, buf, total_len);

	close(fd);
	return 0;
}

int umount2_func(int argc, char **argv)
{
	int ret;
	
	if(argc != 2){
		DISKCK_DBG("Usage:%s MountDir---->Force umont dev\n", argv[0]);
		return -1;
	}
	ret = umount2(argv[1], MNT_DETACH);
	if(ret){
		DISKCK_DBG("umount2 of %s failed (%d) - %s\n",
				argv[1], ret, strerror(errno));
	}

	return 0;	
}

int disktest(int argc, char **argv)
{
	void *request = NULL, *response = NULL;
	disk_proto_t *diskinfo = NULL;
	disk_dirlist_t *dirinfo = NULL;
	disk_disklist_t  *disklist = NULL;
	int item = 0, reqlen = 0, reslen = 0, cur;
	DMSG_ID msg_id;

	if(argc  < 2){
		DISKCK_DBG("Usage:%s TestItem\n", argv[0]);
		printf("Help:\n");
		printf("1--->get disk list\n");
		printf("2--->get dir list\n");		
		printf("3--->get disk info\n");		
		printf("4--->disk remove\n");
		return -1;
	}

	item = atoi(argv[1]);

	if(item == 1){
		msg_id = MSG_DISK_DISKLIST;
	}else if(item == 2){
		msg_id = MSG_DISK_DIRLIST;
	}else if(item == 3){
		msg_id = MSG_DISK_INFO;
	}else if(item == 4 || item == 5){
		udev_action *action;
		if(argc < 3){
			DISKCK_DBG("Remove Aciton need device\n");
			return -1;
		}
		msg_id = MSG_DISK_UDEV;
		request =calloc(1, sizeof(udev_action));
		if(request == NULL){
			DISKCK_DBG("Calloc Memory Failed\n");
			return -1;
		}
		action = (udev_action*)request;
	        strcpy(action->dev, argv[2]);
	        action->major = 0xFFFF;
		if(item == 4){
		        action->action = DISK_SFREMOVE;
		}else{
			action->action = DISK_WAKEUP;
		}
		reqlen = sizeof(udev_action);
	}else{
		DISKCK_DBG("Usage:%s Not Support TestItem %d\n", argv[0], item);
		return -1;
	}

	
	if(disk_api_call(msg_id, request, reqlen, &response, &reslen) == DISK_FAILURE){
		DISKCK_DBG("disk_api_call Failed\n");
		if(request){
			free(request);
		}
		return -1;
	}
	if(request){
		free(request);
	}

	if(msg_id == MSG_DISK_INFO){
		diskinfo = (disk_proto_t*)response;
		cur = 0;
		while(cur < reslen){
			printf("PTYPE=%u......................Used=%llu\n", diskinfo->ptype, diskinfo->used);
			if((diskinfo->ptype &0xFF) == 99){
				printf("DiskMain INFO:\n\tName:%s\n\tDiskFlag:%d\n\tTotal:%llu\n\tUsed:%llu\n\tVendor:%s\n\tSerical:%s\n"
						"\tType:%s\n\tDiskTag:%s\n\tStatus:%d\n\tPartnum:%d\n", diskinfo->devname, diskinfo->ptype>>8, diskinfo->total,
						diskinfo->used, diskinfo->partition.main_info.vendor, diskinfo->partition.main_info.serical,
						diskinfo->partition.main_info.type, diskinfo->partition.main_info.disktag, diskinfo->partition.main_info.status,
						diskinfo->partition.main_info.partnum);
			}else{
				printf("Part INFO:\n\tName:%s\n\tDiskFlag:%d\n\tTotal:%llu\n\tUsed:%llu\n\tFstype:%s\n\tLabel:%s\n"
						"\tMountPoint:%s\n\tMounted:%d\n\tEnablewrite:%d\n", diskinfo->devname, diskinfo->ptype>>8, diskinfo->total,
						diskinfo->used, diskinfo->partition.part_info.fstype, diskinfo->partition.part_info.label,
						diskinfo->partition.part_info.mntpoint, diskinfo->partition.part_info.mounted, 
						diskinfo->partition.part_info.enablewrite);
			}
			diskinfo++;
			cur+=sizeof(disk_proto_t);
			
		}
	}else if(msg_id == MSG_DISK_DIRLIST){
		dirinfo = (disk_dirlist_t*)response;
		cur = 0;
		while(cur < reslen){
			printf("DirListPart INFO:\n\tName:%s\n\tPartname:%s\n\tType:%s\n\tDiskTag:%s\n\tFstype:%s\n\tLabel:%s\n"
					"\tMountPoint:%s\n\tMounted:%d\n\tEnablewrite:%d\n", dirinfo->devname, dirinfo->partname, 
					dirinfo->type, dirinfo->disktag, dirinfo->fstype, dirinfo->label,
					dirinfo->mntpoint, dirinfo->mounted, 
					dirinfo->enablewrite);	

			dirinfo++;
			cur+=sizeof(disk_dirlist_t);
		}
	}else if(msg_id == MSG_DISK_DISKLIST){
		disklist = (disk_disklist_t*)response;
		cur = 0;
		while(cur < reslen){
			 printf("DiskMain INFO:\n\tName:%s\n\tTotal:%llu\n\tVendor:%s\n\tSerical:%s\n"
					 "\tType:%s\n\tDiskTag:%s\n\tStatus:%d\n\tPartnum:%d\n", disklist->devname, disklist->total,
					 disklist->vendor, disklist->serical,
					 disklist->type, disklist->disktag, disklist->status, disklist->partnum);
			disklist++;
			cur+=sizeof(disk_disklist_t);
		 }
	}

	if(response){
		free(response);
	}
	return 0;
}
