/*
* Copyright (c) 2014-2015 Alibaba Group. All rights reserved.
*
* Alibaba Group retains all right, title and interest (including all
* intellectual property rights) in and to this computer program, which is
* protected by applicable intellectual property laws.  Unless you have
* obtained a separate written license from Alibaba Group., you are not
* authorized to utilize all or a part of this computer program for any
* purpose (including reproduction, distribution, modification, and
* compilation into object code), and you must immediately destroy or
* return to Alibaba Group all copies of this computer program.  If you
* are licensed by Alibaba Group, your rights to utilize this computer
* program are limited by the terms of that license.  To obtain a license,
* please contact Alibaba Group.
*
* This computer program contains trade secrets owned by Alibaba Group.
* and, unless unauthorized by Alibaba Group in writing, you agree to
* maintain the confidentiality of this computer program and related
* information and to not disclose this computer program and related
* information to any other person or entity.
*
* THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
* Alibaba Group EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
* INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A PARTICULAR
* PURPOSE, TITLE, AND NONINFRINGEMENT.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <net/if.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include "uci.h"
#include "ioos_uci.h"
#include "uci_fn.h"
#include "linux_list.h"
#include "nlk_ipc.h"
#include "ipc_msg.h"
#include "module.h"
#include "igd_wifi.h"
#include "igd_host.h"
#include "igd_system.h"
#include "igd_qos.h"
#include "alinkgw_api.h"
#include "json/json.h"
#include "igd_interface.h"
#include "igd_ali.h"
#include <sys/sysinfo.h>

/*************************************************************************
alinkgw sdk sample示例
*************************************************************************/

static int g_run_mode = 0;
static int g_loglevel = 0;
//路由器信息
static char g_szDevName[32] =       {"I1"};
static char g_szDevModel[64] =      {"WIAIR_NETWORKING_ROUTER_I1"};
static char g_szManuFacture[32] =   {"CHUYUN"};
//wiair sandbox
static char g_szKey_sandbox[32] = {"s2wOKE4h2QKTribjdvnI"};
static char g_szSecret_sandbox[64] = {"Q2NyUYj1EdALuv2FwF1aHOgUNwZAuNNkbQKMvoAE"};

//wiair online
static char g_szKey[32] =           {"nSVGe6NZVEkFvVxUPn8C"};
static char g_szSecret[64] =        {"LrErst4faLnxHX3dnIK14oh7lf9GOxbkTHXMgxrW"};

static char g_szDevType[32] =       {"ROUTER"};
static char g_szDevCategory[32] =   {"NETWORKING"};
static char g_szSn[32] =            {"12345678"};
static char g_szDevVersion[32] =    {"1.0.0;APP2.0;OTA1.0;APP3.0"};
static char g_szBrand[32] =         {"WIAIR"};
static char g_szCid[64] =           {"107AFF1F147AFF1F187AFF1F"};
static char g_szDefaultMac[20] =    {"00:00:00:12:03:33"};

#define SERVICE_COMMON_RESULT_STRING    "[]"
#define LAN_INTERFACE_NAME              "br-lan"
static char g_szPassWd[32] =        {"admin"};
char g_probe_state = 1;
char g_attack_state = 1;
char g_bw_attack_state = 1;
char g_admin_attack_state = 1;
volatile uint32_t g_probe_num = 0;
volatile uint32_t g_attack_num = 0;
volatile uint32_t g_bw_attack_num = 0;
volatile uint32_t g_admin_attack_num = 0;
volatile unsigned char probe_flag = 0;
volatile unsigned char attack_flag = 0;
volatile unsigned char bw_attack_flag = 0;
volatile unsigned char admin_attack_flag = 0;
unsigned char zero_mac[6] = { 0, };
static char g_szTpsk[64] = {"12345678"};
static pthread_mutex_t ali_mutex;
static long wan_speed_timer = 0;
static long ali_host_timer = 0;
static long apprecord_timer = 0;
static unsigned char upload_wifi_info = 0;
static long wlan24g_upload = 0;
static long wlan5g_upload = 0;
static int noscan, noscan5;

#if 0
struct tpsk {
	char psk[64];
	char mac[18];	
	char duration[14];
};

struct tpsklist {
	int nr;
	struct tpsk list[32];
};
static struct tpsklist tpskl;
#endif

#ifndef FLASH_4_RAM_32
#define ALI_DBG(fmt, args...) do {\
	igd_log("/tmp/ali_log", "%05ld,%05d,%s():"fmt, \
	sys_uptime(), __LINE__, __FUNCTION__, ##args); \
} while(0)
#else
#define ALI_DBG(fmt, args...)
#endif

#define ALI_CONFIG   "aliconf"

//save ali cloud data name
#define ALI_PARAM_PROBENUM_NAME   "param_probe_num"
#define ALI_PARAM_ATTACKNUM_NAME   "param_attack_num"
#define ALI_PARAM_BWATTACKNUM_NAME   "param_bw_attack_num"
#define ALI_PARAM_ADMINATTACKNUM_NAME   "param_admin_attack_num"

#define ALI_ROUTE  "ip -4 route add 224.0.0.0/4 dev br-lan"

#define CONFIG_ALINK_LOCAL_ROUTER_OTA

enum ALI_APP_FLAG {
	AAF_ONLINE = 0,

	AAF_MAX, //must last
};

/*AHF: ali host flag*/
enum ALI_HOST_FLAG {
	AHF_ONLINE = 0,
	AHF_INBLACK,

	AHF_MAX, //must last
};

/*ATF: ali tmp flag*/
enum ALI_TMP_FLAG {
	ATF_MODE = 0,
	ATF_WAN,

	ATF_MAX, //must last
};

struct ali_app_record {
	struct list_head list;
	unsigned int appid;
	unsigned long time;
	unsigned long flag[BITS_TO_LONGS(AAF_MAX)];
};

struct ali_host_record {
	struct list_head list;
	struct list_head app_list;
	unsigned char mac[6];
	unsigned char app_num;
	struct in_addr addr;
	char name[32];
	uint8_t os_type;
	uint16_t vender;
	uint64_t up_bytes;
	uint64_t down_bytes;
	unsigned long flag[BITS_TO_LONGS(AHF_MAX)];
	unsigned long tmp[BITS_TO_LONGS(ATF_MAX)];
	unsigned long ls_up;
	unsigned long ls_down;
};

typedef struct ALINKGW_service_handler
{
	const char *name;
	ALINKGW_SERVICE_execute_cb exec_cb;
} ALINKGW_service_handler_t;

typedef struct ALINKGW_attribute_handler
{
	const char *name;
	ALINKGW_ATTRIBUTE_TYPE_E type;
	ALINKGW_ATTRIBUTE_get_cb get_cb;
	ALINKGW_ATTRIBUTE_set_cb set_cb;
} ALINKGW_attribute_handler_t;

typedef struct ALINKGW_subdevice_attribute_handler
{
	const char *name;
	ALINKGW_ATTRIBUTE_TYPE_E type;
	ALINKGW_ATTRIBUTE_subdevice_get_cb get_cb;
	ALINKGW_ATTRIBUTE_subdevice_set_cb set_cb;
} ALINKGW_subdevice_attribute_handler_t;

#define PROBE_MX  64
short probe_index = 0;
short probe_mac_nr = 0;
unsigned char probe_mac[PROBE_MX][6];
#define MAC_CHECK(i) do{if(!(((i)<= 'F'&& (i)>= 'A') ||  ((i)<= '9' && (i) >= '0') || ((i)<= 'f'&& (i)>= 'a')))return -1;}while(0)

#define ALI_HOST_MX HOST_MX
static int ali_host_num = 0;
static struct list_head ali_host_list = LIST_HEAD_INIT(ali_host_list);

extern int sub_dev_enable(const char *mac);

int checkmac(char*mac)
{
	MAC_CHECK(mac[0]);
	MAC_CHECK(mac[1]);
	if(mac[2] != ':')return -1;
	MAC_CHECK(mac[3]);
	MAC_CHECK(mac[4]);
	if(mac[5] != ':')return -1;
	MAC_CHECK(mac[6]);
	MAC_CHECK(mac[7]);
	if(mac[8] != ':')return -1;
	MAC_CHECK(mac[9]);
	MAC_CHECK(mac[10]);
	if(mac[11] != ':')return -1;
	MAC_CHECK(mac[12]);
	MAC_CHECK(mac[13]);
	if(mac[14] != ':')return -1;
	MAC_CHECK(mac[15]);
	MAC_CHECK(mac[16]);
	if(mac[17] != '\0')return -1;
	return 0;
}

void ali_mac2mac(const char *mac_1, char *mac_2)
{
	while (*mac_1 != '\0') {
		if (*mac_1 == ':' || *mac_1 == '-') {
			;
		} else if (*mac_1 >= 'A' && *mac_1 <= 'Z') {
			*mac_2 = *mac_1 - 'A' + 'a';
			mac_2++;
		} else {
			*mac_2 = *mac_1;
			mac_2++;
		}
		mac_1++;
	}
	*mac_2 = '\0';
}

static void get_terminal_type(enum HOST_OS_TYPE os_type, char *type, int len)
{
	switch(os_type) {
	case OS_PC_WINDOWS:
	case OS_PC_LINUX:
	case OS_PC_MACOS:
		strncpy(type, "PC", len);
		break;
	case OS_PHONE_ANDROID:
	case OS_TABLET_ANDROID:
		strncpy(type, "Android", len);
		break;
	case OS_TABLET_IOS:
		strncpy(type, "iPad", len);
		break;
	case OS_PHONE_IOS:
		strncpy(type, "iPhone", len);
		break;
	default:
		strncpy(type, "unknown", len);					
		break;
	}
	return;
}

#define AUBDEV_ATTRS_MUN 20
subdevice_attr_t *alloc_subdev_attrs(void)
{
	subdevice_attr_t *sat;

	sat = malloc(sizeof(subdevice_attr_t) + sizeof(char *)*AUBDEV_ATTRS_MUN);
	if (!sat) {
		ALI_DBG("malloc fail\n");
		return NULL;
	}
	memset(sat, 0, sizeof(subdevice_attr_t) + sizeof(char *)*AUBDEV_ATTRS_MUN);
	return sat;
}

