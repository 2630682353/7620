#ifndef __NET_WIFI_H__
#define __NET_WIFI_H__

#include"protocol.h"

//wifi
#define WIFI_SSID				"ssid"
#define WIFI_PASSWD			"passwd"
#define WIFI_MODE				"mode"
#define WIFI_CHANNEL			"channel"
#define WIFI_HIDESSID			"hide"
#define WIFI_SECURIRY			"security"

//lan
#define LAN_IP					"ip"
#define LAN_MASK				"mask"

//wan
#define WAN_IP					"ip"
#define WAN_MASK				"mask"
#define WAN_GATEWAY			"gateway"
#define WAN_DNS1				"dns1"
#define WAN_DNS2				"dns2"
#define WAN_OPERATIONMODE	"mode"
#define WAN_PPPOE				"pppoe"
#define WAN_STATIC				"static"
#define WAN_DHCP				"dhcp"

//package
#define NETWORKPACKAGE		"network"
#define WIFIPACKAGE				"wireless"
#define DHCPPACKAGE			"dhcp"

//comand
#define RELOADNETWORK			"ubus call network reload"
#define NETWORKRESTART			"/etc/init.d/network restart"
#define WIFIUP					"/usr/sbin/wifi"


//error number
#define UCI_GETVAL_ERROR		20141209
#define GET_PROVALUE_ERROR	201412010
#define SET_WIFIAP_ERROR		201412011
#define GET_WIFIAP_ERROR		201412012
#define SET_LANIP_ERROR		201412013
#define GET_LANIP_ERROR		201412014
#define SET_DHCP_ERROR			201412015
#define GET_DHCP_ERROR			201412016


#define SUCCESS		1
#define FAILURE		0

#define SSID_MAX_LEN		32
#define PASSWD_MAX_LEN	64

typedef enum
{
	PHY_11B=1,
	PHY_11G=4,
	PHY_11N=6,
	PHY_11BGN_MIXD=9
}phy_wifi_mode;

typedef enum
{
	DISABLE=1,
	WPAPSK,
	WPAPSK2,
	WPA1PSKWPA2PSK,
}wifi_security;

typedef struct __local_ap_info
{
	char ssid[SSID_MAX_LEN+1];
	char passwd[PASSWD_MAX_LEN+1];
	phy_wifi_mode wifi_mode;
	wifi_security security;
	char channel[2];
	char hide[2];
}local_ap_info;


typedef struct __lan_ip_info
{
	char ip[16];
	char mask[16];
	char mac[18];
}lan_ip_info;

typedef struct __dhcp_info
{
	char start[4];
	char end[4];
	char leasetime[3];
}dhcp_info;




extern int check_ssid(char *ssid);

extern int check_passwd(char *passwd);

extern int check_rangeip(char *ipnum);

extern int check_Legalip(char *ip);

extern int net_wifi_set_local_ap(local_ap_info *ap_info);

extern int net_wifi_get_local_ap(local_ap_info *ap_info);

extern int wifi_set_lan_ip_info(lan_ip_info *lan_ip);

extern int wifi_get_lan_ip_info(lan_ip_info *lan_ip);

extern int wifi_set_dhcp(dhcp_info *info);

extern int wifi_get_dhcp(dhcp_info *info);


#endif
