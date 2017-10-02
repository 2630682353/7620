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

#define IGD_BITS_LONG (sizeof(long)*8)
#ifndef BITS_TO_LONGS
#define BITS_TO_LONGS(n) ((n + (sizeof(long)*8) - 1)/ (sizeof(long)* 8))
#endif
#define MAC_SPLIT(mac) mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]

static inline int igd_test_bit(int nr, unsigned long *bit)
{
	unsigned long mask = 1UL << (nr % IGD_BITS_LONG);
	unsigned long *p = bit + nr / IGD_BITS_LONG;

	return (*p & mask) != 0;
}

static inline int igd_set_bit(int nr, unsigned long *bit)
{
	unsigned long mask = 1UL << (nr % IGD_BITS_LONG);
	unsigned long *p = bit + nr / IGD_BITS_LONG;
	*p |= mask;
	return 0;
}

static inline int igd_clear_bit(int nr, unsigned long *bit)
{
	unsigned long mask = 1UL << (nr % IGD_BITS_LONG);
	unsigned long *p = bit + nr / IGD_BITS_LONG;
	*p &= ~mask;
	return 0;
}

#endif //__CLOUD_COMM_H__