void ali_subdev_report(unsigned char *mac, char *attr)
{
	subdevice_attr_t *subdev_attrs[2];

	memset(subdev_attrs, 0, sizeof(subdev_attrs));

	subdev_attrs[0] = alloc_subdev_attrs();
	if (!subdev_attrs[0])
		return;
	snprintf(subdev_attrs[0]->mac, sizeof(subdev_attrs[0]->mac),
		"%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(mac));

	subdev_attrs[0]->attr_name[0] = attr;

	if (ALINKGW_report_attr_subdevices(subdev_attrs) < 0)
		ALI_DBG("fail, %s,%s\n", subdev_attrs[0]->mac, attr);
	else
		ALI_DBG("succ, %s,%s\n", subdev_attrs[0]->mac, attr);
	free(subdev_attrs[0]);
}

static void ali_detach_host(struct ali_host_record *ahr)
{
	int ret;
	char str[20];

	snprintf(str, sizeof(str),
		"%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(ahr->mac));

	if (!list_empty(&ahr->app_list))
		ali_subdev_report(ahr->mac, ALINKGW_SUBDEV_ATTR_APPRECORD);

	ret = ALINKGW_detach_sub_device(str);

	ALI_DBG("%d, %s\n", ret, str);
}

static void ali_attach_host(struct ali_host_record *ahr)
{
	int ret;
	char name[32], type[32], vender[32], macstr[32];

	arr_strcpy(name, ahr->name);
	get_terminal_type(ahr->os_type, type, sizeof(type));
	if (!ahr->vender || mu_msg(IGD_VENDER_NAME,
			&ahr->vender, sizeof(ahr->vender), vender, sizeof(vender))) {
		arr_strcpy(vender, "unknown");
	}
	snprintf(macstr, sizeof(macstr),
		"%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(ahr->mac));

	ret = ALINKGW_attach_sub_device(
		name, type, "unknown", vender, macstr, NULL);
	
	ALI_DBG("ret:%d, %s,%s,%s,%s\n", ret, name, type, vender, macstr);
}

static struct ali_host_record *ali_find_host(unsigned char *mac)
{
	struct ali_host_record *host;

	list_for_each_entry(host, &ali_host_list, list) {
		if (!memcmp(host->mac, mac, 6))
			return host;
	}
	return NULL;
}

static struct ali_host_record *ali_add_host(unsigned char *mac)
{
	struct ali_host_record *host;

	host = ali_find_host(mac);
	if (host) {
		list_move(&host->list, &ali_host_list);
		return host;
	}
	host = malloc(sizeof(*host));
	if (!host) {
		ALI_DBG("malloc fail\n");
		return NULL;
	}
	memset(host, 0, sizeof(*host));
	list_add(&host->list, &ali_host_list);
	INIT_LIST_HEAD(&host->app_list);
	memcpy(host->mac, mac, 6);
	ali_host_num++;
	return host;
}

static void ali_del_host(int num)
{
	struct ali_host_record *host, *_host;
	struct ali_app_record *app, *_app;

	if (num == ali_host_num)
		return;
	ALI_DBG("%d,%d\n", num, ali_host_num);

	list_for_each_entry_safe(host, _host, &ali_host_list, list) {
		if (num-- > 0)
			continue;

		ALI_DBG(MAC_FORMART"\n", MAC_SPLIT(host->mac));
		if (igd_test_bit(AHF_ONLINE, host->flag))
			ali_detach_host(host);

		list_for_each_entry_safe(app, _app, &host->app_list, list) {
			list_del(&app->list);
			free(app);
		}

		list_del(&host->list);
		free(host);
		ali_host_num--;
	}
}

void ali_attach_host_info(unsigned char *mac)
{
	subdevice_attr_t *subdev_attrs[2];

	memset(subdev_attrs, 0, sizeof(subdev_attrs));

	subdev_attrs[0] = alloc_subdev_attrs();
	if (!subdev_attrs[0])
		return;
	snprintf(subdev_attrs[0]->mac, sizeof(subdev_attrs[0]->mac),
		"%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(mac));

	subdev_attrs[0]->attr_name[0] = ALINKGW_SUBDEV_ATTR_BAND;
	subdev_attrs[0]->attr_name[1] = ALINKGW_SUBDEV_ATTR_IPADDRESS;
	subdev_attrs[0]->attr_name[2] = ALINKGW_SUBDEV_ATTR_IPV6ADDRESS;
	subdev_attrs[0]->attr_name[3] = ALINKGW_SUBDEV_ATTR_MAXDLSPEED;
	subdev_attrs[0]->attr_name[4] = ALINKGW_SUBDEV_ATTR_MAXULSPEED;
	subdev_attrs[0]->attr_name[5] = ALINKGW_SUBDEV_ATTR_APPINFO;
	subdev_attrs[0]->attr_name[6] = ALINKGW_SUBDEV_ATTR_INTERNETSWITCHSTATE;
	subdev_attrs[0]->attr_name[7] = ALINKGW_SUBDEV_ATTR_INTERNETLIMITEDSWITCHSTATE;

	if (ALINKGW_report_attr_subdevices(subdev_attrs) < 0)
		ALI_DBG("fail, %s\n", subdev_attrs[0]->mac);
	free(subdev_attrs[0]);
}

void ali_host_apprecord(void)
{
	int i = 0, nr = 0;
	struct ali_host_record *host;
	subdevice_attr_t *sa[ALI_HOST_MX + 1];

	memset(sa, 0, sizeof(sa));
	list_for_each_entry(host, &ali_host_list, list) {
		if (!igd_test_bit(AHF_ONLINE, host->flag))
			continue;
		sa[nr] = alloc_subdev_attrs();
		if (!sa[nr])
			break;
		snprintf(sa[nr]->mac, sizeof(sa[nr]->mac),
			"%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(host->mac));
		sa[nr]->attr_name[0] = ALINKGW_SUBDEV_ATTR_APPRECORD;
		ALI_DBG("%s\n", sa[nr]->mac);
		nr++;
		if (nr >= ALI_HOST_MX)
			break;
	}
	if (ALINKGW_report_attr_subdevices(sa) < 0)
		ALI_DBG("fail\n");
	else
		ALI_DBG("succ\n");

	for (i = 0; i < nr; i++) {
		if (sa[i])
			free(sa[i]);
	}
}

static int guest_auto_auth(unsigned char *mac)
{
	int nr, i;
	struct host_info info[HOST_MX];
	struct iwguest iw;
	struct wifi_conf wcfg;

	ALI_DBG("%02X:%02X:%02X:%02X:%02X:%02X\n", MAC_SPLIT(mac));

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &wcfg, sizeof(wcfg))) {
		ALI_DBG("get wireless cfg err\n");
		return -1;
	}
	if (!wcfg.aliset) {
		ALI_DBG("ali app not set guest, do not auto auth\n");
		return 0;
	}
	nr = mu_msg(WIFI_MOD_VAP_HOST_DUMP, NULL, 0, info, sizeof(info));
	if (nr < 0) {
		ALI_DBG("dump vap fail\n");
		return -1;
	}
	for (i = 0; i < nr; i++) {
		if (memcmp(info[i].mac, mac, 6))
			continue;
		if (info[i].is_wifi) {
			ALI_DBG("add vap "MAC_FORMART"\n", MAC_SPLIT(mac));
			return 0;
		}
		break;
	}
	memcpy(iw.mac, mac, 6);

	if (mu_msg(WIFI_MOD_VAP_HOST_ADD, &iw, sizeof(iw), NULL, 0)) {
		ALI_DBG("add vap fail"MAC_FORMART"\n", MAC_SPLIT(mac));
		return -1;
	}
	return 0;
}

static void ali_host2record(
	struct ali_host_record *ahr, struct host_dump_info *host)
{
	ALI_DBG("%s,%d,%d,0x%X -- %s,%d,%d,0x%X\n",
		ahr->name, ahr->os_type, ahr->vender, ahr->flag[0],
		host->name, host->os_type, host->vender, host->flag[0]);

	arr_strcpy(ahr->name, host->name);
	ahr->os_type = host->os_type;
	ahr->vender = host->vender;
	if (igd_test_bit(HIRF_ONLINE, host->flag))
		igd_set_bit(AHF_ONLINE, ahr->flag);
	else
		igd_clear_bit(AHF_ONLINE, ahr->flag);

	ALI_DBG("%s,%d,%d,0x%X\n",
		ahr->name, ahr->os_type, ahr->vender, ahr->flag[0]);
}

static int find_host_in_list(
	unsigned char *mac, struct acl_list *blist)
{
	int i;

	for (i = 0; (i < blist->nr) && (i < IW_LIST_MX); i++) {
		if (blist->list[i].enable == 0)
			continue;
		if (memcmp(mac, blist->list[i].mac, 6))
			continue;
		return 1;
	}
	return 0;
}

static void ali_host_refresh(void)
{
	int nr, i = 0, f1, f2, f3, f4, f5;
	struct host_dump_info host[IGD_HOST_MX];
	struct ali_host_record *ahr;
	struct iwacl wacl;
	char strmac[64];

	nr = mu_msg(IGD_HOST_DUMP, NULL, 0, host, sizeof(host));
	if (nr <= 0) {
		//ALI_DBG("%d, host fail\n", nr);
		return;
	}

	if (mu_msg(WIFI_MOD_GET_ACL, NULL, 0, &wacl, sizeof(wacl))) {
		ALI_DBG("%d, acl fail\n", nr);
		return;
	}

	for (i = 0; i < nr; i++) {
		ahr = ali_add_host(host[i].mac);
		if (!ahr) {
			ALI_DBG("%d, acl fail\n", nr);
			continue;
		}

		if (igd_test_bit(HIRF_ONLINE, host[i].flag)
			&& find_host_in_list(host[i].mac, &wacl.blist)) {
			igd_clear_bit(HIRF_ONLINE, host[i].flag);
			ALI_DBG(MAC_FORMART" clear online\n", MAC_SPLIT(host[i].mac));
		}

		f3 = 0;
		f1 = igd_test_bit(AHF_ONLINE, ahr->flag) ? 1 : 0;
		f2 = igd_test_bit(HIRF_ONLINE, host[i].flag) ? 1 : 0;
		if ((ahr->os_type != host[i].os_type) 
			|| strcmp(ahr->name, host[i].name)
			|| (ahr->os_type != host[i].os_type)) {
			f3 = 1;
		}

		if (ahr->ls_up != host[i].ls.up) {
			ahr->ls_up = host[i].ls.up;
			if (f1)
				ali_subdev_report(host[i].mac, ALINKGW_SUBDEV_ATTR_MAXULSPEED);
		}
		if (ahr->ls_down != host[i].ls.down) {
			ahr->ls_down = host[i].ls.down;
			if (f1)
				ali_subdev_report(host[i].mac, ALINKGW_SUBDEV_ATTR_MAXDLSPEED);
		}
		if (ahr->addr.s_addr != host[i].ip[0].s_addr) {
			ahr->addr.s_addr = host[i].ip[0].s_addr;
			if (f1)
				ali_subdev_report(host[i].mac, ALINKGW_SUBDEV_ATTR_IPADDRESS);
		}
		f4 = igd_test_bit(AHF_INBLACK, ahr->flag) ? 1 : 0;
		f5 = igd_test_bit(HIRF_INBLACK, host[i].flag) ? 1 : 0;
		if (f4 != f5) {
			if (f5)
				igd_set_bit(AHF_INBLACK, ahr->flag);
			else
				igd_clear_bit(AHF_INBLACK, ahr->flag);
			if (f1)
				ali_subdev_report(host[i].mac, ALINKGW_SUBDEV_ATTR_INTERNETSWITCHSTATE);
		}

		if (!f3 && (f1 == f2))
			continue;
		ali_host2record(ahr, &host[i]);

#ifdef ALI_REQUIRE_SWITCH
		if (igd_test_bit(HIRF_VAP, host[i].flag))
			guest_auto_auth(ahr->mac);
#endif

		if (!igd_test_bit(ATF_MODE, ahr->tmp)) {
			snprintf(strmac, sizeof(strmac), MAC_FORMART, MAC_SPLIT(ahr->mac));
			sub_dev_enable(strmac);
			igd_set_bit(ATF_MODE, ahr->tmp);
		}

		if (f1 != f2) {
			if (f2) {
				ahr->down_bytes = host[i].down_bytes;
				ahr->up_bytes = host[i].up_bytes;
				ali_attach_host(ahr);
				sleep(1);
				ali_attach_host_info(ahr->mac);
			} else {
				ali_detach_host(ahr);
				ahr->down_bytes = 0;
				ahr->up_bytes = 0;
			}
			continue;
		} else if (f3 && f1) {
			ali_attach_host(ahr);
		}
	}
	ali_del_host(nr);
}

static struct ali_app_record *ali_app_find(struct ali_host_record *ahr, int appid)
{
	struct ali_app_record *aar = NULL;

	list_for_each_entry(aar, &ahr->app_list, list) {
		if (aar->appid == appid)
			return aar;
	}
	return NULL;
}

static int ali_app_onoff(unsigned char *mac, unsigned int appid, int flag)
{
	int ret;
	struct ali_app_record *aar;
	struct ali_host_record *ahr;

	ALI_DBG(MAC_FORMART", %d, %d\n", MAC_SPLIT(mac), appid, flag);

	ahr = ali_find_host(mac);
	if (!ahr) {
		ALI_DBG("no find, "MAC_FORMART"\n", MAC_SPLIT(mac));
		return -1;
	}

	aar = malloc(sizeof(*aar));
	if (!aar) {
		ALI_DBG("malloc fail\n");
		return -1;
	}
	aar->appid = appid;
	aar->time = (unsigned long)time(NULL);
	if (flag)
		igd_set_bit(AAF_ONLINE, aar->flag);
	else
		igd_clear_bit(AAF_ONLINE, aar->flag);
	list_add(&aar->list, &ahr->app_list);
	ahr->app_num++;
	if (ahr->app_num > 20)
		ali_subdev_report(mac, ALINKGW_SUBDEV_ATTR_APPRECORD);
	return 0;
}

static int json_getval(char*value, int len, const char*key, const char*msg)
{
	char *p;
	char *pstart;
	char *pend;
	
	if ((p = strstr(msg, key)) == NULL)
		return -1;
	if ((p += strlen(key)) == NULL)
		return -1;
	if ((p = strchr(p, '"')) == NULL)
		return -1;
	if ((pstart = p + 1) == NULL)
		return -1;
	if ((pend = strchr(pstart, '"')) == NULL)
		return -1;
	memset(value, 0x0, len);
	if (!__arr_strcpy_end(value, pstart, len, *pend))
		return -1;
	return 0;
}

char *get_mac(const char *interface)
{
	static char szMac[20] = {0};

	int sockFd = -1;
	struct ifreq ifr;

	if ((sockFd = socket(AF_INET, SOCK_RAW, htons(IPPROTO_RAW))) < 0) {
		perror("Could not open socket\n");
		return szMac;
	}

	ifr.ifr_addr.sa_family = AF_INET;
	strcpy(ifr.ifr_name, interface);

	unsigned char mac_data[6];

	if (ioctl(sockFd, SIOCGIFHWADDR, &ifr) == 0)
		memcpy(mac_data, ifr.ifr_hwaddr.sa_data, sizeof(mac_data));
	
	snprintf(szMac, sizeof(szMac), "%02x:%02x:%02x:%02x:%02x:%02x",
	mac_data[0], mac_data[1], mac_data[2], mac_data[3], mac_data[4], mac_data[5]);
	//snprintf(g_szDevName, sizeof(g_szDevName), "WIAIR-ROUTER-I1-%02X%02X", mac_data[4], mac_data[5]);
	if (sockFd > 0)
		close(sockFd);
	return szMac;
}

int rt_srv_authDevice(const char *json_in, char *buff, unsigned int buff_len)
{
	char retstr[32];
	struct sys_account info;
	
	if (buff_len < strlen(SERVICE_COMMON_RESULT_STRING) + 1)
		return ALINKGW_BUFFER_INSUFFICENT;
	if (!json_in)
		return ALINKGW_ERR;
	if (json_getval(retstr, sizeof(retstr), "\"password\":", json_in))
		return ALINKGW_ERR;
	if (mu_msg(SYSTME_MOD_GET_ACCOUNT, NULL, 0, &info, sizeof(info)))
		return ALINKGW_ERR;
	if (strncmp(retstr, info.password, sizeof(info.password)))
		return ALINKGW_ERR;
	strncpy(buff, SERVICE_COMMON_RESULT_STRING, buff_len - 1);
	buff[buff_len - 1] = 0;
	return ALINKGW_OK;
}

static int get_tspped_result(void)
{
	FILE *fp;
	char line[512] = {0};
	int nr = 0, speed = 0;

	fp = fopen(TSPEED_STATUS_FILE, "r");
	if (!fp)
		return 0;

	while (1) {
		memset(line, 0x0, sizeof(line));
		if (!fgets(line, sizeof(line) - 1, fp))
			break;
		if (strncmp(line, TSF_SPEED, strlen(TSF_SPEED)))
			continue;
		speed += atoi(line + strlen(TSF_SPEED) + 1);
		nr++;
	}
	fclose(fp);

	return speed/nr;
}

static unsigned char inspeeding = 1;
int rt_srv_bwCheck(const char *json_in, char *buff, unsigned int buff_len)
{
	char retstr[32], buf[256];
	int ret = ALINKGW_OK;
	int nr = 0;

	if (buff_len < strlen(SERVICE_COMMON_RESULT_STRING) + 1)
		return ALINKGW_BUFFER_INSUFFICENT;
	if (!json_in)
		return ALINKGW_ERR;
	if (json_getval(retstr, sizeof(retstr), "\"enabled\":", json_in))
		return ALINKGW_ERR;
	if (retstr[0] == '1') {
		if (mu_msg(QOS_TEST_SPEED, NULL, 0, NULL, 0))
			return ALINKGW_ERR;
		if (json_getval(retstr, sizeof(retstr), "\"duration\":", json_in))
			return ALINKGW_ERR;
		nr = atoi(retstr);
		ALI_DBG("duration:%d\n", nr);
		inspeeding = 0;
		while(nr > 0) {
			sleep(1);
			ret = ALINKGW_report_attr(ALINKGW_ATTR_DLBWINFO);
			ret = ALINKGW_report_attr(ALINKGW_ATTR_ULBWINFO);
			nr--;
		}
		inspeeding = 1;
	} else {
		snprintf(retstr, sizeof(retstr), "%lu", get_tspped_result() * 8);
		ALI_DBG("%s\n", retstr);
		ALINKGW_report_attr_direct(ALINKGW_ATTR_DLBWINFO, ALINKGW_ATTRIBUTE_simple, retstr);
		ALINKGW_report_attr(ALINKGW_ATTR_ULBWINFO);

		if (mu_msg(QOS_TEST_BREAK, NULL, 0, NULL, 0))
			return ALINKGW_ERR;
	}
	strncpy(buff, SERVICE_COMMON_RESULT_STRING, buff_len - 1);
	buff[buff_len - 1] = 0;
	return ret;
}

int rt_srv_changePassword(const char *json_in, char *buff, unsigned int buff_len)
{
	char retstr[32];
	struct sys_account info;

	if (buff_len < strlen(SERVICE_COMMON_RESULT_STRING) + 1) {
		ALI_DBG("fail\n");
		return ALINKGW_BUFFER_INSUFFICENT;
	}
	if (!json_in) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}
	if (json_getval(retstr, sizeof(retstr), "\"current\":", json_in)) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}
	if (mu_msg(SYSTME_MOD_GET_ACCOUNT, NULL, 0, &info, sizeof(info))) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}

	if (strncmp(retstr, info.password, sizeof(info.password))) {
		ALI_DBG("current fail, %s,%s\n", retstr, info.password);
		return ALINKGW_ERR;
	}

	if (json_getval(retstr, sizeof(retstr), "\"new\":", json_in)){
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}

	arr_strcpy(info.password, retstr);
	if (mu_msg(SYSTME_MOD_SET_ACCOUNT, &info, sizeof(info), NULL, 0)){
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}

	strncpy(buff, SERVICE_COMMON_RESULT_STRING, buff_len - 1);
	buff[buff_len - 1] = 0;
	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_srv_refineWlanChannel(const char *json_in, char *buff, unsigned int buff_len)
{
	int scan;
	int ch[13];
	int i, nr, nr5;
	int bestch = 0;
	struct iwsurvey survey[IW_SUR_MX];
	struct iwsurvey survey5g[IW_SUR_MX];

	ALI_DBG("\n");

	scan = 1;
	nr = mu_msg(WIFI_MOD_GET_SURVEY, &scan, sizeof(int), survey, sizeof(struct iwsurvey) * IW_SUR_MX);
	nr5 = mu_msg(WIFI_MOD_GET_SURVEY_5G, &scan, sizeof(int), survey5g, sizeof(struct iwsurvey) * IW_SUR_MX);
	if (nr <= 0 || nr5 <= 0) {
		sleep(8);
		scan = 0;
		if (nr <= 0)
			nr = mu_msg(WIFI_MOD_GET_SURVEY, &scan, sizeof(int), survey, sizeof(struct iwsurvey) * IW_SUR_MX);
		if (nr5 <= 0)
			nr5 = mu_msg(WIFI_MOD_GET_SURVEY_5G, &scan, sizeof(int), survey5g, sizeof(struct iwsurvey) * IW_SUR_MX);
	}

	ALI_DBG("24g nr:%d\n", nr);
	if (nr <= 0)
		goto SURVEY5;
	memset(ch, 0x0, sizeof(ch));
	for (i = 0; i < nr; i++) {
		ch[survey[i].ch - 1]++;
	}
	nr = ch[0];
	bestch = 1;
	for (i = 0; i < 13; i++) {
		if (ch[i] < nr) {
			bestch = i + 1;
			nr = ch[i];
		}
	}
	if (bestch <= 0 || bestch > 13)
		return ALINKGW_ERR;
	noscan = 1;
	ALI_DBG("24g bestch %d\n", bestch);
	if (!mu_msg(WIFI_MOD_SET_CHANNEL, &bestch, sizeof(bestch), NULL, 0))
		ALINKGW_report_attr(ALINKGW_ATTR_WLAN_CHANNEL_CONDITION_24G);

SURVEY5:
	ALI_DBG("5g nr:%d\n", nr5);
	if (nr5 <= 0)
		return ALINKGW_OK;
	memset(ch, 0x0, sizeof(ch));
	for (i = 0; i < nr5; i++) {
		switch(survey5g[i].ch) {
		case 149:
			ch[0]++;
			break;
		case 153:
			ch[1]++;
			break;
		case 157:
			ch[2]++;
			break;
		case 161:
			ch[3]++;
			break;
		default:
			break;
		}
	}
	nr5 = ch[0];
	bestch = 149;
	for (i = 0; i < 4; i++) {
		if (ch[i] < nr5) {
			switch(i) {
			case 0:
				bestch = 149;
				break;
			case 1:
				bestch = 153;
				break;
			case 2:
				bestch = 157;
				break;
			case 3:
				bestch = 161;
				break;
			default:
				break;
			}
		}
	}

	ALI_DBG("5g bestch %d\n", bestch);
	if (mu_msg(WIFI_MOD_SET_CHANNEL_5G, &bestch, sizeof(bestch), NULL, 0))
		return ALINKGW_ERR;
	noscan5 = 1;
	ALINKGW_report_attr(ALINKGW_ATTR_WLAN_CHANNEL_CONDITION_5G);
	return ALINKGW_OK;
}


int rt_srv_reboot(const char *json_in, char *buff, unsigned int buff_len)
{
	ALI_DBG("\n");

	if (mu_msg(SYSTEM_MOD_SYS_REBOOT, NULL, 0, NULL, 0))
		system("reboot &");

	return ALINKGW_OK;
}

int rt_srv_factoryReset(const char *json_in, char *buff, unsigned int buff_len)
{
	int ret = ALINKGW_OK;
	int flag = SYSFLAG_RECOVER;

	if (mu_msg(SYSTEM_MOD_SYS_DEFAULT, &flag, sizeof(int), NULL, 0))
		ret = ALINKGW_ERR;

	ALI_DBG("%d\n", ret);
	return ret;
}

/*服务列表*/
ALINKGW_service_handler_t alinkgw_server_handlers[256] =
{
	/*finished*/
	{ALINKGW_SERVICE_AUTHDEVICE, rt_srv_authDevice},
	/*finished*/
	{ALINKGW_SERVICE_BWCHECK, rt_srv_bwCheck},
	/*finished*/
	{ALINKGW_SERVICE_CHANGEPASSWORD, rt_srv_changePassword},
	/*finished*/
	{ALINKGW_SERVICE_REFINEWLANCHANNEL, rt_srv_refineWlanChannel},
	/*finished*/
	{ALINKGW_SERVICE_REBOOT, rt_srv_reboot},
	/*finished*/
	{ALINKGW_SERVICE_FACTORYRESET, rt_srv_factoryReset},

	{NULL, NULL}
};

#define PROBE_SSID_MAX_LEN 32
#define MAC_STR_LEN         18

/*无线频率设置类型定义*/
typedef enum
{
    BAND_TYPE_AUTO = 0,
    BAND_TYPE_5_8_G,
    BAND_TYPE_2_4_G,
    BAND_TYPE_ALL
}band_type_t;

/*************************************************************************
Description:     struct sta_info: station PROBE帧信息定义
Parameters：rssi: 信号强度
                      band_type: 无线频率设置类型
                      ssid: SSID名称
*************************************************************************/
struct sta_info
{
    int rssi;
    band_type_t band_type;
    char ssid[PROBE_SSID_MAX_LEN];
};

/************************************************************************/
/*LAN DEV访问路由器方式类型定义*/
typedef enum
{
    ACCESS_TYPE_WIRE = 1,
    ACCESS_TYPE_WIRELESS
}access_type_t;

/************************************************************************/
/*LAN DEV在线状态消息类型定义*/
typedef enum
{
    MSG_TYPE_ONLINE = 1,
    MSG_TYPE_OFFLINE,
    MSG_TYPE_PROBE,
    MSG_TYPE_NEW_MAC,
    MSG_TYPE_REMOVE_MAC
}network_status_msg_t;

/*************************************************************************
Description:     alink_dev_msg_t :LAN DEV在线状态通知应用层处理消息定义
Parameters：msg_type:设备在线状态消息类型定义
                      client_mac: string类型,接入设备MAC信息
                      u:type为1时u.port有效 type为2时u.info有效
*************************************************************************/
typedef struct dev_msg
{
    network_status_msg_t msg_type;           // 1:online 2:offline 3:probe 4:mac increase 5:mac decrease
    access_type_t access_type;      // 1:wire;   2:wireless
    char client_mac[MAC_STR_LEN];

    union
    {
        int port;
        struct sta_info info;
    }u;

#define msg_rssi u.info.rssi
#define msg_band_type u.info.band_type
#define msg_ssid u.info.ssid
}alink_dev_msg_t;

/************************************************************************/
/*netlink 消息类型定义*/
typedef enum
{
    /* wireless tpsk & ie 处理消息 */
    NL_MSG_TPSK_UPGRADE = 10,                         //物连设备TPSK关联成功
    NL_MSG_IE_REPORT,                                //ali私有IE上报

    /* dev online status 处理消息 */
    NL_MSG_DEV_ONLINE_STATUS                //设备在线状态消息
}alink_netlink_msg_type_t;

/*************************************************************************
Description:  alink_netlink_msg_t定义
Parameters: msg_type    netlink_msg_t类型netlink处理消息类型
                   msg_len 整个netlink_msg_t实际消息长度(sizeof(alink_netlink_msg_t) + length(body))
                   void消息为alink_dev_msg_t, alink_ie_msg_t, alink_tpsk_msg_t
                   reserve 保留字段
*************************************************************************/
typedef struct
{
    alink_netlink_msg_type_t msg_type;
    int msg_len;
    void *body;
}alink_netlink_msg_t;

typedef int (*nl_msg_handler_func) (void *);

extern int ven_alink_sniffer_sta_probe_enable(void);
extern int ven_alink_sniffer_sta_probe_disable(void);
extern int nl_msg_handler_register(const int type, nl_msg_handler_func handler);
extern int nl_msg_handler_unregister(const int type);

void save_probe_mac(unsigned char *mac)
{
	struct host_info *host;
	char buf[32];
	int nr, ret;

	loop_for_each(0, probe_mac_nr) {
		if (memcmp(mac, probe_mac[i], 6))
			continue;
		return;
	} loop_for_each_end();
	host = dump_host_alive(&nr);
	if (!host)
		goto next;
	/*  ignore already connected host */
	loop_for_each(0, nr) {
		if (!host[i].is_wifi)
			continue;
		if (memcmp(host[i].mac, mac, 6))
			continue;
		free(host);
		return;
	} loop_for_each_end();
	free(host);

next:
	if (probe_mac_nr) {
		probe_index++;
		if (probe_index >= PROBE_MX) 
			probe_index = 0;
	}

	if (probe_mac_nr < PROBE_MX)
		probe_mac_nr++;

	g_probe_num++;	
	memcpy(probe_mac[probe_index], mac, 6);
	sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	ret = ALINKGW_report_attr_direct(ALINKGW_ATTR_PROBE_INFO,
			ALINKGW_ATTRIBUTE_simple, buf);
	if (ret < 0)
		ALI_DBG("%s, %s, %d, %d\n", ALINKGW_ATTR_PROBE_INFO, buf, g_probe_num, probe_mac_nr);

	static long timer = 0;
	if ((timer == 0) || (timer + 10) < sys_uptime()) {
		sprintf(buf, "%u", g_probe_num);
		ALINKGW_cloud_save(ALI_PARAM_PROBENUM_NAME, buf, strlen(buf) + 1);
		timer = sys_uptime();
		//ALI_DBG("save %s\n", ALI_PARAM_PROBENUM_NAME);
	}
	return ;
}

int nc_dev_networking_status_msg_handler(void *msg)
{
	int ret = 0;
	alink_netlink_msg_t *alink_nc_msg = (alink_netlink_msg_t *)msg;
	alink_dev_msg_t *dev_msg = NULL;

	if(!msg) {
		ALI_DBG("msg is NULL !\n");
		goto END;
	}

	if (NL_MSG_DEV_ONLINE_STATUS == alink_nc_msg->msg_type) {
		dev_msg = (alink_dev_msg_t *)(&alink_nc_msg->body);
		if(!dev_msg) {
			ALI_DBG("dev_msg is null !\n");
			goto END;
		}

		switch(dev_msg->msg_type) {
		case MSG_TYPE_PROBE:
			if (dev_msg->access_type == ACCESS_TYPE_WIRELESS) {
				unsigned char mac[ETH_ALEN];
				parse_mac(dev_msg->client_mac, mac);
				save_probe_mac(mac);
			}
			break;
		default:
			break;
		}
	}

END:
	return ret;
}

int rt_attr_get_admin_attack_state(char *buff, unsigned int buff_len)
{
	snprintf(buff, buff_len, "%d", g_admin_attack_state);
	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_set_admin_attack_state(const char *json_in)
{
	char cmd[128];

	g_admin_attack_state = !!atoi(json_in);
	ALINKGW_report_attr_direct(ALINKGW_ATTR_ADMIN_ATTACK_SWITCH_STATE, ALINKGW_ATTRIBUTE_simple, json_in);
	sprintf(cmd, "%s.attack.admin=%d", ALI_CONFIG, g_admin_attack_state);
	pthread_mutex_lock(&ali_mutex); 
	uuci_set(cmd);
	pthread_mutex_unlock(&ali_mutex); 

	ALI_DBG("%s\n", json_in);
	return ALINKGW_OK;
}

int rt_attr_get_admin_attack_num(char *buff, unsigned int buff_len)
{
	if (!admin_attack_flag) {
		ALI_DBG("flag\n");
		return ALINKGW_ERR;
	}

	snprintf(buff, buff_len, "%u", g_admin_attack_num);
	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_admin_attack_info(char *buff, unsigned int buff_len)
{
	ALI_DBG("\n");
	return ALINKGW_ERR;
}

int rt_attr_get_attack_state(char *buff, unsigned int buff_len)
{
	snprintf(buff, buff_len, "%d", g_bw_attack_state);
	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_set_attack_state(const char *json_in)
{
	char cmd[128];

	g_bw_attack_state = !!atoi(json_in);
	ALINKGW_report_attr_direct(ALINKGW_ATTR_ATTACK_SWITCH_STATE, ALINKGW_ATTRIBUTE_simple, json_in);
	sprintf(cmd, "%s.attack.bw=%d", ALI_CONFIG, g_bw_attack_state);
	pthread_mutex_lock(&ali_mutex); 
	uuci_set(cmd);
	pthread_mutex_unlock(&ali_mutex); 

	ALI_DBG("%s\n", json_in);
	return ALINKGW_OK;
}

int rt_attr_get_attack_num(char *buff, unsigned int buff_len)
{
	if (!bw_attack_flag) {
		ALI_DBG("flag\n");
		return ALINKGW_ERR;
	}
	snprintf(buff, buff_len, "%u", g_bw_attack_num);
	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_attack_info(char *buff, unsigned int buff_len)
{
	ALI_DBG("\n");
	return ALINKGW_ERR;
}

int rt_attr_get_probe_info(char *buff, unsigned int buff_len)
{
	unsigned char *mac;

	if (!probe_mac_nr)
		return ALINKGW_ERR;

	mac = probe_mac[probe_index];
	snprintf(buff, buff_len, "%02X:%02X:%02X:%02X:%02X:%02X",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_access_attack_info(char *buff, unsigned int buff_len)
{
	return ALINKGW_ERR;
}

int rt_attr_set_probe_switch_state(const char *json_in)
{
	int old;
	char cmd[128];

	old = g_probe_state;
	g_probe_state = !!atoi(json_in);
	ALINKGW_report_attr_direct(ALINKGW_ATTR_PROBE_SWITCH_STATE, ALINKGW_ATTRIBUTE_simple, json_in);
	sprintf(cmd, "%s.probe.on=%d", ALI_CONFIG, g_probe_state);
	pthread_mutex_lock(&ali_mutex); 
	uuci_set(cmd);
	pthread_mutex_unlock(&ali_mutex); 
	if (old == g_probe_state)
		goto out;
	if (g_probe_state) {
		ven_alink_sniffer_sta_probe_enable();
		nl_msg_handler_register(NL_MSG_DEV_ONLINE_STATUS, nc_dev_networking_status_msg_handler);
	} else {
		//注销probe上报处理函数
		nl_msg_handler_unregister(NL_MSG_DEV_ONLINE_STATUS);
		ven_alink_sniffer_sta_probe_disable();
	}

out:
	return ALINKGW_OK;
}

int rt_attr_get_probe_switch_state(char *buff, unsigned int buff_len)
{
	snprintf(buff, buff_len, "%u", g_probe_state);
	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_probe_number(char *buff, unsigned int buff_len)
{
	if (!probe_flag) {
		ALI_DBG("flag\n");
		return ALINKGW_ERR;
	}
	snprintf(buff, buff_len, "%u", g_probe_num);
	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_access_attack_state(char *buff, unsigned int buff_len)
{
	snprintf(buff, buff_len, "%d", g_attack_state);
	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_set_access_attack_state(const char *json_in)
{
	char cmd[128];

	g_attack_state = !!atoi(json_in);
	ALINKGW_report_attr_direct(ALINKGW_ATTR_ACCESS_ATTACK_STATE, ALINKGW_ATTRIBUTE_simple, json_in);
	sprintf(cmd, "%s.attack.on=%d", ALI_CONFIG, g_attack_state);
	pthread_mutex_lock(&ali_mutex); 
	uuci_set(cmd);
	pthread_mutex_unlock(&ali_mutex); 

	ALI_DBG("%s\n", json_in);
	return ALINKGW_OK;
}

int rt_attr_get_access_attack_num(char *buff, unsigned int buff_len)
{
	if (!attack_flag) {
		ALI_DBG("flag\n");
		return ALINKGW_ERR;
	}

	snprintf(buff, buff_len, "%u", g_attack_num);
	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_wlanSwitchState(char *buff, unsigned int buff_len)
{
	struct wifi_conf config, config_5g;

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
		return ALINKGW_ERR;

	memset(&config_5g, 0, sizeof(config_5g));
	mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config_5g, sizeof(config_5g));

	if (config.enable || config_5g.enable)
		snprintf(buff, buff_len, "%d", 1);
	else
		snprintf(buff, buff_len, "%d", 0);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_set_wlanSwitchState(const char *json_in)
{
	struct wifi_conf config;

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config))) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}
	config.enable = atoi(json_in);
	if (mu_msg(WIFI_MOD_SET_TIME, &config, sizeof(config), NULL, 0)){
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}

	if (!mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config))) {
		config.enable = atoi(json_in);
		config.ifindex = 1;
		if (mu_msg(WIFI_MOD_SET_TIME, &config, sizeof(config), NULL, 0)) {
			ALI_DBG("fail\n");
		}
	}

	ALINKGW_report_attr_direct(ALINKGW_ATTR_WLAN_SWITCH_STATE, ALINKGW_ATTRIBUTE_simple, json_in);
	upload_wifi_info = 1;

	ALI_DBG("%s\n", json_in);
	return ALINKGW_OK;
}

int rt_attr_get_wanDlSpeed(char *buff, unsigned int buff_len)
{
	struct if_statistics statis;

	if (get_if_statistics(1, &statis) != 0) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}
	snprintf(buff, buff_len, "%lu", statis.in.all.speed);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_wanUlSpeed(char *buff, unsigned int buff_len)
{
	struct if_statistics statis;

	if (get_if_statistics(1, &statis) != 0) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}
	snprintf(buff, buff_len, "%lu", statis.out.all.speed);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_wanDlbytes(char *buff, unsigned int buff_len)
{
	struct if_statistics statis;

	if (get_if_statistics(1, &statis) != 0) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}

	snprintf(buff, buff_len, "%llu", statis.in.all.bytes);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_wanUlbytes(char *buff, unsigned int buff_len)
{
	struct if_statistics statis;

	if (get_if_statistics(1, &statis) != 0) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}
	snprintf(buff, buff_len, "%llu", statis.out.all.bytes);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_dlBwinfo(char *buff, unsigned int buff_len)
{
	struct if_statistics statis;

	if (inspeeding)
		return ALINKGW_ERR;

	if (get_if_statistics(1, &statis) != 0) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}

	/*bit/s*/
	snprintf(buff, buff_len, "%lu", statis.in.all.speed * 8);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_ulBwInfo(char *buff, unsigned int buff_len)
{
	struct if_statistics statis;

	if (inspeeding)
		return ALINKGW_ERR;

	if (get_if_statistics(1, &statis) != 0) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}

	/*bit/s*/
	snprintf(buff, buff_len, "%lu", statis.out.all.speed * 8);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_wlanPaMode(char *buff, unsigned int buff_len)
{
	struct wifi_conf config;

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
		return ALINKGW_ERR;
	if (config.txrate == 30)
		snprintf(buff, buff_len, "0");
	else if (config.txrate == 60)
		snprintf(buff, buff_len, "1");
	else if (config.txrate == 100)
		snprintf(buff, buff_len, "2");
	else
		return ALINKGW_ERR;

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_set_wlanPaMode(const char *json_in)
{
	int txrate = 0;

	ALI_DBG("%s\n", json_in);

	/*0:低辐射模式　1:标准模式2:强力模式*/
	if (json_in[0] == '0')
		txrate = 30;
	else if (json_in[0] == '1')
		txrate = 60;
	else if (json_in[0] == '2')
		txrate = 100;
	else
		return ALINKGW_ERR;
	if (mu_msg(WIFI_MOD_SET_TXRATE, &txrate, sizeof(txrate), NULL, 0))
		return ALINKGW_ERR;
	mu_msg(WIFI_MOD_SET_TXRATE_5G, &txrate, sizeof(txrate), NULL, 0);
	ALINKGW_report_attr_direct(ALINKGW_ATTR_WLAN_PA_MODE, ALINKGW_ATTRIBUTE_simple, json_in);

	ALI_DBG("%s\n", json_in);
	return ALINKGW_OK;
}

int rt_attr_get_wlanSetting24g(char *buff, unsigned int buff_len)
{
	char *mac;
	char retstr[32];
	struct wifi_conf config;
	struct json_object *obj, *response;

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
		return ALINKGW_ERR;
	response = json_object_new_object();
	snprintf(retstr, sizeof(retstr), "%d", config.enable);
	obj = json_object_new_string(retstr);
	json_object_object_add(response, "enabled", obj);
	mac = get_mac(config.apname);
	obj = json_object_new_string(mac);
	json_object_object_add(response, "bssid", obj);
	obj = json_object_new_string(config.ssid);
	json_object_object_add(response, "ssid", obj);
	if (config.hidssid)
		obj = json_object_new_string("0");
	else
		obj = json_object_new_string("1");
	json_object_object_add(response, "ssidBroadcast", obj);
	switch (config.mode) {
	case MODE_802_11_BG:
		obj = json_object_new_string("bg");
		break;
	case MODE_802_11_B:
		obj = json_object_new_string("b");
		break;
	case MODE_802_11_G:
		obj = json_object_new_string("g");
		break;
	case MODE_802_11_N:
		obj = json_object_new_string("n");
		break;
	case MODE_802_11_GN:
		obj = json_object_new_string("gn");
		break;
	case MODE_802_11_BGN:
		obj = json_object_new_string("bgn");
		break;
	default:
		json_object_put(response);
		return ALINKGW_ERR;
	}
	json_object_object_add(response, "mode", obj);
	strncpy(buff, json_object_to_json_string(response), buff_len - 1);
	buff[buff_len - 1] = 0;
	json_object_put(response);

	return ALINKGW_OK;
}

int rt_attr_set_wlanSetting24g(const char *json_in)
{
	int len;
	char value[32];
	struct wifi_conf config;

	ALI_DBG("%s\n", json_in);

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
		return ALINKGW_ERR;
	if (json_getval(value, sizeof(value), "\"enabled\":", json_in))
		return ALINKGW_ERR;
	config.enable = atoi(value);
	if (json_getval(config.ssid, sizeof(value), "\"ssid\":", json_in))
		return ALINKGW_ERR;
	if (json_getval(value, sizeof(value), "\"ssidBroadcast\":", json_in))
		return ALINKGW_ERR;
	if (atoi(value) == 0)
		config.hidssid = 1;
	else
		config.hidssid = 0;
	if (json_getval(value, sizeof(value), "\"mode\":", json_in))
		return ALINKGW_ERR;
	len = strlen(value);
	if (!strcmp(value, "bgn") && (len == 3))
		config.mode = MODE_802_11_BGN;
	else if (!strcmp(value, "bg") && (len == 2))
		config.mode = MODE_802_11_BG;
	else if (!strcmp(value, "gn") && (len == 2))
		config.mode = MODE_802_11_GN;
	else if (value[0] == 'b' && (len == 1))
		config.mode = MODE_802_11_B;
	else if (value[0] == 'g' && (len == 1))
		config.mode = MODE_802_11_G;
	else if (value[0] == 'n' && (len == 1))
		config.mode = MODE_802_11_N;
	else
		return ALINKGW_ERR;
	if (mu_msg(WIFI_MOD_SET_AP, &config, sizeof(config), NULL, 0))
		return ALINKGW_ERR;
	if (mu_msg(WIFI_MOD_SET_TIME, &config, sizeof(config), NULL, 0))
		return ALINKGW_ERR;
	ALINKGW_report_attr_direct(ALINKGW_ATTR_WLAN_SETTING_24G, ALINKGW_ATTRIBUTE_complex, json_in);

	ALI_DBG("%s\n", json_in);
	return ALINKGW_OK;
}

int rt_attr_get_wlanSetting5g(char *buff, unsigned int buff_len)
{
	char *mac;
	char retstr[32];
	struct wifi_conf config;
	struct json_object *obj, *response;

	if (mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config)))
		return ALINKGW_ERR;
	response = json_object_new_object();
	snprintf(retstr, sizeof(retstr), "%d", config.enable);
	obj = json_object_new_string(retstr);
	json_object_object_add(response, "enabled", obj);
	mac = get_mac(config.apname);
	obj = json_object_new_string(mac);
	json_object_object_add(response, "bssid", obj);
	obj = json_object_new_string(config.ssid);
	json_object_object_add(response, "ssid", obj);
	if (config.hidssid)
		obj = json_object_new_string("0");
	else
		obj = json_object_new_string("1");
	json_object_object_add(response, "ssidBroadcast", obj);
	switch (config.mode) {
	case MODE_802_11_ANAC:
		obj = json_object_new_string("acan");
		break;
	case MODE_802_11_AN:
		obj = json_object_new_string("an");
		break;
	case MODE_802_11_AANAC:
		obj = json_object_new_string("ac");
		break;
	case MODE_802_11_A:
		obj = json_object_new_string("a");
		break;
	case MODE_802_11_5N:
		obj = json_object_new_string("n");
		break;
	default:
		json_object_put(response);
		return ALINKGW_ERR;
	}
	json_object_object_add(response, "mode", obj);
	strncpy(buff, json_object_to_json_string(response), buff_len - 1);
	buff[buff_len - 1] = 0;
	json_object_put(response);

	return ALINKGW_OK;
}

int rt_attr_set_wlanSetting5g(const char *json_in)
{
	int len;
	char value[32];
	struct wifi_conf config;

	ALI_DBG("%s\n", json_in);

	if (mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config)))
		return ALINKGW_ERR;
	if (json_getval(value, sizeof(value), "\"enabled\":", json_in))
		return ALINKGW_ERR;
	config.enable = atoi(value);
	if (json_getval(config.ssid, sizeof(value), "\"ssid\":", json_in))
		return ALINKGW_ERR;
	if (json_getval(value, sizeof(value), "\"ssidBroadcast\":", json_in))
		return ALINKGW_ERR;
	if (atoi(value) == 0)
		config.hidssid = 1;
	else
		config.hidssid = 0;
	if (json_getval(value, sizeof(value), "\"mode\":", json_in))
		return ALINKGW_ERR;
	len = strlen(value);
	if (!strcmp(value, "acan") && (len == 4))
		config.mode = MODE_802_11_ANAC;
	else if (!strcmp(value, "an") && (len == 2))
		config.mode = MODE_802_11_AN;
	else if (!strcmp(value, "ac") && (len == 2))
		config.mode = MODE_802_11_AANAC;
	else if (value[0] == 'a' && (len == 1))
		config.mode = MODE_802_11_A;
	else if (value[0] == 'n' && (len == 1))
		config.mode = MODE_802_11_5N;
	else
		return ALINKGW_ERR;
	if (mu_msg(WIFI_MOD_SET_AP, &config, sizeof(config), NULL, 0))
		return ALINKGW_ERR;
	if (mu_msg(WIFI_MOD_SET_TIME, &config, sizeof(config), NULL, 0))
		return ALINKGW_ERR;
	ALINKGW_report_attr_direct(ALINKGW_ATTR_WLAN_SETTING_5G, ALINKGW_ATTRIBUTE_complex, json_in);

	ALI_DBG("%s\n", json_in);
	return ALINKGW_OK;
}

int rt_attr_get_wlanSecurity24g(char *buff, unsigned int buff_len)
{
	char *p = NULL;
	struct wifi_conf config;
	struct json_object *obj, *response;

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
		return ALINKGW_ERR;
	response = json_object_new_object();
	if (!strncmp(config.encryption, "none", 4)) {
		obj = json_object_new_string("0");
		json_object_object_add(response, "enabled", obj);
		obj = json_object_new_string("");
		json_object_object_add(response, "type", obj);
		obj = json_object_new_string("");
		json_object_object_add(response, "encryption", obj);
		obj = json_object_new_string("");
		json_object_object_add(response, "passphrase", obj);
	} else {
		obj = json_object_new_string("1");
		json_object_object_add(response, "enabled", obj);
		if (!strncmp(config.encryption, "psk2+", 5))
			obj = json_object_new_string("WPA2");
		else if (!strncmp(config.encryption, "psk+", 4))
			obj = json_object_new_string("WPA");
		else if (!strncmp(config.encryption, "psk-mixed+", 9))
			obj = json_object_new_string("WPA+2");
		else
			goto out;
		json_object_object_add(response, "type", obj);
		p = strchr(config.encryption, '+');
		if (!p)
			goto out;
		p++;
		if ((strlen(p) == 9) && !strncmp(p, "tkip+ccmp", 9))
			obj = json_object_new_string("AES+TKIP");
		else if ((strlen(p) == 4) && !strncmp(p, "tkip", 4))
			obj = json_object_new_string("TKIP");
		else if ((strlen(p) == 4) && !strncmp(p, "ccmp", 4))
			obj = json_object_new_string("AES");
		else
			goto out;
		json_object_object_add(response, "encryption", obj);
		obj = json_object_new_string(config.key);
		json_object_object_add(response, "passphrase", obj);
	}
	strncpy(buff, json_object_to_json_string(response), buff_len - 1);
	buff[buff_len - 1] = 0;

out:
	json_object_put(response);
	return ALINKGW_OK;
}

int rt_attr_set_wlanSecurity24g(const char *json_in)
{
	char retstr[32];
	char *p = NULL;
	struct wifi_conf config;

	ALI_DBG("%s\n", json_in);

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
		return ALINKGW_ERR;
	if (json_getval(retstr, sizeof(retstr), "\"enabled\":", json_in))
		return ALINKGW_ERR;
	if (retstr[0] == '0')
		arr_strcpy(config.encryption, "none");
	else {
		if (json_getval(retstr, sizeof(retstr), "\"type\":", json_in))
			return ALINKGW_ERR;
		p = config.encryption;
		memset(config.encryption, 0x0, sizeof(config.encryption));
		if ((strlen(retstr) == 3) && !strncmp(retstr, "WPA", 3)) {
			arr_strcpy(config.encryption, "psk+");
			p += 4;
		} else if ((strlen(retstr) == 4) && !strncmp(retstr, "WPA2", 4)) {
			arr_strcpy(config.encryption, "psk2+");
			p += 5;
		} else if ((strlen(retstr) == 5) && !strncmp(retstr, "WPA+2", 5)) {
			arr_strcpy(config.encryption, "psk-mixed+");
			p += 10;
		} else
			return ALINKGW_ERR;
		if (json_getval(retstr, sizeof(retstr), "\"encryption\":", json_in))
			return ALINKGW_ERR;
		if ((strlen(retstr) == 3) && !strncmp(retstr, "AES", 3))
			strncpy(p, "ccmp", 4);
		else if ((strlen(retstr) == 4) && !strncmp(retstr, "TKIP", 4))
			strncpy(p, "tkip", 4);
		else if ((strlen(retstr) == 8) && !strncmp(retstr, "AES+TKIP", 8))
			strncpy(p, "tkip+ccmp", 9);
		else 
			return ALINKGW_ERR;
		if (json_getval(config.key, sizeof(retstr), "\"passphrase\":", json_in))
			return ALINKGW_ERR;
	}
	if (mu_msg(WIFI_MOD_SET_AP, &config,  sizeof(config), NULL, 0))
		return ALINKGW_ERR;
	ALINKGW_report_attr_direct(ALINKGW_ATTR_WLAN_SECURITY_24G, ALINKGW_ATTRIBUTE_complex, json_in);

	ALI_DBG("%s\n", json_in);
    return ALINKGW_OK;
}

int rt_attr_get_wlanSecurity5g(char *buff, unsigned int buff_len)
{
	char *p = NULL;
	struct wifi_conf config;
	struct json_object *obj, *response;

	if (mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config)))
		return ALINKGW_ERR;
	response = json_object_new_object();
	if (!strncmp(config.encryption, "none", 4)) {
		obj = json_object_new_string("0");
		json_object_object_add(response, "enabled", obj);
		obj = json_object_new_string("");
		json_object_object_add(response, "type", obj);
		obj = json_object_new_string("");
		json_object_object_add(response, "encryption", obj);
		obj = json_object_new_string("");
		json_object_object_add(response, "passphrase", obj);
	} else {
		obj = json_object_new_string("1");
		json_object_object_add(response, "enabled", obj);
		if (!strncmp(config.encryption, "psk2+", 5))
			obj = json_object_new_string("WPA2");
		else if (!strncmp(config.encryption, "psk+", 4))
			obj = json_object_new_string("WPA");
		else if (!strncmp(config.encryption, "psk-mixed+", 9))
			obj = json_object_new_string("WPA+2");
		else
			goto out;
		json_object_object_add(response, "type", obj);
		p = strchr(config.encryption, '+');
		if (!p)
			goto out;
		p++;
		if ((strlen(p) == 9) && !strncmp(p, "tkip+ccmp", 9))
			obj = json_object_new_string("AES+TKIP");
		else if ((strlen(p) == 4) && !strncmp(p, "tkip", 4))
			obj = json_object_new_string("TKIP");
		else if ((strlen(p) == 4) && !strncmp(p, "ccmp", 4))
			obj = json_object_new_string("AES");
		else
			goto out;
		json_object_object_add(response, "encryption", obj);
		obj = json_object_new_string(config.key);
		json_object_object_add(response, "passphrase", obj);
	}
	strncpy(buff, json_object_to_json_string(response), buff_len - 1);
	buff[buff_len - 1] = 0;

out:
	json_object_put(response);
	return ALINKGW_OK;
}

