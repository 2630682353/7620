#include "comlib.h"
#include "disk_manager.h"
#include "libblkid-tiny.h"
#include <linux/hdreg.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#include <ctype.h>
#include <uci.h>
#include "sg_io.h"
#define _GNU_SOURCE    /* or _SVID_SOURCE or _BSD_SOURCE */
#include <mntent.h>
#include "diskm_ipc_msg.h"


#define MMCBLK_MAX_PART	8
#define DISK_LIMIT			(100*1024*1024*1024) //Byte
#define DISK_DEVICE_PATH	"/proc/devices"
#define DISK_PROC_PARTITION	"/proc/partitions"
#define DISK_MNT_PARAMETER	"/etc/config/mntpara"
#define DISK_MNT_SERVICE	"/etc/config/mntservice"
#define DISK_USER_LIST		"/etc/config/system2"
#define DISK_SMB_USER_ONOFF		"/etc/system.conf"
#define DISK_COMMERICAL_PATH		"/proc/modules"
#define PATH_MTAB_FILE         "/proc/mounts"
#define DISK_TAG_FILE		"/etc/init.d/disktag"
#define DISK_MNT_PREFIX	"/tmp/mnt"
#define DISK_MNT_DIR		"/etc/mnt_preffix"
#define UPNPD_EVENT_IPC	"/tmp/kinston.ipc"
#define DISK_SAMBA_PATH	"/var/etc/smb.conf"
#define DISK_SAMBA_PATH_ADMIN	"/var/etc/smb.conf.admin"
#define DISK_SAMBA_PATH_GUEST	"/var/etc/smb.conf.guest"
#define DISK_SAMBA_DIR	"/var/etc/"

#define DISK_KINGSTON	1

static const char smb_head[] =
	"[global]\n"
	"\tnetbios name = OpenWrt\n"
	"\tdisplay charset = UTF-8\n"
    //"\tinterfaces = loopback lan\n"	
   	"\tserver string = OpenWrt\n"
	"\tunix charset = UTF-8\n"
	"\tworkgroup = WORKGROUP\n"
	"\tdeadtime = 30\n"
   	"\tdomain master = yes\n"
   	"\tencrypt passwords = true\n"
	"\tenable core files = no\n"
	"\tguest account = nobody\n"
	"\tguest ok = yes\n"
	"\tinvalid users = root\n"
	"\tlocal master = yes\n"
	"\tload printers = no\n"
	"\t;map to guest = Bad User\n"
	"\tmax protocol = SMB2\n"
	"\tmin receivefile size = 16384\n"
	"\tnull passwords = yes\n"
	"\tobey pam restrictions = yes\n"
	"\tos level = 20\n"
	"\tpassdb backend = smbpasswd\n"
	"\tpreferred master = yes\n"
	"\tprintable = no\n"
	"\tsecurity = user\n"
	"\tsmb encrypt = disabled\n"
	"\tsmb passwd file = /etc/samba/smbpasswd\n"
	"\tsocket options = TCP_NODELAY IPTOS_LOWDELAY\n"
	"\tsyslog = 2\n"
	"\tuse sendfile = yes\n"
	"\twriteable = yes\n"
	"\tconfig file = /var/etc/smb.conf.%U\n";

static const char smb_head_share[] =
	"[global]\n"
	"\tnetbios name = OpenWrt\n"
	"\tdisplay charset = UTF-8\n"
	"\tinterfaces = loopback lan\n"	
   	"\tserver string = OpenWrt\n"
	"\tunix charset = UTF-8\n"
	"\tworkgroup = WORKGROUP\n"
	"\tdeadtime = 30\n"
   	"\tdomain master = yes\n"
   	"\tencrypt passwords = true\n"
	"\tenable core files = no\n"
	"\tguest account = nobody\n"
	"\tguest ok = yes\n"
	"\tinvalid users = root\n"
	"\tlocal master = yes\n"
	"\tload printers = no\n"
	"\tmap to guest = Bad User\n"
	"\tmax protocol = SMB2\n"
	"\tmin receivefile size = 16384\n"
	"\tnull passwords = yes\n"
	"\tobey pam restrictions = yes\n"
	"\tos level = 20\n"
	"\tpassdb backend = smbpasswd\n"
	"\tpreferred master = yes\n"
	"\tprintable = no\n"
	"\tsecurity = user\n"
	"\tsmb encrypt = disabled\n"
	"\tsmb passwd file = /etc/samba/smbpasswd\n"
	"\tsocket options = TCP_NODELAY IPTOS_LOWDELAY\n"
	"\tsyslog = 2\n"
	"\tuse sendfile = yes\n"
	"\twriteable = yes\n";

static const char smb_conent_share[] = 
	"[%s]\n"
	"\tpath = %s\n"
	"\tread only = no\n"
	"\tguest ok = yes\n"
	"\tcreate mask = 0700\n"
	"\tdirectory mask = 0700\n";

static const char smb_conent[] = 
	"[%s%s]\n"
	"\tpath = %s%s\n"
	"\tread only = no\n"
	"\tvalid users = %s\n"
	"\tcreate mask = 0700\n"
	"\tdirectory mask = 0700\n";

enum{
	DISK_MMC_MAIN=1,
	DISK_MMC_PART,
	DISK_USB_MAIN,
	DISK_USB_PART,
	DISK_MAIN = 99,
	DISK_PART = 100
};

typedef struct _event_block{
	char action[32];
	char type[32];
	char	dev[32];
	int partnum;	
}event_block; //notify upnpd disk add or remove

typedef struct _disk_mnt_para_t{	
	struct list_head node;
	char fsname[DSHORT_STR];
	char readahead[DSHORT_STR];
	int rw;
	char umask[DSHORT_STR];
	char iocharset[DSHORT_STR];
	char shortname[DSHORT_STR];
	char errors[DSHORT_STR];
}disk_mnt_para_t;

typedef struct _disktag_t{	
	struct list_head node;
	char busflag[DSHORT_STR];
	char disktag[DSHORT_STR];	
	char diskvolume[DSHORT_STR];
}disktag_t;

typedef struct _disk_major_t{
	struct list_head node;
	int major;
	char name[DSHORT_STR];
}disk_major_t;

typedef struct _disk_baseinfo_t{
	char devname[DSHORT_STR];
	int major;
	int minor;
	unsigned long long total;
	unsigned long long used;
}disk_baseinfo_t;

typedef struct _disk_partinfo_t{
	struct list_head node;
	disk_baseinfo_t info;
	int mounted;
	char fstype[DSHORT_STR];
	char label[DSHORT_STR];
	char mntpoint[DMID_STR];
	int enablewrite;
	time_t uptime;
}disk_partinfo_t;


typedef struct _disk_maininfo_t{	
	struct list_head node;
	disk_baseinfo_t info;
	char vendor[DSHORT_STR];
	char serical[DMID_STR];
	char type[DSHORT_STR]; //usb or sdcard
	char disktag[DSHORT_STR];
	int status; //mounted or saferemove
	int partnum; //more than 0
	int isgpt;
	struct list_head partlist;
}disk_maininfo_t;

typedef struct _disk_info_t{
	struct list_head list;
	int disk_num;
	struct list_head disk_major;	
	struct list_head mnt_parameter;
	struct list_head mnt_disktag;
}disk_info_t;

typedef struct _duser_list{
	char username[DSHORT_STR];
	char passwd[DMID_STR];
	char directory[DMAX_STR];
	int enable;
}duser_list;

static disk_info_t *i4disk = NULL;
#define LED_CONTROL_ON  system("tlcy gpio $(sed -n 's/GPIO_USB=//p' /etc/firmware) on &")
//#define LED_CONTROL_ON  system("pioctl hdderr 0")

#define LED_CONTROL_OFF  system("tlcy gpio $(sed -n 's/GPIO_USB=//p' /etc/firmware) off &")
//#define LED_CONTROL_OFF  system("pioctl hdderr 1")

/*Function Declear*/
int disk_chk_init(void);
void disk_print_partition_info(disk_info_t *pdisk);

int disk_major_list_parse(disk_info_t *pdisk)
{
	FILE *procpt = NULL;
	char line[DSHORT_STR] = {0}, ptname[DSHORT_STR];
	int ma;
	disk_major_t *node;

	if(!pdisk){
		DISKCK_DBG("Please INIT disk structure...\n");
		return DISK_FAILURE;
	}
	if ((procpt = fopen(DISK_DEVICE_PATH, "r")) == NULL) {
		DISKCK_DBG("Fail to fopen(%s)", DISK_DEVICE_PATH);		
		return DISK_FAILURE;
	}

	while (fgets(line, sizeof(line), procpt) != NULL) {
		memset(ptname, 0, sizeof(ptname));
		if (sscanf(line, " %d %[^\n ]",
			&ma,  ptname) != 2)
			continue;
		if (strcmp(ptname, "sd") != 0 &&
				strcmp(ptname, "mmc") != 0) {
			continue;
		}

		node = calloc(1, sizeof(disk_major_t));
		if(node == NULL){
			DISKCK_DBG("Memory Calloc Failed\n");
			fclose(procpt);			
			return DISK_FAILURE;
		}
		node->major = ma;
		snprintf(node->name, DSHORT_STR, "%s", ptname);		
		list_add_tail(&node->node, &pdisk->disk_major);
		memset(line, 0, sizeof(line));
	}
	
	fclose(procpt);
	return DISK_SUCCESS;
}

int disk_mnt_parameter_parse(disk_info_t *pdisk)
{
	struct uci_element *se, *oe;
	struct uci_section *section;
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;	
	struct uci_option *option;
	disk_mnt_para_t *pamter, *_pamter;

	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, DISK_MNT_PARAMETER, &pkg)) {
		uci_free_context(ctx);
		DISKCK_DBG("UCI Alloc Failed\n");
		return DISK_FAILURE;
	}
	uci_foreach_element(&pkg->sections, se) {
		section = uci_to_section(se);
		if (section == NULL)
			continue;
		pamter = calloc(1, sizeof(disk_mnt_para_t));
		if(pamter == NULL){
			DISKCK_DBG("Calloc Memeory Error\n");
			uci_unload(ctx,pkg);
			uci_free_context(ctx);
			return DISK_FAILURE;
		}
		uci_foreach_element(&section->options, oe) {
			option = uci_to_option(oe);
			if (option == NULL){
					continue;
			}
			if(strcmp(oe->name, "fsname") == 0){
				strcpy(pamter->fsname, option->v.string);
			}else if(strcmp(oe->name, "readahead") == 0){
				strcpy(pamter->readahead, option->v.string);
			}else if(strcmp(oe->name, "rw") == 0){
				pamter->rw = atoi(option->v.string);
			}else if(strcmp(oe->name, "umask") == 0){
				strcpy(pamter->umask, option->v.string);
			}else if(strcmp(oe->name, "iocharset") == 0){
				strcpy(pamter->iocharset, option->v.string);
			}else if(strcmp(oe->name, "shortname") == 0){
				strcpy(pamter->shortname, option->v.string);
			}else if(strcmp(oe->name, "errors") == 0){
				strcpy(pamter->errors, option->v.string);
			}else{
				DISKCK_DBG("Unknown Mount Paramter-->%s/%s\n", oe->name,  option->v.string);
			}
		}		
		list_add_tail(&pamter->node , &pdisk->mnt_parameter);
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx);
	
	list_for_each_entry_safe(pamter, _pamter, &(pdisk->mnt_parameter), node) {
		printf("\nFsname:%s\n", pamter->fsname);
		if(pamter->readahead[0])
			printf("\treadahead:%s\n", pamter->readahead);
		printf("\trw:%d\n", pamter->rw);	
		if(pamter->umask[0])
			printf("\tumask:%s\n", pamter->umask);		
		if(pamter->iocharset[0])
			printf("\tiocharset:%s\n", pamter->iocharset);	
		if(pamter->shortname[0])
			printf("\tshortname:%s\n", pamter->shortname);
		printf("\n");
	}

	return DISK_SUCCESS;
}

