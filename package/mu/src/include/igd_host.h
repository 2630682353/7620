#ifndef __IGD_HOST_HEADER__
#define __IGD_HOST_HEADER__

#ifdef __cplusplus
extern "C" {
#endif

#include <arpa/inet.h>
#include <ctype.h>
#include <stdlib.h>
#include "linux_list.h"
#include "igd_share.h"
#include "module.h"

#define IGD_HOST_MX   HOST_MX //is unsigned char max 255
#define IGD_HOST_IP_MX   IGD_HOST_MX //is unsigned char max 255

#ifdef FLASH_4_RAM_32
#define IGD_HOST_APP_MX   30 //is unsigned char max 255
#define IGD_HOST_URL_MX   20
#define IGD_HOST_LOG_MX   50 //is unsigned char max 255
#define IGD_HOST_UA_MX   30 //is unsigned char max 255
#else
#define IGD_HOST_APP_MX   60 //is unsigned char max 255
#define IGD_HOST_URL_MX   20
#define IGD_HOST_LOG_MX   200 //is unsigned char max 255
#define IGD_HOST_UA_MX   50 //is unsigned char max 255
#endif

#define IGD_HOST_INTERCEPT_URL_MX   20
#define IGD_HOST_FILTER_MX   20
#define IGD_APP_MOD_TIME_MX   10
#define IGD_STUDY_TIME_NUM   10
#define IGD_STUDY_URL_SELF_NUM   128
#define IGD_STUDY_URL_SELF_ID_MIN   (L7_APP_MX*L7_MID_HTTP + 1)
#define IGD_STUDY_URL_SELF_ID_MAX   (IGD_STUDY_URL_SELF_ID_MIN + IGD_STUDY_URL_SELF_NUM)
#define IGD_STUDY_URL_DEFAULT_NUM   256
#define IGD_STUDY_URL_MX   (IGD_STUDY_URL_DEFAULT_NUM + IGD_STUDY_URL_SELF_NUM)
#define IGD_STUDY_FILE_SIZE   500*1024
#define IGD_STUDY_URL_FILE   "/tmp/rconf/study_url.txt"
#define IGD_TEST_STUDY_URL(id)  (L7_GET_MID(id) == (L7_MID_HTTP))
#define IGD_SELF_STUDY_URL(id)  ((id)/((L7_APP_MX)/10) == (L7_MID_HTTP)*10)
#define IGD_REDIRECT_STUDY_URL_APPID   (L7_APP_MX*(L7_MID_HTTP + 1) - 1)
#define IGD_CHUYUN_APPID   14000020
#define IGD_HOST_UA_FILE  "/tmp/rconf/UA.txt"
#define IGD_HOST_UA_FILE_SIZE  (500*1024)


#define HOST_UPLOAD_NO_PUSH   0
#define HOST_UPLOAD_FIRST_PUSH   1
#define HOST_UPLOAD_EVERY_PUSH   2
#define HOST_UPLOAD_INFO_UPTATE   3

/*HIRF: host info record flag*/
enum HOST_INFO_RECORD_FLAG {
	HIRF_ONLINE = 0,
	HIRF_INBLACK,
	HIRF_ISKING,
	HIRF_LIMIT_SPEED,
	HIRF_LIMIT_TIME,
	HIRF_NICK_NAME,
	HIRF_ONLINE_PUSH,
	HIRF_VAP,
	HIRF_WIRELESS,
	HIRF_WIRELESS_5G,
	HIRF_VAP_ALLOWED,

	HIRF_MAX, //must last
};

/*HTRF: host tmp record flag*/
enum HOST_TMP_RECORD_FLAG {
	HTRF_DHCP = 0,
	HTRF_LOAD,
	HTRF_NEW,
	HTRF_INSTUDY,
	HTRF_INCLASSIFY,

	HTRF_MAX, //must last
};

/*HTRF: host tmp record flag*/
enum HOST_APP_RECORD_FLAG {
	HARF_ONLINE = 0,
	HARF_INBLACK,
	HARF_LIMIT_SPEED,
	HARF_LIMIT_TIME,
	HARF_INWHITE,

	HARF_MAX, //must last
};

/*HF: host flag*/
enum HOST_FLAG {
	HF_HISTORY = 0, //get history host  for server
	HF_STUDY_URL,
	HF_GLOBAL_INFO,

	HF_MAX, //must last
};

/*HNM: host mode*/
enum HOST_MODE {
	HM_FREE = 0,
	HM_STUDY,
	HM_CLASSIFY,

	HM_MAX, //must last
};

struct host_num_info {
	unsigned char app;
	unsigned char ip;
	unsigned char log;
};

struct host_ip_info {
	struct list_head list;
	struct in_addr ip;
};

struct host_base_info {
	char name[32];
	char nick[32];
	uint8_t os_type;
	unsigned char mode;
	unsigned char pic;
	uint16_t vender;
};

#define UPTIME_SYS 1
#define UPTIME_NET 2
struct uptime_record_info {
	unsigned char flag;
	unsigned long time;
};

struct time_record_info {
	struct uptime_record_info up;
	unsigned long all;
	unsigned long old;
};

struct bytes_record_info {
	uint64_t up;
	uint64_t down;
	uint64_t old_up;
	uint64_t old_down;
};

struct host_app_mod_info {
	unsigned long up_time;
	unsigned long all_time;
	unsigned long clock_gap;
	unsigned long clock_last;
	unsigned long upload_time;
};

/*RLT: rule list type*/
enum RULE_LIST_TYPE {
	RLT_BLACK = 1,
	RLT_SPEED,
	RLT_STUDY_URL,
	RLT_STUDY_TIME,
	RLT_URL_BLACK,
	RLT_URL_WHITE,
	RLT_APP_BLACK,
	RLT_INTERCEPT_URL,
	RLT_APP_MOD_QUEUE,
	RLT_APP_WHITE,
};

struct limit_speed_info {
	int rule_id;
	unsigned long up;
	unsigned long down;
};

struct allow_study_url_list {
	struct list_head list;
	unsigned int url_id;
};

struct allow_study_url_info {
	int grp_id;
	int rule_id;
	int redirect_id;
	int black_id;
	struct list_head url_list;
};

struct study_time_list {
	struct list_head list;
	unsigned char enable;
	struct time_comm time;
};

struct study_time_info {
	int time_num;
	struct list_head time_list;
};

struct host_url_list {
	struct list_head list;
	char *url;
};

struct host_url_info {
	int grp_id;
	int rule_id;
	int url_num;
	struct list_head url_list;
};

struct app_mod_black_list {
	struct list_head list;
	unsigned char enable;
	struct time_comm time;
	unsigned long mid_flag[BITS_TO_LONGS(L7_MID_MX)];
};

struct app_black_info {
	int rule_id;
	int num;
	unsigned long mid_old[BITS_TO_LONGS(L7_MID_MX)];
	struct list_head mod_list;
};

struct host_intercept_url_list {
	struct list_head list;
	char *url;
	char *type;
	char *type_en;
	unsigned long time;
	unsigned short dport;
	unsigned short sport;
	struct in_addr daddr;
	struct in_addr saddr;
};

struct host_intercept_url_info {
	int grp_id;
	int rule_id;
	int url_num;
	struct list_head url_list;
};

struct app_mod_queue {
	int rule_id[L7_MID_MX];
	unsigned char mid[L7_MID_MX];
};

struct app_white_info {
	int rule_id;
};

struct rule_info_list {
	struct list_head list;
	unsigned char type;
	void *rule;
};

//for save host ua info
struct host_ua_record {
	struct list_head list;
	char *ua;
};

/*HLT: host log type*/
enum HOST_LOG_TYPE {
	HLT_APP = 1,
	HLT_HOST,
	HLT_MODE,
	HLT_SELF_URL,

	HLT_MAX, //must last
};

/*HLF: host log flag*/
enum HOST_LOG_FLAG {
	HLF_UP = 0,
	HLF_CLEAR,

	HLF_MAX, //must last
};

//for save host log info
struct host_log_record {
	struct list_head list;
	unsigned char type;
	union {
		char url_name[32];
		unsigned int appid;
		unsigned char mode;
	} v;
	unsigned long time;
	unsigned long oltime;
	unsigned long flag[BITS_TO_LONGS(HLF_MAX)];
};

//for save study url info
struct study_url_record {
	struct list_head list;
	unsigned int id;
	unsigned char used;
	char *name;
	char *url;
	char *info;
};

//for save host app info
struct host_app_record {
	struct list_head list;
	uint32_t appid;
	unsigned long flag[BITS_TO_LONGS(HARF_MAX)];
	struct time_record_info time;
	struct bytes_record_info bytes;
	struct time_comm lt;
};

//for save host info
struct host_info_record {
	struct list_head list;
	struct list_head ip_list;
	struct list_head app_list;
	struct list_head rule_list;
	struct list_head log_list;
	unsigned char mac[6];
	struct host_num_info num;
	struct host_base_info base;
	struct time_record_info time;
	unsigned long flag[BITS_TO_LONGS(HIRF_MAX)];
	unsigned long tmp[BITS_TO_LONGS(HTRF_MAX)];
	struct host_app_mod_info mid[L7_MID_MX];
	struct bytes_record_info bytes;
};

//for save host filter list
struct host_filter_list {
	struct list_head list;
	int rule_id;
	unsigned char enable;
	struct time_comm time;
	struct inet_host host;
	struct net_rule_api arg;
};

//for dump host_app
struct host_app_dump_info {
	uint32_t appid;
	unsigned long down_speed;
	unsigned long up_speed;
	uint64_t up_bytes;
	uint64_t down_bytes;
	unsigned long uptime;
	unsigned long ontime;
	struct time_comm lt;
	unsigned long flag[BITS_TO_LONGS(HARF_MAX)];
};

//for dump host
struct host_dump_info {
	unsigned char mac[6];
	unsigned char mode;
	unsigned char pic;
	struct in_addr ip[1];
	char name[32];
	uint8_t os_type;
	uint16_t vender;
	unsigned long flag[BITS_TO_LONGS(HIRF_MAX)];
	unsigned long up_speed;
	unsigned long down_speed;
	uint64_t up_bytes;
	uint64_t down_bytes;
	unsigned long uptime;
	unsigned long ontime;
	struct limit_speed_info ls;
};

//for dump study url
struct study_url_dump {
	unsigned int id;
	char name[32];
	char url[64];
	char info[32];
};

//for dump host url
struct host_url_dump {
	unsigned char type;
	char url[IGD_URL_LEN];
};

//for dump study time
struct study_time_dump {
	unsigned char enable;
	struct time_comm time;
};

//for dump app mod
struct app_mod_dump {
	unsigned char enable;
	struct time_comm time;
	unsigned long mid_flag[BITS_TO_LONGS(L7_MID_MX)];
};

//for dump host intercept url black
struct host_intercept_url_dump {
	char url[IGD_URL_LEN];
	char type[32];
	char type_en[32];
	unsigned long time;
	unsigned short dport;
	unsigned short sport;
	struct in_addr daddr;
	struct in_addr saddr;
};

//for host filter dump/set
struct host_filter_info {
	int act;
	unsigned char enable;
	struct time_comm time;
	struct inet_host host;
	struct net_rule_api arg;
};

//for cgi set info
struct host_set_info {
	unsigned char mac[6];
	unsigned int appid;
	int act;
	union {
		char name[32];
		unsigned char mode;
		unsigned char pic;
		unsigned char mid[L7_MID_MX];
		int new_push;
		struct limit_speed_info ls;
		struct study_url_dump surl;
		struct time_comm time;
		struct study_time_dump study_time;
		struct host_url_dump bw_url;
		struct app_mod_dump app_mod;
		struct host_intercept_url_dump intercept_url;
	} v;
};

struct igd_host_global_info {
	int new_push;
	int king_rule_id;
	unsigned char king_mac[6];
	struct app_mod_queue amq; //app queue
};

enum {
	IGD_HOST_INIT = DEFINE_MSG(MODULE_HOST, 0),
	IGD_HOST_DAY_CHANGE,
	IGD_HOST_ONLINE,
	IGD_HOST_OFFLINE,
	IGD_HOST_APP_ONLINE,
	IGD_HOST_APP_OFFLINE,
	IGD_HOST_DBG_FILE,
	IGD_HOST_DUMP,
	IGD_HOST_APP_DUMP,
	IGD_HOST_ADD_HISTORY,
	IGD_HOST_DEL_HISTORY,
	IGD_STUDY_URL_UPDATE,
	IGD_STUDY_URL_DNS_UPDATE,
	IGD_HOST_UA_UPDATE,
	IGD_HOST_VENDER_UPDATE,
	IGD_HOST_IP2MAC,
	IGD_VENDER_NAME,
	IGD_HOST_NAME_UPDATE,
	IGD_HOST_OS_TYPE_UPDATE,
	IGD_HOST_UA_COLLECT,
	IGD_HOST_DHCP_INFO,
	IGD_HOST_SET_NICK,
	IGD_HOST_SET_ONLINE_PUSH,
	IGD_HOST_SET_PIC,
	IGD_HOST_SET_BLACK,
	IGD_HOST_SET_LIMIT_SPEED,
	IGD_HOST_SET_MODE,
	IGD_HOST_SET_KING,
	IGD_HOST_SET_NEW_PUSH,
	IGD_STUDY_URL_ACTION,
	IGD_HOST_STUDY_URL_ACTION,
	IGD_HOST_STUDY_TIME,
	IGD_HOST_URL_ACTION,
	IGD_HOST_SET_APP_BLACK,
	IGD_HOST_SET_APP_LIMIT_TIME,
	IGD_HOST_APP_MOD_ACTION,
	IGD_HOST_INTERCEPT_URL_BLACK_ACTION,
	IGD_HOST_APP_MOD_QUEUE,
	IGD_APP_MOD_QUEUE,
	IGD_HOST_SET_APP_WHITE,
	IGD_HOST_VAP_ALLOW,
	IGD_HOST_SET_ALI_BLACK,
	IGD_HOST_SET_FILTER,
};

extern int igd_host_call(MSG_ID msgid, void *data, int d_len, void *back, int b_len);

#ifdef __cplusplus
}
#endif
#endif