int rt_attr_set_wlanSecurity5g(const char *json_in)
{
	char retstr[32];
	char *p = NULL;
	struct wifi_conf config;

	ALI_DBG("%s\n", json_in);

	if (mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config)))
		return ALINKGW_ERR;
	if (json_getval(retstr, sizeof(retstr), "\"enabled\":", json_in))
		return ALINKGW_ERR;
	if (retstr[0] == '0')
		arr_strcpy(config.encryption, "none");
	else {
		if (json_getval(retstr, sizeof(retstr), "\"type\":", json_in))
			return ALINKGW_ERR;
		p = config.encryption;
		memset(config.encryption, 0x0, sizeof(config.encryption));
		if ((strlen(retstr) == 3) && !strncmp(retstr, "WPA", 3)) {
			arr_strcpy(config.encryption, "psk+");
			p += 4;
		} else if ((strlen(retstr) == 4) && !strncmp(retstr, "WPA2", 4)) {
			arr_strcpy(config.encryption, "psk2+");
			p += 5;
		} else if ((strlen(retstr) == 5) && !strncmp(retstr, "WPA+2", 5)) {
			arr_strcpy(config.encryption, "psk-mixed+");
			p += 10;
		} else
			return ALINKGW_ERR;
		if (json_getval(retstr, sizeof(retstr), "\"encryption\":", json_in))
			return ALINKGW_ERR;
		if ((strlen(retstr) == 3) && !strncmp(retstr, "AES", 3))
			strncpy(p, "ccmp", 4);
		else if ((strlen(retstr) == 4) && !strncmp(retstr, "TKIP", 4))
			strncpy(p, "tkip", 4);
		else if ((strlen(retstr) == 8) && !strncmp(retstr, "AES+TKIP", 8))
			strncpy(p, "tkip+ccmp", 9);
		else 
			return ALINKGW_ERR;
		if (json_getval(config.key, sizeof(retstr), "\"passphrase\":", json_in))
			return ALINKGW_ERR;
	}
	if (mu_msg(WIFI_MOD_SET_AP, &config,  sizeof(config), NULL, 0))
		return ALINKGW_ERR;
	ALINKGW_report_attr_direct(ALINKGW_ATTR_WLAN_SECURITY_5G, ALINKGW_ATTRIBUTE_complex, json_in);

	ALI_DBG("%s\n", json_in);
    return ALINKGW_OK;
}

int rt_attr_get_wlanChannelCondition24g(char *buff, unsigned int buff_len)
{
	int scan = 0;
	int nr, i;
	char retstr[128];
	int channel, sch = 0;
	struct wifi_conf wconfig;
	struct iwsurvey survey[IW_SUR_MX];
	struct json_object* obj, *response;

	ALI_DBG("%d\n", buff_len);

	if (!noscan)
		scan = 1;
	noscan = 0;
	if (mu_msg(WIFI_MOD_GET_CHANNEL, NULL, 0, &channel, sizeof(channel)))
		return ALINKGW_ERR;
	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &wconfig, sizeof(wconfig)))
		return ALINKGW_ERR;
	if (channel <= 0 ||channel > 13)
		return ALINKGW_ERR;
	nr = mu_msg(WIFI_MOD_GET_SURVEY, &scan, sizeof(int), survey, sizeof(struct iwsurvey) * IW_SUR_MX);
	if (nr <= 0) {
		sleep(8);
		scan = 0;
		nr = mu_msg(WIFI_MOD_GET_SURVEY, &scan, sizeof(int), survey, sizeof(struct iwsurvey) * IW_SUR_MX);
		if (nr < 0)
			return ALINKGW_ERR;
	}
	for (i = 0; i < nr; i++) {
		if (channel == survey[i].ch)
			sch++;
	}
	response = json_object_new_object();
	snprintf(retstr, sizeof(retstr), "%d", wconfig.channel);
	obj = json_object_new_string(retstr);
	json_object_object_add(response, "channel", obj);
	if (wconfig.htbw == 1)
		obj = json_object_new_string("20/40");
	else
		obj = json_object_new_string("20");
	json_object_object_add(response, "bindwidth", obj);
	if (sch == 0)
		obj = json_object_new_string("3");
	else if (sch > 0 && sch < 3)
		obj = json_object_new_string("2");
	else
		obj = json_object_new_string("1");
	json_object_object_add(response, "quality", obj);
	strncpy(buff, json_object_to_json_string(response), buff_len - 1);
	buff[buff_len - 1] = 0;
	json_object_put(response);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_wlanChannelCondition5g(char *buff, unsigned int buff_len)
{
	int scan = 0;
	int nr, i;
	char retstr[128];
	int channel, sch = 0;
	struct wifi_conf wconfig;
	struct iwsurvey survey[IW_SUR_MX];
	struct json_object* obj, *response;

	ALI_DBG("%d\n", buff_len);
	
	if (!noscan5)
		scan = 1;
	noscan5 = 0;
	if (mu_msg(WIFI_MOD_GET_CHANNEL_5G, NULL, 0, &channel, sizeof(channel)))
		return ALINKGW_ERR;
	if (mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &wconfig, sizeof(wconfig)))
		return ALINKGW_ERR;
	if (channel < 149 || channel > 161)
		return ALINKGW_ERR;
	nr = mu_msg(WIFI_MOD_GET_SURVEY_5G, &scan, sizeof(int), survey, sizeof(struct iwsurvey) * IW_SUR_MX);
	if (nr <= 0) {
		sleep(8);
		scan = 0;
		nr = mu_msg(WIFI_MOD_GET_SURVEY_5G, &scan, sizeof(int), survey, sizeof(struct iwsurvey) * IW_SUR_MX);
		if (nr < 0)
			return ALINKGW_ERR;
	}
	for (i = 0; i < nr; i++) {
		if (channel == survey[i].ch)
			sch++;
	}
	response = json_object_new_object();
	snprintf(retstr, sizeof(retstr), "%d", wconfig.channel);
	obj = json_object_new_string(retstr);
	json_object_object_add(response, "channel", obj);
	if (wconfig.htbw == 1)
		obj = json_object_new_string("80");
	else
		obj = json_object_new_string("40");
	json_object_object_add(response, "bindwidth", obj);
	if (sch == 0)
		obj = json_object_new_string("3");
	else if (sch > 0 && sch < 3)
		obj = json_object_new_string("2");
	else
		obj = json_object_new_string("1");
	json_object_object_add(response, "quality", obj);
	strncpy(buff, json_object_to_json_string(response), buff_len - 1);
	buff[buff_len - 1] = 0;
	json_object_put(response);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_get_speedupSetting(char *buff, unsigned int buff_len)
{
	int i, nr;
	char mac[32];
	struct host_dump_info *host = NULL;
	struct json_object *obj, *response;

	host = malloc(sizeof(struct host_dump_info) * IGD_HOST_MX);
	if (!host)
		return ALINKGW_ERR;
	memset(host, 0x0, sizeof(struct host_dump_info) * IGD_HOST_MX);
	nr = mu_msg(IGD_HOST_DUMP, NULL, 0, host, sizeof(struct host_dump_info) * IGD_HOST_MX);
	if (nr < 0) {
		free(host);
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}

	for (i = 0; i < nr; i++) {
		if (igd_test_bit(HIRF_ISKING, host[i].flag))
			break;
	}

	response = json_object_new_object();
	if (i >= nr) {
		obj = json_object_new_string("0");
		json_object_object_add(response, "enabled", obj);
		obj = json_object_new_string("");
		json_object_object_add(response, "mac", obj);
	} else {
		obj = json_object_new_string("1");
		json_object_object_add(response, "enabled", obj);
		snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(host[i].mac));
		obj = json_object_new_string(mac);
		json_object_object_add(response, "mac", obj);
	}

	strncpy(buff, json_object_to_json_string(response), buff_len - 1);
	buff[buff_len - 1] = 0;
	json_object_put(response);
	free(host);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_set_speedupSetting(const char *json_in)
{
	char retstr[32];
	struct qos_conf config;
	struct host_set_info info;

	memset(&info, 0, sizeof(info));
	if (json_getval(retstr, sizeof(retstr), "\"mac\":", json_in)) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}
	parse_mac(retstr, info.mac);
	if (json_getval(retstr, sizeof(retstr), "\"enabled\":", json_in)) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}
	if (retstr[0] == '0')
		info.act = NLK_ACTION_DEL;
	else if (retstr[0] == '1') {
		info.act = NLK_ACTION_ADD;
		if (mu_msg(QOS_PARAM_SHOW, NULL, 0, &config, sizeof(config))) {
			ALI_DBG("fail\n");
			return ALINKGW_ERR;
		}
		if (config.enable == 0) {
			config.enable = 1;
			if (mu_msg(QOS_PARAM_SET, &config, sizeof(config), NULL, 0)){
				ALI_DBG("fail\n");
				return ALINKGW_ERR;
			}
		}
	} else {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}
	if (mu_msg(IGD_HOST_SET_KING, &info, sizeof(info), NULL, 0) < 0) {
		ALI_DBG("fail\n");
		return ALINKGW_ERR;
	}
	ALINKGW_report_attr_direct(ALINKGW_ATTR_SPEEDUP_SETTING, ALINKGW_ATTRIBUTE_complex, json_in);

	ALI_DBG("%s\n", json_in);
	return ALINKGW_OK;
}

#if 0
int get_tpsklist(char *buff, unsigned int buff_len)
{
	int i;
	struct json_object *obj, *obja, *response;

	response = json_object_new_array();
	for (i = 0; i < tpskl.nr; i++) {
		obja = json_object_new_object();
		obj = json_object_new_string(tpskl.list[i].psk);
		json_object_object_add(obja, "tpsk", obj);
		obj = json_object_new_string(tpskl.list[i].mac);
		json_object_object_add(obja, "mac", obj);
		obj = json_object_new_string(tpskl.list[i].duration);
		json_object_object_add(obja, "duration", obj);
		json_object_array_add(response, obja);
	}
	strncpy(buff, json_object_to_json_string(response), buff_len - 1);
	buff[buff_len - 1] = 0;
	json_object_put(response);
	return ALINKGW_OK;
}