int disk_mnt_disktag_parse(disk_info_t *pdisk)
{
        FILE *procpt = NULL;
        char line[DMID_STR]={0}, diskflag[DSHORT_STR], disktag[DSHORT_STR], diskvolume[DSHORT_STR];
	disktag_t *ptag, *pptag, *ntag;
	
	if(!pdisk){
		DISKCK_DBG("Please INIT disk structure...\n");
		return DISK_FAILURE;
	}

        if ((procpt = fopen(DISK_TAG_FILE, "r")) == NULL) {
                DISKCK_DBG("Fail to fopen(%s), DiskTag List is NULL", DISK_TAG_FILE);
                return DISK_FAILURE;
        }

        while (fgets(line, sizeof(line), procpt) != NULL) {
                memset(disktag, 0, sizeof(disktag));			
                memset(diskflag, 0, sizeof(diskflag));				
                memset(diskvolume, 0, sizeof(diskvolume));
                if (sscanf(line, " %s  %s %[^\n ]",
                                diskflag, disktag, diskvolume) != 3)
                        continue;
		list_for_each_entry_safe(ptag, pptag, &pdisk->mnt_disktag, node) {
			if(strcmp(diskflag, ptag->busflag) == 0){
				DISKCK_DBG("Update DiskTag: %s==>%s->%s...\n", diskflag, ptag->disktag, disktag);
				strcpy(ptag->disktag, disktag);				
				strcpy(ntag->diskvolume, diskvolume);
				continue;
			}
		}
		/*New Node to insert*/
		ntag = calloc(1, sizeof(disktag_t));
		if(ntag == NULL){
			DISKCK_DBG("Calloc Memory Failed\n");
			fclose(procpt);
			return DISK_FAILURE;
		}
		strcpy(ntag->busflag, diskflag);
		strcpy(ntag->disktag, disktag);
		strcpy(ntag->diskvolume, diskvolume);
		list_add_tail(&ntag->node , &pdisk->mnt_disktag);
		DISKCK_DBG("Insert DISKTAT-->%s %s %s\n", ntag->busflag, ntag->disktag, ntag->diskvolume);
		memset(line, 0, sizeof(line));
        }
	fclose(procpt);

	return DISK_SUCCESS;
}

int disk_aciton_notify_upnp(disk_partinfo_t *part, int action, char *dev)
{
	int fd;
	event_block event;
	

	fd = diskm_ipc_client_init(UPNPD_EVENT_IPC);
	if(fd < 0){
		DISKCK_DBG("Error\n");
		return DISK_FAILURE;
	}
	memset(&event, 0, sizeof(event_block));
	if(action == DISK_UDEV_ADD){
		strcpy(event.action, "add");
	}else if(action == DISK_UDEV_REMOVE){
		strcpy(event.action, "remove");
	}else{
		DISKCK_DBG("Unknown Inotify Aciton=%d\n", action);
		close(fd);
		return DISK_FAILURE;
	}
#ifdef DISK_KINGSTON
	char *ptr = NULL;
	if((ptr = strstr(part->mntpoint, "USB")) != NULL){
		strcpy(event.type, "USB");
		event.partnum = atoi(ptr+strlen("USB"));
	}else if((ptr = strstr(part->mntpoint, "SD_Card")) != NULL){
		strcpy(event.type, "SD_CARD");
		event.partnum = atoi(ptr+strlen("SD_CARD"));
	}else if((ptr = strstr(part->mntpoint, "TF_Card")) != NULL){
		strcpy(event.type, "TF_Card");
		event.partnum = atoi(ptr+strlen("TF_Card"));
	}else{
		strcpy(event.type, dev);
		event.partnum = atoi(part->info.devname+strlen(dev));
	}
#else
	strcpy(event.type,  dev);
	event.partnum = atoi(part->info.devname+strlen(dev));
#endif
	DISKCK_DBG("Notify To UPNPD: %s%d %s\n", event.type, event.partnum, event.action);
	diskm_ipc_write(fd, (char*)&event, sizeof(event_block));

	close(fd);

	return DISK_SUCCESS;
}

int disk_update_samba_config_share(disk_info_t *pdisk, char *smbconfig)
{
	int fd, wbyte, hdrlen;
	char smbconf[DMID_STR] = {0}, smbcnt[DMAX_STR], coment[DMID_STR];
	char *ptr;
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode;

	if(pdisk == NULL || smbconfig == NULL){
		return DISK_FAILURE;
	}

	sprintf(smbconf, "%s.%ld", smbconfig, time(NULL));
	DISKCK_DBG("Generate Samba Config %s\n", smbconf);
	fd = open(smbconf, O_CREAT|O_TRUNC|O_RDWR, 0755);
	if(fd < 0){
		DISKCK_DBG("Error Create %s [%s]...\n", smbconf, strerror(errno));		
	}
	hdrlen = strlen(smb_head_share);
	wbyte = write(fd, smb_head_share, hdrlen);
	if(wbyte != hdrlen){
		DISKCK_DBG("Error Write SMABA Header [%s]...\n", strerror(errno));
		close(fd);
		remove(smbconf);
		return DISK_FAILURE;
	}
	list_for_each_entry_safe(node, _node, &(pdisk->list), node) {
		if(node->status != DISK_MOUNTED){
			continue;
		}
		list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node) {
			if(pnode->mounted != 1){
				continue;
			}
			memset(coment, 0, sizeof(coment));
			if(strstr(pnode->mntpoint, mnt_preffix) == NULL){
				ptr = strrchr(pnode->mntpoint, '/');
				strcpy(coment, ptr?(ptr+1):pnode->mntpoint);
			}else{
				strcpy(coment, pnode->mntpoint+strlen(mnt_preffix)+1);
			}	
			if(coment[strlen(coment)-1] == '/'){
				coment[strlen(coment)-1] = '\0';
			}
			if(strlen(coment)  == 0){
				strcpy(coment,  pnode->info.devname);
			}			
			while((ptr = strchr(coment, '/'))){
				*ptr = '_';
			}
			memset(smbcnt, 0, sizeof(smbcnt));
			sprintf(smbcnt, smb_conent_share, coment, pnode->mntpoint);
			wbyte = write(fd, smbcnt, strlen(smbcnt));
			if(wbyte != strlen(smbcnt)){
				DISKCK_DBG("Error Write SMABA Content [%s]...\n", strerror(errno));
				close(fd);
				remove(smbconf);
				return DISK_FAILURE;
			}
		}
	}
	close(fd);

	if(rename(smbconf, smbconfig)){
		DISKCK_DBG("Rename Samba Config Failed [%s]...\n", strerror(errno));
		remove(smbconf);
		return DISK_FAILURE;
	}
#if 0	
	/*KIll Samba progress*/
	DISKCK_DBG("Restart Samba Progress...\n");
	exec_cmd("killall -KILL smbd");
	exec_cmd("killall -KILL nmbd");
	exec_cmd("/usr/sbin/smbd -D");
	exec_cmd("/usr/sbin/nmbd -D");
#endif
	DISKCK_DBG("Update Samba Config Successful...\n");
	return DISK_SUCCESS;
}

int disk_update_samba_config_user(disk_info_t *pdisk, char *path, char *username, char *directory)
{
	int fd, wbyte, uselen = 0, total = 0;
	char *smb_cnt, smbcnt[DMAX_STR], coment[DMID_STR], dispaly[DMID_STR];
	char *ptr;
	char config[DMID_STR] = {0};
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode;

#define SMB_MALLOC_SIZE		10*1024
	if(pdisk == NULL || path == NULL){
		return DISK_FAILURE;
	}
	smb_cnt = calloc(1, SMB_MALLOC_SIZE);
	if(smb_cnt == NULL){
		DISKCK_DBG("Calloc Memory Failed [%s]...\n", strerror(errno));
		return DISK_FAILURE;
	}
	uselen = strlen(smb_head);
	total = SMB_MALLOC_SIZE;
	memcpy(smb_cnt, smb_head, uselen);
	if(username == NULL && directory == NULL){
		DISKCK_DBG("Direct wirte header only...\n");
		goto write_conf;
		
	}
	list_for_each_entry_safe(node, _node, &(pdisk->list), node) {
		if(node->status != DISK_MOUNTED){
			continue;
		}
		list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node) {
			if(pnode->mounted != 1){
				continue;
			}
			memset(coment, 0, sizeof(coment));
			if(strstr(pnode->mntpoint, mnt_preffix) == NULL){
				ptr = strrchr(pnode->mntpoint, '/');
				strcpy(coment, ptr?(ptr+1):pnode->mntpoint);
			}else{
				strcpy(coment, pnode->mntpoint+strlen(mnt_preffix)+1);
			}	
			if(coment[strlen(coment)-1] == '/'){
				coment[strlen(coment)-1] = '\0';
			}
			if(strlen(coment)  == 0){
				strcpy(coment,  pnode->info.devname);
			}
			while((ptr = strchr(coment, '/'))){
				*ptr = '_';
			}
			memset(smbcnt, 0, sizeof(smbcnt));
			memset(dispaly, 0, sizeof(dispaly));
			strncpy(dispaly, directory, sizeof(dispaly)-1);
			while((ptr = strchr(dispaly, '/'))){
				*ptr = '_';
			}
			if(strlen(directory) == 1 && directory[0] == '/'){
				wbyte = sprintf(smbcnt, smb_conent, coment, "", pnode->mntpoint, "", username);
			}else{
				wbyte = sprintf(smbcnt, smb_conent, coment, dispaly, pnode->mntpoint, directory, username);
			}
			if(uselen +wbyte > total){
				smb_cnt = realloc(smb_cnt, SMB_MALLOC_SIZE+uselen);
				if(smb_cnt == NULL){
					DISKCK_DBG("Realloc memory Failed[%s]...\n", strerror(errno));
					return DISK_FAILURE;
				}
				total = SMB_MALLOC_SIZE+uselen;
			}
			memcpy(smb_cnt+uselen, smbcnt, wbyte);
			uselen += wbyte;	
		}
	}
