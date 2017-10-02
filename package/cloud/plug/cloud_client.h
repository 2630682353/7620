#ifndef __CLOUD_CLIENT_H__
#define __CLOUD_CLIENT_H__

#include "linux_list.h"

#define CC_LOG_FILE   "/tmp/cc_dbg"
#define CC_ERR_FILE   "/tmp/cc_err"

#if 0
#define CC_DBG(fmt,args...) do {}while(0)
#define CC_ERR(fmt,args...) do {}while(0)
#else
#define CC_DBG(fmt,args...) do { \
	igd_log(CC_LOG_FILE, "DBG:[%05ld,%05d],%s:"fmt, \
		sys_uptime(), __LINE__, __FUNCTION__, ##args);\
} while(0)
#define CC_ERR(fmt,args...) do { \
	igd_log(CC_ERR_FILE, "ERR:[%05ld,%05d],%s:"fmt, \
		sys_uptime(), __LINE__, __FUNCTION__, ##args);\
} while(0)
#endif

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

/*CCF: cloud client flag*/
enum CLOUD_CLIENT_FLAG {
	CCF_HARDWARE = 0,
	CCF_RESET,
	CCF_UPGRADE,

	CCF_MAX, //must last
};

struct cloud_client_info {
	int sock;
	unsigned int key;
	unsigned int devid;
	unsigned int userid;
	int len;
	unsigned char *data;
	unsigned char ctime;
	unsigned long rtime;
	unsigned long flag[BITS_TO_LONGS(CCF_MAX)];
};

struct plug_cache_info {
	struct list_head list;
	pid_t pid;
	char plug_name[64];
	char url[128];
	char md5[33];
};

extern int cc_send(int sock, unsigned char *buf, int len);
extern int cc_plug_start(struct plug_cache_info *pci);
extern int cc_plug_stop(struct plug_cache_info *pci);
extern void cc_plug_loop(struct cloud_client_info *cci);
extern int cc_plug_wait(pid_t pid, int state, struct cloud_client_info *cci);
#endif

