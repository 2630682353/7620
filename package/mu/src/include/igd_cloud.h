#ifndef __CLOUD_COMM_H__
#define __CLOUD_COMM_H__

#include "module.h"

/*
CSO: cloud server order
*/
#define CSO_XX -1
#define CSO_LINK 1
#define CSO_WAIT 2
#define CSO_NTF_KEY 1000
#define CSO_REQ_ROUTER_FIRST 1002
#define CSO_ACK_ROUTER_FIRST 1003
#define CSO_REQ_ROUTER_AGAIN 1004
#define CSO_ACK_ROUTER_AGAIN 1005
#define CSO_REQ_ROUTER_LOGIN 1006
#define CSO_ACK_ROUTER_LOGIN 1007
#define CSO_NTF_ROUTER_REDIRECT 1008
#define CSO_REQ_ROUTER_KEEP 1101
#define CSO_ACK_ROUTER_KEEP 1102
#define CSO_NTF_ROUTER_TRANSFER 1103
#define CSO_NTF_ROUTER_VERSION 1104
#define CSO_REQ_ROUTER_RELEASE 1105
#define CSO_ACK_ROUTER_RELEASE 1106
#define CSO_NTF_ROUTER_HARDWARE 1107
#define CSO_NTF_ROUTER_BIND 1108
#define CSO_NTF_ROUTER_RELEASE 1109

#define CSO_NTF_ROUTER_FLOW  1110
#define CSO_NTF_ROUTER_TERMINAL  1111
#define CSO_NTF_ROUTER_APP  1112
#define CSO_REQ_ROUTER_RECORD  1113
#define CSO_ACK_ROUTER_RECORD  1114
#define CSO_REQ_CONNECT_ONLINE  1115
#define CSO_ACK_CONNECT_ONLINE  1116
#define CSO_NTF_ROUTER_MMODEL  1117
#define CSO_NTF_ROUTER_MAC  1118
#define CSO_REQ_ROUTER_RESET  1119
#define CSO_ACK_ROUTER_RESET  1120
#define CSO_REQ_SITE_CUSTOM  1121
#define CSO_ACK_SITE_CUSTOM  1122
#define CSO_REQ_SITE_RECOMMAND  1123
#define CSO_ACK_SITE_RECOMMAND  1124
#define CSO_REQ_VISITOR_MAC  1125
#define CSO_ACK_VISITOR_MAC  1126
#define CSO_NTF_GAME_TIME  1127
#define CSO_NTF_DEL_HISTORY  1128
#define CSO_NTF_ROUTER_APP_ONOFF  1129
#define CSO_NTF_ROUTER_ONLINE  1130
#define CSO_REQ_ROUTER_SSID  1131
#define CSO_ACK_ROUTER_SSID  1132
#define CSO_NTF_ROUTER_UPDOWN  1133
#define CSO_NTF_ROUTER_CODE  1134
#define CSO_NTF_ROUTER_PRICE  1135
#define CSO_ACK_ROUTER_PRICE  1136
#define CSO_REQ_ROUTER_CONFIG  1137
#define CSO_ACK_ROUTER_CONFIG  1138
#define CSO_REQ_CONFIG_VER	1139
#define CSO_ACK_CONFIG_VER  1140
#define CSO_REQ_AREA_CONFIG  1141
#define CSO_ACK_AREA_CONFIG  1142
#define CSO_REQ_AREA_VER  1143
#define CSO_ACK_AREA_VER  1144
#define CSO_REQ_SWITCH_STATUS  1145
#define CSO_NTF_SWITCH_STATUS  1146
#define CSO_UP_SWITCH_STATUS  1147
#define CSO_NTF_PPPOE_ACTION  1148
#define CSO_REQ_PLUGIN_INFO  1149
#define CSO_NTF_PLUGIN_INFO  1150
#define CSO_UP_PLUGIN_INFO  1151


/*
CPO: cloud private order
*/
#define CPO_EXIT 0
#define CPO_WAIT 1
#define CPO_FREE 2
#define CPO_SERVER_TO_ROUTE 10
#define CPO_ROUTE_TO_SERVER 11