write_conf:
	if(username == NULL){
		strncpy(config, path, sizeof(config)-1);
	}else{
		snprintf(config, sizeof(config)-1, "%s.%s", path, username);
	}
	fd = open(config, O_CREAT|O_TRUNC|O_RDWR, 0755);
	if(fd < 0){
		DISKCK_DBG("Error Create %s [%s]...\n", config, strerror(errno));		
		free(smb_cnt);
		return DISK_FAILURE;
	}
	wbyte = write(fd, smb_cnt, uselen);
	if(wbyte != uselen){
		DISKCK_DBG("Error Write SMABA Header [%s]...\n", strerror(errno));
		close(fd);
		free(smb_cnt);
		return DISK_FAILURE;
	}
	close(fd);
	free(smb_cnt);

	
	DISKCK_DBG("Update %s Samba Config[%s] Successful...\n", username?username:"main", config);
	return DISK_SUCCESS;
}

int disk_get_userlist(void **ulist, int *ulen)
{
	struct uci_element *se, *oe;
	struct uci_section *section;
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;	
	struct uci_option *option;
	duser_list user;
	void *tmpuser = NULL;
	int tmpulen = 0, chkflag = 0, usecnt = 0;
	
	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, DISK_USER_LIST, &pkg)) {
		uci_free_context(ctx);
		DISKCK_DBG("UCI Alloc Failed\n");
		return -1;
	}
	uci_foreach_element(&pkg->sections, se) {
		section = uci_to_section(se);
		if (section == NULL)
			continue;
		
		memset(&user, 0, sizeof(user));		
		chkflag = 0;
		uci_foreach_element(&section->options, oe) {
			option = uci_to_option(oe);
			if (option == NULL){
					continue;
			}		
			if(strcmp(oe->name, "user") == 0){
				strncpy(user.username, option->v.string, sizeof(user.username)-1);
				chkflag++;
			}else if(strcmp(oe->name, "pwd") == 0){
				strncpy(user.directory, option->v.string, sizeof(user.directory)-1);
				if(user.directory[0] != '/'){
					DISKCK_DBG("illigel directory %s\n", user.directory);
					chkflag = -1;
					continue;
				}
				chkflag++;
			}else if(strcmp(oe->name, "passwd") == 0){
				strncpy(user.passwd, option->v.string, sizeof(user.passwd)-1);		
			}else if(strcmp(oe->name, "enable") == 0){
				user.enable = atoi(option->v.string);				
				chkflag++;
			}else{
				DISKCK_DBG("Unknown Service Action-->%s/%s\n", oe->name,  option->v.string);
			}			
		}
		if(chkflag < 3){
			DISKCK_DBG("Inleggle user config name:%s\npwd:%s\nenable:%d\n", 
					user.username, user.directory, user.enable);
			continue;
		}
		tmpuser = realloc(tmpuser, tmpulen+sizeof(duser_list));
		if(tmpuser == NULL){
			DISKCK_DBG("Calloc Memory Failed[%s]\n", strerror(errno));
			uci_unload(ctx,pkg);
			uci_free_context(ctx);
			return -1;
		}
		memcpy(tmpuser+tmpulen, &user, sizeof(duser_list));
		tmpulen += sizeof(duser_list);
		usecnt++;
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx);

	*ulist = tmpuser;
	*ulen = tmpulen;
	DISKCK_DBG("Found %d Users in Config\n", usecnt);

	return usecnt;
}

/*
* 0 : private
* 1 : public
*/
int disk_check_smb_user_onoff(char *config)
{
	int keylen;
	FILE *fp;
	char line[DMAX_STR] = {0};
#define SMB_USER_ONOFF		"smb_public"

	if(config == NULL){
		return 1;
	}
	fp = fopen(config, "r");
	if (fp == NULL){
		return 1;
	}
	keylen = strlen(SMB_USER_ONOFF);
	while (fgets(line, 2048, fp)) {
		if (strncmp(SMB_USER_ONOFF, line, keylen) == 0) {
			if (line[keylen] == '=') {
				fclose(fp);
				DISKCK_DBG("Public SMB is %d...\n", atoi(line+keylen+1));
				return atoi(line+keylen+1);
			}
		}
	}
	fclose(fp);
	return 1;
}
int disk_update_samba_config(disk_info_t *pdisk)
{
	int ret;
	void *userbuf = NULL;
	int userlen = 0, cur = 0;
	duser_list *puser = NULL;

	/*Get Userlist*/
	if(disk_check_smb_user_onoff(DISK_SMB_USER_ONOFF) == 1
			|| disk_get_userlist(&userbuf, &userlen) <= 0){
		DISKCK_DBG("Found Error or zero User...\n");
		return disk_update_samba_config_share(pdisk, DISK_SAMBA_PATH);
	}
	
	ret = disk_update_samba_config_user(pdisk, DISK_SAMBA_PATH, NULL, NULL);
	if(ret == DISK_FAILURE){
		DISKCK_DBG("Update Main Samba Config Failed...\n");
		return DISK_FAILURE;
	}

	puser = (duser_list *)userbuf;
	cur = 0;
	while(cur < userlen){
		ret = disk_update_samba_config_user(pdisk, DISK_SAMBA_PATH, puser->username,puser->directory);
		if(ret == DISK_FAILURE){
			DISKCK_DBG("Update %s Samba Config Failed...\n", puser->username);
			free(userbuf);
			return DISK_FAILURE;
		}
		puser++;
		cur += sizeof(duser_list);
	}
	

	if(userbuf&& userlen){
		free(userbuf);
	}

	return DISK_SUCCESS;
}

int disk_restart_related_service(char *srvname, char *action)
{
	struct uci_element *se, *oe;
	struct uci_section *section;
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;	
	struct uci_option *option;
	char srv[DSHORT_STR], srvPath[DMID_STR];
#define DEFAULT_ACITON		"restart"	

	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, DISK_MNT_SERVICE, &pkg)) {
		uci_free_context(ctx);
		DISKCK_DBG("UCI Alloc Failed\n");
		return DISK_FAILURE;
	}
	uci_foreach_element(&pkg->sections, se) {
		section = uci_to_section(se);
		if (section == NULL)
			continue;
		
		memset(srv, 0, sizeof(srv));
		memset(srvPath, 0, sizeof(srvPath));
		uci_foreach_element(&section->options, oe) {
			option = uci_to_option(oe);
			if (option == NULL){
					continue;
			}
		
			if(strcmp(oe->name, "service") == 0){
				strcpy(srv, option->v.string);
			}else if(strcmp(oe->name, "path") == 0){
				strcpy(srvPath, option->v.string);
			}else{
				DISKCK_DBG("Unknown Service Action-->%s/%s\n", oe->name,  option->v.string);
			}
		}	
		if(srvname && strcmp(srv, srvname) == 0){
			DISKCK_DBG("WARNING: %s %s[%s]\n", srv, action, srvPath);
			exec_cmd("%s %s &", srvPath, action);
			break;
		}else if(srvname == NULL){
			exec_cmd("%s %s &", srvPath, action==NULL?DEFAULT_ACITON:action);				
			DISKCK_DBG("DEFAULT: %s %s[%s]\n", srv, action==NULL?DEFAULT_ACITON:action, srvPath);
		}		
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx);
	
	return DISK_SUCCESS;
}

int disk_dirlist_display_name(char *preffix, char *before, char *after)
{
	char *flag = NULL;
	
	if(!before || !after){
		return DISK_FAILURE;
	}
	if(preffix && strlen(before) < strlen(preffix)){
		return DISK_FAILURE;
	}
	if(preffix && strncmp(before, preffix, strlen(preffix))){
		DISKCK_DBG("No found %s in %s...\n", preffix, before);
		return DISK_FAILURE;
	}
	flag = before+strlen(preffix);
	while(*flag == '/'){
		flag++;
	}
	strcpy(after, flag);
	while((flag = strchr(after, '/'))){
		*flag = '_';
	}
	DISKCK_DBG("Diskplay name %s->%s...\n", before, after);
	
	return DISK_SUCCESS;
}

char* find_mount_point(char *block)
{
	FILE *fp = fopen("/proc/mounts", "r");
	char line[DMID_STR];
	int len = strlen(block);
	char *point = NULL;

	if(!fp)
		return NULL;

	while (fgets(line, sizeof(line), fp)) {
		if (!strncmp(line, block, len)) {
			char *p = &line[len + 1];
			char *t = strstr(p, " ");

			if (!t) {
				fclose(fp);
				return NULL;
			}
			*t = '\0';
			point = p;
			break;
		}
	}

	fclose(fp);

	return point;
}

int check_commerical_driver(char *fstype, char *caldrv)
{
	FILE *procpt;
	char line[DMID_STR] = {0}, drver[DSHORT_STR];
	int size;
	char *fsflag  = NULL;
	
	if(fstype == NULL || caldrv == NULL){
		return DISK_FAILURE;
	}
	if(strcmp(fstype, "vfat") == 0||
			strcmp(fstype, "fat32") == 0){
		fsflag = "tfat";
	}else if(strcmp(fstype, "exfat") == 0){
		fsflag = "texfat";
	}else if(strcmp(fstype, "ntfs") == 0){
		fsflag = "tntfs";
	}else if(strncmp(fstype, "hfs", 3) == 0){
		fsflag = "thfsplus";
	}else{
		DISKCK_DBG("Unknown filesystem (%s)", fstype);
		return DISK_FAILURE;
	}
        if ((procpt = fopen(DISK_COMMERICAL_PATH, "r")) == NULL) {
                DISKCK_DBG("Fail to fopen(%s)", DISK_COMMERICAL_PATH);
		return DISK_FAILURE;
        }

        while (fgets(line, sizeof(line), procpt) != NULL) {
                memset(drver, 0, sizeof(drver));
                if (sscanf(line, " %s %d",
                                drver, &size) != 2)
                        continue;

		if(strcmp(fsflag, drver) == 0){
			strcpy(caldrv, drver);
			fclose(procpt); 
			DISKCK_DBG("Found Commerical Filesystem Driver (%s)\n", caldrv);
			return DISK_SUCCESS;
		}
		memset(line, 0, sizeof(line));
        }
	fclose(procpt);

	return DISK_FAILURE;
}

int generate_mount_parameter(struct list_head *plist, char *fstype, char *para)
{
	disk_mnt_para_t *pamter, *_pamter;
	int byte=0;
	char *tpara = para;

	if(fstype == NULL || para == NULL){
		return DISK_FAILURE;
	}
	list_for_each_entry_safe(pamter, _pamter, plist, node) {
		if(strcmp(pamter->fsname, fstype) == 0){
			/*Find the filesystem*/
			if(pamter->iocharset[0]){
				byte = sprintf(tpara, "iocharset=%s,", pamter->iocharset);				
				tpara += byte;
			}
			if(pamter->readahead[0]){
				byte = sprintf(tpara, "readahead=%s,", pamter->readahead);				
				tpara += byte;
			}
			if(pamter->shortname[0]){
				byte = sprintf(tpara, "shortname=%s,", pamter->shortname);				
				tpara += byte;
			}
			if(pamter->errors[0]){
				byte = sprintf(tpara, "errors=%s,", pamter->errors);				
				tpara += byte;
			}			
			if(pamter->umask[0]){
				byte = sprintf(tpara, "umask=%s,", pamter->umask);				
				tpara += byte;
			}
			if(pamter->rw){
				byte = sprintf(tpara, "%s,", "rw");				
				tpara += byte;
			}
			if(tpara != para){
				*(--tpara) = '\0';
			}
			DISKCK_DBG("Mount Parameter is %s\n", para);
			return DISK_SUCCESS;
		}
	}

	return DISK_SUCCESS;
}


