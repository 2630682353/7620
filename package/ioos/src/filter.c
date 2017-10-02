//#include <linux/if.h>
#include "nlk_ipc.h"
#include "ioos_uci.h"
#include "nlk_ipc.h"
#include <string.h>
#include <stdio.h>
#include "common.h"
#include "filter.h"
#include "log.h"
#if 1
void ifilter_init(ifilter_t *filter){
	memset(filter,0,sizeof(ifilter_t));
	INIT_LIST_HEAD(&filter->node);
	filter->number = 0;
	INIT_LIST_HEAD(&filter->it.node);
}
static int rate_mac_add_in_data(char*mac,int appid,int speed,irate_t *ratedata,int online);
static int add_blacklist_app(char*macs,int n,int*app);
static int add_blacklist(char*macs);
static inline ifilter_t * ifilter_new(unsigned char*mac){
	ifilter_t * ptr;
	//unsigned char macbuf[7];
	ptr = malloc(sizeof(ifilter_t));
	if(ptr == NULL){
		return NULL;
	}
	ifilter_init(ptr);
	memcpy(ptr->mac,mac,6);
	// ptr->appid[] = id;
	//int *appid;
	//appid = malloc(sizeof(int));
	//appid[0] = appids;
	//ptr->appid = appid;
	//ptr->number++;
	return ptr;
}
static inline void ifilter_add(ifilter_t *new,ifilter_t *head){
	list_add(&new->node,&head->node);
}
static inline void ifilter_del(ifilter_t *entry){
	list_del(&entry->node);
}
static inline ifilter_t * ifilter_search(char*mac,ifilter_t *head){
	struct list_head *pos;
	ifilter_t * ptr;
	int i=0;
	list_for_each(pos, &head->node){
		if (ptr = list_entry(pos,ifilter_t,node)){
			i++;
			if (memcmp(ptr->mac,mac,6) == 0){
				return ptr;
			}
		}
	}
	return NULL;
}
int  ifilter_check(char*mac,int id,ifilter_t *head){
	struct list_head *pos;
	ifilter_t * ptr;
	list_for_each(pos, &head->node){
		if (ptr = list_entry(pos,ifilter_t,node)){

			unsigned char macbuf[32];
			sprintf(macbuf,"%02X:%02X:%02X:%02X:%02X:%02X", ptr->mac[0], ptr->mac[1], \
					ptr->mac[2], ptr->mac[3], ptr->mac[4], ptr->mac[5]);
			//DEBUG("list mac %s\n",macbuf);
			if (memcmp(ptr->mac,mac,6) == 0){
				//DEBUG("mac compare ok,number=%d,searchid=%d\n",ptr->number,id);
				if(ptr->number >0 && ptr->appid ){
					int i;
					for(i=0;i<ptr->number;i++){
						//DEBUG("id=%u\n",ptr->appid[i]);
						if(ptr->appid[i] == id){
							return 1;
						}

					}

				}
			}
		}
	}
	//printf("check end\n");
	return 0;
}

static int set_blacklist(int n,int*appid,ifilter_t *entry){
	int id;
#if 1
	if(entry->online == 0){
		return 0;
	}
#endif
	if(appid[0] == 0){
		id = add_blacklist(entry->mac);
		if(id <= 0){
			printf("test=%d\n",__LINE__);
			return -1;
		}
	}else{
		DEBUG("appid[0]=%u",appid[0]);
		id =add_blacklist_app(entry->mac,n,appid);
		if(id <= 0){
			DEBUG("ERROR:appid[0]=%u",appid[0]);
			printf("test=%d\n",__LINE__);
			//free(appid);
			return -1;
		}
		DEBUG("appid[0]=%u",appid[0]);
	}
	return id;

}
static inline void ifilter_clear(ifilter_t *entry){
#if 1
	if(entry->online == 0){
		return;
	}
#endif
	if(entry->id >0 && entry->number > 0){
		if(entry->appid[0] != 0){
			unregister_app_filter(entry->id);
			entry->id = -1;
		}else{
			del_filter_rule(NF_MOD_NET,entry->id);
			entry->id = -1;
		}
	}
	return;

}

