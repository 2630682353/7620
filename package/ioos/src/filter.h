#ifndef _http_filter_h_
#define _http_filter_h_
#include "list.h"
typedef struct irate{
	int id;
	int speed;
	int flag;
}irate_t;
typedef struct itime{
	struct list_head node;
	time_t max;
	time_t start;
	time_t end;
	time_t min;
	int repeat;
	int setflag;
	int id;
	unsigned int appid;
}itime_t;
struct ifiler{
	struct list_head node;
	unsigned char mac[32];
	int number;
	int online;
	int *appid;
	int id;
	irate_t rate;
	itime_t it;
};
typedef struct ifiler ifilter_t;
int set_mac(unsigned char*dest,char*src);
int  ifilter_check(char*mac,int id,ifilter_t *head);
int ifilter_add_mac(char*mac, int appid,ifilter_t *head);
int ifilter_del_mac(char*mac, int appid,ifilter_t *head);
void ifilter_init(ifilter_t *filter);
int rate_mac_add(char*macbuf,int appid,int speed,ifilter_t *head);
int rate_mac_del(char*macbuf,int appid,ifilter_t *head);
int rate_mac_get(char*macbuf,int appid,ifilter_t *head);
int rate_mac_get2(char*mac,int appid,ifilter_t *head);
int ifilter_mac_up(unsigned char*mac, ifilter_t *head);
int ifilter_mac_down(unsigned char*mac, ifilter_t *head);
int itime_add_mac(unsigned char *mac,int appid,time_t start,time_t end, int repeatflag,ifilter_t *head);
int itime_del_mac(unsigned char *mac,int appid,ifilter_t *head);
int itime_check(unsigned char*mac,int appid,ifilter_t *head);
int itime_update(ifilter_t *head,time_t now);
int  add_blacklist_id(char*macs,int id);
int itime_get(unsigned char*mac,int appid,char*start,char*end,int *repeatflag,ifilter_t *head);
#endif