int generate_mount_point(char *disktag, disk_partinfo_t *pinfo)
{
	int volnum = 0, numax = 0;
	char tmntdir[DMAX_STR] = {0}, *ptr = NULL;
	
	if(access(mnt_preffix, F_OK) != 0){
		mkdir(mnt_preffix, 0755);
	}
	ptr = pinfo->info.devname+strlen(pinfo->info.devname)-1;
	while(isdigit(*ptr) && numax < 2){
		volnum += ((numax*10)+(*ptr-'0'));
		numax++;
		ptr--;
	}
	if(strlen(disktag) == 0){
		sprintf(pinfo->mntpoint, "%s/%s", mnt_preffix, pinfo->info.devname);
	}else{
		sprintf(pinfo->mntpoint, "%s/%s%d", mnt_preffix, disktag, volnum==0?1:volnum);
	}
	DISKCK_DBG("Generate Mount Dir ==>%s\n", pinfo->mntpoint);
	if(access(pinfo->mntpoint, F_OK) != 0){
		ptr = strrchr(pinfo->mntpoint, '/');
		if(ptr){
			strncpy(tmntdir, pinfo->mntpoint, ptr-pinfo->mntpoint);
			if(access(tmntdir, F_OK) != 0){
				mkdir(tmntdir, 0777);
			}
		}
		mkdir(pinfo->mntpoint, 0777);
	}
	return DISK_SUCCESS;
}

int update_partition_capacity_rw(disk_partinfo_t *part)
{
	struct statfs s;
	char filename[DMAX_STR] = {0};
	int fd;
	time_t cmlog = 0;
#define WRITE_TIME_LIMIT	45

	if(part == NULL){
		return DISK_FAILURE;
	}
	
	if (!statfs(part->mntpoint, &s) && (s.f_blocks > 0)) {

		part->info.total = (unsigned long long)s.f_blocks * 
			(unsigned long long)s.f_bsize;
		part->info.used  = (unsigned long long)(s.f_blocks - s.f_bfree) * 
			(unsigned long long)s.f_bsize;
		DISKCK_DBG("Update %s Capacity Finish-->[%lld/%lld]\n", 
			part->info.devname, part->info.used, part->info.total);
	}

	cmlog = time(NULL)-part->uptime;
	if(part->enablewrite == 0 || 
		cmlog  >= WRITE_TIME_LIMIT){
		DISKCK_DBG("Cache time Out or %s Read Only...\n", part->info.devname);		
		sprintf(filename, "%s/.readonly.tst", part->mntpoint);
		fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC, 0755);
		if(fd < 0 && errno == EROFS){
			DISKCK_DBG("WARNING %s Read Only...\n", part->info.devname);		
			part->enablewrite = 0;
		}else{
			DISKCK_DBG("%s Enable Write...\n", part->info.devname);		
			part->enablewrite = 1;
		}
		close(fd);
		remove(filename);
		part->uptime = time(NULL);
	}else{
		DISKCK_DBG("Cache time left %lds...\n", WRITE_TIME_LIMIT -cmlog);		
	}
	return DISK_SUCCESS;
}

int get_partition_capacity_rw(disk_partinfo_t *part)
{
	FILE *file = NULL;
	struct mntent *mount_entry = NULL, mntdata;
	char mbuf[DMID_STR] = {0}, devbuf[DSHORT_STR] = {0};
	int status = 0;
	struct statfs s;
	char filename[DMAX_STR] = {0};
	int fd;

	if(part == NULL){
		return DISK_FAILURE;
	}
	/* Open mount file	*/
	if ((file = setmntent(PATH_MTAB_FILE, "r")) == NULL) {
		DISKCK_DBG("System call setmntent return error\n");
		return DISK_FAILURE;
	}
	memset(mbuf, 0, sizeof(mbuf));
	sprintf(devbuf, "/dev/%s", part->info.devname);
	/* Read mount file  */
	do {
		mount_entry = getmntent_r(file, &mntdata, mbuf, sizeof(mbuf));
		if (mount_entry == NULL) { /* Finish read file */
			status = 0;
			break;
		}
		if (strcmp(devbuf, mount_entry->mnt_fsname)) {
			continue;
		}
		
		strcpy(part->mntpoint, mount_entry->mnt_dir);
		if (!statfs(part->mntpoint, &s) && (s.f_blocks > 0)) {	
			part->info.total = (unsigned long long)s.f_blocks * 
				(unsigned long long)s.f_bsize;
			part->info.used  = (unsigned long long)(s.f_blocks - s.f_bfree) * 
				(unsigned long long)s.f_bsize;
			DISKCK_DBG("Update %s Capacity Finish-->[%lld/%lld]\n", 
				part->info.devname, part->info.used, part->info.total);
		}
		status = 1;
		break; /* Terminate searching  */
		
	} while (1);

	endmntent(file);

	if(status == 0){
		DISKCK_DBG("%s No Found In mounts...\n", part->info.devname);
		return DISK_SUCCESS;
	}
	sprintf(filename, "%s/.readonly.tst", part->mntpoint);
	fd = open(filename, O_CREAT|O_TRUNC|O_RDWR, 0755);
	if(fd < 0 && errno == EROFS){
		DISKCK_DBG("WARNING %s Read Only...\n", part->info.devname);		
		part->enablewrite = 0;
	}else{
		DISKCK_DBG("%s Enable Write...\n", part->info.devname);		
		part->enablewrite = 1;
	}
	close(fd);
	remove(filename);	
	part->uptime = time(NULL);
	
	return DISK_SUCCESS;
}

void disk_trigger_udevadd(char *devname, int major)
{
	char cmdbuf[DMID_STR] = {0};
	if(devname == NULL){
		return;
	}
	
	sprintf(cmdbuf, "/usr/sbin/disktriger 4 %d %s", major, devname);
	DISKCK_DBG("Mannual Exe %s\n", cmdbuf);
	system(cmdbuf);
}
disk_major_t* disk_search_major_list(struct list_head *mlist, int major)
{
	disk_major_t *pnode, *_node;

	if(mlist == NULL){
		return NULL;
	}
	list_for_each_entry_safe(pnode, _node, mlist, node) {
		if(pnode->major == major){
			DISKCK_DBG("Found Major %d\n", major);
			return pnode;
		}
	}
	DISKCK_DBG("No Found Major %d\n", major);
	return NULL;
}

disk_maininfo_t* disk_search_partition_list(disk_info_t *pdisk, char *name, int search_part)
{
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode;

	if(!pdisk || !name){
		return NULL;
	}

	list_for_each_entry_safe(node, _node, &(pdisk->list), node) {
		if(search_part == 0){
			/*Just search Main Partition*/
			if(strcmp(node->info.devname, name) == 0){
				return node;
			}
		}else if(search_part == 1){
			if(strncmp(node->info.devname, name, strlen(node->info.devname)) == 0){
				list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node) {
					if(strcmp(pnode->info.devname, name) == 0){
						return node;
					}
				}
				return NULL;
			}
		}
	}

	return NULL;
}

int disk_fill_main_partition_baseinfo(disk_info_t *pdisk, disk_maininfo_t *mdisk)
{	
	char devbuf[DSHORT_STR] = {0}, rlink[DMID_STR], linkbuf[DMID_STR] = {0};
	int ret;
	disktag_t *ptag, *pptag;
	
	if(!mdisk){
		return DISK_FAILURE;
	}
	sprintf(devbuf, "/dev/%s", mdisk->info.devname);
	ret = sg_get_disk_vendor_serical(devbuf, mdisk->vendor, mdisk->serical);
	if(ret != 0){
		DISKCK_DBG("Get %s Vendor Serical Failed\n", devbuf);
	}
	ret = sg_get_disk_space(devbuf, &(mdisk->info.total));
	if(ret != 0){
		DISKCK_DBG("SCSI Get %s Capacity Failed, Use IOCTRL\n", devbuf);
		ret = ioctl_get_disk_space(devbuf, &(mdisk->info.total));
		if(ret != 0){
			DISKCK_DBG("IOCTRL Get %s Capacity Failed, Give up\n", devbuf);
		}
	}
	/*Get disk pt*/
	if(probe_ptable_gpt(devbuf) == 1){
		mdisk->isgpt = 1;
	}else{
		mdisk->isgpt = 0;
	}	
	DISKCK_DBG("Foggy Jundge %s PT is %s\n", 
			mdisk->info.devname, mdisk->isgpt ==1?"GPT":"MBR");
	/*Loop Up List*/
	sprintf(rlink, "/sys/block/%s", mdisk->info.devname);
	
	ret = readlink(rlink, linkbuf, DMID_STR-1);
	if(ret == -1){
		DISKCK_DBG("ReadLink %s Failed:%s..\n", rlink, strerror(errno));
		memset(mdisk->disktag, 0, sizeof(mdisk->disktag));
		return DISK_SUCCESS;
	}
	list_for_each_entry_safe(ptag, pptag, &pdisk->mnt_disktag, node) {
		DISKCK_DBG("LinkBuf=%s-->%s\n", linkbuf, ptag->busflag);
		if(strstr(linkbuf, ptag->busflag) != NULL){
			if(strcasecmp(ptag->disktag, "None") == 0){
				strcpy(mdisk->disktag, ptag->diskvolume);
			}else{
				sprintf(mdisk->disktag, "%s/%s", ptag->disktag, ptag->diskvolume);
			}
			DISKCK_DBG("Found DiskTag: %s==>%s..\n", ptag->busflag, mdisk->disktag);
			return DISK_SUCCESS;
		}
	}
	/*We Need to confirm Disktag*/
	if(strcpy(mdisk->type, "SD") == 0){
		strcpy(mdisk->disktag, "SD_Card");
		DISKCK_DBG("No Config DiskTag Finish[%s]..\n", mdisk->disktag);
		return DISK_SUCCESS;
	}
	memset(mdisk->disktag, 0, sizeof(mdisk->disktag));
	
	DISKCK_DBG("No DiskTag [%s]..\n", mdisk->info.devname);
	return DISK_SUCCESS;	
}

int disk_insert_main_partition(disk_info_t *pdisk, disk_maininfo_t **mdisk, char *devname)
{
	FILE *procpt = NULL;
	char line[DMID_STR]={0}, ptname[DSHORT_STR], devbuf[DMID_STR]= {0};
	int ma, mi, sz, ret;
	disk_maininfo_t *tmain = NULL;

	if(!pdisk || !mdisk || !devname){
		return DISK_FAILURE;
	}
	
	if ((procpt = fopen(DISK_PROC_PARTITION, "r")) == NULL) {
			DISKCK_DBG("Fail to fopen(%s)", DISK_PROC_PARTITION);
			return DISK_FAILURE;
	}

	while (fgets(line, sizeof(line), procpt) != NULL) {
				memset(ptname, 0, sizeof(ptname));
		if (sscanf(line, " %d %d %d %[^\n ]",
				&ma, &mi, &sz, ptname) != 4){				
			memset(line, 0, sizeof(line));
			continue;
		}	

		if (strcmp(ptname, devname) != 0) {
			memset(line, 0, sizeof(line));			
			continue;
		}
		memset(devbuf, 0, sizeof(devbuf));
		sprintf(devbuf, "/dev/%s", ptname);
		if(access(devbuf, F_OK) != 0){
			DISKCK_DBG("Create Node %s", devbuf);
			mknod(devbuf, S_IFBLK|0600, makedev(ma, mi));
		}
		tmain = calloc(1, sizeof(disk_maininfo_t));
		if(!tmain){
			fclose(procpt);
			return DISK_FAILURE;
		}
		strcpy(tmain->info.devname, devname);
		tmain->info.major = ma;
		tmain->info.minor = mi;
		tmain->info.total = sz *DSIZ_KB;
		INIT_LIST_HEAD(&tmain->partlist);
		ret = disk_fill_main_partition_baseinfo(pdisk, tmain);
		if(ret != 0){
			free(tmain);
			fclose(procpt);
			return DISK_FAILURE;
		}
		/*Insert it to list*/
		list_add_tail(&tmain->node, &pdisk->list);
		pdisk->disk_num++;
		*mdisk = tmain;
		break;
	}
	
	fclose(procpt);
	if(tmain == NULL)
		return DISK_FAILURE;
	return DISK_SUCCESS;
}

