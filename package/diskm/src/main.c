#include "comlib.h"
#include "dmodule.h"
#include "disk_manager.h"
#include "diskm_ipc_msg.h"
#include "disk_triger.h"
void try_start_tecent_nas()
{
	system("nas&");
}
int main(int argc, char *argv[])
{
	fd_set fds;
	int ipc_fd;
	struct timeval tv;
	socklen_t len = 0;
	struct sockaddr addr;
	int read_sock = 0, ret;	
	struct disk_ipc_header hdr, *phdr, *reply_phdr;
	void *resbuf = NULL;
	int reslen;
	
	char *base = basename(*argv);

	umask(0);

	if (!strcmp(base, "disktriger")){
		return disktriger(argc, argv);
	}else if(!strcmp(base, "disktest")){
		return disktest(argc, argv);
	}else if(!strcmp(base, "umount2")){
		return umount2_func(argc, argv);
	}	

	if(argc == 1){		
		do_daemon();
	}
	handler_sig();
	ipc_fd = diskm_ipc_server_init(IPC_PATH_MDISK);
	
	disk_init();
	
	try_start_tecent_nas();
	
	for (;;) {
		FD_ZERO(&fds);
		FD_SET(ipc_fd, &fds);

		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = 2;		
		if (select(ipc_fd + 1, &fds, NULL, NULL, &tv) <= 0) {
			continue;			
		}

		if (!FD_ISSET(ipc_fd, &fds)){
			continue;
		}
		read_sock = accept(ipc_fd, &addr, &len);
		if(read_sock < 0) {
			DISKCK_DBG("accept fail, sock:%d[%s]\n", ipc_fd, strerror(errno));
			continue;
		}
		
		memset(&hdr, 0, sizeof(hdr));
		if(diskm_ipc_read(read_sock, (char *)&hdr, sizeof(hdr)) < 0) {
			DISKCK_DBG("read header err\n");
            close(read_sock);
			continue;
		}
		phdr = (struct disk_ipc_header *)malloc(DISKM_IPC_TOTAL_LEN(hdr.len));
		if (phdr == NULL) {
			DISKCK_DBG("malloc fail\n");
            close(read_sock);
			continue;
		}
		
		memcpy(phdr, &hdr, DISKM_IPC_HEADER_LEN);
		if ((phdr->len > 0) && \
				(diskm_ipc_read(read_sock, (char *)DISKM_IPC_DATA(phdr), phdr->len) < 0)) {
			free(phdr);			
			DISKCK_DBG("read body err\n");
            close(read_sock);
			continue;
		}		
		DISKCK_DBG("Read IPC Data: Command->%d Payload->%d\n", phdr->msg, phdr->len);
		reslen = 0;
		resbuf  = NULL;
		if(DMODUEL_GET(phdr->msg) == MODULE_DISK){			
			DISKCK_DBG("Disk Module %d Handle\n", phdr->msg);
			ret = disk_call(phdr->msg, DISKM_IPC_DATA(phdr), 
				phdr->len, &resbuf, &reslen);
		}else{
			DISKCK_DBG("Unknow Module %d\n", phdr->msg);
			free(phdr);
            close(read_sock);
			continue;
		}
		if (hdr.direction.flag == IPCF_ONLY_SEND) {
			DISKCK_DBG("Disk Handle %d Finish IPCF_ONLY_SEND\n", phdr->msg);
			free(phdr);
            close(read_sock);
			continue;
		}
		
		reply_phdr = (struct disk_ipc_header *)malloc(DISKM_IPC_TOTAL_LEN(reslen));
		if (reply_phdr == NULL) {
			DISKCK_DBG("malloc fail\n");
			free(phdr);			
            close(read_sock);
			continue;
		}
		memset(reply_phdr, 0, DISKM_IPC_TOTAL_LEN(reslen));
		reply_phdr->msg = phdr->msg;
		reply_phdr->direction.response= ret;//response result
		reply_phdr->len = reslen;
		/*Free header memory*/
		free(phdr);
		
		if (reply_phdr->len) {
			memcpy(DISKM_IPC_DATA(reply_phdr), resbuf, reslen);
			free(resbuf);
			resbuf = NULL;
		}
		
		if(diskm_ipc_write(read_sock, (char *)reply_phdr, DISKM_IPC_TOTAL_LEN(reply_phdr->len)) < 0) {
			DISKCK_DBG("write header err, 0x%X, %d\n", hdr.msg, hdr.len);
		}

		free(reply_phdr);
        close(read_sock);
	}
	return 0;
}