int set_tpsklist(const char *json_in)
{
	int nr = 0;
	char *cur = NULL;
	char retstr[128];
	
	if (!json_in || !strlen(json_in))
		return ALINKGW_ERR;
	cur = strchr(json_in, '{') + 1;
	memset(&tpskl, 0x0, sizeof(tpskl));
	while (1) {
		if (json_getval(retstr, sizeof(retstr), "\"tpsk\":", cur))
			break;
		arr_strcpy(tpskl.list[nr].psk, retstr);
		if (json_getval(retstr, sizeof(retstr), "\"mac\":", cur))
			continue;
		arr_strcpy(tpskl.list[nr].mac, retstr);
		if (json_getval(retstr, sizeof(retstr), "\"duration\":", cur))
			continue;
		arr_strcpy(tpskl.list[nr].duration, retstr);
		nr++;
		cur = strchr(cur, '{');
		if (!cur)
			break;
		cur += 1;
	}
	tpskl.nr = nr;
	save_tpsklist();
	ALINKGW_report_attr_direct(ALINKGW_ATTR_TPSK_LIST, ALINKGW_ATTRIBUTE_array, json_in);
	return ALINKGW_OK;
}
#endif

int ali_kvp_save_cb(const char *key, const char *value)
{
	char cmd[256];
	pthread_mutex_lock(&ali_mutex); 
	snprintf(cmd, sizeof(cmd), "%s.kvp=kvp", ALI_CONFIG);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.kvp.%s=%s", ALI_CONFIG, key, value);
	uuci_set(cmd);
	pthread_mutex_unlock(&ali_mutex);
	return 0;
}

int ali_kvp_load_cb(const char *key, char *value, unsigned int buf_sz)
{
	int ret = -2;
	struct uci_element *se, *oe;
	struct uci_option *option;
	struct uci_section *section;
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;

	pthread_mutex_lock(&ali_mutex); 
	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, ALI_CONFIG, &pkg)) {
		uci_free_context(ctx);
		pthread_mutex_unlock(&ali_mutex); 
		return -2;
	}
	
	uci_foreach_element(&pkg->sections, se) {
		section = uci_to_section(se);
		if (section == NULL)
			continue;
		if (strcmp(section->type, "kvp") || strcmp(se->name, "kvp")) 
			continue;
		uci_foreach_element(&section->options, oe) {
			option = uci_to_option(oe);
			if (option == NULL)
				continue;
			if (option->type != UCI_TYPE_STRING || strcmp(oe->name, key))
				continue;
			if (strlen(option->v.string) > (buf_sz - 1)) {
				ret = -1;
				goto out;
			}
			strncpy(value, option->v.string, buf_sz - 1);
			ret = 0;
			goto out;
		}
	}
out:
	uci_unload(ctx, pkg);
	uci_free_context(ctx);
	pthread_mutex_unlock(&ali_mutex); 
	return ret;
}

#ifdef CONFIG_ALINK_LOCAL_ROUTER_OTA
int ali_ota_key_get(const char *key, char *buff, unsigned int buff_size)
{
	int ret;
	char str[256];

	pthread_mutex_lock(&ali_mutex);

	snprintf(str, sizeof(str), "ali_%s", key);
	ret = mtd_get_val(str, buff, buff_size);
	ALI_DBG("%d, [%s], [%s]\n", ret, key, buff);

	pthread_mutex_unlock(&ali_mutex);
	return (ret == 0) ? ALINKGW_OK : ALINKGW_ERR;
}

int ali_ota_key_set(const char *key, const char *value)
{
	int ret;
	char str[256];

	pthread_mutex_lock(&ali_mutex);

	snprintf(str, sizeof(str), "ali_%s=%s", key, value);
	ret = mtd_set_val(str);

	ALI_DBG("%d, [%s], [%s]\n", ret, key, value);

	pthread_mutex_unlock(&ali_mutex);
	return (ret == 0) ? ALINKGW_OK : ALINKGW_ERR;
}
#endif

int rt_attr_get_wlist_state(char *buff, unsigned int buff_len)
{
	struct iwacl wacl;

	if (mu_msg(WIFI_MOD_GET_ACL, NULL, 0, &wacl, sizeof(wacl)))
		return ALINKGW_ERR;
	if (wacl.wlist.enable)
		snprintf(buff, buff_len, "1");
	else
		snprintf(buff, buff_len, "0");

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_set_wlist_state(const char *json_in)
{
	int i, nr;
	struct iwacl wacl;
	struct host_dump_info host[IGD_HOST_MX];

	ALI_DBG("%s\n", json_in);

	if (mu_msg(WIFI_MOD_GET_ACL, NULL, 0, &wacl, sizeof(wacl)))
		return ALINKGW_ERR;

	wacl.wlist.enable = atoi(json_in);

	if (wacl.wlist.enable) {
		nr = mu_msg(IGD_HOST_DUMP, NULL, 0, &host, sizeof(host));
		nr = (nr <= 0) ? 0 : nr;
		wacl.wlist.nr = 0;
		memset(&wacl.wlist.list, 0, sizeof(wacl.wlist.list));
		for (i = 0; (i < nr) && (wacl.wlist.nr < IW_LIST_MX); i++) {
			if (find_host_in_list(host[i].mac, &wacl.blist))
				continue;
			wacl.wlist.list[wacl.wlist.nr].enable = 1;
			memcpy(wacl.wlist.list[wacl.wlist.nr].mac, host[i].mac, 6);
			wacl.wlist.nr++;
		}
	}

	if (mu_msg(WIFI_MOD_SET_ACL, &wacl, sizeof(wacl), NULL, 0))
		return ALINKGW_ERR;
	wacl.ifindex = 1;
	mu_msg(WIFI_MOD_SET_ACL_5G, &wacl, sizeof(wacl), NULL, 0);
	ALINKGW_report_attr_direct(ALINKGW_ATTR_WHITE_LIST_STATE, ALINKGW_ATTRIBUTE_simple, json_in);
	return ALINKGW_OK;
}

int rt_attr_get_blist_state(char *buff, unsigned int buff_len)
{
	struct iwacl wacl;
	
	if (mu_msg(WIFI_MOD_GET_ACL, NULL, 0, &wacl, sizeof(wacl)))
		return ALINKGW_ERR;
	if (wacl.blist.enable)
		snprintf(buff, buff_len, "1");
	else
		snprintf(buff, buff_len, "0");

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_set_blist_state(const char *json_in)
{
	struct iwacl wacl;

	ALI_DBG("%s\n", json_in);

	if (mu_msg(WIFI_MOD_GET_ACL, NULL, 0, &wacl, sizeof(wacl)))
		return ALINKGW_ERR;
	wacl.blist.enable = atoi(json_in);
	if (mu_msg(WIFI_MOD_SET_ACL, &wacl, sizeof(wacl), NULL, 0))
		return ALINKGW_ERR;
	wacl.ifindex = 1;
	mu_msg(WIFI_MOD_SET_ACL_5G, &wacl, sizeof(wacl), NULL, 0);
	ALINKGW_report_attr_direct(ALINKGW_ATTR_BLACK_LIST_STATE, ALINKGW_ATTRIBUTE_simple, json_in);

	return ALINKGW_OK;
}

int rt_attr_get_wlist(char *buff, unsigned int buff_len)
{
	char mac[18];
	struct iwacl wacl;
	struct json_object *wlist, *obj;

	if (mu_msg(WIFI_MOD_GET_ACL, NULL, 0, &wacl, sizeof(wacl)))
		return ALINKGW_ERR;
	wlist = json_object_new_array();
	loop_for_each(0, wacl.wlist.nr) {
		snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(wacl.wlist.list[i].mac));
		obj = json_object_new_string(mac);
		json_object_array_add(wlist, obj);
	} loop_for_each_end();
	strncpy(buff, json_object_to_json_string(wlist), buff_len - 1);
	buff[buff_len - 1] = 0;
	json_object_put(wlist);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_set_wlist(const char *json_in)
{	
	int i = 0;
	struct iwacl wacl;
	unsigned char mac[18];
	unsigned char *cur, *dot;

	ALI_DBG("%s\n", json_in);

	if (mu_msg(WIFI_MOD_GET_ACL, NULL, 0, &wacl, sizeof(wacl)))
		return ALINKGW_ERR;
	wacl.wlist.nr = 0;
	memset(wacl.wlist.list, 0x0, sizeof(wacl.wlist.list));
	cur = (unsigned char *)json_in + 3;
	while (1) {
		dot = strchr(cur, '"');
		if (!dot)
			break;
		__arr_strcpy_end(mac, cur, sizeof(mac), '"');
		if (!checkmac(mac)) {
			wacl.wlist.list[i].enable = 1;
			parse_mac(mac, wacl.wlist.list[i].mac);
			i++;
			wacl.wlist.nr = i;
		}
		if (*(dot + 2) == ']')
			break;
		cur = dot + 4;
		if (!cur)
			break;
	}
	if (mu_msg(WIFI_MOD_SET_ACL, &wacl, sizeof(wacl), NULL, 0))
		return ALINKGW_ERR;
	wacl.ifindex = 1;
	mu_msg(WIFI_MOD_SET_ACL_5G, &wacl, sizeof(wacl), NULL, 0);
	ALINKGW_report_attr_direct(ALINKGW_ATTR_WHITE_LIST, ALINKGW_ATTRIBUTE_array, json_in);

	return ALINKGW_OK;
}

int rt_attr_get_blist(char *buff, unsigned int buff_len)
{
	char mac[18];
	struct iwacl wacl;
	struct json_object *blist, *obj;

	if (mu_msg(WIFI_MOD_GET_ACL, NULL, 0, &wacl, sizeof(wacl)))
		return ALINKGW_ERR;
	blist = json_object_new_array();
	loop_for_each(0, wacl.blist.nr) {
		snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(wacl.blist.list[i].mac));
		obj = json_object_new_string(mac);
		json_object_array_add(blist, obj);
	} loop_for_each_end();
	strncpy(buff, json_object_to_json_string(blist), buff_len - 1);
	buff[buff_len - 1] = 0;
	json_object_put(blist);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int rt_attr_set_blist(const char *json_in)
{	
	int i = 0;
	char cmd[64];
	struct iwacl wacl;
	unsigned char mac[18];
	unsigned char *cur, *dot;

	ALI_DBG("%s\n", json_in);

	if (mu_msg(WIFI_MOD_GET_ACL, NULL, 0, &wacl, sizeof(wacl)))
		return ALINKGW_ERR;
	wacl.blist.nr = 0;
	memset(wacl.blist.list, 0x0, sizeof(wacl.blist.list));
	cur = (unsigned char *)json_in + 3;
	while (1) {
		dot = strchr(cur, '"');
		if (!dot)
			break;
		__arr_strcpy_end(mac, cur, sizeof(mac), '"');
		if (!checkmac(mac)) {
			wacl.blist.list[i].enable = 1;
			parse_mac(mac, wacl.blist.list[i].mac);
			i++;
			wacl.blist.nr = i;
		}
		if (*(dot + 2) == ']')
			break;
		cur = dot + 4;
		if (!cur)
			break;
	}
	if (mu_msg(WIFI_MOD_SET_ACL, &wacl, sizeof(wacl), NULL, 0))
		return ALINKGW_ERR;
	wacl.ifindex = 1;
	mu_msg(WIFI_MOD_SET_ACL_5G, &wacl, sizeof(wacl), NULL, 0);
	ALINKGW_report_attr_direct(ALINKGW_ATTR_BLACK_LIST, ALINKGW_ATTRIBUTE_array, json_in);

	if (wacl.wlist.enable)
		rt_attr_set_wlist_state("1");

	for (i = 0; (i < wacl.blist.nr) && (i < IW_LIST_MX); i++) {
		snprintf(cmd, sizeof(cmd), 
			"DisConnectSta=%02x:%02x:%02x:%02x:%02x:%02x",
			MAC_SPLIT(wacl.blist.list[i].mac));
		system(cmd);
	}
	ali_host_timer = (sys_uptime() - 8);
	return ALINKGW_OK;
}

int rt_attr_get_guestSetting24g(char *buff, unsigned int buff_len)
{
	char *mac;
	char retstr[32];
	struct wifi_conf config;
	struct json_object *obj, *response;

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
		return ALINKGW_ERR;
	response = json_object_new_object();
	snprintf(retstr, sizeof(retstr), "%d", config.vap);
	obj = json_object_new_string(retstr);
	json_object_object_add(response, "enabled", obj);
	mac = get_mac(LAN_INTERFACE_NAME);
	obj = json_object_new_string(mac);
	json_object_object_add(response, "bssid", obj);
	obj = json_object_new_string(config.vssid);
	json_object_object_add(response, "ssid", obj);
	if (config.hidvssid)
		obj = json_object_new_string("0");
	else
		obj = json_object_new_string("1");
	json_object_object_add(response, "ssidBroadcast", obj);
	switch (config.mode) {
	case 0:
		obj = json_object_new_string("bg");
		break;
	case 1:
		obj = json_object_new_string("b");
		break;
	case 4:
		obj = json_object_new_string("g");
		break;
	case 6:
		obj = json_object_new_string("n");
		break;
	case 7:
		obj = json_object_new_string("gn");
		break;
	case 9:
		obj = json_object_new_string("bgn");
		break;
	default:
		json_object_put(response);
		return ALINKGW_ERR;
	}
	json_object_object_add(response, "mode", obj);
	strncpy(buff, json_object_to_json_string(response), buff_len - 1);
	buff[buff_len - 1] = 0;
	json_object_put(response);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

#ifdef ALI_REQUIRE_SWITCH
static long set_guest_timer = 1;
#else
static long set_guest_timer = 0;
#endif
static int set_guestSetting24g_hide(void)
{
	struct wifi_conf config;

	ALI_DBG("start\n");

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config))) {
		ALI_DBG("get fail\n");
		return -1;
	}

	if (!config.aliset || !config.vap || config.hidvssid) {
		ALI_DBG("dont set, %d, %d, %d\n", config.aliset, config.vap, config.hidvssid);
		return 0;
	}
	config.hidvssid = 1;
	if (mu_msg(WIFI_MOD_SET_VAP, &config, sizeof(config), NULL, 0)) {
		ALI_DBG("set fail\n");
		return -1;
	}
	ALI_DBG("set hide succ\n");
	return 0;
}

int rt_attr_set_guestSetting24g(const char *json_in)
{
	int len;
	char value[32];
	int mode, hidden, enable;
	char ssid[IGD_NAME_LEN];
	struct wifi_conf config;

	ALI_DBG("%s\n", json_in);

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
		return ALINKGW_ERR;
	if (json_getval(value, sizeof(value), "\"enabled\":", json_in))
		return ALINKGW_ERR;
	enable = atoi(value);
	if (json_getval(ssid, sizeof(value), "\"ssid\":", json_in))
		return ALINKGW_ERR;
	if (json_getval(value, sizeof(value), "\"ssidBroadcast\":", json_in))
		return ALINKGW_ERR;
	if (atoi(value) == 0)
		hidden = 1;
	else
		hidden = 0;
	if (json_getval(value, sizeof(value), "\"mode\":", json_in))
		return ALINKGW_ERR;
	len = strlen(value);
	if (!strcmp(value, "bgn") && (len == 3))
		mode = MODE_802_11_BGN;
	else if (!strcmp(value, "bg") && (len == 2))
		mode = MODE_802_11_BG;
	else if (!strcmp(value, "gn") && (len == 2))
		mode = MODE_802_11_GN;
	else if (value[0] == 'b' && (len == 1))
		mode = MODE_802_11_B;
	else if (value[0] == 'g' && (len == 1))
		mode = MODE_802_11_G;
	else if (value[0] == 'n' && (len == 1))
		mode = MODE_802_11_N;
	else
		return ALINKGW_ERR;
	config.aliset = 1;
	config.wechat = 0;
	config.vap = enable;
	config.hidvssid = hidden;
	arr_strcpy(config.vssid, ssid);
	arr_strcpy(config.vencryption, "none");
	if (mu_msg(WIFI_MOD_SET_VAP, &config, sizeof(config), NULL, 0))
		return ALINKGW_ERR;
	if (mode != config.mode) {
		config.mode = mode;
		if (mu_msg(WIFI_MOD_SET_MODE, &config.mode, sizeof(config.mode), NULL, 0))
			return ALINKGW_ERR;
	}
	ALINKGW_report_attr_direct(ALINKGW_ATTR_GUEST_SETTING_24G, ALINKGW_ATTRIBUTE_complex, json_in);
	if (enable) {
		set_guest_timer = sys_uptime();
	} else {
		set_guest_timer = 0;
	}
	ALI_DBG("timer:%d, %d\n", enable, set_guest_timer);
	return ALINKGW_OK;
}

int rt_attr_get_wanStat(char *buff, unsigned int buff_len)
{
	int uid = 1;
	struct iface_info info;

	if (mu_msg(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &info, sizeof(info)))
		return ALINKGW_ERR;
	snprintf(buff, buff_len, "%d", info.net);

	return ALINKGW_OK;
}

int rt_attr_get_wanSetting(char *buff, unsigned int buff_len)
{
	int uid = 1;
	int mode, mtu;
	char retstr[32];
	char user[32] = {0};
	char passwd[32] = {0};
	struct iface_info info;
	struct iface_conf config;
	struct json_object *obj, *response;

	if (mu_msg(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &info, sizeof(info)))
		return ALINKGW_ERR;
	if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &config, sizeof(config)))
		return ALINKGW_ERR;
	response = json_object_new_object();
	switch(config.mode) {
	case MODE_DHCP:
		mode = 1;
		mtu = config.dhcp.mtu;
		break;
	case MODE_PPPOE:
		mtu = config.pppoe.comm.mtu;
		arr_strcpy(user, config.pppoe.user);
		arr_strcpy(passwd, config.pppoe.passwd);
		mode = 2;
		break;
	case MODE_STATIC:
		mtu = config.statip.comm.mtu;
		mode = 3;
		break;
	default:
		return ALINKGW_ERR;
	}
	snprintf(retstr, sizeof(retstr), "%d", mode);
	obj = json_object_new_string(retstr);
	json_object_object_add(response, "type", obj);
	obj = json_object_new_string(user);
	json_object_object_add(response, "username", obj);
	obj = json_object_new_string(passwd);
	json_object_object_add(response, "password", obj);
	obj = json_object_new_string(inet_ntoa(info.ip));
	json_object_object_add(response, "ipAddress", obj);
	obj = json_object_new_string(inet_ntoa(info.mask));
	json_object_object_add(response, "subnetMask", obj);
	obj = json_object_new_string(inet_ntoa(info.gw));
	json_object_object_add(response, "gateway", obj);
	obj = json_object_new_string(inet_ntoa(info.dns[0]));
	json_object_object_add(response, "dnsPrimary", obj);
	obj = json_object_new_string(inet_ntoa(info.dns[1]));
	json_object_object_add(response, "dnsSecondary", obj);
	snprintf(retstr, sizeof(retstr), "%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(config.mac));
	obj = json_object_new_string(retstr);
	json_object_object_add(response, "macAddress", obj);
	snprintf(retstr, sizeof(retstr), "%d", mtu);
	obj = json_object_new_string(retstr);
	json_object_object_add(response, "mtu", obj);
	strncpy(buff, json_object_to_json_string(response), buff_len - 1);
	buff[buff_len - 1] = 0;
	json_object_put(response);

	return ALINKGW_OK;
}

int rt_attr_set_wanSetting(const char *json_in)
{
	int mtu = 0;
	char retstr[32];
	struct iconf_comm comm;
	struct iface_conf config;

	memset(&comm, 0x0, sizeof(comm));
	memset(&config, 0x0, sizeof(config));
	if (json_getval(retstr, sizeof(retstr),"\"type\":" , json_in))
		return ALINKGW_ERR;
	config.mode = atoi(retstr);
	if (json_getval(retstr, sizeof(retstr),"\"macAddress\":" , json_in))
		return ALINKGW_ERR;
	parse_mac(retstr, config.mac);
	if (json_getval(retstr, sizeof(retstr),"\"mtu\":" , json_in))
		return ALINKGW_ERR;
	mtu = atoi(retstr);
	if (json_getval(retstr, sizeof(retstr),"\"dnsPrimary\":" , json_in))
		return ALINKGW_ERR;
	inet_aton(retstr, &comm.dns[0]);
	if (json_getval(retstr, sizeof(retstr),"\"dnsSecondary\":" , json_in))
		return ALINKGW_ERR;
	inet_aton(retstr, &comm.dns[1]);
	switch(config.mode) {
	case MODE_DHCP:
		config.dhcp = comm;
		if (config.dhcp.mtu > 1500)
			return ALINKGW_ERR;
		break;
	case MODE_PPPOE:
		config.pppoe.comm = comm;
		if (config.pppoe.comm.mtu > 1492)
			return ALINKGW_ERR;
		if (json_getval(retstr, sizeof(retstr),"\"username\":" , json_in))
			return ALINKGW_ERR;
		arr_strcpy(config.pppoe.user, retstr);
		if (json_getval(retstr, sizeof(retstr),"\"password\":" , json_in))
			return ALINKGW_ERR;
		arr_strcpy(config.pppoe.passwd, retstr);
		break;
	case MODE_STATIC:
		config.statip.comm = comm;
		if (config.statip.comm.mtu > 1500)
			return ALINKGW_ERR;
		if (json_getval(retstr, sizeof(retstr),"\"ipAddress\":" , json_in))
			return ALINKGW_ERR;
		inet_aton(retstr, &config.statip.ip);
		if (json_getval(retstr, sizeof(retstr),"\"subnetMask\":" , json_in))
			return ALINKGW_ERR;
		inet_aton(retstr, &config.statip.mask);
		if (json_getval(retstr, sizeof(retstr),"\"gateway\":" , json_in))
			return ALINKGW_ERR;
		inet_aton(retstr, &config.statip.gw);
		break;
	default:
		return ALINKGW_ERR;
	}
	config.uid = 1;
	if (mu_msg(IF_MOD_PARAM_SET, &config, sizeof(config), NULL, 0))
		return ALINKGW_ERR;
	ALINKGW_report_attr_direct(ALINKGW_ATTR_WAN_SETTING, ALINKGW_ATTRIBUTE_complex, json_in);

	return ALINKGW_OK;
}

struct ali_nat_info {
	struct list_head list;
	char *name;
	char *proto;
	struct in_addr ip;
	unsigned short out_port;
	unsigned short in_port;
};

#define ALI_STATIC_DNAT_CHAIN  "ALI_STATIC_DNAT"
static struct list_head ali_nat_list = LIST_HEAD_INIT(ali_nat_list);

static int ali_nat_add(struct ali_nat_info *ani)
{
	int uid = 1;
	struct iface_info winfo;
	char str[128];

	if (mu_msg(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &winfo, sizeof(winfo))) {
		ALI_DBG("call IF_MOD_IFACE_INFO fail\n");
		return -1;
	}
	
	snprintf(str, sizeof(str), "iptables -t nat -A %s -i %s -p %s --dport %d -j DNAT --to-destination %s:%d",
			ALI_STATIC_DNAT_CHAIN, winfo.devname, ani->proto, ani->out_port, inet_ntoa(ani->ip), ani->in_port);
	system(str);
	return 0;
}

static int ali_nat_restart(void)
{
	int uid = 1;
	char cmd[2048];
	struct iface_info winfo;
	struct ali_nat_info *ani = NULL;

	memset(&winfo, 0, sizeof(winfo));
	if (mu_msg(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &winfo, sizeof(winfo))) {
		ALI_DBG("call IF_MOD_IFACE_INFO fail\n");
		return -1;
	}

	snprintf(cmd, sizeof(cmd), "iptables -t nat -F %s", ALI_STATIC_DNAT_CHAIN);
	system(cmd);
	snprintf(cmd, sizeof(cmd), "iptables -t nat -D PREROUTING -i %s -j %s", winfo.devname, ALI_STATIC_DNAT_CHAIN);
	system(cmd);
	snprintf(cmd, sizeof(cmd), "iptables -t nat -A PREROUTING -i %s -j %s", winfo.devname, ALI_STATIC_DNAT_CHAIN);
	system(cmd);

	list_for_each_entry(ani, &ali_nat_list, list) {
		ali_nat_add(ani);
	}
	return 0;
}