int disk_chk_partition_mounted(disk_info_t *pdisk, char *spedev)
{
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode;
	
	if(!pdisk){
		DISKCK_DBG("Please INIT disk structure...\n");
		return DISK_FAILURE;
	}

	list_for_each_entry_safe(node, _node, &(pdisk->list), node) {
		if(spedev && strcmp(node->info.devname, spedev)){
			DISKCK_DBG("Special Chk Mount %s-->IGNORE %s\n", spedev, node->info.devname);			
			continue;
		}
		list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node) {
			if(pnode->mounted == 1){
				DISKCK_DBG("%s have disk partition mounted\n", spedev ?spedev:"System");
				return DISK_SUCCESS;
			}
		}	
	}
	
	return DISK_FAILURE;
}


int disk_insert_partition_list(disk_info_t *pdisk, disk_baseinfo_t* pinfo)
{
	int ret, type = 0, len;
	char ptname[DSHORT_STR] = {0}, devname[DSHORT_STR] = {0}, *ptr;
	char devbuf[DSHORT_STR] = {0}, tpbuf[DSHORT_STR] = {0};
	disk_maininfo_t *dmain = NULL;
	disk_partinfo_t *dpart = NULL;
	disk_major_t *dmajor = NULL;
	
	if(!pdisk || !pinfo){
		return DISK_FAILURE;
	}
	
	strcpy(ptname, pinfo->devname);
	DISKCK_DBG("Prase Dev %s\n", ptname);
	
	dmajor = disk_search_major_list(&pdisk->disk_major, pinfo->major);
	if(dmajor == NULL){
		DISKCK_DBG("No Found Major %d\n", pinfo->major);
		return DISK_FAILURE;
	}
	if(strcmp(dmajor->name, "mmc") == 0 && dmajor->major != 8){
		/*We think it is mmcblk*/
		if(pinfo->minor % MMCBLK_MAX_PART == 0){
			type = DISK_MMC_MAIN;			
			strcpy(devname,  ptname);	
		}else{
			type = DISK_MMC_PART;
			len = 0;
			ptr = ptname;
			while(*ptr && len < DSHORT_STR-1 &&
					!isdigit(*ptr)){
				tpbuf[len++] = *ptr++;
			}
			tpbuf[len] = '\0';
			DISKCK_DBG("tpbuf=%s@ptr=%d!\n", tpbuf, atoi(ptr));
		
			sprintf(devname, "%s%d", tpbuf, atoi(ptr));
		}
	}else if(strcmp(dmajor->name, "sd") == 0 && dmajor->major == 8){
		if(!isdigit(ptname[strlen(ptname) -1 ])){
			type = DISK_USB_MAIN;			
			strcpy(devname,  ptname);	
		}else{
			type = DISK_USB_PART;
			strcpy(tpbuf, ptname);
			ptr = tpbuf+strlen(tpbuf)-1;
			while(isdigit(*ptr)){
				*ptr--= '\0';
			}
			strcpy(devname, tpbuf);	
		}
	}else{
		DISKCK_DBG("Unknow Disk Type %s\n", ptname);
		return DISK_FAILURE;
	}

	dmain = disk_search_partition_list(pdisk, devname, 0);
	if(type == DISK_USB_MAIN || 
			type == DISK_MMC_MAIN){

		if(dmain){
			DISKCK_DBG("No Need To Insert Main Partition %s\n", devname);
			return DISK_SUCCESS;
		}
		DISKCK_DBG("Insert Main Partition %s\n", devname);
		
		memset(devbuf, 0, sizeof(devbuf));
		sprintf(devbuf, "/dev/%s", ptname);
		if(access(devbuf, F_OK) != 0){
			DISKCK_DBG("Create Node %s", devbuf);
			mknod(devbuf, S_IFBLK|0600, 
				makedev(pinfo->major, pinfo->minor));
		}
		dmain = calloc(1, sizeof(disk_maininfo_t));
		if(!dmain){
			return DISK_FAILURE;
		}
		strcpy(dmain->info.devname, ptname);
		dmain->info.major = pinfo->major;
		dmain->info.minor = pinfo->minor;
		dmain->info.total = pinfo->total;
		if(type == DISK_USB_MAIN){
			strcpy(dmain->type, "USB");
		}else{
			strcpy(dmain->type, "SD");
		}		
		INIT_LIST_HEAD(&dmain->partlist);
		ret = disk_fill_main_partition_baseinfo(pdisk, dmain);
		if(ret != 0){
			free(dmain);
			return DISK_FAILURE;
		}
		/*Insert it to list*/
		list_add_tail(&dmain->node, &pdisk->list);
		pdisk->disk_num++;
		return DISK_SUCCESS;
	}
	/*Insert Disk Parition*/
	if(dmain == NULL){
		DISKCK_DBG("Something Error..Need Insert Partition Main %s..\n", devname);
		if(disk_chk_init() == 0){
			ret = disk_insert_main_partition(pdisk, &dmain, devname);
			if(ret != 0){
				DISKCK_DBG("Main Partition %s Insert Error...\n", devname);
				return DISK_FAILURE;
			}
		}else{
			disk_trigger_udevadd(devname, dmajor->major);
			return DISK_FAILURE;
		}
	}
	/*Check it again*/
	if(disk_search_partition_list(pdisk, ptname, 1)){
		DISKCK_DBG("No Need to Insert Partition %s..\n", ptname);
		return 0;
	}
	/*Insert Partition num to list*/
	dpart = calloc(1, sizeof(disk_partinfo_t));
	if(dpart == NULL){
		DISKCK_DBG("Calloc Memeory Failed\n");
		return DISK_FAILURE;
	}
	dpart->uptime = 0;
	memcpy(&dpart->info, pinfo, sizeof(disk_baseinfo_t));
	dmain->partnum++;
	list_add_tail(&dpart->node, &dmain->partlist);

	return DISK_SUCCESS;
}

int disk_mnt1_loop_partition(disk_info_t *pdisk, char *spedev)
{
        FILE *procpt = NULL;
        int ma, mi;
	unsigned long long sz;
        char line[DMID_STR]={0}, ptname[DSHORT_STR];
        int ret;
	disk_baseinfo_t dinfo;
	
	if(!pdisk){
		DISKCK_DBG("Please INIT disk structure...\n");
		return DISK_FAILURE;
	}

        if ((procpt = fopen(DISK_PROC_PARTITION, "r")) == NULL) {
                DISKCK_DBG("Fail to fopen(%s)", DISK_PROC_PARTITION);
                return DISK_FAILURE;
        }

        while (fgets(line, sizeof(line), procpt) != NULL) {
                memset(ptname, 0, sizeof(ptname));
                if (sscanf(line, " %d %d %llu %[^\n ]",
                                &ma, &mi, &sz, ptname) != 4)
                        continue;
		if(sz == 1){
			DISKCK_DBG("IGNORE Extend Partition %s\n", ptname);
			continue;
		}
		if(spedev && strncmp(ptname, spedev, strlen(spedev))){
			DISKCK_DBG("Special Loop %s-->IGNORE %s\n", spedev, ptname);			
			continue;
		}
		if(disk_search_major_list(&pdisk->disk_major, ma) == NULL){
			continue;
		}
		memset(&dinfo, 0, sizeof(disk_baseinfo_t));
		strcpy(dinfo.devname, ptname);
		dinfo.major = ma;
		dinfo.minor = mi;
		dinfo.total = sz * DSIZ_KB;
		ret = disk_insert_partition_list(pdisk, &dinfo);
		if(ret != DISK_SUCCESS){
			DISKCK_DBG("Insert %s Error\n", ptname);
		}
		memset(line, 0, sizeof(line));
        }
	fclose(procpt);
	if(spedev){
		DISKCK_DBG("Special Loop %s Handle Finish\n", spedev);		
	}else{
		DISKCK_DBG("Found %d Storage Device...\n", pdisk->disk_num);
	}
	return DISK_SUCCESS;
}

int disk_mnt2_confirm_partition_info(disk_info_t *pdisk, char *spedev)
{
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode, *newnode;	
	struct blkid_struct_probe pr;
	char devname[DSHORT_STR];
	
	if(!pdisk){
		DISKCK_DBG("Please INIT disk structure...\n");
		return DISK_FAILURE;
	}

	list_for_each_entry_safe(node, _node, &(pdisk->list), node) {
		/*Get disktag*/
		if(spedev && strcmp(node->info.devname, spedev)){
			DISKCK_DBG("Special Confirm %s-->IGNORE %s\n", spedev, node->info.devname);			
			continue;
		}			
		if(node->partnum == 0){
			DISKCK_DBG("[%s]Only have main partition....\n", node->info.devname);			

			newnode = calloc(1, sizeof(disk_partinfo_t));
			if(newnode == NULL){
				DISKCK_DBG("No Memory!!!!\n");
				return DISK_FAILURE;
			}
			memcpy(&(newnode->info), &node->info, sizeof(disk_baseinfo_t));
			memset(&pr, 0, sizeof(pr));
			memset(devname, 0, sizeof(devname));
			sprintf(devname, "/dev/%s", node->info.devname);
			probe_block(devname, &pr);
			if (pr.err || !pr.id) {
				DISKCK_DBG("[%s]Can not recginze....\n", node->info.devname);		
				strcpy(newnode->fstype, "unknown");
			}else{			
				strcpy(newnode->fstype, pr.id->name);
				strcpy(newnode->label, pr.label);
			}

			/*add node*/
			list_add_tail(&newnode->node, &node->partlist);
			if(spedev){				
				DISKCK_DBG("Special Confirm %s Handle Finish[Only have main]\n", spedev);			
				return DISK_SUCCESS;
			}
			continue;
		}
		list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node) {			
			memset(&pr, 0, sizeof(pr));
			memset(devname, 0, sizeof(devname));
			sprintf(devname, "/dev/%s", pnode->info.devname);
			probe_block(devname, &pr);
			if (pr.err || !pr.id) {
				DISKCK_DBG("[%s]Can not recginze....\n", pnode->info.devname);		
				strcpy(pnode->fstype, "unknown");
			}else{
				strcpy(pnode->fstype, pr.id->name);
				strcpy(pnode->label, pr.label);
			}
		}
		//if confirm special disk, now we handle it finish, so return
		if(spedev){			
			DISKCK_DBG("Special Confirm %s Handle Finish\n", spedev);			
			return DISK_SUCCESS;
		}		
	}
	if(spedev){ 		
		DISKCK_DBG("Special Confirm No Found %s\n", spedev);			
		return 2;
	}
	return DISK_SUCCESS;
}

