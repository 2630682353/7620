#ifndef _http_common_h_
#define _http_common_h_
#include <time.h>
int set_mac(unsigned char*dest,char*src);
void print_mac(unsigned char*mac);
int checkip(char*ip);
int checkmac(char*mac);
#define I_SUCCESS 0
//#define I_FAILED -1
#define I_TRUE 1
//#define I_FAULSE 0
int dm_daemon(void);
int get_val(char *file, char*key,char*value,int len);
int timetostr(time_t t,char*str,int len);
time_t strtotime(char*str);
int get_mode(int *mode, const char *filename);
void str_decode_url(const char *src, int src_len, char *dst, int dst_len);
int ip_check2(unsigned int ips, unsigned int ipe, unsigned int mask);
int checkmask(unsigned int mask);
#endif