static int ali_nat_load(struct uci_section *s)
{
	struct uci_element *oe = NULL;
	struct uci_option *o = NULL;
	struct ali_nat_info *ani = NULL;
	char *ptr = NULL, *dst = NULL;

	uci_foreach_element(&s->options, oe) {
		o = uci_to_option(oe);
		ani = malloc(sizeof(*ani));
		if (!ani) {
			ALI_DBG("malloc fail\n");
			return -1;
		}
		ani->name = strdup(oe->name);
		dst = o->v.string;
		ptr = strchr(dst, ',');
		if (!ptr)
			goto err;
		*ptr = '\0';
		ani->proto = strdup(dst);
		dst = ptr + 1;

		ptr = strchr(dst, ',');
		if (!ptr)
			goto err;
		*ptr = '\0';
		inet_aton(dst, &ani->ip);
		dst = ptr + 1;

		ptr = strchr(dst, ',');
		if (!ptr)
			goto err;
		*ptr = '\0';
		ani->in_port = atoi(dst);
		ani->out_port = atoi(ptr + 1);
		ali_nat_add(ani);
		list_add(&ani->list, &ali_nat_list);
	}
	return 0;
err:
	if (ani) {
		if (ani->name)
			free(ani->name);
		if (ani->proto)
			free(ani->proto);
		free(ani);
	}
	return -1;
}

int rt_attr_get_portmappingsetting(char *buff, unsigned int buff_len)
{
	int len = 0;
	struct ali_nat_info *ani = NULL;

	list_for_each_entry(ani, &ali_nat_list, list) {
		len += snprintf(buff + len, buff_len - len,
			"{ \"name\": \"%s\", "
			"\"internalClient\": \"%s\", "
			"\"protocol\": \"%s\", "
			"\"internalPort\": \"%d\", "
			"\"externalPort\": \"%d\" }, ",
			ani->name, inet_ntoa(ani->ip),
			ani->proto, ani->in_port, ani->out_port);
	}
	if (len < 3)
		return ALINKGW_OK;
	if (buff[len - 1] == ' ' && buff[len - 2] == ',')
		buff[len - 2] = '\0';
	return ALINKGW_OK;
}

int rt_attr_set_portmappingsetting(const char *json_in)
{
	char str[128], *ptr;
	struct ali_nat_info *ani, *_ani;

	list_for_each_entry_safe(ani, _ani, &ali_nat_list, list) {
		list_del(&ani->list);
		free(ani->name);
		free(ani->proto);
		free(ani);
	}
	uuci_set(ALI_CONFIG".nat=");
	uuci_set(ALI_CONFIG".nat=ali");
	ali_nat_restart();

	ptr = strchr(json_in, '{');
	while (ptr) {
		ani = malloc(sizeof(*ani));
		if (!ani) {
			ALI_DBG("malloc fail\n");
			return ALINKGW_ERR;
		}

		if (json_getval(str, sizeof(str), "\"name\":" , ptr))
			goto err;
		ALI_DBG("name:%s\n", str);
		ani->name = strdup(str);
		if (!ani->name)
			goto err;
		if (json_getval(str, sizeof(str), "\"protocol\":" , ptr))
			goto err;
		ALI_DBG("proto:%s\n", str);
		ani->proto= strdup(str);
		if (!ani->proto)
			goto err;
		if (json_getval(str, sizeof(str), "\"internalClient\":" , ptr))
			goto err;
		ALI_DBG("ip:%s\n", str);
		if (!inet_aton(str, &ani->ip))
			goto err;
		if (json_getval(str, sizeof(str), "\"internalPort\":" , ptr))
			goto err;
		ALI_DBG("in_port:%s\n", str);
		ani->in_port = atoi(str);
		if (json_getval(str, sizeof(str), "\"externalPort\":" , ptr))
			goto err;
		ALI_DBG("out_port:%s\n", str);
		ani->out_port= atoi(str);

		snprintf(str, sizeof(str), "%s.nat.%s=%s,%s,%d,%d",
			ALI_CONFIG, ani->name, ani->proto, inet_ntoa(ani->ip),
			ani->in_port, ani->out_port);
		ALI_DBG("str:%s\n", str);
		uuci_set(str);
		ali_nat_add(ani);
		list_add(&ani->list, &ali_nat_list);
		ptr = strchr(ptr + 1, '{');
	}
	return ALINKGW_OK;
err:
	if (ani) {
		if (ani->name)
			free(ani->name);
		if (ani->proto)
			free(ani->proto);
		free(ani);
	}
	return ALINKGW_ERR;
}

/*属性列表*/
ALINKGW_attribute_handler_t alinkgw_attribute_handlers[256] =
{
	{ALINKGW_ATTR_PROBE_SWITCH_STATE, ALINKGW_ATTRIBUTE_simple, rt_attr_get_probe_switch_state, rt_attr_set_probe_switch_state},
	{ALINKGW_ATTR_PROBE_NUMBER, ALINKGW_ATTRIBUTE_simple, rt_attr_get_probe_number, NULL},
	{ALINKGW_ATTR_PROBE_INFO, ALINKGW_ATTRIBUTE_simple, rt_attr_get_probe_info, NULL},
	{ALINKGW_ATTR_ACCESS_ATTACK_STATE, ALINKGW_ATTRIBUTE_simple, rt_attr_get_access_attack_state, rt_attr_set_access_attack_state},
	{ALINKGW_ATTR_ACCESS_ATTACK_NUM, ALINKGW_ATTRIBUTE_simple, rt_attr_get_access_attack_num, NULL},
	{ALINKGW_ATTR_ACCESS_ATTACKR_INFO, ALINKGW_ATTRIBUTE_simple, rt_attr_get_access_attack_info, NULL},
	{ALINKGW_ATTR_WLAN_SWITCH_STATE, ALINKGW_ATTRIBUTE_simple, rt_attr_get_wlanSwitchState, rt_attr_set_wlanSwitchState},
	{ALINKGW_ATTR_WAN_DLSPEED, ALINKGW_ATTRIBUTE_simple, rt_attr_get_wanDlSpeed, NULL},
	{ALINKGW_ATTR_WAN_ULSPEED, ALINKGW_ATTRIBUTE_simple, rt_attr_get_wanUlSpeed, NULL},
	{ALINKGW_ATTR_WAN_DLBYTES, ALINKGW_ATTRIBUTE_simple, rt_attr_get_wanDlbytes, NULL},
	{ALINKGW_ATTR_WAN_ULBYTES, ALINKGW_ATTRIBUTE_simple, rt_attr_get_wanUlbytes, NULL},
	{ALINKGW_ATTR_DLBWINFO, ALINKGW_ATTRIBUTE_simple, rt_attr_get_dlBwinfo, NULL},
	{ALINKGW_ATTR_ULBWINFO , ALINKGW_ATTRIBUTE_simple, rt_attr_get_ulBwInfo, NULL},
	{ALINKGW_ATTR_SPEEDUP_SETTING, ALINKGW_ATTRIBUTE_complex, rt_attr_get_speedupSetting, rt_attr_set_speedupSetting},
	{ALINKGW_ATTR_WLAN_PA_MODE, ALINKGW_ATTRIBUTE_simple, rt_attr_get_wlanPaMode, rt_attr_set_wlanPaMode},
	{ALINKGW_ATTR_WLAN_SETTING_24G, ALINKGW_ATTRIBUTE_complex, rt_attr_get_wlanSetting24g, rt_attr_set_wlanSetting24g},
	{ALINKGW_ATTR_WLAN_SECURITY_24G, ALINKGW_ATTRIBUTE_complex, rt_attr_get_wlanSecurity24g, rt_attr_set_wlanSecurity24g},
	{ALINKGW_ATTR_WLAN_CHANNEL_CONDITION_24G, ALINKGW_ATTRIBUTE_complex, rt_attr_get_wlanChannelCondition24g, NULL},
	//{ALINKGW_ATTR_TPSK_LIST, ALINKGW_ATTRIBUTE_array, get_tpsklist, set_tpsklist},
	{ALINKGW_ATTR_BLACK_LIST_STATE, ALINKGW_ATTRIBUTE_simple, rt_attr_get_blist_state, rt_attr_set_blist_state},
	{ALINKGW_ATTR_WHITE_LIST_STATE, ALINKGW_ATTRIBUTE_simple, rt_attr_get_wlist_state, rt_attr_set_wlist_state},
	{ALINKGW_ATTR_BLACK_LIST, ALINKGW_ATTRIBUTE_array, rt_attr_get_blist, rt_attr_set_blist},
	{ALINKGW_ATTR_WHITE_LIST, ALINKGW_ATTRIBUTE_array, rt_attr_get_wlist, rt_attr_set_wlist},
	{ALINKGW_ATTR_GUEST_SETTING_24G, ALINKGW_ATTRIBUTE_complex, rt_attr_get_guestSetting24g, rt_attr_set_guestSetting24g},
	{ALINKGW_ATTR_WAN_STAT, ALINKGW_ATTRIBUTE_simple, rt_attr_get_wanStat, NULL},
	{ALINKGW_ATTR_WAN_SETTING, ALINKGW_ATTRIBUTE_complex, rt_attr_get_wanSetting, rt_attr_set_wanSetting},
	{ALINKGW_ATTR_PORTMAPPINGSETTING, ALINKGW_ATTRIBUTE_array, rt_attr_get_portmappingsetting, rt_attr_set_portmappingsetting},
	{ALINKGW_ATTR_ATTACK_SWITCH_STATE, ALINKGW_ATTRIBUTE_simple, rt_attr_get_attack_state, rt_attr_set_attack_state},
	{ALINKGW_ATTR_ATTACK_NUM, ALINKGW_ATTRIBUTE_simple, rt_attr_get_attack_num, NULL},
	{ALINKGW_ATTR_ATTACK_INFO, ALINKGW_ATTRIBUTE_complex, rt_attr_get_attack_info, NULL},
	{ALINKGW_ATTR_ADMIN_ATTACK_SWITCH_STATE, ALINKGW_ATTRIBUTE_simple, rt_attr_get_admin_attack_state, rt_attr_set_admin_attack_state},
	{ALINKGW_ATTR_ADMIN_ATTACK_NUM, ALINKGW_ATTRIBUTE_simple, rt_attr_get_admin_attack_num, NULL},
	{ALINKGW_ATTR_ADMIN_ATTACK_INFO, ALINKGW_ATTRIBUTE_simple, rt_attr_get_admin_attack_info, NULL},
	{ALINKGW_ATTR_WLAN_SETTING_5G, ALINKGW_ATTRIBUTE_complex, rt_attr_get_wlanSetting5g, rt_attr_set_wlanSetting5g},
	{ALINKGW_ATTR_WLAN_SECURITY_5G, ALINKGW_ATTRIBUTE_complex, rt_attr_get_wlanSecurity5g, rt_attr_set_wlanSecurity5g},
	{ALINKGW_ATTR_WLAN_CHANNEL_CONDITION_5G, ALINKGW_ATTRIBUTE_complex, rt_attr_get_wlanChannelCondition5g, NULL},
	{NULL, 0, NULL}
};