#define CPO_NTF_ROUTER_REDIRECT  5302
#define CPO_REQ_ROUTER_KEEP  5303
#define CPO_ACK_ROUTER_KEEP  5304
#define CPO_NTF_ROUTER_TRANSFER  5305
#define CPO_ACK_ROUTER_TRANSFER  5306
#define CPO_REQ_START_SYNC_TO_ROUTER  5500
#define CPO_ACK_START_SYNC_TO_ROUTER  5501
#define CPO_REQ_START_SYNC_FROM_ROUTER  5502
#define CPO_ACK_START_SYNC_FROM_ROUTER  5503
#define CPO_NTF_SYNC_FILE  5504
#define CPO_ACK_SYNC_FILE  5505
#define CPO_NTF_SYNC_OVER  5506
#define CPO_ACK_SYNC_OVER  5507


#define IGD_CLOUD_CACHE_MX 100

#define CC_PUSH1(buf, i, d)   (*((unsigned char *)&buf[i]) = (unsigned char)(d))
#define CC_PUSH2(buf, i, d)   (*((unsigned short *)&buf[i]) = (unsigned short)htons((unsigned short)d))
#define CC_PUSH4(buf, i, d)   (*((unsigned int *)&buf[i]) = (unsigned int)htonl((unsigned int)d))
#define CC_PUSH8(buf, i, d)   (*((unsigned long long *)&buf[i]) = (unsigned long long)cc_htonll((unsigned long long)d))
#define CC_PUSH_LEN(buf, i, data, len)   memcpy(buf+(i), data, len)

#define CC_PULL1(buf, i)   *(unsigned char *)&buf[i]
#define CC_PULL2(buf, i)   ntohs(*(unsigned short *)&buf[i])
#define CC_PULL4(buf, i)   ntohl(*(unsigned int *)&buf[i])
#define CC_PULL8(buf, i)   cc_ntohll(*(unsigned long long *)&buf[i])
#define CC_PULL_LEN(buf, i, dst, len)   memcpy(dst, &buf[i], len)

#define N_MAX(a, b)  (((a) > (b)) ? (a) : (b))
#define N_MIN(a, b)  (((a) > (b)) ? (b) : (a))

#define NF2_MAC "%02X%02X%02X%02X%02X%02X"

//used by mu progress
#define CC_CALL_ADD(sbuf,slen)  mu_call(IGD_CLOUD_CACHE_ADD, sbuf, slen, NULL, 0)
#define CC_CALL_DUMP(rbuf,rlen)  mu_call(IGD_CLOUD_CACHE_DUMP, NULL, 0, rbuf, rlen)

//used by other progress
#define CC_MSG_ADD(sbuf,slen)  mu_msg(IGD_CLOUD_CACHE_ADD, sbuf, slen, NULL, 0)
#define CC_MSG_DUMP(rbuf,rlen)  mu_msg(IGD_CLOUD_CACHE_DUMP, NULL, 0, rbuf, rlen)

//used by mu progress, fail: <=0, success: >0, return strlen(value)
#define CC_CALL_RCONF(flag,value,value_len) \
	mu_call(IGD_CLOUD_RCONF_GET, flag, strlen(flag), value, value_len)

//used by other progress, fail: <=0, success: >0, return strlen(value)
#define CC_MSG_RCONF(flag,value,value_len) \
	mu_msg(IGD_CLOUD_RCONF_GET, flag, strlen(flag) + 1, value, value_len)

//used by mu progress, fail: < 0, success: 0
#define CC_CALL_RCONF_INFO(flag,value,value_len) \
	mu_call(IGD_CLOUD_RCONF_GET_INFO, flag, strlen(flag), value, value_len)

//used by other progress, fail: < 0, success: 0
#define CC_MSG_RCONF_INFO(flag,value,value_len) \
	mu_msg(IGD_CLOUD_RCONF_GET_INFO, flag, strlen(flag) + 1, value, value_len)

/* CCT: cloud config type */
enum CLOUD_CONF_TYPE {
	CCT_L7 = 0,
	CCT_UA,
	CCT_DOMAIN,
	CCT_WHITE,
	CCT_VENDER,
	CCT_TSPEED,
	CCT_STUDY_URL,
	CCT_VPN_DNS,