int disk_mnt3_automount_partition(disk_info_t *pdisk, char *spedev)
{
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode;
	char devbuf[DSHORT_STR], parameter[DSHORT_STR] = {0}, fs[DSHORT_STR] = {0};
	int ret;
	
	if(!pdisk){
		DISKCK_DBG("Please INIT disk structure...\n");
		return DISK_FAILURE;
	}

	list_for_each_entry_safe(node, _node, &(pdisk->list), node) {
		if(spedev && strcmp(node->info.devname, spedev)){
			DISKCK_DBG("Special Mount %s-->IGNORE %s\n", spedev, node->info.devname);			
			continue;
		}
		list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node) {
			memset(devbuf, 0, sizeof(devbuf));
			sprintf(devbuf, "/dev/%s", pnode->info.devname);
			if(node->isgpt == 1 &&
				probe_efi_partition(node->info.devname, pnode->info.devname) == 1){
				DISKCK_DBG("EFI Partition Found %s-->IGNORE\n", pnode->info.devname);		
				continue;
			}
			if(find_mount_point(devbuf)){
				DISKCK_DBG("%s is already mounted\n", devbuf);				
				get_partition_capacity_rw(pnode);
				pnode->mounted = 1;
				continue;
			}
			if(generate_mount_point(node->disktag, pnode) != DISK_SUCCESS){
				DISKCK_DBG("Generate Mnt Point Failed\n");
				pnode->mounted = 0;
				continue;
			}
			/*Get commerical filesystem driver*/
			memset(fs, 0, sizeof(fs));
			if(check_commerical_driver(pnode->fstype, fs) != DISK_SUCCESS){
				strcpy(fs, pnode->fstype);
			}
			memset(parameter, 0, sizeof(parameter));
			if(generate_mount_parameter(&(pdisk->mnt_parameter), fs, parameter) != DISK_SUCCESS){
				DISKCK_DBG("Get Mount Parameter Error:%s\n", pnode->info.devname);
			}
			ret = mount(devbuf, pnode->mntpoint , fs, 0,strlen(parameter)?parameter:"");
			if(ret){
				DISKCK_DBG("Mount Error: dev=%s mntpoint=%s fstype=%s option=%s![%s]\n", 
						devbuf, pnode->mntpoint, fs, parameter, strerror(errno));
				pnode->mounted = 0;
				rmdir(pnode->mntpoint);
				continue;
			}else{
				DISKCK_DBG("Mount %s Successful, Update Partition Capatibty\n",devbuf);
				update_partition_capacity_rw(pnode);
			}
			/*Notify to upnpd*/
			disk_aciton_notify_upnp(pnode, DISK_UDEV_ADD, node->info.devname);	
			
			pnode->mounted = 1;
		}
		node->status = DISK_MOUNTED;
		if(spedev){			
			/*No Disk Mount so we need to display this situation*/
			DISKCK_DBG("Special Mount %s Handle Finish\n", spedev);			
			return DISK_SUCCESS;
		}		
	}
	
	return 2;
}

int disk_mount_process(disk_info_t *pdisk, char *spedev)
{
	int ret = DISK_FAILURE;
	
	if(!pdisk){
		DISKCK_DBG("Please INIT disk structure...\n");
		return DISK_FAILURE;
	}

	ret = disk_mnt1_loop_partition(pdisk, spedev);
	if(ret == DISK_FAILURE){
		DISKCK_DBG("Loop Error...\n");
		return DISK_FAILURE;
	}
	ret = disk_mnt2_confirm_partition_info(pdisk, spedev);
	if(ret  != DISK_SUCCESS){
		return ret;
	}
	ret = disk_mnt3_automount_partition(pdisk, spedev);
	if(ret != DISK_SUCCESS){
		return ret;
	}

	return DISK_SUCCESS;
}


void disk_print_partition_info(disk_info_t *pdisk)
{
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode;
	
	if(!pdisk){
		DISKCK_DBG("Please INIT disk structure...\n");
		return ;
	}
	DISKCK_DBG("\n\n\tDISK Manager Partition\n\n");
	list_for_each_entry_safe(node, _node, &(pdisk->list), node) {
		printf("DiskName:%s\t DiskVendor=%s\nDiskSerical=%s\nDiskPartnum=%d\tDiskSize=%llu\n", node->info.devname, node->vendor,
				node->serical, node->partnum, node->info.total);		
		printf("\nDiskPartition:\n");
		list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node) {
			printf("PartitionName:%s\t  Mounted=%d PartitionType:%s\tPartitionLabel=%s\tTotal=%llu\tUsed=%llu\n", 
					pnode->info.devname, pnode->mounted, pnode->fstype, pnode->label, pnode->info.total, pnode->info.used);
		}
		printf("\n\n");
	}
	printf("\n");
}

int udev_action_func_add(disk_info_t *pdisk, udev_action *action)
{
	if(action == NULL){
		return DISK_FAILURE;
	}
	return disk_mount_process(pdisk, action->dev);
}

int udev_action_func_wakeup(disk_info_t *pdisk, udev_action *action)
{
	int ret;
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode;
	char devbuf[DSHORT_STR], fs[DSHORT_STR] = {0}, parameter[DSHORT_STR] = {0};

	if(action->action != DISK_WAKEUP){
		return 2;
	}
	list_for_each_entry_safe(node, _node, &(pdisk->list), node) {
		if(strcmp(node->info.devname, action->dev) == 0&&
			(action->major == 0xFFFF|| node->info.major == action->major)){
			if(node->status != DISK_SFREMOVE){
				DISKCK_DBG("Disk %s Status is Not  DISK_SFREMOVE\n", node->info.devname);
				return 2;
			}
			DISKCK_DBG("WakeUP %s Begin...\n", node->info.devname);
			list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node) {
				memset(devbuf, 0, sizeof(devbuf));
				sprintf(devbuf, "/dev/%s", pnode->info.devname);
				if(find_mount_point(devbuf)){
					DISKCK_DBG("%s is already mounted\n", devbuf);				
					get_partition_capacity_rw(pnode);
					node->status = DISK_MOUNTED;
					return 2;
				}
				if(generate_mount_point(node->disktag, pnode) != DISK_SUCCESS){
					DISKCK_DBG("Generate Mnt Point Failed\n");
					pnode->mounted = 0;
					return DISK_FAILURE;
				}
				/*Get commerical filesystem driver*/
				memset(fs, 0, sizeof(fs));
				if(check_commerical_driver(pnode->fstype, fs) != DISK_SUCCESS){
					strcpy(fs, pnode->fstype);
				}
				memset(parameter, 0, sizeof(parameter));
				if(generate_mount_parameter(&(pdisk->mnt_parameter), fs, parameter) != DISK_SUCCESS){
					DISKCK_DBG("Get Mount Parameter Error:%s\n", pnode->info.devname);
				}
				ret = mount(devbuf, pnode->mntpoint , fs, 0,strlen(parameter)?parameter:"");
				if(ret){
					DISKCK_DBG("Mount Error: dev=%s mntpoint=%s fstype=%s option=%s![%s]\n", 
							devbuf, pnode->mntpoint, fs, parameter, strerror(errno));
					pnode->mounted = 0;
					continue;
				}else{
					DISKCK_DBG("Mount %s Successful, Update Partition Capatibty\n",devbuf);
					update_partition_capacity_rw(pnode);
				}
				pnode->mounted = 1;

			}
			/*Update Main Disk status*/
			node->status = DISK_MOUNTED;

			return DISK_SUCCESS;
		}
	}
	DISKCK_DBG("No Found Safe Remove Disk: %s\n",action->dev);

	return 2;
}

int udev_action_func_remove(disk_info_t *pdisk, udev_action *action)
{
	int ret;
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode;

	list_for_each_entry_safe(node, _node, &(pdisk->list), node) {
		if(strcmp(node->info.devname, action->dev) == 0&&
			(action->major== 0xFFFF ||node->info.major == action->major)){
			if(node->status == action->action){
				DISKCK_DBG("Disk %s Have %s\n", node->info.devname, 
					action->action==DISK_UDEV_REMOVE?"Removed":"Safe Removed");
				return DISK_SUCCESS;
			}
			DISKCK_DBG("%s %s Begin...\n", action->action==DISK_UDEV_REMOVE?"Remove":"Safe Remove"
				,node->info.devname);
			list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node) {
				if(pnode->mounted == 1){
					DISKCK_DBG("Umount %s[Mounted]\n", pnode->info.devname);
					ret = umount2(pnode->mntpoint, MNT_DETACH);
					if(ret){
						DISKCK_DBG("umount of %s failed (%d) - %s\n",
							pnode->mntpoint, ret, strerror(errno));
					}
				}
				/*Update status*/
				if(action->action == DISK_SFREMOVE){
					pnode->mounted = 0;					
					DISKCK_DBG("Safe Remove %s Successful\n", pnode->info.devname);
				}else if(action->action == DISK_UDEV_REMOVE){
					list_del(&pnode->node);
					rmdir(pnode->mntpoint);
					/*Notify to upnpd*/
					disk_aciton_notify_upnp(pnode, action->action, action->dev);
					DISKCK_DBG("Remove %s Successful\n", pnode->info.devname);
					free(pnode);
				}else{
					DISKCK_DBG("Error Action %d\n", action->action);
				}
			}
			/*Update Main Disk status*/
			if(action->action == DISK_SFREMOVE){
				node->status = DISK_SFREMOVE;
			}else if(action->action == DISK_UDEV_REMOVE){
				list_del(&node->node);
				DISKCK_DBG("Last Remove %s Successful\n", node->info.devname);
				free(node);
			}
			return DISK_SUCCESS;
		}
	}

	return 2;
}

void handle_tecent_nas_disk(int action)
{
	switch (action) {
		case DISK_UDEV_ADD:
			printf("Udisk is on,run nas...\n");
			system("nas&");
			break;
		case DISK_UDEV_REMOVE:
			//fallen through
		default:
			printf("Udisk is away, stop nas...\n");
			system("killall nas");
			break;
	}
}
int udev_action_func(disk_info_t *pdisk, udev_action *action)
{
	int ret = DISK_FAILURE;
	
	if(pdisk == NULL || action == NULL){
		return DISK_FAILURE;
	}
	if(action->action == DISK_UDEV_ADD){
		ret =  udev_action_func_add(pdisk, action);
		if (ret == DISK_SUCCESS) {
			handle_tecent_nas_disk(DISK_UDEV_ADD);
		}
	}else if(action->action == DISK_WAKEUP){
		ret = udev_action_func_wakeup(pdisk, action);
	}else{
		handle_tecent_nas_disk(DISK_UDEV_REMOVE);
		ret =  udev_action_func_remove(pdisk, action);	
	}
	if(ret == DISK_SUCCESS){
		DISKCK_DBG("Update Samba Config & Restart Service...\n");
		disk_update_samba_config(pdisk);
		disk_restart_related_service(NULL, NULL);
	}
	if(disk_chk_partition_mounted(pdisk, NULL) == DISK_SUCCESS){		
		DISKCK_DBG("System Have Mounted Disk Partition Red LED ON...\n");
		LED_CONTROL_ON;
	}else{
		DISKCK_DBG("System Found No Mounted Disk Partition Red LED OFF...\n");	
		LED_CONTROL_OFF;
	}
	return ret;
}