/*子设备属性处理函数*/
int sub_dev_attr_get_dlSpeed(const char *mac, char *buff, unsigned int buf_sz)
{
	unsigned char tmp[6];
	struct host_dump_info info;

	parse_mac(mac, tmp);

	if (mu_msg(IGD_HOST_DUMP, tmp, sizeof(tmp), &info, sizeof(info)))
		return ALINKGW_ERR;
	snprintf(buff, buf_sz, "%lu", info.down_speed);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int sub_dev_attr_get_ulSpeed(const char *mac, char *buff, unsigned int buf_sz)
{
	unsigned char tmp[6];
	struct host_dump_info info;

	parse_mac(mac, tmp);

	if (mu_msg(IGD_HOST_DUMP, tmp, sizeof(tmp), &info, sizeof(info)))
		return ALINKGW_ERR;
	snprintf(buff, buf_sz, "%lu", info.up_speed);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int sub_dev_attr_get_dlBytes(const char *mac, char *buff, unsigned int buf_sz)
{
	unsigned char tmp[6];
	struct host_dump_info info;
	struct ali_host_record *ahr;

	parse_mac(mac, tmp);

	if (mu_msg(IGD_HOST_DUMP, tmp, sizeof(tmp), &info, sizeof(info)))
		return ALINKGW_ERR;

	ahr = ali_find_host(tmp);
	if (!igd_test_bit(HIRF_ONLINE, info.flag) ||
		(ahr && (ahr->down_bytes >= info.down_bytes))) {
		snprintf(buff, buf_sz, "0");
	} else {
		snprintf(buff, buf_sz, "%llu", info.down_bytes - 
			(ahr ? ahr->down_bytes : 0));
	}

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int sub_dev_attr_get_ulBytes(const char *mac, char *buff, unsigned int buf_sz)
{
	unsigned char tmp[6];
	struct host_dump_info info;
	struct ali_host_record *ahr;

	parse_mac(mac, tmp);

	if (mu_msg(IGD_HOST_DUMP, tmp, sizeof(tmp), &info, sizeof(info)))
		return ALINKGW_ERR;

	ahr = ali_find_host(tmp);
	if (!igd_test_bit(HIRF_ONLINE, info.flag) ||
		(ahr && (ahr->up_bytes >= info.up_bytes))) {
		snprintf(buff, buf_sz, "0");
	} else {
		snprintf(buff, buf_sz, "%llu", info.up_bytes - 
			(ahr ? ahr->up_bytes : 0));
	}

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int sub_dev_attr_get_internetSwitchState(const char *mac, char *buff, unsigned int buf_sz)
{
	unsigned char tmp[6];
	struct host_dump_info info;

	parse_mac(mac, tmp);

	if (mu_msg(IGD_HOST_DUMP, tmp, sizeof(tmp), &info, sizeof(info)))
		return ALINKGW_ERR;
	snprintf(buff, buf_sz, "%d", igd_test_bit(HIRF_INBLACK, info.flag) ? 0 : 1);

	ALI_DBG("%s, %s\n", mac, buff);
	return ALINKGW_OK;
}

int sub_dev_attr_set_internetSwitchState(const char *mac, const char *json_in)
{
	struct host_set_info info;

	ALI_DBG("%s, %s\n", mac, json_in);

	memset(&info, 0, sizeof(info));
	parse_mac(mac, info.mac);
	info.act = atoi(json_in) ? NLK_ACTION_DEL : NLK_ACTION_ADD;

	if (mu_msg(IGD_HOST_SET_BLACK, &info, sizeof(info), NULL, 0) < 0)
		return ALINKGW_ERR;

	return ALINKGW_OK;
}

int sub_dev_attr_get_ipAddress(const char *mac, char *buff, unsigned int buf_sz)
{
	unsigned char tmp[6];
	struct host_dump_info info;

	parse_mac(mac, tmp);

	if (mu_msg(IGD_HOST_DUMP, tmp, sizeof(tmp), &info, sizeof(info)))
		return ALINKGW_ERR;

	snprintf(buff, buf_sz, "%s", inet_ntoa(info.ip[0]));

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int sub_dev_attr_get_ipv6Address(const char *mac, char *buff, unsigned int buf_sz)
{

	snprintf(buff, buf_sz, "");

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int sub_dev_attr_get_band(const char *mac, char *buff, unsigned int buf_sz)
{
	unsigned char tmp[6];
	struct host_dump_info info;

	parse_mac(mac, tmp);

	if (mu_msg(IGD_HOST_DUMP, tmp, sizeof(tmp), &info, sizeof(info)))
		return ALINKGW_ERR;

	// 0:wire, 1:2.4G, 2:5G
	if (!igd_test_bit(HIRF_WIRELESS, info.flag))
		snprintf(buff, buf_sz, "%d", 0);
	else if (!igd_test_bit(HIRF_WIRELESS_5G, info.flag))
		snprintf(buff, buf_sz, "%d", 1);
	else
		snprintf(buff, buf_sz, "%d", 2);

	ALI_DBG("%s,%s\n", mac, buff);
	return ALINKGW_OK;
}

int sub_dev_attr_get_appInfo(const char *mac, char *buff, unsigned int buf_sz)
{
	int nr, i;
	const char *json;
	unsigned char tmp[6];
	struct host_app_dump_info app[IGD_HOST_APP_MX];
	struct json_object *obj, *arr, *t;

	if (buf_sz <= 2) {
		ALI_DBG("buf_sz:%d\n", buf_sz);
		return ALINKGW_ERR;
	}

	parse_mac(mac, tmp);
	nr = mu_msg(IGD_HOST_APP_DUMP, tmp, 6, app, sizeof(app));
	if (nr < 0)
		return ALINKGW_ERR;

	arr = json_object_new_array();
	for (i = 0; i < nr; i++) {
		if (!igd_test_bit(HARF_ONLINE, app[i].flag) &&
			!igd_test_bit(HARF_INBLACK, app[i].flag))
			continue;
		obj = json_object_new_object();

		t = json_object_new_int(app[i].appid);
		json_object_object_add(obj, "appId", t);

		t = json_object_new_int(
			igd_test_bit(HARF_INBLACK, app[i].flag) ? 0 : 1);
		json_object_object_add(obj, "internetSwitchState", t);

		t = json_object_new_int(
			igd_test_bit(HARF_ONLINE, app[i].flag) ? 1 : 0);
		json_object_object_add(obj, "alive", t);

		t = json_object_new_int((int)app[i].uptime);
		json_object_object_add(obj, "timestamp", t);

		json_object_array_add(arr, obj);
	}

	json = json_object_to_json_string(arr);

	if (buf_sz <= strlen(json)) {
		ALI_DBG("%d,%d\n", buf_sz, strlen(json));
		json_object_put(arr);
		return ALINKGW_BUFFER_INSUFFICENT;
	}
	strncpy(buff, json, buf_sz - 1);
	buff[buf_sz - 1] = 0;
	json_object_put(arr);
	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int sub_dev_attr_set_appInfo(const char *mac, const char *json_in)
{
	char value[512], *ptr;
	struct host_set_info info;

	memset(&info, 0, sizeof(info));
	parse_mac(mac, info.mac);

	ALI_DBG("%s\n", json_in);

	ptr = (char *)json_in;
	while (1) {
		ptr = strchr(ptr, '{');
		if (!ptr)
			break;
		ptr++;
		if (json_getval(value, sizeof(value), "\"appId\":", ptr)) {
			ALI_DBG("get appId fail, %s\n", ptr);
			return ALINKGW_ERR;
		}
		info.appid = atoi(value);

		if (json_getval(value, sizeof(value), "\"internetSwitchState\":", ptr)) {
			ALI_DBG("get internetSwtichState fail, %s\n", ptr);
			return ALINKGW_ERR;
		}

		if (atoi(value)) {
			info.act = NLK_ACTION_DEL;
		} else {
			info.act = NLK_ACTION_ADD;
		}

		if (IGD_TEST_STUDY_URL(info.appid)) {
			ALI_DBG("appid err\n", ptr);
			return ALINKGW_ERR;
		}
	 
		if (info.appid == IGD_CHUYUN_APPID) {
			ALI_DBG("appid err\n", ptr);
			return ALINKGW_ERR;
		}
	 
		ALI_DBG("%d, %d\n", info.appid, info.act);
		if (mu_msg(IGD_HOST_SET_APP_BLACK, &info, sizeof(info), NULL, 0) < 0){
			ALI_DBG("IGD_HOST_SET_APP_BLACK fail, %s\n", ptr);
			return ALINKGW_ERR;
		}
	}
	ALI_DBG("success\n");
	return ALINKGW_OK;
}

#define WAN_PTK_APPID  14999999
int sub_dev_attr_get_appRecord(const char *mac, char *buff, unsigned int buf_sz)
{
	const char *json;
	unsigned char tmp[6];
	struct ali_host_record *ahr;
	struct ali_app_record *app, *_app;
	struct json_object *obj, *arr, *t;
	struct host_info *alive;
	struct app_conn_info *app_conn;
	int nr, i, app_nr, j, f, wan_app, ret;
	struct ali_app_record *aar = NULL;

	if (buf_sz < 2000) {
		ALI_DBG("buf_sz:%d\n", buf_sz);
		return ALINKGW_BUFFER_INSUFFICENT;
	}

	parse_mac(mac, tmp);
	ahr = ali_find_host(tmp);
	if (!ahr) {
		ALI_DBG("no find\n");
		return ALINKGW_ERR;
	}

	arr = json_object_new_array();

	alive = dump_host_alive(&nr);
	if (alive) {
		for (i = 0; i < nr; i++) {
			if (memcmp(alive[i].mac, ahr->mac, 6))
				continue;
			f = 0;
			app_conn = dump_host_app(alive[i].addr, &app_nr);
			if (app_conn) {
				for (j = 0; j < app_nr; j++) {
					if (!app_conn[j].conn_cnt)
						continue;
					aar = ali_app_find(ahr, app_conn[j].appid);
					if (aar)
						continue;
					obj = json_object_new_object();
					
					t = json_object_new_int(app_conn[j].appid);
					json_object_object_add(obj, "appId", t);
					
					t = json_object_new_int(1);
					json_object_object_add(obj, "alive", t);
					
					t = json_object_new_int((int)time(NULL));
					json_object_object_add(obj, "timestamp", t);
					
					json_object_array_add(arr, obj);

					f = 1;
					break;
				} 
				free(app_conn);
			}

			if (!f && !alive[i].wan_pkts) {
				if (!igd_test_bit(ATF_WAN, ahr->tmp))
					break;
				wan_app = 0;
				igd_clear_bit(ATF_WAN, ahr->tmp);
			} else {
				wan_app = 1;
				igd_set_bit(ATF_WAN, ahr->tmp);
			}
			obj = json_object_new_object();

			t = json_object_new_int(WAN_PTK_APPID);
			json_object_object_add(obj, "appId", t);
			
			t = json_object_new_int(wan_app);
			json_object_object_add(obj, "alive", t);
			
			t = json_object_new_int((int)time(NULL));
			json_object_object_add(obj, "timestamp", t);

			json_object_array_add(arr, obj);
			break;
		}
		free(alive);
	}

	list_for_each_entry_safe_reverse(app, _app, &ahr->app_list, list) {
		obj = json_object_new_object();

		t = json_object_new_int(app->appid);
		json_object_object_add(obj, "appId", t);

		t = json_object_new_int(
			igd_test_bit(AAF_ONLINE, app->flag) ? 1 : 0);
		json_object_object_add(obj, "alive", t);

		t = json_object_new_int((int)app->time);
		json_object_object_add(obj, "timestamp", t);

		json_object_array_add(arr, obj);

		list_del(&app->list);
		free(app);
		ahr->app_num--;
	}

	ret = ALINKGW_BUFFER_INSUFFICENT;
	strncpy(buff, "ALINKGW_BUFFER_INSUFFICENT", buf_sz - 1);
	json = json_object_to_json_string(arr);
	if (strlen(json) < (buf_sz - 1)) {
		strncpy(buff, json, buf_sz - 1);
		buff[buf_sz - 1] = 0;
		ret = ALINKGW_OK;
	}
	json_object_put(arr);

	ALI_DBG("[%s]:%s\n", mac, buff);
	return ret;
}

int sub_dev_attr_get_maxDlSpeed(const char *mac, char *buff, unsigned int buf_sz)
{
	unsigned char tmp[6];
	struct host_dump_info info;

	parse_mac(mac, tmp);

	if (mu_msg(IGD_HOST_DUMP, tmp, sizeof(tmp), &info, sizeof(info)))
		return ALINKGW_ERR;
	snprintf(buff, buf_sz, "%ld", info.ls.down ? (info.ls.down*1024) : -1);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int sub_dev_attr_set_maxDlSpeed(const char *mac, const char *json_in)
{
	unsigned char tmp[6];
	struct host_set_info info;
	struct host_dump_info host;

	memset(&info, 0, sizeof(info));
	parse_mac(mac, tmp);

	if (mu_msg(IGD_HOST_DUMP, tmp, sizeof(tmp), &host, sizeof(host)))
		return ALINKGW_ERR;

	memcpy(info.mac, tmp, 6);
	info.v.ls.down = atol(json_in)/1024;
	info.v.ls.up = host.ls.up;

	if (mu_msg(IGD_HOST_SET_LIMIT_SPEED, &info, sizeof(info), NULL, 0) < 0)
		return ALINKGW_ERR;

	ALI_DBG("%s\n", json_in);
	return ALINKGW_OK;
}

int sub_dev_attr_get_maxUlSpeed(const char *mac, char *buff, unsigned int buf_sz)
{
	unsigned char tmp[6];
	struct host_dump_info info;

	parse_mac(mac, tmp);

	if (mu_msg(IGD_HOST_DUMP, tmp, sizeof(tmp), &info, sizeof(info)))
		return ALINKGW_ERR;
	snprintf(buff, buf_sz, "%ld", info.ls.up ? (info.ls.up*1024) : -1);

	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int sub_dev_attr_set_maxUlSpeed(const char *mac, const char *json_in)
{
	unsigned char tmp[6];
	struct host_set_info info;
	struct host_dump_info host;

	memset(&info, 0, sizeof(info));
	parse_mac(mac, tmp);

	if (mu_msg(IGD_HOST_DUMP, tmp, sizeof(tmp), &host, sizeof(host)))
		return ALINKGW_ERR;

	memcpy(info.mac, tmp, 6);
	info.v.ls.up = atol(json_in)/1024;
	info.v.ls.down = host.ls.down;

	if (mu_msg(IGD_HOST_SET_LIMIT_SPEED, &info, sizeof(info), NULL, 0) < 0)
		return ALINKGW_ERR;

	ALI_DBG("%s\n", json_in);
	return ALINKGW_OK;
}

int sub_dev_attr_get_appblacklist(const char *mac, char *buff, unsigned int buf_sz)
{
	int nr, i;
	const char *json;
	unsigned char tmp[6];
	char str[64];
	struct host_app_dump_info app[IGD_HOST_APP_MX];
	struct json_object *obj, *arr, *t;

	if (buf_sz <= 2) {
		ALI_DBG("buf_sz:%d\n", buf_sz);
		return ALINKGW_ERR;
	}

	parse_mac(mac, tmp);
	nr = mu_msg(IGD_HOST_APP_DUMP, tmp, 6, app, sizeof(app));
	if (nr < 0)
		return ALINKGW_ERR;

	arr = json_object_new_array();
	for (i = 0; i < nr; i++) {
		if (!igd_test_bit(HARF_INBLACK, app[i].flag))
			continue;
		snprintf(str, sizeof(str), "%d", app[i].appid);
		t = json_object_new_string(str);
		json_object_array_add(arr, t);
	}

	json = json_object_to_json_string(arr);
	if (buf_sz <= strlen(json)) {
		ALI_DBG("%d,%d\n", buf_sz, strlen(json));
		json_object_put(arr);
		return ALINKGW_BUFFER_INSUFFICENT;
	}
	strncpy(buff, json, buf_sz - 1);
	buff[buf_sz - 1] = 0;
	json_object_put(arr);
	ALI_DBG("%s\n", buff);
	return ALINKGW_OK;
}

int sub_dev_attr_set_appblacklist(const char *mac, const char *json_in)
{
	char *appid;
	struct host_set_info info;

	ALI_DBG("%s\n", json_in);

	memset(&info, 0, sizeof(info));
	parse_mac(mac, info.mac);
	info.act = NLK_ACTION_CLEAN;

	if (mu_msg(IGD_HOST_SET_APP_BLACK, &info, sizeof(info), NULL, 0) < 0)
		return ALINKGW_ERR;

	appid = strchr(json_in, '[');
	if (appid)
		appid++;
	info.act = NLK_ACTION_ADD;
	while(appid) {
		if (*appid == ',')
			appid++;
		appid = strchr(appid, '"');
		if (!appid)
			break;
		appid++;
		info.appid = atoi(appid);
		ALI_DBG("appid:%d\n", info.appid);
		if (mu_msg(IGD_HOST_SET_APP_BLACK, &info, sizeof(info), NULL, 0) < 0)
			return ALINKGW_ERR;
		appid = strchr(appid, ',');
	}
	ALINKGW_report_attr_direct(ALINKGW_SUBDEV_ATTR_APPBLACKLIST, ALINKGW_ATTRIBUTE_array, json_in);
	return ALINKGW_OK;
}

static int get_sub_mode(const char *mac)
{
	int num, ret = 0;
	char tmp[64] = {0,}, str[512] = {0,}, **val;

	ali_mac2mac(mac, tmp);
	snprintf(str, sizeof(str), "aliconf.submode.%s", tmp);
	if (!uuci_get(str, &val, &num)) {
		if (val[0])
			ret = atoi(val[0]);
		uuci_get_free(val, num);
	}
	return ret;
}

extern int sub_dev_attr_get_appwhitelist(const char *mac, char *buff, unsigned int buf_sz);
extern int sub_dev_attr_get_webwhitelist(const char *mac, char *buff, unsigned int buf_sz);
int sub_dev_enable(const char *mac)
{
	char buf[4096], *ptr;
	struct host_set_info info;
	struct host_url_dump *url = &info.v.bw_url;

	ALI_DBG("mac:%s\n", mac);

	parse_mac(mac, info.mac);
	url->type = RLT_URL_WHITE;
	info.act = NLK_ACTION_CLEAN;

	if (mu_msg(IGD_HOST_SET_APP_WHITE, &info, sizeof(info), NULL, 0)) {
		ALI_DBG("appwhite clean fail\n");
		return -1;
	}
	if (mu_msg(IGD_HOST_URL_ACTION, &info, sizeof(info), NULL, 0)) {
		ALI_DBG("webwhite clean fail\n");
		return -1;
	}

	info.act = NLK_ACTION_DEL;
	if (mu_msg(IGD_HOST_SET_ALI_BLACK, &info, sizeof(info), NULL, 0) < 0) {
		ALI_DBG("black del fail\n");
		return -1;
	}

	if (!get_sub_mode(mac))
		return 0;

	info.act = NLK_ACTION_ADD;

	if (mu_msg(IGD_HOST_SET_ALI_BLACK, &info, sizeof(info), NULL, 0) < 0) {
		ALI_DBG("black add fail\n");
		return -1;
	}

	if (sub_dev_attr_get_appwhitelist(mac, buf, sizeof(buf)) != ALINKGW_OK) {
		ALI_DBG("appwhitelist get fail\n");
		return -1;
	}
	ptr = strchr(buf, '[');
	while (ptr) {
		ptr++;
		ptr = strchr(ptr, '"');
		if (!ptr)
			break;
		ptr++;
		info.appid = atoi(ptr);
		if (!info.appid)
			break;
		ALI_DBG("appwhite:%d\n", info.appid);
		if (mu_msg(IGD_HOST_SET_APP_WHITE, &info, sizeof(info), NULL, 0)) {
			ALI_DBG("appwhite add fail, %d\n", info.appid);
			break;
		}
		ptr = strchr(ptr, ',');
	}

	if (sub_dev_attr_get_webwhitelist(mac, buf, sizeof(buf)) != ALINKGW_OK) {
		ALI_DBG("webwhitelist get fail\n");
		return -1;
	}

	info.act = NLK_ACTION_ADD;
	ptr = strchr(buf, '"');
	while (ptr) {
		ptr++;
		ptr += arr_strcpy_end(url->url, ptr, '"');
		ptr++;
		ALI_DBG("[%s]\n", url->url);
		if (mu_msg(IGD_HOST_URL_ACTION, &info, sizeof(info), NULL, 0)) {
			ALI_DBG("appwhite add fail\n");
			break;
		}
		ptr = strchr(ptr, '"');
	}
	return 0;
}

int sub_dev_attr_get_appwhitelist(const char *mac, char *buff, unsigned int buf_sz)
{
	int num;
	char tmp[32], str[512], **val;

	ali_mac2mac(mac, tmp);
	snprintf(str, sizeof(str), "aliconf.appwhitelist.%s", tmp);

	if (uuci_get(str, &val, &num)) {
		snprintf(buff, buf_sz, "[]");
	} else {
		if (buf_sz < strlen(val[0])) {
			uuci_get_free(val, num);
			ALI_DBG("buf_sz:%d\n", buf_sz);
			return ALINKGW_BUFFER_INSUFFICENT;
		}
		snprintf(buff, buf_sz, "%s", val[0]);
		uuci_get_free(val, num);
	}
	ALI_DBG("%s, %s\n", mac, buff);
	return ALINKGW_OK;
}

int sub_dev_attr_set_appwhitelist(const char *mac, const char *json_in)
{
	int ret = ALINKGW_ERR, len = strlen(json_in) + 128;
	char tmp[32], *str;

	ALI_DBG("%s, %s\n", mac, json_in);

	ali_mac2mac(mac, tmp);
	str = malloc(len);
	if (!str)
		return ALINKGW_ERR;
	snprintf(str, len, "aliconf.appwhitelist.%s=%s", tmp, json_in);
	uuci_set("aliconf.appwhitelist=info");
	if (!uuci_set(str))
		ret = ALINKGW_OK;
	free(str);
	if (sub_dev_enable(mac))
		return ALINKGW_ERR;
	ALINKGW_report_attr_direct(ALINKGW_SUBDEV_ATTR_WEBWHITELIST, ALINKGW_ATTRIBUTE_array, json_in);
	return ret;
}

int sub_dev_attr_get_webwhitelist(const char *mac, char *buff, unsigned int buf_sz)
{
	int num;
	char tmp[32], str[512], **val;

	ali_mac2mac(mac, tmp);
	snprintf(str, sizeof(str), "aliconf.webwhitelist.%s", tmp);

	if (uuci_get(str, &val, &num)) {
		snprintf(buff, buf_sz, "[]");
	} else {
		if (buf_sz < strlen(val[0])) {
			uuci_get_free(val, num);
			return ALINKGW_BUFFER_INSUFFICENT;
		}
		snprintf(buff, buf_sz, "%s", val[0]);
		uuci_get_free(val, num);
	}
	ALI_DBG("%s, %s\n", mac, buff);
	return ALINKGW_OK;
}

int sub_dev_attr_set_webwhitelist(const char *mac, const char *json_in)
{
	int ret = ALINKGW_ERR, len = strlen(json_in) + 128;
	char tmp[32], *str;

	ALI_DBG("%s, %s\n", mac, json_in);

	ali_mac2mac(mac, tmp);
	str = malloc(len);
	if (!str)
		return ALINKGW_ERR;
	snprintf(str, len, "aliconf.webwhitelist.%s=%s", tmp, json_in);
	uuci_set("aliconf.webwhitelist=info");
	if (!uuci_set(str))
		ret = ALINKGW_OK;
	free(str);
	if (sub_dev_enable(mac))
		return ALINKGW_ERR;
	ALINKGW_report_attr_direct(ALINKGW_SUBDEV_ATTR_WEBWHITELIST, ALINKGW_ATTRIBUTE_array, json_in);
	return ret;
}

int sub_dev_attr_get_internetLimitedSwitchState(const char *mac, char *buff, unsigned int buf_sz)
{
	int num;
	char tmp[32], str[512], **val;

	ali_mac2mac(mac, tmp);
	snprintf(str, sizeof(str), "aliconf.submode.%s", tmp);

	if (uuci_get(str, &val, &num)) {
		snprintf(buff, buf_sz, "0");
	} else {
		if (buf_sz < strlen(val[0])) {
			uuci_get_free(val, num);
			return ALINKGW_BUFFER_INSUFFICENT;
		}
		snprintf(buff, buf_sz, "%s", val[0]);
		uuci_get_free(val, num);
	}
	ALI_DBG("%s, %s\n", mac, buff);
	return ALINKGW_OK;
}

int sub_dev_attr_set_internetLimitedSwitchState(const char *mac, const char *json_in)
{
	int ret = ALINKGW_ERR, len = strlen(json_in) + 128;
	char tmp[32], *str;

	ALI_DBG("%s, %s\n", mac, json_in);

	ali_mac2mac(mac, tmp);
	str = malloc(len);
	if (!str)
		return ALINKGW_ERR;
	snprintf(str, len, "aliconf.submode.%s=%s", tmp, json_in);
	uuci_set("aliconf.submode=info");
	if (!uuci_set(str))
		ret = ALINKGW_OK;
	free(str);

	sub_dev_enable(mac);
	ALINKGW_report_attr_direct(ALINKGW_SUBDEV_ATTR_INTERNETLIMITEDSWITCHSTATE, ALINKGW_ATTRIBUTE_simple, json_in);
	return ret;
}

/*子设备属性列表*/
ALINKGW_subdevice_attribute_handler_t alinkgw_subdevice_attribute_handlers[256] = 
{
	{ALINKGW_SUBDEV_ATTR_DLSPEED, ALINKGW_ATTRIBUTE_simple, sub_dev_attr_get_dlSpeed, NULL},
	{ALINKGW_SUBDEV_ATTR_ULSPEED, ALINKGW_ATTRIBUTE_simple, sub_dev_attr_get_ulSpeed, NULL},
	{ALINKGW_SUBDEV_ATTR_DLBYTES, ALINKGW_ATTRIBUTE_simple, sub_dev_attr_get_dlBytes, NULL},
	{ALINKGW_SUBDEV_ATTR_ULBYTES, ALINKGW_ATTRIBUTE_simple, sub_dev_attr_get_ulBytes, NULL},
	{ALINKGW_SUBDEV_ATTR_INTERNETSWITCHSTATE, ALINKGW_ATTRIBUTE_simple, sub_dev_attr_get_internetSwitchState, sub_dev_attr_set_internetSwitchState},
	{ALINKGW_SUBDEV_ATTR_IPADDRESS, ALINKGW_ATTRIBUTE_simple, sub_dev_attr_get_ipAddress, NULL},
	{ALINKGW_SUBDEV_ATTR_IPV6ADDRESS, ALINKGW_ATTRIBUTE_simple, sub_dev_attr_get_ipv6Address, NULL},
	{ALINKGW_SUBDEV_ATTR_BAND, ALINKGW_ATTRIBUTE_simple, sub_dev_attr_get_band, NULL},
	{ALINKGW_SUBDEV_ATTR_APPINFO, ALINKGW_ATTRIBUTE_array, sub_dev_attr_get_appInfo, sub_dev_attr_set_appInfo},
	{ALINKGW_SUBDEV_ATTR_APPRECORD, ALINKGW_ATTRIBUTE_array, sub_dev_attr_get_appRecord, NULL},
	{ALINKGW_SUBDEV_ATTR_MAXDLSPEED, ALINKGW_ATTRIBUTE_simple, sub_dev_attr_get_maxDlSpeed, sub_dev_attr_set_maxDlSpeed},
	{ALINKGW_SUBDEV_ATTR_MAXULSPEED, ALINKGW_ATTRIBUTE_simple, sub_dev_attr_get_maxUlSpeed, sub_dev_attr_set_maxUlSpeed},

	{ALINKGW_SUBDEV_ATTR_APPBLACKLIST, ALINKGW_ATTRIBUTE_array, sub_dev_attr_get_appblacklist, sub_dev_attr_set_appblacklist},
	{ALINKGW_SUBDEV_ATTR_APPWHITELIST, ALINKGW_ATTRIBUTE_array, sub_dev_attr_get_appwhitelist, sub_dev_attr_set_appwhitelist},
	{ALINKGW_SUBDEV_ATTR_WEBWHITELIST, ALINKGW_ATTRIBUTE_array, sub_dev_attr_get_webwhitelist, sub_dev_attr_set_webwhitelist},
	{ALINKGW_SUBDEV_ATTR_INTERNETLIMITEDSWITCHSTATE, ALINKGW_ATTRIBUTE_simple, sub_dev_attr_get_internetLimitedSwitchState, sub_dev_attr_set_internetLimitedSwitchState},
	{NULL, 0, NULL, NULL}
};

void connection_status_update(ALINKGW_CONN_STATUS_E status)
{
	static ALINKGW_CONN_STATUS_E last_status = ALINKGW_STATUS_INITAL;
	last_status = status;
	return;
}

#ifdef CONFIG_ALINK_LOCAL_ROUTER_OTA
//#define ALI_OTA_DBG
static void ota_processUnload(ALINKGW_OTA_StatusPost cb)
{
	post_callback(cb, 0);

	ALI_DBG("\n");

#ifndef ALI_OTA_DBG
	system("rmmod l7 &");
	system("rmmod safe &");
	system("echo 3 > /proc/sys/vm/drop_caches &");
#endif

	sleep(3);
	post_callback(cb, 100);
}

static int ota_memCheck(int fwSize, ALINKGW_OTA_StatusPost cb)
{
	int ret = ALINKGW_OK;
	struct sysinfo info;

	post_callback(cb, 0);

#ifndef ALI_OTA_DBG
	if (sysinfo(&info)) {
		ret = ALINKGW_ERR;
		ALI_DBG("sysinfo err, %s\n", strerror(errno));
	} else if (info.freeram < (fwSize + 1000)) {
		ret = ALINKGW_ERR;
		ALI_DBG("mem not enough :%d,%ld\n", fwSize, info.freeram); 
	} else {
		ALI_DBG("ok :%d,%ld\n", fwSize, info.freeram); 
	}
#endif

	post_callback(cb, 100);
	return ret;
}

static int ota_fwFlash(const char *fwName, ALINKGW_OTA_StatusPost cb)
{
    int ret = ALINKGW_OK;
    char cmd[256];

	ALI_DBG("start, [%s]\n", fwName);

    post_callback(cb, 0);

#ifndef ALI_OTA_DBG
	if (!fwName || !strlen(fwName)) {
		ret = ALINKGW_ERR;
		ALI_DBG("input err: %s\n", fwName ? : "");
	} else {
		ALI_DBG("file name: %s\n", fwName ? : "");
		snprintf(cmd, sizeof(cmd), 
			"/sbin/sysupgrade -A %s > /dev/console \n", fwName);
		if (system(cmd))
			ret = ALINKGW_ERR;
	}
#else
	sleep(20);
#endif
	
    post_callback(cb, 100);

	ALI_DBG("end, %d\n", ret);
    return ret;
}

static int ota_sysReboot(ALINKGW_OTA_StatusPost cb)
{
	ALI_DBG("\n");

    post_callback(cb, 100);

	sleep(5);
#ifndef ALI_OTA_DBG
	system("/sbin/reboot -f");
#endif

    return ALINKGW_OK;
}
#endif

void usage(void)
{
	fprintf(stderr, "usage:\n"
		"\t -run_mode [0,2]   //0:online_mode 1:sandbox_mode 2:daily_mode\n"
		"\t -loglevel [0-4]   //set alink sdk log level, default 1(error level)\n");
}


void argument_parse(int argc, const char **argv)
{
	int i = 0;
	for (i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-run_mode") == 0) {
			++i;
			g_run_mode = atoi(argv[i]);
			if ((g_run_mode > 2) || (g_run_mode < 0))
				g_run_mode = 0;
			ALI_DBG("sandbox_mode: %d\n", g_run_mode);
		} else if (strcmp(argv[i], "-loglevel") == 0) {
			++i;
			g_loglevel = atoi(argv[i]);
			ALI_DBG("loglevel: %d\n", g_loglevel);
		}
	}
}

static int ali_load_key(void)
{
	char *retstr;
	unsigned char mac[6];
	unsigned int devid;

	retstr = read_firmware("VENDOR");
	if (!retstr) {
		ALI_DBG("read vendor fail\n");
		return -1;
	}

	if (!strcmp(retstr, "CY_WiFi")) {
		retstr = read_firmware("PRODUCT");
		if (!retstr)
			return -1;

		arr_strcpy(g_szDevName, retstr);
		snprintf(g_szDevModel, sizeof(g_szDevModel), "WIAIR_NETWORKING_ROUTER_I1");

		arr_strcpy(g_szBrand, "WIAIR");
		arr_strcpy(g_szManuFacture, "CHUYUN");
		arr_strcpy(g_szKey_sandbox, "s2wOKE4h2QKTribjdvnI");
		arr_strcpy(g_szSecret_sandbox, "Q2NyUYj1EdALuv2FwF1aHOgUNwZAuNNkbQKMvoAE");
		arr_strcpy(g_szKey, "nSVGe6NZVEkFvVxUPn8C");
		arr_strcpy(g_szSecret, "LrErst4faLnxHX3dnIK14oh7lf9GOxbkTHXMgxrW");
	} else if (!strcmp(retstr, "BLINK")) {
		arr_strcpy(g_szBrand, "B-LINK");
		arr_strcpy(g_szManuFacture, "B-LINK");

		g_szDevName[0] = 0;
		retstr = read_firmware("devname");
		if (retstr)
			arr_strcpy(g_szDevName, retstr);

		retstr = read_firmware("PRODUCT");
		if (!retstr)
			return -1;

		if (!g_szDevName[0])
			arr_strcpy(g_szDevName, retstr);
		snprintf(g_szDevModel, sizeof(g_szDevModel), "BLINK_NETWORKING_ROUTER_%s", retstr);

		if (!strcmp(retstr, "WR4000")) {
			arr_strcpy(g_szKey_sandbox, "UM6J19eO2JGn2YQYUDTC");
			arr_strcpy(g_szSecret_sandbox, "EnxInoldpKUN1FIvKhILxtkWpW3wmCU5xeCIxV4E");
			arr_strcpy(g_szKey, "scc4sIeonlz1Bh125ymN");
			arr_strcpy(g_szSecret, "KEBRElN3vDzY2cKWudNb7FxrWa3MZ1OuMN8da3rb");
		} else if (!strcmp(retstr, "D9103")) {
			arr_strcpy(g_szKey_sandbox, "RQCpvrnFiAUDyAxfU70m");
			arr_strcpy(g_szSecret_sandbox, "0wPlvdR3PO5ooP1gf3xhhc2r099zpiYwr392xdqn");
			arr_strcpy(g_szKey, "El3yVcaQwvaFcrRguEF7");
			arr_strcpy(g_szSecret, "riJfMmH2Uw6nHzlTL0z6wp8JIxKkPWtf716ZRhMB");
		} else if (!strcmp(retstr, "AC886")) {
			arr_strcpy(g_szKey_sandbox, "ExqXGfvwZ4o3tC4wAqvJ");
			arr_strcpy(g_szSecret_sandbox, "n2Emg0Nv6BkWWEnfdEyg7Um9sbnlW2fNubjyCffy");
			arr_strcpy(g_szKey, "XXEr6ksUUE8Ixohhqhbu");
			arr_strcpy(g_szSecret, "K0D89s3MfMc90rlaOZd3Eff3YSHlrnXTAla0VSpm");
		} else if (!strcmp(retstr, "WR2000")) {
			arr_strcpy(g_szKey_sandbox, "ydcC4SyW0RU41q5tOQhV");
			arr_strcpy(g_szSecret_sandbox, "DSX5X4y5f98YgaMLic65rQZeQ1c1QdlZgmyewcYg");
			arr_strcpy(g_szKey, "bGoums6MG6nBbhLtOAYS");
			arr_strcpy(g_szSecret, "ZtI9aHs6xiVS5g0dOhef9xf483zWHIFkIo5cSkEw");
		} else if (!strcmp(retstr, "WR9000")) {
			arr_strcpy(g_szKey_sandbox, "y5a9V5JJq7shPv7Vkop2");
			arr_strcpy(g_szSecret_sandbox, "lTLSf01DDdn7oy7ZhargOFu1SJuxbgT9a1SwmqzW");
			arr_strcpy(g_szKey, "NUAo9HGGlXKIYuqYcu8Z");
			arr_strcpy(g_szSecret, "3X5UHvWQdk9cqdrGPZ4xNM6viDdv3kGp6HTJJIKp");
		} else if (!strcmp(retstr, "BL360")) {
			arr_strcpy(g_szDevName, "BL-360");
			arr_strcpy(g_szKey_sandbox, "H94jltm0mlQ6emzPZa7A");
			arr_strcpy(g_szSecret_sandbox, "KSfiAiN1qY5ekjYBHa9kl36UqqGfOCYrqfWGOEQa");
			arr_strcpy(g_szKey, "kLn26maai7p0Wl1QWmeU");
			arr_strcpy(g_szSecret, "4S6gaV0pNTwPvoGGcQbeoO7cnKd4PYNoUr5EK5Io");
		} else if (!strcmp(retstr, "BL360_V2")) {
			arr_strcpy(g_szDevName, "BL-360U");
			arr_strcpy(g_szDevModel, "BLINK_NETWORKING_ROUTER_BL_360U");
			arr_strcpy(g_szKey_sandbox, "ExqXGfvwZ4o3tC4wAqvJ");
			arr_strcpy(g_szSecret_sandbox, "n2Emg0Nv6BkWWEnfdEyg7Um9sbn1W2fNubjyCffy");
			arr_strcpy(g_szKey, "9nJhxXD5qNnZhXiWL24u");
			arr_strcpy(g_szSecret, "mVWvAt8yDVUvdtsEmXbD3ZfSpYicjRpAFHhe2iJs");
		} else if (!strcmp(retstr, "BL-WR361")) {
			arr_strcpy(g_szDevName, "BL-WR361");
			arr_strcpy(g_szDevModel, "BLINK_NETWORKING_ROUTER_BL_WR361");
			arr_strcpy(g_szKey_sandbox, "ExqXGfvwZ4o3tC4wAqvJ");
			arr_strcpy(g_szSecret_sandbox, "n2Emg0Nv6BkWWEnfdEyg7Um9sbnlW2fNubjyCffy");
			arr_strcpy(g_szKey, "cT5JdV9EwWftn3G1QuBE");
			arr_strcpy(g_szSecret, "LeupJW3DTFWQbLiarOVW2UJWRy9VB4xYOR4rBlNu");
		} else if (!strcmp(retstr, "D9103_V2")) {
			arr_strcpy(g_szDevName, "D9103");
			arr_strcpy(g_szDevModel, "BLINK_NETWORKING_ROUTER_D9103");
			arr_strcpy(g_szKey_sandbox, "ExqXGfvwZ4o3tC4wAqvJ");
			arr_strcpy(g_szSecret_sandbox, "n2Emg0Nv6BkWWEnfdEyg7Um9sbnlW2fNubjyCffy");
			arr_strcpy(g_szKey, "El3yVcaQwvaFcrRguEF7");
			arr_strcpy(g_szSecret, "riJfMmH2Uw6nHzlTL0z6wp8JIxKkPWtf716ZRhMB");
		} else if (!strcmp(retstr, "WR4000_V2")) {
			arr_strcpy(g_szDevName, "WR4000");
			arr_strcpy(g_szDevModel, "BLINK_NETWORKING_ROUTER_WR4000");
			arr_strcpy(g_szKey_sandbox, "ExqXGfvwZ4o3tC4wAqvJ");
			arr_strcpy(g_szSecret_sandbox, "n2Emg0Nv6BkWWEnfdEyg7Um9sbnlW2fNubjyCffy");
			arr_strcpy(g_szKey, "scc4sIeonlz1Bh125ymN");
			arr_strcpy(g_szSecret, "KEBRElN3vDzY2cKWudNb7FxrWa3MZ1OuMN8da3rb");
		} else if (!strcmp(retstr, "AC1200")) {
			arr_strcpy(g_szKey_sandbox, "ExqXGfvwZ4o3tC4wAqvJ");
			arr_strcpy(g_szSecret_sandbox, "n2Emg0Nv6BkWWEnfdEyg7Um9sbnlW2fNubjyCffy");
			arr_strcpy(g_szKey, "7kWpnxHyzQEG715PGPMR");
			arr_strcpy(g_szSecret, "Jt2yoGZT9C70dEXbkfCNnIJgRHBQCsMYzzAEoV4U");
		} else if (!strcmp(retstr, "WR88N")) {
			arr_strcpy(g_szDevName, "WR88N");
			arr_strcpy(g_szDevModel, "BLINK_NETWORKING_ROUTER_WR88N");
			arr_strcpy(g_szKey_sandbox, "ExqXGfvwZ4o3tC4wAqvJ");
			arr_strcpy(g_szSecret_sandbox, "n2Emg0Nv6BkWWEnfdEyg7Um9sbnlW2fNubjyCffy");
			arr_strcpy(g_szKey, "CKR3OOT0ULhPUqxIiXkw");
			arr_strcpy(g_szSecret, "a3ZFpZqIJp2CINYQljA6gvk0T54yrUbg68UnfSP3");
		} else if (!strcmp(retstr, "1200U_V2")) {
			arr_strcpy(g_szKey_sandbox, "ExqXGfvwZ4o3tC4wAqvJ");
			arr_strcpy(g_szSecret_sandbox, "n2Emg0Nv6BkWWEnfdEyg7Um9sbnlW2fNubjyCffy");
			arr_strcpy(g_szKey, "7kWpnxHyzQEG715PGPMR");
			arr_strcpy(g_szSecret, "Jt2yoGZT9C70dEXbkfCNnIJgRHBQCsMYzzAEoV4U");
		} else if (!strcmp(retstr, "WR308")) {
			arr_strcpy(g_szDevModel, "BLINK_NETWORKING_ROUTER_BL_WR308");
			arr_strcpy(g_szKey_sandbox, "ExqXGfvwZ4o3tC4wAqvJ");
			arr_strcpy(g_szSecret_sandbox, "n2Emg0Nv6BkWWEnfdEyg7Um9sbnlW2fNubjyCffy");
			arr_strcpy(g_szKey, "0B12aT0LQUh6pc6nSiQi");
			arr_strcpy(g_szSecret, "7nDcop94fDsinGWAjeXYfV1Ai65FSXszV5bahlZn");
		} else {
			retstr = read_firmware("szKey");
			if (!retstr) {
				ALI_DBG("read szKey fail\n");
				return -1;
			}
			arr_strcpy(g_szKey, retstr);

			retstr = read_firmware("szSecret");
			if (!retstr) {
				ALI_DBG("read szSecret fail\n");
				return -1;
			}
			arr_strcpy(g_szSecret, retstr);
		}
	} else if (!strcmp(retstr, "ZXHN")) {
		arr_strcpy(g_szBrand, "ZXHN");
		arr_strcpy(g_szManuFacture, "ZXHN");
	
		retstr = read_firmware("PRODUCT");
		if (!retstr)
			return -1;
		arr_strcpy(g_szDevName, retstr);
	
		if (!strcmp(retstr, "ER300")) {
			arr_strcpy(g_szDevModel, "ZTE_NETWORKING_ROUTER_ZXHN_ER300");
			arr_strcpy(g_szKey_sandbox, "vKLseP6FuSN3jiiwSfPF");
			arr_strcpy(g_szSecret_sandbox, "sA2l9GpJXFGrrSczwI4eVAosvFiCesLQOTb8r8Ht");
			arr_strcpy(g_szKey, "Eug3VyKqFIQxefj50Lnl");
			arr_strcpy(g_szSecret, "O17flaaYD2n85TM6lwgUGMOkllfQCFo4F5sT7JAs");
		}
	} else if (!strcmp(retstr, "GAOKE")) {
		arr_strcpy(g_szBrand, "GAOKE");
		arr_strcpy(g_szManuFacture, "GAOKE");
	
		retstr = read_firmware("PRODUCT");
		if (!retstr)
			return -1;
		arr_strcpy(g_szDevName, retstr);
	
		if (!strcmp(retstr, "Q339")) {
			arr_strcpy(g_szDevModel, "GAOKE_NETWORKING_ROUTER_Q339");
			arr_strcpy(g_szKey_sandbox, "HRNRuhp3SjraHSDGWRoM");
			arr_strcpy(g_szSecret_sandbox, "NuKt9UdG7kgnvSGQd3yrGJ2kuL1uUqgVLlJu01v1");
			arr_strcpy(g_szKey, "UCb58ZnZ6PhGiR4XpL4Q");
			arr_strcpy(g_szSecret, "FqY61KcJC1lpFXcJwpBLkgKqBCmIYN8npJfXKlUP");
		} else if (!strcmp(retstr, "W307")) {
			arr_strcpy(g_szDevModel, "GAOKE_NETWORKING_ROUTER_Q331");
			arr_strcpy(g_szKey_sandbox, "D8tyKCMKqPdchgV0QYNT");
			arr_strcpy(g_szSecret_sandbox, "nw5iBlgy0sT0wbeDCbSPgF1UprLwWpvn9Bq8HZWX");
			arr_strcpy(g_szKey, "v7hJsEZOnUXkIu6zveX7");
			arr_strcpy(g_szSecret, "3CNURNWInDKQXkyfBJMsqIVfdmAQWZ7JpaDCVzB8");
		} else if (!strcmp(retstr, "Q307R")) {
			arr_strcpy(g_szDevModel, "GAOKE_NETWORKING_ROUTER_Q307R");
			arr_strcpy(g_szKey_sandbox, "mNHaozGJ4fNgW0uVHBaW");
			arr_strcpy(g_szSecret_sandbox, "PcuEtlNjLReHHcXcF3zLJmK5EBKQgzb4TjtgMtJa");
			arr_strcpy(g_szKey, "E8eUmCnWNfgNUaB2KpLT");
			arr_strcpy(g_szSecret, "q2o92XUpPaZvLBbFmmi4I6BSvqcxInqNtMWEMfRC");
		} else if (!strcmp(retstr, "Q363")) {
			arr_strcpy(g_szDevModel, "GAOKE_NETWORKING_ROUTER_Q363");
			arr_strcpy(g_szKey_sandbox, "Qu9PRxLlmL5UpIXvbJEM");
			arr_strcpy(g_szSecret_sandbox, "IcAepp1kpPR9WsWAM2kjfkPemNHhyCkCPJcF6HPc");
			arr_strcpy(g_szKey, "0lDY6t38kgP0b7QzPkLi");
			arr_strcpy(g_szSecret, "cLw0ZYaZcQTx7PoW2ZUZHRoPkdfLFky4669XocgX");
		} else if (!strcmp(retstr, "Q338") ||
			!strcmp(retstr, "W300") || !strcmp(retstr, "W311")) {
			arr_strcpy(g_szDevModel, "GAOKE_NETWORKING_ROUTER_Q338");
			arr_strcpy(g_szKey_sandbox, "Qu9PRxLlmL5UpIXvbJEM");
			arr_strcpy(g_szSecret_sandbox, "IcAepp1kpPR9WsWAM2kjfkPemNHhyCkCPJcF6HPc");
			arr_strcpy(g_szKey, "sEpzbEFPQVG06oHHoNYY");
			arr_strcpy(g_szSecret, "P5S5rAodlanfbEo6BFpsgZCVANOgmh0P65Yv8HsK");
		} else if (!strcmp(retstr, "W305")) {
			arr_strcpy(g_szDevModel, "GAOKE_NETWORKING_ROUTER_Q339");
			arr_strcpy(g_szKey_sandbox, "HRNRuhp3SjraHSDGWRoM");
			arr_strcpy(g_szSecret_sandbox, "NuKt9UdG7kgnvSGQd3yrGJ2kuL1uUqgVLlJu01v1");
			arr_strcpy(g_szKey, "UCb58ZnZ6PhGiR4XpL4Q");
			arr_strcpy(g_szSecret, "FqY61KcJC1lpFXcJwpBLkgKqBCmIYN8npJfXKlUP");
		} else if (!strcmp(retstr, "Q330")) {
			arr_strcpy(g_szDevModel, "GAOKE_NETWORKING_ROUTER_Q330");
			arr_strcpy(g_szKey_sandbox, "05qFoaP998kq4b54DhmE");
			arr_strcpy(g_szSecret_sandbox, "3eeLojibV0bPjygxcKcoltxFJvdhKqLlni7wl5G0");
			arr_strcpy(g_szKey, "WW3nz8JKHOFlbGN9UkKk");
			arr_strcpy(g_szSecret, "8a4R8tzgXzMuN265gpCn4J20693kCDHQ6WfB6x24");
		} else if (!strcmp(retstr, "W310")) {
			arr_strcpy(g_szDevModel, "GAOKE_NETWORKING_ROUTER_Q307R");
			arr_strcpy(g_szKey_sandbox, "mNHaozGJ4fNgW0uVHBaW");
			arr_strcpy(g_szSecret_sandbox, "PcuEtlNjLReHHcXcF3zLJmK5EBKQgzb4TjtgMtJa");
			arr_strcpy(g_szKey, "E8eUmCnWNfgNUaB2KpLT");
			arr_strcpy(g_szSecret, "q2o92XUpPaZvLBbFmmi4I6BSvqcxInqNtMWEMfRC");
		}
	} else if (!strcmp(retstr, "HAIER")) {
		arr_strcpy(g_szBrand, "HAIER");
		arr_strcpy(g_szManuFacture, "HAIER");
		
		retstr = read_firmware("PRODUCT");
		if (!retstr)
			return -1;
		arr_strcpy(g_szDevName, retstr);
		if (!strcmp(retstr, "RT-M332")) {
			arr_strcpy(g_szDevModel, "SUZHOU_NETWORKING_ROUTER_RT_M332_A");
			arr_strcpy(g_szKey_sandbox, "8XHrUMqx3yHgR5f70kfz");
			arr_strcpy(g_szSecret_sandbox, "0Y1mPpZZ1CRtOQpUjORr1ojrQoAKi7QsH2EoBGbw");
			arr_strcpy(g_szKey, "DauJ6sIUpEsGC8OZuXY8");
			arr_strcpy(g_szSecret, "3KGcm8sizylPBJQWyguWjkcH8v20CYwvNensvyXj");
		}
	} else if (!strcmp(retstr, "MIDEA")) {
		arr_strcpy(g_szBrand, "MIDEA");
		arr_strcpy(g_szManuFacture, "MIDEA");

		retstr = read_firmware("PRODUCT");
		if (!retstr)
			return -1;
		arr_strcpy(g_szDevName, retstr);
		if (!strcmp(retstr, "M1")) {
			arr_strcpy(g_szDevModel, "MIDEA_NETWORKING_ROUTER_MSRR_R00");
			arr_strcpy(g_szKey_sandbox, "c0LSssN5Uzqd8DUJXC4G");
			arr_strcpy(g_szSecret_sandbox, "FleZ8TUGBw1J2GhUjA4n9jxBJthwRbEW5rgTyBHY");
			arr_strcpy(g_szKey, "gX2lXIePCdyxiaNYUDfW");
			arr_strcpy(g_szSecret, "jhvGfxIh5TUis87KQ6uQfqatR7EVW4UdZdA5luJe");
		}
	} else if (!strcmp(retstr, "EDUP")) {
		arr_strcpy(g_szBrand, "EDUP");
		arr_strcpy(g_szManuFacture, "EDUP");

		retstr = read_firmware("PRODUCT");
		if (!retstr)
			return -1;
		arr_strcpy(g_szDevName, retstr);
		if (!strcmp(retstr, "RT2638C")) {
			arr_strcpy(g_szDevModel, "XILETAO_NETWORKING_ROUTER_EP_RT2638");
			arr_strcpy(g_szKey_sandbox, "You0J47aWZY2yhmeNIUF");
			arr_strcpy(g_szSecret_sandbox, "DUF20Un6fRc0MCb6BXUsGY1DbBJtjSGH6K0FCO5s");
			arr_strcpy(g_szKey, "x5RO4KHiRked4hVrkKm3");
			arr_strcpy(g_szSecret, "UEm2ksK8UcmBkSuVomeYcJmOt6IAe4vlIPtzcnCe");
		}
	} else if (!strcmp(retstr, "WAVLINK")) {
		arr_strcpy(g_szBrand, "WAVLINK");
		arr_strcpy(g_szManuFacture, "WAVLINK");

		retstr = read_firmware("PRODUCT");
		if (!retstr)
			return -1;
		arr_strcpy(g_szDevName, retstr);
		if (!strcmp(retstr, "N300")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_WN529N2P");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "954GpvfPDb62a3VUSTh7");
			arr_strcpy(g_szSecret, "sPpR4iz3yR4g1liZMVKBR1GFi1ZW6z3Aajm4znoi");
		} else if (!strcmp(retstr, "S33")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_S33");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "Cgcq4yUYjXxfUUJTxTVZ");
			arr_strcpy(g_szSecret, "EyZQDyY7bREue2yOUnxlmBSKAAc4Qw2DIYdR6LED");
		} else if (!strcmp(retstr, "WN529N2P")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_WN529N2P");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "954GpvfPDb62a3VUSTh7");
			arr_strcpy(g_szSecret, "sPpR4iz3yR4g1liZMVKBR1GFi1ZW6z3Aajm4znoi");
		} else if (!strcmp(retstr, "529N2A")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_WN529N2P");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "954GpvfPDb62a3VUSTh7");
			arr_strcpy(g_szSecret, "sPpR4iz3yR4g1liZMVKBR1GFi1ZW6z3Aajm4znoi");
		} else if (!strcmp(retstr, "H1")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_H1");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "iRA6oIXzNvRXxyZwZclC");
			arr_strcpy(g_szSecret, "rNHzANBxGgBrPbJtH2KDeE8N0BOM5BPbAeZK47zs");
		} else if (!strcmp(retstr, "521N2A")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_WN521N2A");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "zjNaVsEoOKdPI96jxkTj");
			arr_strcpy(g_szSecret, "Fugia1WuSqu6re40FJcCJUB28nrYBtsJwIjL1q6N");
		} else if (!strcmp(retstr, "WN529") \
			|| !strcmp(retstr, "529_V1") || !strcmp(retstr, "529_V2")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_WN529N2P");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "954GpvfPDb62a3VUSTh7");
			arr_strcpy(g_szSecret, "sPpR4iz3yR4g1liZMVKBR1GFi1ZW6z3Aajm4znoi");
		} else if (!strcmp(retstr, "WN532A2")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_WN532A2");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "hf7rA2FD2tpqZrmyw4FA");
			arr_strcpy(g_szSecret, "agyDlPL9JPtrnvGFsov3blTxjWP70sVMX6hHTmd7");
		} else if (!strcmp(retstr, "S31")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_S31");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "oIVcJ6flZnXCOX7LjaHD");
			arr_strcpy(g_szSecret, "gR5apQwNVUrqdNmSXU09AiT3O1f3FeN4rbN3Nvp4");
		} else if (!strcmp(retstr, "WN578R2")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_WN578R2");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "g8rphj6Xljcr5rXxvmuu");
			arr_strcpy(g_szSecret, "tG9nqnqYHOxbtF4EbQ5Vu0YNehiRdiekcAHdbbfl");
		} else if (!strcmp(retstr, "WN532A3")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_WN532A3");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "wtm1cTFoKY5VGMinmuHE");
			arr_strcpy(g_szSecret, "UA59B5eSlI3j6RThOF0HWRnAFez8xm33X6XqOf9r");
		} else if (!strcmp(retstr, "WN529B3")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_WN529B3");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "XJUoVxi59KIeX7T9fsVf");
			arr_strcpy(g_szSecret, "Itm2cHPf4RvLdx8QbVYOpifLhGw5xitxh9doWcIs");
		}
	} else if (!strcmp(retstr, "ANTBANG")) {
		arr_strcpy(g_szBrand, "ANTBANG");
		arr_strcpy(g_szManuFacture, "ANTBANG");
	
		retstr = read_firmware("PRODUCT");
		if (!retstr)
			return -1;
		arr_strcpy(g_szDevName, retstr);
		if (!strcmp(retstr, "A3")) {
			arr_strcpy(g_szDevModel, "ANTBANG_NETWORKING_ROUTER_A3");
			arr_strcpy(g_szKey_sandbox, "j8HaNXjZHLDEhEz7xE7i");
			arr_strcpy(g_szSecret_sandbox, "B53kZwVqVPb7sluU3FxUJIxkikH7qRdOhn6HqTNd");
			arr_strcpy(g_szKey, "EAdglLaPt9LQnu2SUOfo");
			arr_strcpy(g_szSecret, "wag39oYAM8gHNSB6tnAztmNFyFKcHD2NwTEpZyom");
		} else if (!strcmp(retstr, "A5")) {
			arr_strcpy(g_szDevModel, "ANTBANG_NETWORKING_ROUTER_A5");
			arr_strcpy(g_szKey_sandbox, "j8HaNXjZHLDEhEz7xE7i");
			arr_strcpy(g_szSecret_sandbox, "B53kZwVqVPb7sluU3FxUJIxkikH7qRdOhn6HqTNd");
			arr_strcpy(g_szKey, "1fyP96MrJ4oIN7apeRDI");
			arr_strcpy(g_szSecret, "skrwGdL2d4sxYH3kJOi61RiNA8cwEa0vxqWjaug1");
		} else if (!strcmp(retstr, "A6S")) {
			arr_strcpy(g_szDevModel, "ANTBANG_NETWORKING_ROUTER_A6S");
			arr_strcpy(g_szKey_sandbox, "j8HaNXjZHLDEhEz7xE7i");
			arr_strcpy(g_szSecret_sandbox, "B53kZwVqVPb7sluU3FxUJIxkikH7qRdOhn6HqTNd");
			arr_strcpy(g_szKey, "EPpAl93r3tCxkRWrlgSN");
			arr_strcpy(g_szSecret, "RciVKixZwtiswSX0YwY0I1HsnaANdeG0etbv0uaM");
		} else if(!strcmp(retstr, "A3_V2")){
			arr_strcpy(g_szDevModel, "ANTBANG_NETWORKING_ROUTER_A3");
			arr_strcpy(g_szKey_sandbox, "j8HaNXjZHLDEhEz7xE7i");
			arr_strcpy(g_szSecret_sandbox, "B53kZwVqVPb7sluU3FxUJIxkikH7qRdOhn6HqTNd");
			arr_strcpy(g_szKey, "EAdglLaPt9LQnu2SUOfo");
			arr_strcpy(g_szSecret, "wag39oYAM8gHNSB6tnAztmNFyFKcHD2NwTEpZyom");
		} else if(!strcmp(retstr, "A3S_V2")){
			arr_strcpy(g_szDevModel, "ANTBANG_NETWORKING_ROUTER_A3S");
			arr_strcpy(g_szKey_sandbox, "j8HaNXjZHLDEhEz7xE7i");
			arr_strcpy(g_szSecret_sandbox, "B53kZwVqVPb7sluU3FxUJIxkikH7qRdOhn6HqTNd");
			arr_strcpy(g_szKey, "xzhzLifezxVPcI54HFhU");
			arr_strcpy(g_szSecret, "TsOi6amF0kStYlcjLJv19UJh5bHSwqkIqYTepQKB");
		} else if (!strcmp(retstr, "A3S")) {
			arr_strcpy(g_szDevModel, "ANTBANG_NETWORKING_ROUTER_A3S");
			arr_strcpy(g_szKey_sandbox, "j8HaNXjZHLDEhEz7xE7i");
			arr_strcpy(g_szSecret_sandbox, "B53kZwVqVPb7sluU3FxUJIxkikH7qRdOhn6HqTNd");
			arr_strcpy(g_szKey, "xzhzLifezxVPcI54HFhU");
			arr_strcpy(g_szSecret, "TsOi6amF0kStYlcjLJv19UJh5bHSwqkIqYTepQKB");
		} else if (!strcmp(retstr, "A4")) {
			arr_strcpy(g_szDevModel, "ANTBANG_NETWORKING_ROUTER_A4");
			arr_strcpy(g_szKey_sandbox, "j8HaNXjZHLDEhEz7xE7i");
			arr_strcpy(g_szSecret_sandbox, "B53kZwVqVPb7sluU3FxUJIxkikH7qRdOhn6HqTNd");
			arr_strcpy(g_szKey, "PHRtPoLPqEK8px9kk5A8");
			arr_strcpy(g_szSecret, "WW9Il0UyuMrNdFDkUf6JKFcHHspZknk8IeUtfxa1");
		} else if (!strcmp(retstr, "A4S")) {
			arr_strcpy(g_szDevModel, "ANTBANG_NETWORKING_ROUTER_A4S");
			arr_strcpy(g_szKey_sandbox, "j8HaNXjZHLDEhEz7xE7i");
			arr_strcpy(g_szSecret_sandbox, "B53kZwVqVPb7sluU3FxUJIxkikH7qRdOhn6HqTNd");
			arr_strcpy(g_szKey, "r9wPqzdlqJBVEPGnYgwa");
			arr_strcpy(g_szSecret, "uVl5Xfo0AhYlDekuCVvljBHtdhaJp37SCpkLPoW2");
		} else if (!strcmp(retstr, "S1")) {
			arr_strcpy(g_szDevModel, "WAVLINK_NETWORKING_ROUTER_S1");
			arr_strcpy(g_szKey_sandbox, "jZNB9paw6Vpxl2db4iX0");
			arr_strcpy(g_szSecret_sandbox, "ZlID6hnl7P7vE6IYTk4Pdfku03uuu4Y7AW9wrRyH");
			arr_strcpy(g_szKey, "TYmelH4DQ1S5rYgEv9YW");
			arr_strcpy(g_szSecret, "TsXzjOjTovw9owra6WtPhn9WQiAXw8bx1QwP8NDe");
		}
	}

	retstr = read_firmware("CURVER");
	if (!retstr) {
		ALI_DBG("read curver fail\n");
		return -1;
	}
	snprintf(g_szDevVersion, sizeof(g_szDevVersion), "%s", retstr);

	if (read_mac(mac)) {
		ALI_DBG("read mac fail\n");
		return -1;
	}

	while(1) {
		devid = get_devid();
		if (!devid) {
			ALI_DBG("devid read fail\n");
			sleep(6);
			continue;
		}
		break;
	}
	snprintf(g_szCid, sizeof(g_szCid), "%u_%02X%02X%02X%02X%02X%02X",
		devid, MAC_SPLIT(mac));

	snprintf(g_szDefaultMac, sizeof(g_szDefaultMac),
		"%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(mac));

	printf("g_szCid:%s\n", g_szCid);
	printf("g_szBrand:%s\n", g_szBrand);
	printf("g_szManuFacture:%s\n", g_szManuFacture);
	printf("g_szDevName:%s\n", g_szDevName);
	printf("g_szDevModel:%s\n", g_szDevModel);
	printf("g_szKey_sandbox:%s\n", g_szKey_sandbox);
	printf("g_szSecret_sandbox:%s\n", g_szSecret_sandbox);
	printf("g_szKey:%s\n", g_szKey);
	printf("g_szSecret:%s\n", g_szSecret);
	printf("g_szDefaultMac:%s\n", g_szDefaultMac);
	return 0;
}

static int ali_config_init(void)
{	
	int nr = 0;
	struct uci_element *se, *oe;
	struct uci_option *option;
	struct uci_section *section;
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;

	pthread_mutex_lock(&ali_mutex);
	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, ALI_CONFIG, &pkg)) {
		uci_free_context(ctx);
		pthread_mutex_unlock(&ali_mutex); 
		return -1;
	}
	uci_foreach_element(&pkg->sections, se) {
		section = uci_to_section(se);
		if (section == NULL)
			continue;
		if (!strcmp(section->type, "system") && !strcmp(se->name, "login")) {
			uci_foreach_element(&section->options, oe) {
				option = uci_to_option(oe);
				if (!option)
					continue;
				if (option->type != UCI_TYPE_STRING)
					continue;
				if (!strcmp(oe->name, "password")) {
					arr_strcpy(g_szPassWd, option->v.string);
					break;
				}
			}
		} else if (!strcmp(section->type, "probe")) {
			uci_foreach_element(&section->options, oe) {
				option = uci_to_option(oe);
				if (!option)
					continue;
				if (option->type != UCI_TYPE_STRING)
					continue;
				if (!strcmp(oe->name, "on"))
					g_probe_state = !!atoi(option->v.string);
			}
		} else if (!strcmp(section->type, "attack")) {
			uci_foreach_element(&section->options, oe) {
				option = uci_to_option(oe);
				if (!option)
					continue;
				if (option->type != UCI_TYPE_STRING)
					continue;
				if (!strcmp(oe->name, "on"))
					g_attack_state = !!atoi(option->v.string);
				else if (!strcmp(oe->name, "bw"))
					g_bw_attack_state = !!atoi(option->v.string);
				else if (!strcmp(oe->name, "admin"))
					g_admin_attack_state = !!atoi(option->v.string);
			}
		} else if (!strcmp(section->type, "nat")) {
			ali_nat_load(section);
		}
	}
	//tpskl.nr = nr;
	uci_unload(ctx, pkg);
	uci_free_context(ctx);
	pthread_mutex_unlock(&ali_mutex);

	while(1) {
		if (!ali_load_key())
			break;
		sleep(3);
		ALI_DBG("load key fail\n");
	}
	return 0;
}

static int check_is_reset(void)
{
	int r = 0;

	ALI_DBG("\n");

	r = get_sysflag(SYSFLAG_ALIRESET);
	if (r < 0) {
		ALI_DBG("get sysflag fail, sleep 6\n");
		sleep(6);
		r = get_sysflag(SYSFLAG_ALIRESET);
		if (r < 0) {
			ALI_DBG("get sysflag fail, return\n");
			return -1;
		}
	}

	if (r != 1)
		return 0;

	if (ALINKGW_reset_binding() != ALINKGW_OK)
		ALI_DBG("reset binding failed\n");
	else 
		ALI_DBG("reset binding succeed\n");
	set_sysflag(SYSFLAG_ALIRESET, 0);
	return 0;
}

int ali_get_lan_ip(char *str, int len)
{
	int uid = 0;
	struct iface_info info;

	if (mu_msg(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &info, sizeof(info)))
		return -1;
	strncpy(str, inet_ntoa(info.ip), len - 1);
	str[len - 1] = '\0';
	ALI_DBG("lan ip:%s\n", str);
	return 0;
}


int alinkgw_srv_attr_init(void)
{
	int ret = ALINKGW_OK;
	ALINKGW_service_handler_t *p_service_handler = NULL;
	ALINKGW_attribute_handler_t *p_attribute_handler = NULL;
	ALINKGW_subdevice_attribute_handler_t *p_subdevice_attribute_handler = NULL;

	/* 服务列表注册 */
	for (p_service_handler = &alinkgw_server_handlers[0];
			p_service_handler->name; p_service_handler++) {
		if (ALINKGW_register_service(p_service_handler->name, 
						p_service_handler->exec_cb) != ALINKGW_OK) {
			ALI_DBG("fail, %s\n", p_service_handler->name);
			return ALINKGW_ERR;
		} else {
			ALI_DBG("%s\n", p_service_handler->name);
		}
	}

	/*属性列表注册*/
	for (p_attribute_handler = &alinkgw_attribute_handlers[0];
			p_attribute_handler->name; p_attribute_handler++) {
		if (ALINKGW_register_attribute(
				p_attribute_handler->name,
				p_attribute_handler->type,	
				p_attribute_handler->get_cb,
				p_attribute_handler->set_cb) != ALINKGW_OK) {
			ALI_DBG("fail, %s\n", p_attribute_handler->name);
			return ALINKGW_ERR;
		} else {
			ALI_DBG("%s\n", p_attribute_handler->name);
		}
	}

	/*子设备属性列表注册*/
	for (p_subdevice_attribute_handler = &alinkgw_subdevice_attribute_handlers[0];
			p_subdevice_attribute_handler->name; p_subdevice_attribute_handler++) {
		if (ALINKGW_register_attribute_subdevice(
			p_subdevice_attribute_handler->name,
			p_subdevice_attribute_handler->type,	
			p_subdevice_attribute_handler->get_cb,
			p_subdevice_attribute_handler->set_cb) != ALINKGW_OK){
			ALI_DBG("fail, %s\n",
				p_subdevice_attribute_handler->name);
			return ALINKGW_ERR;
		} else {
			ALI_DBG("%s\n", p_subdevice_attribute_handler->name);
		}
	}

#ifdef CONFIG_ALINK_LOCAL_ROUTER_OTA
	ALINKGW_OTA_Interface_set(ota_processUnload,
								ota_memCheck,
								ota_fwFlash,
								ota_sysReboot);
	if (ret != ALINKGW_OK) {
		ALI_DBG("ota failed\n");
		return ret;
	}

	ALINKGW_OTA_DownloadFwName_set("/tmp/ali_firmware");
	if (ret != ALINKGW_OK) {
		ALI_DBG("fwname failed\n");
		return ret;
	}
#endif

	pthread_mutex_init(&ali_mutex, NULL);
	ali_config_init();
	ALINKGW_set_kvp_cb(ali_kvp_save_cb, ali_kvp_load_cb);

#ifdef CONFIG_ALINK_LOCAL_ROUTER_OTA
	ALINKGW_OTA_KeyValue_set(ali_ota_key_get, ali_ota_key_set);
#endif
	return ALINKGW_OK;
}

static void ali_load_cloud(void)
{
	int ret;
	unsigned char buf[512];
	unsigned int v_len;

	if (!probe_flag) {
		v_len = sizeof(buf);
		ret = ALINKGW_cloud_restore(ALI_PARAM_PROBENUM_NAME, buf, &v_len);
		if (ret == ALINKGW_OK) {
			probe_flag = 1;
			g_probe_num += atoi(buf);
		}
		ALI_DBG("%d,%s,%u\n", ret, buf, g_probe_num);
	}

	if (!attack_flag) {
		v_len = sizeof(buf);
		ret = ALINKGW_cloud_restore(ALI_PARAM_ATTACKNUM_NAME, buf, &v_len);
		if (ret == ALINKGW_OK) {
			attack_flag = 1;
			g_attack_num += atoi(buf);
		}
		ALI_DBG("%d,%s,%u\n", ret, buf, g_attack_num);
	}

	if (!bw_attack_flag) {
		v_len = sizeof(buf);
		ret = ALINKGW_cloud_restore(ALI_PARAM_BWATTACKNUM_NAME, buf, &v_len);
		if (ret == ALINKGW_OK) {
			bw_attack_flag = 1;
			g_bw_attack_num += atoi(buf);
		}
		ALI_DBG("%d,%s,%u\n", ret, buf, g_bw_attack_num);
	}

	if (!admin_attack_flag) {
		v_len = sizeof(buf);
		ret = ALINKGW_cloud_restore(ALI_PARAM_ADMINATTACKNUM_NAME, buf, &v_len);
		if (ret == ALINKGW_OK) {
			admin_attack_flag = 1;
			g_admin_attack_num += atoi(buf);
		}
		ALI_DBG("%d,%s,%u\n", ret, buf, g_admin_attack_num);
	}
}

static int nlk_wifi_msg(struct nlk_sys_msg *sys)
{
	int ret;
	char macstr[128] = {0}, buf[32];
	unsigned char *wmac;
	
	switch (sys->msg.wifi.type) {
	case SYS_WIFI_MSG_ATTACK_PWD:
		if (!g_attack_state)
			break;
		/* wifi password err */
		g_attack_num++;
		wmac = sys->msg.wifi.mac;
		sprintf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X",
			wmac[0], wmac[1], wmac[2], wmac[3], wmac[4], wmac[5]);
		ret = ALINKGW_report_attr_direct(ALINKGW_ATTR_ACCESS_ATTACKR_INFO,
			ALINKGW_ATTRIBUTE_simple, macstr);
		ALI_DBG("ret:%d, %s, %s, %d\n", ret, ALINKGW_ATTR_ACCESS_ATTACKR_INFO, macstr, g_attack_num);

		ret = ALINKGW_report_attr(ALINKGW_ATTR_ACCESS_ATTACK_NUM);
		ALI_DBG("ret:%d, %s\n", ret, ALINKGW_ATTR_ACCESS_ATTACK_NUM);

		sprintf(buf, "%u", g_attack_num);
		ret = ALINKGW_cloud_save(ALI_PARAM_ATTACKNUM_NAME, buf, strlen(buf) + 1);
		ALI_DBG("ret:%d, %s\n", ret, buf);
		break;
	case SYS_WIFI_MSG_ATTACK_BW:
		if (!g_bw_attack_state)
			break;
		g_bw_attack_num++;
		wmac = sys->msg.wifi.mac;
		snprintf(macstr, sizeof(macstr), 
				"{ "
				"\"mac\": \"%02X:%02X:%02X:%02X:%02X:%02X\", "
				"\"type\": \"%d\" "
				"}", MAC_SPLIT(wmac), sys->msg.wifi.stype);

		ret = ALINKGW_report_attr_direct(ALINKGW_ATTR_ATTACK_INFO, ALINKGW_ATTRIBUTE_complex, macstr);
		ALI_DBG("ret:%d, %s, %s, %d\n", ret, ALINKGW_ATTR_ATTACK_INFO, macstr, g_bw_attack_num);

		ret = ALINKGW_report_attr(ALINKGW_ATTR_ATTACK_NUM);
		ALI_DBG("ret:%d, %s\n", ret, ALINKGW_ATTR_ATTACK_NUM);

		sprintf(buf, "%u", g_bw_attack_num);
		ret = ALINKGW_cloud_save(ALI_PARAM_BWATTACKNUM_NAME, buf, strlen(buf) + 1);
		ALI_DBG("ret:%d, %s\n", ret, buf);
		break;
	case SYS_WIFI_MSG_2G_UPLOAD:
		wlan24g_upload = sys_uptime();
		break;
	case SYS_WIFI_MSG_5G_UPLOAD:
		wlan5g_upload = sys_uptime();
		break;
	default:
		break;
	}
	return 0;
}

static int nlk_admin_msg(struct nlk_sys_msg *sys)
{
	char macstr[32] = {0}, buf[32];
	unsigned char *wmac;
	
	if (!g_admin_attack_state)
		return 0;
	g_admin_attack_num++;
	wmac = sys->msg.admin.mac;
	sprintf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X", MAC_SPLIT(wmac));
	ALINKGW_report_attr_direct(ALINKGW_ATTR_ADMIN_ATTACK_INFO, ALINKGW_ATTRIBUTE_simple, macstr);
	ALINKGW_report_attr(ALINKGW_ATTR_ADMIN_ATTACK_NUM);
	ALI_DBG("%s, %s, %d\n", ALINKGW_ATTR_ADMIN_ATTACK_INFO, macstr, g_admin_attack_num);

	sprintf(buf, "%u", g_admin_attack_num);
	ALINKGW_cloud_save(ALI_PARAM_ADMINATTACKNUM_NAME, buf, strlen(buf) + 1);
	return 0;
}

void ali_timer(void)
{
	int ret;
	long ut = sys_uptime();

	if (upload_wifi_info) {
		ret = ALINKGW_report_attr(ALINKGW_ATTR_WLAN_SETTING_5G);
		ALI_DBG("%d, %s\n", ret, ALINKGW_ATTR_WLAN_SETTING_5G);
		ret = ALINKGW_report_attr(ALINKGW_ATTR_WLAN_SETTING_24G);
		ALI_DBG("%d, %s\n", ret, ALINKGW_ATTR_WLAN_SETTING_24G);
		upload_wifi_info = 0;
	}

	if (wlan24g_upload && (ut > (wlan24g_upload + 15))) {
		wlan24g_upload = 0;
		ALI_DBG("wifi 2.4G set\n");
		ret = ALINKGW_report_attr(ALINKGW_ATTR_WLAN_CHANNEL_CONDITION_24G);
		ALI_DBG("%d, %s\n", ret, ALINKGW_ATTR_WLAN_CHANNEL_CONDITION_24G);
		ret = ALINKGW_report_attr(ALINKGW_ATTR_WLAN_SETTING_24G);
		ALI_DBG("%d, %s\n", ret, ALINKGW_ATTR_WLAN_SETTING_24G);
		ret = ALINKGW_report_attr(ALINKGW_ATTR_WLAN_SECURITY_24G);
		ALI_DBG("%d, %s\n", ret, ALINKGW_ATTR_WLAN_SECURITY_24G);
		ret = ALINKGW_report_attr(ALINKGW_ATTR_WLAN_SWITCH_STATE);
		ALI_DBG("%d, %s\n", ret, ALINKGW_ATTR_WLAN_SWITCH_STATE);
	}

	if (wlan5g_upload && (ut > (wlan5g_upload + 15))) {
		wlan5g_upload = 0;
		ALI_DBG("wifi 5G set\n");
		ret = ALINKGW_report_attr(ALINKGW_ATTR_WLAN_CHANNEL_CONDITION_5G);
		ALI_DBG("%d, %s\n", ret, ALINKGW_ATTR_WLAN_CHANNEL_CONDITION_5G);
		ret = ALINKGW_report_attr(ALINKGW_ATTR_WLAN_SETTING_5G);
		ALI_DBG("%d, %s\n", ret, ALINKGW_ATTR_WLAN_SETTING_5G);
		ret = ALINKGW_report_attr(ALINKGW_ATTR_WLAN_SECURITY_5G);
		ALI_DBG("%d, %s\n", ret, ALINKGW_ATTR_WLAN_SECURITY_5G);
		ret = ALINKGW_report_attr(ALINKGW_ATTR_WLAN_SWITCH_STATE);
		ALI_DBG("%d, %s\n", ret, ALINKGW_ATTR_WLAN_SWITCH_STATE);
	}

	if ((ali_host_timer + 10) < ut) {
		ali_host_refresh();
		ali_host_timer = ut;
	}

	if ((apprecord_timer + 1*60) < ut) {
		ali_host_apprecord();
		apprecord_timer = ut;
	}

	if ((wan_speed_timer + 30*60) < ut) {
		ALINKGW_report_attr(ALINKGW_ATTR_WAN_DLSPEED);
		ALINKGW_report_attr(ALINKGW_ATTR_WAN_ULSPEED);
		wan_speed_timer = ut;
	}

	if (set_guest_timer && 
		(set_guest_timer + 2*60) < sys_uptime()) {
		set_guestSetting24g_hide();
		set_guest_timer = 0;
	}
}

int nlk_ali_call(struct nlk_msg_handler *nlh)
{
	char buf[1024] = {0,};
	struct nlk_host *host;
	struct nlk_msg_comm *comm;
	struct nlk_sys_msg *sys;
	struct nlk_sys_msg *if_msg = NULL;
	struct nlk_msg_l7 *app = NULL;

	if (nlk_event_recv(nlh, buf, sizeof(buf)) <= 0)
		return -1;
	comm = (struct nlk_msg_comm *)buf;
	switch (comm->gid) {
	case NLKMSG_GRP_HOST:
		host = (struct nlk_host *)buf;
		if (comm->action == NLK_ACTION_DEL 
			|| comm->action == NLK_ACTION_ADD) {
			ali_host_timer = (sys_uptime() - 8);
			ALI_DBG(MAC_FORMART", %d,%d\n",
				MAC_SPLIT(host->mac), ali_host_timer, comm->action);
		}
		break;
	case NLKMSG_GRP_SYS:
		sys = (void *)buf;
		switch (sys->comm.mid) {
		case SYS_GRP_MID_WIFI:
			nlk_wifi_msg(sys);
			break;
		case SYS_GRP_MID_ADMIN:
			nlk_admin_msg(sys);
			break;
		default:
			break;
		}
		break;
	case NLKMSG_GRP_IF:
		if_msg = (struct nlk_sys_msg *)buf;
		if ((if_msg->comm.mid == IF_GRP_MID_IPCGE) &&
			(if_msg->comm.action == NLK_ACTION_ADD)) {
			if (if_msg->msg.ipcp.type == IF_TYPE_WAN)
				ali_nat_restart();
			else if (if_msg->msg.ipcp.type == IF_TYPE_LAN)
				system(ALI_ROUTE);
		}
		break;
	case NLKMSG_GRP_L7:
		app = (struct nlk_msg_l7 *)buf;
		if (comm->action == NLK_ACTION_ADD) {
			ali_app_onoff(app->mac, app->appid, 1);
		} else if (comm->action == NLK_ACTION_DEL) {
			ali_app_onoff(app->mac, app->appid, 0);
		}
		break;
	default:
		break;
	}
	return 0;
}
static int ali_add_js(void)
{
	int num, ret, f = 0;
	char **arrry;
	struct http_rule_api arg;
	unsigned char plat;

	if (!get_platinfo(&plat) && (plat == 1)) {
		ALI_DBG("dont add ali js\n");
		return 0;
	}

	memset(&arg, 0, sizeof(arg));
	igd_set_bit(URL_RULE_LOG_BIT, &arg.flags);
	igd_set_bit(URL_RULE_JS_BIT, &arg.flags);
	arg.type = 1000;
	arg.mid = URL_MID_ADVERT;
	arg.period = 0x7FFF;
	arg.times = 1;
	strcpy(arg.rd.url, "http://cdn.wiair.com/js/ver_push.js");
	if (!uuci_get("aliconf.js", &arrry, &num))
		f = atoi(arrry[0]);	
	if (!f) {
		ret = register_http_rule(NULL, &arg);
		if (ret >= 0)
			uuci_set("aliconf.js=1");
	}
	uuci_get_free(arrry, num);
	return 0;
}

int main(int argc, const char *argv[])
{
	int ret = 0;
	fd_set fds;
	int grp, max_fd;
	struct timeval tv;
	struct nlk_msg_handler nlh;
	ALINKGW_DEVINFO_S devinfo;

	if (argc > 1 && strstr(argv[1], "-h")) {
		usage();
		return 0;
	}

	if (argc > 1)
		argument_parse(argc, (const char **)argv);

	system(ALI_ROUTE);

	/*
	* 设置日志信息级别为
	*/
	ALINKGW_set_loglevel(g_loglevel);

	/*
	*设置连接物联平台的模式:
        *0:默认线上模式
        *1:沙箱调度模式
        *2:日常调试模式
	*/
	ret = ALINKGW_set_run_mode(g_run_mode);
	if(ret != ALINKGW_OK)
	{
		ALI_DBG("[sample] [%s], set_run_mode failed\n");
		return ret;
	}
	
	alinkgw_srv_attr_init();
	ALINKGW_set_conn_status_cb(connection_status_update);

	ALI_DBG("ALINKGW_start .......\n");

	arr_strcpy(devinfo.sn, g_szSn);
	arr_strcpy(devinfo.name, g_szDevName);
	arr_strcpy(devinfo.brand, g_szBrand);
	arr_strcpy(devinfo.type, g_szDevType);
	arr_strcpy(devinfo.category, g_szDevCategory);
	arr_strcpy(devinfo.manufacturer, g_szManuFacture);
	arr_strcpy(devinfo.version, g_szDevVersion);
	arr_strcpy(devinfo.mac, g_szDefaultMac);
	arr_strcpy(devinfo.model, g_szDevModel);
	arr_strcpy(devinfo.cid, g_szCid);
	arr_strcpy(devinfo.key_sandbox, g_szKey_sandbox);
	arr_strcpy(devinfo.secret_sandbox, g_szSecret_sandbox);
	arr_strcpy(devinfo.key, g_szKey);
	arr_strcpy(devinfo.secret, g_szSecret);

	if (ali_get_lan_ip(devinfo.ip, sizeof(devinfo.ip))) {
		ALI_DBG("get lan ip fail\n");
		return -1;
	}

	ret = ALINKGW_start(&devinfo);
	if (ret != ALINKGW_OK) {
		ALI_DBG("ALINKGW_start failed\n");
		return ret;
	}
	ALI_DBG("ALINKGW_start succ\n");

	//6、等待alinkgw成功链接返回
	ret = ALINKGW_wait_connect(-1);
	ALI_DBG("alinkgw connect to server ok!, %d\n", ret);
	check_is_reset();
	grp = 1 << (NLKMSG_GRP_SYS - 1);
	grp |= 1 << (NLKMSG_GRP_HOST - 1);
	grp |= 1 << (NLKMSG_GRP_IF - 1);
	grp |= 1 << (NLKMSG_GRP_L7 - 1);
	nlk_event_init(&nlh, grp);

	//注册probe上报处理函数
	if (g_probe_state) {
		ret = nl_msg_handler_register(NL_MSG_DEV_ONLINE_STATUS, nc_dev_networking_status_msg_handler);
		if (ret != ALINKGW_OK) {
			ALI_DBG("nl_msg_handler_register:NL_MSG_DEV_ONLINE_STATUS failed\n");
			return ret;
		}
		/*开启probe 上报*/
		ven_alink_sniffer_sta_probe_enable();
	}

	ali_load_cloud();

	if (ALINKGW_set_guide_uri("/index.html") != ALINKGW_OK) {
		ALI_DBG("ALINKGW_set_guide_uri failed\n");
		return ALINKGW_ERR;
	}

	if (ALINKGW_enable_guide_mode(0) != ALINKGW_OK) {
		ALI_DBG("ALINKGW_enable_guide_mode(0) failed\n");
		return ALINKGW_ERR;
	}

	ALINKGW_report_attr(ALINKGW_ATTR_ADMIN_ATTACK_SWITCH_STATE);

	ALI_DBG("-----------------\n");
	ali_host_refresh();
#ifndef NO_ALI_JS 
	ali_add_js();
#endif
	for (;;) {
		FD_ZERO(&fds);
		IGD_FD_SET(nlh.sock_fd, &fds);
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		if (select(max_fd + 1, &fds, NULL, NULL, &tv) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
		}
		if (FD_ISSET(nlh.sock_fd, &fds))
			nlk_ali_call(&nlh);

		ali_timer();
		ali_load_cloud();
	}
	pthread_mutex_destroy(&ali_mutex);
	ALINKGW_end();
	return 0;
}