static inline int ifilter_addid(int id, ifilter_t *entry){
	int *appid;
	int i;
	int setid;
	for(i=0;i<entry->number;i++){
		DEBUG("entry->appid[i]=%d",entry->appid[i]);
		if(entry->appid[i] == id){
			printf("test=%d\n",__LINE__);
			return 0;
		}
	}
	appid = malloc(sizeof(int)*(entry->number+1));
	if(appid == NULL){
		DEBUG("ERROR:malloc error\n");
		return -1;
	}
	DEBUG("entry->number=%d\n",entry->number);
	if((entry->number > 0) &&(entry->appid)&& (entry->appid[0] == 0)){
		appid[0] = 0;
		appid[1] = id;
		if(entry->number > 1){
			memcpy(&appid[2],&entry->appid[1],sizeof(int)*(entry->number - 1));
		}
	}else {
		appid[0] = id;
		if(entry->number > 0){
			DEBUG("entry->appid[0]=%d\n",entry->appid[0]);
			memcpy(&appid[1],&entry->appid[0],sizeof(int)*entry->number);
			DEBUG("appid[1]=%d\n",appid[1]);
		}
	}
	DEBUG("%x\n",entry->appid);
	if(entry->number > 0){
		DEBUG("==============================appid[1]=%d\n",appid[1]);
	}
	setid = set_blacklist(entry->number+1,appid,entry);
	if(setid == -1){
		DEBUG("ERROR:set error\n");
		free(appid);
		return -1;
	}
	if(entry->appid){
		ifilter_clear(entry);
		free(entry->appid);
		entry->appid = NULL;
	}
	if(entry->number > 0){
		DEBUG("================================appid[1]=%d\n",appid[1]);
	}
	//printf("appid=test%d\n",entry->appid[0]);
	entry->appid = appid;
	DEBUG("%x\n",entry->appid);
	entry->number++;
	entry->id = setid;
	for(i=0;i<entry->number;i++){
		DEBUG("END:entry->appid[%d]=%d\n",i,entry->appid[i]);
		DEBUG("END:appid[%d]=%d\n",i,appid[i]);
	}

	return 0;
}
static inline int ifilter_delid(int id, ifilter_t *entry){
	int *appid;
	int i,j;
	int setid;
	int flag = 0;
	for(i=0;i<entry->number;i++){
		if(entry->appid[i] == id){
			flag = 1;
		}
	}
	if(flag == 0){
		return 0;
	}
	if(entry->number == 1){
		ifilter_clear(entry);
		free(entry->appid);
		entry->appid = 0;
		entry->number = 0;
		entry->id =0;
		return 0;
	}
	appid = malloc(sizeof(int)*(entry->number-1));
	if(appid == NULL){
		return -1;
	}
	for(i=0,j=0;i<entry->number;i++){
		if(entry->appid[i] == id){
			continue;
		}else{
			appid[j++] = entry->appid[i]; 
		}
	}
	setid = set_blacklist(entry->number-1,appid,entry);
	if(id == -1){
		free(appid);
		return -1;
	}
	if(entry->appid){
		ifilter_clear(entry);
		free(entry->appid);
		entry->appid = NULL;
	}
	entry->appid = appid;
	entry->number--;
	entry->id = setid;
	return 0;
}
int ifilter_mac_up(unsigned char*mac, ifilter_t *head){
	ifilter_t *ptr;
	ptr = ifilter_search(mac,head);
	if(ptr == NULL){
		ptr=ifilter_new(mac);
		if(ptr == NULL){
			DEBUG("ERROR:ifilter_new error,mac=%s\n",mac);
			return -1;
		}
		ifilter_add(ptr,head);
		ptr->online = 1;		
	}else{
		if(ptr->online == 1){
			DEBUG("it is online now");
			return 0;
		}
		DEBUG("to change to  online now");
		ptr->online = 1;
		if(ptr->number > 0){		
			ptr->id = set_blacklist(ptr->number,ptr->appid,ptr);
			if(ptr->id <= 0){
				DEBUG("set_blacklist error\n");
				return -1;
			}
		}
		if(ptr->rate.flag == 1 && ptr->rate.id <= 0){
			rate_mac_add_in_data(ptr->mac,0,ptr->rate.speed,&ptr->rate,ptr->online);
		}
	}
	return 0;
}
int ifilter_mac_down(unsigned char*mac, ifilter_t *head){
	ifilter_t *ptr;
	int ret;
	ptr = ifilter_search(mac,head);
	if(ptr == NULL){
		return 0;
	}else{
		if(ptr->online == 0){
			return 0;
		}
		if(ptr->number > 0){
			ifilter_clear(ptr);
		}
		ptr->online = 0;
		if(ptr->rate.id > 0){
			ret =del_filter_rule(NF_MOD_RATE,ptr->rate.id);
			if(ret != 0 ){
				DEBUG("del ratte error\n");
			}
			ptr->rate.id = 0;
		}
		if(!list_empty(&ptr->it.node)){
			itime_t *t_cur,*t_tmp;
			list_for_each_entry_safe(t_cur, t_tmp, &ptr->it.node, node){
				if(t_cur->id > 0){
					del_filter_rule(NF_MOD_NET, t_cur->id);
					t_cur->id = 0;
				}
			}
		}
	}
	return 0;
}
int ifilter_add_mac(char*mac, int appid,ifilter_t *head){
	unsigned char macbuf[32];
	set_mac(macbuf,mac);
	ifilter_t * ptr;
	int ret;
	ptr = ifilter_search(macbuf,head);
	if(ptr == NULL){
		ptr=ifilter_new(macbuf);
		if(ptr == NULL){
			DEBUG("ERROR:ifilter_new error,mac=%s,id=%u\n",mac,appid);
			return -1;
		}
		//memcpy(ptr->mac,macbuf,sizeof(int)*7);	
		ifilter_add(ptr,head);
	}else{
		DEBUG("find ptr\n");
	}
	//ifilter_clear(ptr);
	//ifilter_addid(appid,ptr);

	ret =ifilter_addid(appid, ptr);
	if(ret < 0){
		DEBUG("ERROR:ADD mac %s,%u\n",mac,appid);
	}else{
		DEBUG("SUCCESS:ADD mac %s,%u\n",mac,appid);
	}
	//ifilter_set(ptr);
	return ret;
}
int ifilter_del_mac(char*mac, int appid,ifilter_t *head){
	unsigned char macbuf[32];
	set_mac(macbuf,mac);
	ifilter_t * ptr;
	int ret;
	ptr = ifilter_search(macbuf,head);
	if(ptr == NULL){
		DEBUG("ERROR:can't find,mac=%s,id=%u\n",mac,appid);
		return 0;
	}
	ret =ifilter_delid(appid,ptr);
	DEBUG("SUCCESS:DEL mac %s,%u\n",mac,appid);
	return ret;
}
#endif
#if 0
#define INET_HOST_NONE	0 /* match all */
#define INET_HOST_STD	1 /* match ip range */
#define INET_HOST_UGRP	2 /* match ugrp */
#define INET_HOST_MAC	3 /* match mac */
struct inet_host {
	int type;
	struct ip_range addr; /*  ip range */
	group_mask_t ugrp; /*  user group */
	unsigned char mac[6]; /* one mac */
};
#endif
static int  add_blacklist(char*macs){
	int ret;
	struct nf_rule_info info;
	memset(&info, 0x0, sizeof(info));
	info.base.host.type = INET_HOST_MAC;
	memcpy(info.base.host.mac,macs,6);
	//set_mac(info.base.host.mac,macs);
	info.base.protocol = INET_PROTO_ALL;
	info.target.t_type = NF_TARGET_DENY;
	unsigned char *mac = info.base.host.mac;
	if ((ret = add_filter_rule(NF_MOD_NET, 0, &info)) <= 0) {
		DEBUG("ERROR: ADD ERROR\n");
	}
	return ret;
	//del_filter_rule(INET_HOST_MAC, ret);
}
static int add_blacklist_app(char*macs,int n,int*app)
{
	struct inet_host info;
	struct app_filter_rule appinfo;

	if (n >= APP_RULE_MAX)
		n = APP_RULE_MAX;

	memset(&info, 0x0, sizeof(info));
	info.type = INET_HOST_MAC;
	memcpy(info.mac,macs,6);

	appinfo.nr = n;
	appinfo.prio = -1;
	appinfo.type = NF_TARGET_DENY;
	memcpy(appinfo.appid, app, sizeof(int)*n);
	return register_app_filter(&info, &appinfo);
}
static int rate_mac_del_in_data(char*mac,int appid,irate_t *ratedata){
	int ret;
	if(ratedata->id > 0){
		ret =del_filter_rule(NF_MOD_RATE,ratedata->id);
		if(ret < 0){
			return -1;
		}
	}
	ratedata->flag = 0;
	ratedata->speed = 0;
	ratedata->id = 0;

	return 0;
}
static int rate_mac_add_in_data(char*mac,int appid,int speed,irate_t *ratedata,int online){
	int ret;
	struct nf_rule_info info;
	if(!online){
		ratedata->flag = 1;
		ratedata->speed = speed;
		ratedata->id = 0;
		return 0;
	}
	memset(&info, 0x0, sizeof(info));
	info.base.host.type = INET_HOST_MAC;
	memcpy(info.base.host.mac,mac,6);
	info.base.l7.mid = 0;
	info.base.l7.appid = 0;
	//set_mac(info.base.host.mac,macs);
	info.base.protocol = INET_PROTO_ALL;
	info.target.t_type = NF_TARGET_RATE;
	info.target.target.rate.down = speed;
	//unsigned char *mac = info.base.host.mac;
	if ((ret = add_filter_rule(NF_MOD_RATE, 0, &info)) <= 0) {
		DEBUG("ERROR: ADD ERROR3,ret=%d\n",ret);
		return -1;
	}
	ratedata->flag = 1;
	ratedata->speed = speed;
	ratedata->id = ret;
	DEBUG("id=%d\n",ret);
	return 0;
}
static int rate_mac_add_in(char*mac,int appid,int speed,ifilter_t *head){
	ifilter_t * ptr;
	int ret;
	ptr = ifilter_search(mac,head);
	if(ptr == NULL){
		ptr=ifilter_new(mac);
		if(ptr == NULL){
			DEBUG("ERROR: MALLOC");
			return -1;
		}
		ifilter_add(ptr,head);
	}else{
		if(ptr->rate.flag == 1){
			if(ptr->rate.speed == speed){
				return 0;
			}else{
				ret =rate_mac_del_in_data(mac,appid,&ptr->rate);
				if(ret < 0){
					DEBUG("ERROR 2");
					return -1;
				}
			}
		}
	}
	return rate_mac_add_in_data(mac,appid,speed,&ptr->rate,ptr->online);
}
static int rate_mac_del_in(char*mac,int appid,ifilter_t *head){
	ifilter_t * ptr;
	int ret;
	ptr = ifilter_search(mac,head);
	if(ptr == NULL){
		return 0;
	}
	if(ptr->rate.flag == 0){
		return 0;
	}
	return rate_mac_del_in_data(mac,appid,&ptr->rate);
}
static int rate_mac_get_in(char*mac,int appid,ifilter_t *head){
	ifilter_t * ptr;
	int ret;
	ptr = ifilter_search(mac,head);
	if(ptr == NULL){
		return -1;
	}else{
		if(ptr->rate.flag == 0){
			return -1;
		}else{
			return ptr->rate.speed;
		}
	}
	return -1;
}
int rate_mac_add(char*macbuf,int appid,int speed,ifilter_t *head){
	int ret;
	int rspeed;
	unsigned char mac[6];
	DEBUG("regirset mac=%s\n",macbuf);
	set_mac(mac,macbuf);
	rspeed =rate_mac_get_in(mac,appid,head);
	if(rspeed >= 0){
		if(rspeed == speed){
			return 0;
		}
		ret = rate_mac_del_in(mac,appid,head);
		if(ret != 0){
			DEBUG("ERROR 1");
			return -1;
		}
	}
	DEBUG("mac=%s,appid=%d,speed=%d,rspeed=%d",macbuf,appid,speed,rspeed);
	return rate_mac_add_in(mac,appid,speed,head);
}
int rate_mac_del(char*macbuf,int appid,ifilter_t *head){
	int ret;
	int rspeed;
	DEBUG("regirset mac=%s\n",macbuf);
	unsigned char mac[6];
	set_mac(mac,macbuf);
	rspeed =rate_mac_get_in(mac,appid,head);
	if(rspeed < 0){
		return 0;
	}
	return rate_mac_del_in(mac,appid,head);
}
int rate_mac_get(char*macbuf,int appid,ifilter_t *head){
	unsigned char mac[6];
	set_mac(mac,macbuf);
	return rate_mac_get_in(mac,appid,head);
}