	CCT_ERRLOG, //must together be first
	CCT_URLLOG, //must together
	CCT_URLSAFE, //must together
	CCT_URLSTUDY, //must together
	CCT_URLHOST, //must together
	CCT_URLREDIRECT, //must together
	CCT_VPN_SERVEER, //must together
	CCT_CHECKUP, //must together
	CCT_MAX, //must be last
};

/*CCA: cloud conf alone */
enum CLOUD_CONF_ALONE {
	CCA_JS = 1, //must first
	CCA_P_L7,
	CCA_SYSTEM,

	CCA_MAX, //must last
};

/*CSS: cloud switch status */
enum CLOUD_SWITCH_STATUS {
	CSS_PROXY = 1, //must first
	CSS_RPGBIND,
	CSS_LANXUN,
	CSS_XINSIGHT,
	CSS_TESLA,
	CSS_DBGLOG,
	CSS_DBCLOUD = 7,//can`t change,combine to script,must be 7
	CSS_WWLH = 10,
	CSS_MAX, //must last
};

#define RCONF_RETRY_NUM   3
#ifdef FLASH_4_RAM_32
#define RCONF_CHECK   "/tmp/rconf_check_old"
#else
#define RCONF_CHECK   "/etc/rconf_check"
#endif
#define RCONF_CHECK_TMP   "/tmp/rconf_check"
#define RCONF_DIR  "/etc/rconf"
#define RCONF_DIR_TMP  "/tmp/rconf"
#define RCONF_DIR_NEW  "/tmp/rconf_new"
#define ALONE_DIR  "/tmp/alone"

#define RCONF_FLAG_VER           "VER:"
#define RCONF_FLAG_L7            "F01:"
#define RCONF_FLAG_UA            "F02:"
#define RCONF_FLAG_DOMAIN        "F03:"
#define RCONF_FLAG_WHITE         "F04:"
#define RCONF_FLAG_VENDER        "F05:"
#define RCONF_FLAG_TSPEED        "F06:"
#define RCONF_FLAG_STUDY_URL     "F07:"
#define RCONF_FLAG_ERRLOG        "F09:"
#define RCONF_FLAG_URLLOG        "F10:"
#define RCONF_FLAG_URLSAFE       "F11:"
#define RCONF_FLAG_URLSTUDY      "F12:"
#define RCONF_FLAG_URLHOST       "F13:"
#define RCONF_FLAG_URLREDIRECT   "F14:"
#define RCONF_FLAG_CHECKUP       "F15:"
#define RCONF_FLAG_VPNSERVER     "F16:"
#define RCONF_FLAG_VPNDNS        "F17:"
#define RCONF_ZZZZZZZZZZZZ       "F25:"	


struct nlk_cloud_config {
	struct nlk_msg_comm comm;
	int ver[CCT_MAX];
};

struct nlk_alone_config {
	struct nlk_msg_comm comm;
	int flag;
};

struct nlk_switch_config {
	struct nlk_msg_comm comm;
	unsigned char flag;
	char status[32];
};

/*ICFT: igd cloud flag*/
enum IGD_CLOUD_FLAG_TYPE {
	ICFT_ONLINE = 0,
	ICFT_CHECK_RCONF,
	ICFT_UP_RCONF_VER,
	ICFT_UP_FIRST_RCONF,

	ICFT_MAX, //must last
};

enum {
	IGD_CLOUD_INIT = DEFINE_MSG(MODULE_CLOUD, 0),
	IGD_CLOUD_CACHE_ADD,
	IGD_CLOUD_CACHE_DUMP,
	IGD_CLOUD_RCONF_FLAG,
	IGD_CLOUD_RCONF_GET,
	IGD_CLOUD_RCONF_GET_INFO,
	IGD_CLOUD_FLAG,
	IGD_CLOUD_ALONE_CHECK_UP,
	IGD_CLOUD_ALONE_LOCK,
	IGD_CLOUD_ALONE_VER,
	IGD_CLOUD_IP,
	IGD_CLOUD_DNS_UPLOAD
};

extern int igd_cloud_call(MSG_ID msgid, void *data, int d_len, void *back, int b_len);
#endif //__CLOUD_COMM_H__
