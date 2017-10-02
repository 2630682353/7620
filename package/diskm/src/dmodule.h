#ifndef _DISK_MODEULE_
#define _DISK_MODEULE_

typedef int DMSG_ID;
typedef int DMOD_ID;



#define DDEFINE_MSG(module, code) (((module&0x7FFF)<<16)|(code&0xFFFF))

#define DMODUEL_GET(msg) ((msg)>>16)
#define DMODUEL_MSGID(msg) ((msg) &0xFFFF)

#define MODULE_DISK 1

#define MODULE_MAX 256

#define IPC_PATH_MDISK "/tmp/ipc_path_disk"

#endif