int rate_mac_get2(char*mac,int appid,ifilter_t *head){
	return rate_mac_get_in(mac,appid,head);
}
int ratetest(char*macbuf,int appid,int speed){
	int ret;
	unsigned char mac[6];
	DEBUG("???\n");
	DEBUG("mac=%s,appid=%d,speed=%d",macbuf,appid,speed);
	set_mac(mac,macbuf);
	irate_t test;
	memset(&test,0,sizeof(irate_t));
	ret =rate_mac_add_in_data(mac,appid,speed,&test,1);
	DEBUG("ret=%d\n",ret);
	ret =rate_mac_del_in_data(mac,appid,&test);
	DEBUG("ret=%d\n",ret);
}
#if 0
int ratetest(char*macbuf,int appid,int speed){
	unsigned char mac[7];
	DEBUG("regirset mac=%s\n",macbuf);
	set_mac(mac,macbuf);
	int ret;
	struct nf_rule_info info;
	memset(&info, 0x0, sizeof(info));
	info.base.host.type = INET_HOST_MAC;
	memcpy(info.base.host.mac,mac,7);
	info.base.l7.mid = 0;
	info.base.l7.appid = 0;
	//set_mac(info.base.host.mac,macs);
	info.base.protocol = INET_PROTO_ALL;
	info.target.t_type = NF_TARGET_RATE;
	info.target.target.rate.down = speed;
	//unsigned char *mac = info.base.host.mac;
	if ((ret = add_filter_rule(NF_MOD_RATE, 0, &info)) <= 0) {
		DEBUG("ERROR: ADD ERROR\n");
		return -1;
	}
	DEBUG("add =ret=%d\n",ret);
	ret =del_filter_rule(NF_MOD_RATE,ret);
	DEBUG("del ret=%d\n",ret);
	return 0;
}
#endif
int  add_blacklist_id(char*macs,int id){
	int ret;
	print_mac(macs);
	printf("id=%d\n",id);
	struct nf_rule_info info;
	memset(&info, 0x0, sizeof(info));
	info.base.host.type = INET_HOST_MAC;
	memcpy(info.base.host.mac,macs,6);
	//set_mac(info.base.host.mac,macs);
	if(id > 0){
		info.base.l7.type = INET_L7_ALL;
		info.base.l7.mid = L7_GET_MID(id);
		info.base.l7.appid = L7_GET_APPID(id);
		}
	info.base.protocol = INET_PROTO_ALL;
	info.target.t_type = NF_TARGET_DENY;
	unsigned char *mac = info.base.host.mac;
	if ((ret = add_filter_rule(NF_MOD_NET, 0, &info)) <= 0) {
		DEBUG("ERROR: ADD ERROR\n");
	}
	return ret;
	//del_filter_rule(INET_HOST_MAC, ret);
}
static inline itime_t* itime_search(unsigned appid ,itime_t *head){
	struct list_head *pos;
	itime_t* ptr;
	list_for_each(pos, &head->node){
		if (ptr = list_entry(pos,itime_t,node)){
			if(appid == ptr->appid)return ptr;
		}
	}
	return NULL;
}
int itime_del_id_in(char *mac,int appid,itime_t *head){
	itime_t* ptr;
	ptr = itime_search(appid,head);
	if(ptr == NULL)return 0;
	if(ptr->id > 0)
		del_filter_rule(NF_MOD_NET,ptr->id);
	list_del(&ptr->node);
	free(ptr);
	return 0;
}
int itime_add_id_in(char *mac,int appid,time_t start, time_t end, int repeat,itime_t *tf){
	itime_t* ptr;
	ptr = (itime_t*)malloc(sizeof(itime_t));
	if(ptr == NULL){
		DEBUG("malloc error\n");
		return -1;
	}
	DEBUG("start");
	memset(ptr,0,sizeof(itime_t));
	INIT_LIST_HEAD(&ptr->node);
	ptr->appid = appid;
	ptr->start = start;
	ptr->end = end;
	ptr->repeat = repeat;
	if(!repeat){
		ptr->min = (ptr->start/(24*60*60))*(24*60*60);
		ptr->max = ((ptr->end/(24*60*60))+1)*(24*60*60);
	}
	DEBUG("start");
	list_add(&ptr->node,&tf->node);
	DEBUG("start");
	return 0;
}
int itime_add_mac(unsigned char *mac,int appid,time_t start,time_t end, int repeatflag,ifilter_t *head){
	ifilter_t * ptr;
	int ret;
	DEBUG("start");
	ptr = ifilter_search(mac,head);
	if(ptr == NULL){
		DEBUG("start");
		ptr=ifilter_new(mac);
		if(ptr == NULL){
			DEBUG("ERROR: MALLOC");
			return -1;
		}
		ifilter_add(ptr,head);
		DEBUG("start");
	}else{
		itime_t *tf;
		tf = itime_search(appid,&ptr->it);
		if(tf != NULL){
			itime_del_id_in(mac,appid,tf);
		}
	}
	DEBUG("start");
	return itime_add_id_in(mac,appid,start,end,repeatflag,&ptr->it);
}
int itime_del_mac(unsigned char *mac,int appid,ifilter_t *head){
	ifilter_t *ptr;
	int ret;
	ptr = ifilter_search(mac,head);
	if(ptr == NULL){
		return 0;
	}
	return itime_del_id_in(mac,appid,&ptr->it);
}
int itime_check(unsigned char*mac,int appid,ifilter_t *head){
	ifilter_t * ptr;
	int ret;
	ptr = ifilter_search(mac,head);
	if(ptr == NULL){
		return 0;
	}
	itime_t *tf;
	tf = itime_search(appid,&ptr->it);
	if(tf != NULL){
		return 1;
	}
	return 0;
}
int itime_get(unsigned char*mac,int appid,char*start,char*end,int *repeatflag,ifilter_t *head){
	ifilter_t * ptr;
	int ret;
	//DEBUG("start");
	ptr = ifilter_search(mac,head);
	if(ptr == NULL){
		return -1;
	}
	//DEBUG("start");
	itime_t *tf;
	tf = itime_search(appid,&ptr->it);
	//DEBUG("start");
	if(tf != NULL){
		//DEBUG("start");
		//DEBUG("tf->start=%ld,tf->end=%ld\n",tf->start,tf->end);
		timetostr(tf->start,start,7);
		//DEBUG("tf->start=%ld,tf->end=%ld\n",tf->start,tf->end);
		timetostr(tf->end,end,7);
		//DEBUG("tf->start=%ld,tf->end=%ld\n",tf->start,tf->end);
		*repeatflag = tf->repeat;
		DEBUG("start");
		return 0;
	}
	return -1;
}
