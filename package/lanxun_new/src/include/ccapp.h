#ifndef _CCAPP_H_
#define _CCAPP_H_

#include <arpa/inet.h>

//err no
#define HB_OK 0 //OK, no update
#define HB_OK_T_UP 1 //OK, need traffic update
#define HB_OK_D_UP 2 //OK, need dns update
#define HB_OK_A_UP 3 //OK, all need update
#define HB_CON_ERR 4 //error, connect error

int ccapp_heart_beat();

int ccapp_dns_probe(char *pFilepath);

int ccapp_upload_log(char *pLogfilepath, int logType);

int ccapp_cdn_probe(char *file_path, int type);

#endif