int udev_action_func_all(disk_info_t *pdisk, int *action)
{
	int ret = DISK_FAILURE;
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode;
	
	if(pdisk == NULL || action == NULL){
		return DISK_FAILURE;
	}
	disk_restart_related_service(NULL, "stop");
	DISKCK_DBG("Handle ALL: %s\n", *action==DISK_UDEV_ADD?"Add":"Remove");
	if(*action == DISK_UDEV_ADD){
		return disk_mount_process(pdisk, NULL);
	}
	/*Remove Action*/
	list_for_each_entry_safe(node, _node, &(pdisk->list), node) {
		DISKCK_DBG("Remove %s Begin...\n", node->info.devname);
		list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node) {
			if(pnode->mounted == 0){
				DISKCK_DBG("No Need To Handle %s[Not Mounted]\n", pnode->info.devname);
				continue;
			}
			ret = umount2(pnode->mntpoint, MNT_DETACH);
			if(ret){
				DISKCK_DBG("umount of %s failed (%d) - %s\n",
						pnode->mntpoint, ret, strerror(errno));
				continue;
			}

			list_del(&pnode->node);
			rmdir(pnode->mntpoint);
			/*Notify to upnpd*/
			disk_aciton_notify_upnp(pnode, *action, node->info.devname);
			DISKCK_DBG("Remove %s Successful\n", pnode->info.devname);
			free(pnode);
		}	
		/*Update Main Disk status*/
		list_del(&node->node);
		sg_sleep_disk(node->info.devname, 0);
		DISKCK_DBG("Last Remove %s Successful\n", node->info.devname);
		free(node);
	}

	return DISK_SUCCESS;
}

int protocol_get_all_disk_info(disk_info_t *pdisk, void *data, int len, void **rbuf, int *rlen)
{
	int reslen = 0, partmem = 0;
	void *response = NULL, *prealloc = NULL;
	disk_proto_t *diskinfo, *partdisk = NULL;
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode;
	unsigned char same_disk = (time(NULL)&0xFF);
	unsigned long long cused = 0;
	char devname[DSHORT_STR] = {0};
	
	if(pdisk == NULL){
		return DISK_FAILURE;
	}
	if(data && len){		
		memcpy(devname, data, len);
		DISKCK_DBG("Protocol Get Info--->Get Special Disk %s\n", devname);
	}
	list_for_each_entry_safe(node, _node, &(pdisk->list), node){
		if(strlen(devname) && strcmp(node->info.devname, devname)){			
			DISKCK_DBG("Protocol Filter---%s-->%s\n", devname, node->info.devname);
			continue;
		}
		cused = 0;
		response = realloc(response, sizeof(disk_proto_t)+reslen);
		if(response == NULL){
			DISKCK_DBG("Memory Realloc Failed\n");
			return DISK_FAILURE;
		}		
		diskinfo = (disk_proto_t*)(response+reslen);
		/*Increase memory*/
		reslen += sizeof(disk_proto_t);
		DISKCK_DBG("Protocol Get [%s] Info--->Address[0x%p] memory size[%d]\n", 
			node->info.devname, response, reslen);
		
		/*Fill in main partition*/
		memset(diskinfo, 0, sizeof(disk_proto_t));
		strcpy(diskinfo->devname, node->info.devname);
		diskinfo->total = node->info.total;
		diskinfo->ptype = (same_disk << 8) |  DISK_MAIN;
		strcpy(diskinfo->partition.main_info.vendor, node->vendor);
		strcpy(diskinfo->partition.main_info.serical, node->serical);
		strcpy(diskinfo->partition.main_info.type, node->type);
		strcpy(diskinfo->partition.main_info.disktag, node->disktag);
		diskinfo->partition.main_info.status = node->status;
		diskinfo->partition.main_info.partnum = node->partnum;

		prealloc = NULL;
		partmem = 0;
		list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node){
			/*Update storage information*/
			if(pnode->mounted == 1){
				update_partition_capacity_rw(pnode);
			}
			prealloc = realloc(prealloc, sizeof(disk_proto_t)+partmem);
			if(prealloc == NULL){
				DISKCK_DBG("Memory Realloc Failed\n");
				return DISK_FAILURE;
			}
			partdisk = (disk_proto_t*)(prealloc+partmem);
			/*Increase memory*/
			partmem += sizeof(disk_proto_t);			
			memset(partdisk, 0, sizeof(disk_proto_t));
			DISKCK_DBG("Protocol Get Part [%s] Info--->Address[0x%p] memory size[%d]\n", 
					pnode->info.devname, prealloc, partmem);
			/*Fill in main partition*/
			strcpy(partdisk->devname, pnode->info.devname);
			partdisk->total = pnode->info.total;
			partdisk->ptype = (same_disk << 8) |  DISK_PART;			
			partdisk->partition.part_info.mounted =  pnode->mounted;
			strcpy(partdisk->partition.part_info.fstype, pnode->fstype);
			strcpy(partdisk->partition.part_info.label, pnode->label);			
			strcpy(partdisk->partition.part_info.mntpoint, pnode->mntpoint);
			partdisk->partition.part_info.enablewrite =  pnode->enablewrite;
			
			if(partdisk->partition.part_info.mounted == 0){				
				partdisk->used = pnode->info.total;
			}else{
				partdisk->used = pnode->info.used;
			}
			cused += partdisk->used;

			printf("Part INFO:\n\tName:%s\n\tDiskFlag:%d\n\tTotal:%llu\n\tUsed:%llu\n\tFstype:%s\n\tLabel:%s\n"
					"\tMountPoint:%s\n\tMounted:%d\n\tEnablewrite:%d\n\tcused:%llu\n", partdisk->devname, partdisk->ptype>>8, partdisk->total,
					partdisk->used, partdisk->partition.part_info.fstype, partdisk->partition.part_info.label,
					partdisk->partition.part_info.mntpoint, partdisk->partition.part_info.mounted, 
					partdisk->partition.part_info.enablewrite, cused);			
		}
		/*update disk used info*/
		diskinfo->used = node->info.used = cused;
		printf("DiskMain INFO:\n\tName:%s\n\tDiskFlag:%u\n\tTotal:%llu\n\tUsed:%llu\n\tVendor:%s\n\tSerical:%s\n"
				"\tType:%s\n\tDiskTag:%s\n\tStatus:%d\n\tPartnum:%d\n", diskinfo->devname, diskinfo->ptype>>8, diskinfo->total,
				diskinfo->used, diskinfo->partition.main_info.vendor, diskinfo->partition.main_info.serical,
				diskinfo->partition.main_info.type, diskinfo->partition.main_info.disktag, diskinfo->partition.main_info.status,
				diskinfo->partition.main_info.partnum);	
		
		response = realloc(response, partmem+reslen);
		if(response == NULL){
				DISKCK_DBG("Memory Realloc Failed\n");
				return DISK_FAILURE;
		}
		memcpy(response+reslen, prealloc, partmem);
		reslen += partmem;
		if(prealloc){
			free(prealloc);
			prealloc = NULL;
			partmem = 0;
		}	
		if(strlen(devname)){			
			DISKCK_DBG("Protocol Get Special Disk %s Finish\n", devname);
			break;
		}
		same_disk = ((same_disk+1)%0xFF);		
		DISKCK_DBG("DiskFlag change to  %u! diskinfo->used=%llu\n", same_disk, diskinfo->used);
	}
	*rbuf = response;
	*rlen = reslen;
	DISKCK_DBG("Protocol Get Info Finish--->Address[0x%p] memory size[%d]\n", response, *rlen);

	return DISK_SUCCESS;
}

int protocol_disk_dirlist(disk_info_t *pdisk, void *data, int len, void **rbuf, int *rlen)
{
	int reslen = 0;
	void *response = NULL;
	disk_dirlist_t  *partdisk = NULL;
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode;
	
	if(pdisk == NULL){
		return DISK_FAILURE;
	}

	list_for_each_entry_safe(node, _node, &(pdisk->list), node){
		if(node->status != DISK_MOUNTED){
			DISKCK_DBG("Filter Not mounted disk---%s\n", node->info.devname);
			continue;
		}
		list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node){
			/*Update storage information*/
			if(pnode->mounted != 1){
				DISKCK_DBG("Filter Not mounted partition---%s\n", pnode->info.devname);
				continue;
			}
			response = realloc(response, sizeof(disk_dirlist_t)+reslen);
			if(response == NULL){
				DISKCK_DBG("Memory Realloc Failed\n");
				return DISK_FAILURE;
			}
			partdisk = (disk_dirlist_t*)(response+reslen);
			/*Increase memory*/
			reslen += sizeof(disk_dirlist_t);			
			memset(partdisk, 0, sizeof(disk_dirlist_t));
			DISKCK_DBG("Protocol Get Part [%s] Info--->Address[0x%p] memory size[%d]\n", 
					pnode->info.devname, response, reslen);
			/*Fill in main partition*/
			strcpy(partdisk->devname, node->info.devname);			
			strcpy(partdisk->partname, pnode->info.devname);
			partdisk->mounted =  pnode->mounted;
			strcpy(partdisk->type, node->type);		
			strcpy(partdisk->disktag, node->disktag);			
			strcpy(partdisk->fstype, pnode->fstype);
			strcpy(partdisk->label, pnode->label);			
			strcpy(partdisk->mntpoint, pnode->mntpoint);
			partdisk->enablewrite =  pnode->enablewrite;
			if(disk_dirlist_display_name(mnt_preffix, pnode->mntpoint, partdisk->displayname) == DISK_FAILURE){
				strcpy(partdisk->displayname,  pnode->info.devname);
			}
			
			printf("DirListPart INFO:\n\tName:%s\n\tPartname:%s\n\tType:%s\n\tDiskTag:%s\n\tFstype:%s\n\tLabel:%s\n"
					"\tMountPoint:%s\n\tMounted:%d\n\tEnablewrite:%d\n\tDiskplay:%s\n", partdisk->devname, partdisk->partname, 
					partdisk->type, partdisk->disktag, partdisk->fstype, partdisk->label,
					partdisk->mntpoint, partdisk->mounted, 
					partdisk->enablewrite, partdisk->displayname);
		}
	}
	*rbuf = response;
	*rlen = reslen;
	DISKCK_DBG("Protocol Get Dirlist Finish--->Address[0x%p] memory size[%d]\n", response, *rlen);

	return DISK_SUCCESS;
}

int protocol_disk_disklist(disk_info_t *pdisk, void *data, int len, void **rbuf, int *rlen)
{
	int reslen = 0;
	void *response = NULL;
	disk_disklist_t  *disklist = NULL;
	disk_maininfo_t *node, *_node;
	
	if(pdisk == NULL){
		return DISK_FAILURE;
	}

	list_for_each_entry_safe(node, _node, &(pdisk->list), node){
		response = realloc(response, sizeof(disk_disklist_t)+reslen);
		if(response == NULL){
			DISKCK_DBG("Memory Realloc Failed\n");
			return DISK_FAILURE;
		}
		disklist = (disk_disklist_t*)(response+reslen);
		/*Increase memory*/
		reslen += sizeof(disk_disklist_t);			
		memset(disklist, 0, sizeof(disk_disklist_t));
		DISKCK_DBG("Protocol Get Disk List [%s] Info--->Address[0x%p] memory size[%d]\n", 
				node->info.devname, response, reslen);
		/*Fill in main partition*/
		strcpy(disklist->devname, node->info.devname);
		disklist->total = node->info.total;
		strcpy(disklist->vendor, node->vendor);
		strcpy(disklist->serical, node->serical);
		strcpy(disklist->type, node->type);
		strcpy(disklist->disktag, node->disktag);
		disklist->status = node->status;
		disklist->partnum = node->partnum;
		
		printf("DiskMain INFO:\n\tName:%s\n\tTotal:%llu\n\tVendor:%s\n\tSerical:%s\n"
				"\tType:%s\n\tDiskTag:%s\n\tStatus:%d\n\tPartnum:%d\n", disklist->devname, disklist->total,
				disklist->vendor, disklist->serical,
				disklist->type, disklist->disktag, disklist->status, disklist->partnum);			
	}

	*rbuf = response;
	*rlen = reslen;
	DISKCK_DBG("Protocol Get Disklist Finish--->Address[0x%p] memory size[%d]\n", response, *rlen);

	return DISK_SUCCESS;
}
int disk_chk_init(void)
{
	return i4disk==NULL?0:1;
}
void disk_destory(disk_info_t *pdisk)
{
	int ret;
	disk_maininfo_t *node, *_node;
	disk_partinfo_t *pnode, *_pnode;
	disk_major_t *mnode, *_mpnode;
	disk_mnt_para_t *pamter, *_pamter;
	disktag_t *ptag, *_ptag;

	if(pdisk == NULL){
		return;
	}		
	list_for_each_entry_safe(node, _node, &(pdisk->list), node) {
		DISKCK_DBG("Destory %s Begin...\n", node->info.devname);
		list_for_each_entry_safe(pnode, _pnode, &(node->partlist), node) {
			if(pnode->mounted == 0){
				ret = umount2(pnode->mntpoint, MNT_DETACH);
				if(ret){
					DISKCK_DBG("umount of %s failed (%d) - %s\n",
							pnode->mntpoint, ret, strerror(errno));
				}				
				rmdir(pnode->mntpoint);
			}

			list_del(&pnode->node);
			/*Notify to upnpd*/
			disk_aciton_notify_upnp(pnode, DISK_UDEV_REMOVE, node->info.devname);
			DISKCK_DBG("Destory %s Successful\n", pnode->info.devname);
			free(pnode);
		}	
		/*Update Main Disk status*/
		list_del(&node->node);
		sg_sleep_disk(node->info.devname, 0);
		DISKCK_DBG("Last Destory %s Successful\n", node->info.devname);
		free(node);
	}
	/*Destory Major list*/
	list_for_each_entry_safe(mnode, _mpnode, &(pdisk->disk_major), node) {
		if(mnode){
			free(mnode);
		}
	}
	/*Dstory mount parameter*/
	list_for_each_entry_safe(pamter, _pamter, &(pdisk->mnt_parameter), node) {
		if(pamter){
			free(pamter);
		}
	}
	/*Dstory disktag list*/
	list_for_each_entry_safe(ptag, _ptag, &(pdisk->mnt_disktag), node) {
		if(ptag){
			free(ptag);
		}
	}
	/*Free struct memory*/
	free(pdisk);
	
}

void disk_preinit(void)
{
	FILE *fp;
	char line[DMID_STR] = {0}, tmp[DMID_STR] = {0};
	int slen;

	/*umount unused mount point*/



	/*find mount preffix*/
	if(access(DISK_MNT_DIR, F_OK) ||
			(fp = fopen(DISK_MNT_DIR, "r")) == NULL){
		strcpy(mnt_preffix, DISK_MNT_PREFIX);
		DISKCK_DBG("Use Default Mount preffix %s\n", mnt_preffix);
		return ;
	}
	while (fgets(line, sizeof(line), fp) != NULL) {
		memset(tmp, 0, sizeof(tmp));
		if (sscanf(line, "preffix=%[^\n ]",
						tmp) != 1){
			memset(line, 0, sizeof(line));
			continue;
		}		
		DISKCK_DBG("Found Mount preffix %s\n", tmp);
		slen = strlen(tmp)-1;
		while(tmp[slen] == '/'){
			tmp[slen] = '\0';
			slen--;
		}
		if(slen == 0){
			DISKCK_DBG("Use root as mount preffix, we change it to default\n");
			strcpy(tmp, DISK_MNT_PREFIX);
		}
		DISKCK_DBG("Found Mount preffix %s[Fileter]\n", tmp);
		strcpy(mnt_preffix, tmp);
		fclose(fp);
		return ;
	}		

	fclose(fp);
	
	strcpy(mnt_preffix, DISK_MNT_PREFIX);
	DISKCK_DBG("Finnal Use Default Mount preffix %s\n", mnt_preffix);
}

int disk_init(void)
{
	int ret;
	disk_info_t *i4tmp = NULL;

	/*Preinit disk*/
	disk_preinit();
	/*Init Smaba Path*/
	if(access(DISK_SAMBA_DIR, F_OK)){
		mkdir(DISK_SAMBA_DIR, 0755);
	}
	i4tmp = calloc(1, sizeof(disk_info_t));
	if(i4tmp == NULL){
		DISKCK_DBG("Memory Calloc Failed\n");
		return DISK_FAILURE;
	}
	
	INIT_LIST_HEAD(&i4tmp->list);	
	INIT_LIST_HEAD(&i4tmp->disk_major);	
	INIT_LIST_HEAD(&i4tmp->mnt_parameter);	
	INIT_LIST_HEAD(&i4tmp->mnt_disktag);

	/*Prase device major list, find out block major list*/
	ret = disk_major_list_parse(i4tmp);
	if(ret == DISK_FAILURE){
		DISKCK_DBG("Init Major List Error..\n");
		goto err_destory;
	}
	/*Prase mnt parameter*/
	ret = disk_mnt_parameter_parse(i4tmp);
	if(ret == DISK_FAILURE){
		goto err_destory;
	}
	/*Prase mnt disktag*/
	ret = disk_mnt_disktag_parse(i4tmp);
	if(ret == DISK_FAILURE){
		goto err_destory;
	}
	ret = disk_mount_process(i4tmp, NULL);
	if(ret == DISK_FAILURE){	
		goto err_destory;
	}
	/*Check Disk if not mounted*/
	if(disk_chk_partition_mounted(i4tmp, NULL) == DISK_SUCCESS){		
		DISKCK_DBG("System Have Mounted Disk Partition Red LED ON...\n");
		LED_CONTROL_ON;
	}else{
		DISKCK_DBG("System Found No Mounted Disk Partition Red LED OFF...\n");	
		LED_CONTROL_OFF;
	}	
	disk_update_samba_config(i4tmp);
	disk_print_partition_info(i4tmp);
	/*Restart service*/
	disk_restart_related_service(NULL, NULL);

	i4disk = i4tmp;
	
	return DISK_SUCCESS;
	
err_destory:
	
	disk_destory(i4tmp);
	return DISK_FAILURE;
}

int disk_api_call(DMSG_ID msgid, void *data, int len, void **rbuf, int *rlen)
{
	int fd, ret;
	char *buf = NULL;
	struct disk_ipc_header *hdr, header; 
	int total_len = 0;

	fd = diskm_ipc_client_init(IPC_PATH_MDISK);
	if(fd < 0){
		DISKCK_DBG("Error\n");
		return DISK_FAILURE;
	}

	buf = calloc(1, sizeof(struct disk_ipc_header)+len);
	if(buf == NULL){
		DISKCK_DBG("Calloc Memory Failed\n");
		return DISK_FAILURE;
	}
	hdr = (struct disk_ipc_header*)buf;
	hdr->msg = msgid;
	hdr->len = len;
	hdr->direction.flag = IPCF_NORMAL;
	total_len = sizeof(struct disk_ipc_header)+len;
	if(len){
		memcpy(buf+sizeof(struct disk_ipc_header), data, len);
	}
		
	ret  = diskm_ipc_write(fd, buf, total_len);
	if(ret < 0){
		DISKCK_DBG("Disk API IPC Write Error\n");
		free(buf);
		close(fd);
		return DISK_FAILURE;
	}
	free(buf);
	buf = NULL;
	
	/*Get Response*/
	memset(&header, 0, sizeof(header));
	if(diskm_ipc_read(fd, (char *)&header, sizeof(header)) < 0) {
		DISKCK_DBG("Disk API Read Header error\n");
		close(fd);
		return DISK_FAILURE;
	}
	if(header.direction.response  == DISK_FAILURE){
		DISKCK_DBG("Disk API Response Error\n");
		close(fd);
		return DISK_FAILURE;
	}
	if(header.len == 0){
		DISKCK_DBG("No Response!!\n");
		close(fd);
		if(rbuf)
			*rbuf = NULL;
		if(rlen)
			*rlen = 0;
		return DISK_SUCCESS;
	}
	buf = calloc(1, header.len);
	if (buf == NULL) {
		DISKCK_DBG("Calloc Memory Payload Failed\n");
		close(fd);
		return DISK_FAILURE;
	}
	
	if(diskm_ipc_read(fd, (char *)buf, header.len) < 0) {
		DISKCK_DBG("Disk API Read Payload Failed\n");		
		free(buf); 		
		close(fd);
		return DISK_FAILURE;
	}
	close(fd);
	DISKCK_DBG("Receive  IPC Response: Command->%d Payload->%d\n", header.msg, header.len);
	if(rbuf){
		*rbuf = buf;
	}
	if(rlen){
		*rlen = header.len;
	}
	
	return DISK_SUCCESS;
}

int disk_call(DMSG_ID msgid, void *data, int len, void **rbuf, int *rlen)
{
	int ret = DISK_FAILURE;
	
	DISKCK_DBG("MsgID=%d..\n", msgid);
	/*Reset varaible */
	*rlen = 0;
	*rbuf = NULL;
	switch (msgid) {
	case DISK_MOD_INIT:
		return  disk_init();
	case MSG_DISK_INFO:
		return protocol_get_all_disk_info(i4disk, data, len, rbuf, rlen);
	case MSG_DISK_UDEV:
		return udev_action_func(i4disk, (udev_action*)data);
	case MSG_DISK_TRIGER:
		return udev_action_func_all(i4disk, (int *)data);
	case MSG_DISK_DIRLIST:
		return protocol_disk_dirlist(i4disk, data, len, rbuf, rlen);
	case MSG_DISK_DISKLIST:
		return protocol_disk_disklist(i4disk, data, len, rbuf, rlen);		
	default:
		break;
	}
	
	return ret;
}

