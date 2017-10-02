#ifdef DISKM_SUPPORT
#define _FILE_OFFSET_BITS 64
#endif
#include "protocol.h"
#include "server.h"
#include <linux/if.h>
#include "nlk_ipc.h"
#include "ioos_uci.h"
#include "log.h"
#include "igd_host.h"
#include "igd_lib.h"
#include "igd_interface.h"
#include "igd_wifi.h"
#include "igd_dnsmasq.h"
#include "igd_qos.h"
#include "igd_ali.h"
#include "igd_system.h"
#include "igd_url_safe.h"
#include "igd_upnp.h"
#include "igd_nat.h"
#include "igd_cloud.h"
#include "igd_advert.h"
#include "igd_vpn.h"
#include "uci_fn.h"
#include "igd_md5.h"
#include "igd_lanxun.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "image.h"
#include "printbuf.h"
#include "igd_log.h"
#include <sys/sysinfo.h>
#ifdef DISKM_SUPPORT
#include "disk_manager.h"
#endif
#include "igd_tunnel.h"
#include "igd_route.h"
#include "igd_dnsmasq.h"

#if 0
#define CGI_PRINTF(fmt,args...) do{}while(0)
#else
#define CGI_PRINTF(fmt,args...) do{console_printf("[CGI:%05d]DBG:"fmt, __LINE__, ##args);}while(0)
#endif

#define CGI_BIT_SET(d,f)  (d) |= (1<<(f))
#define CGI_BIT_CLR(d,f)  (d) &= (~(1<<(f)))
#define CGI_BIT_TEST(d,f)  ((d) & (1<<(f)))

enum {
	CGI_ERR_FAIL = 10001,
	CGI_ERR_INPUT,
	CGI_ERR_MALLOC,
	CGI_ERR_EXIST,
	CGI_ERR_NONEXIST,
	CGI_ERR_FULL,
	CGI_ERR_NOLOGIN,
	CGI_ERR_NOSUPPORT,
	CGI_ERR_ACCOUNT_NOTREADY,
	CGI_ERR_TIMEOUT,
	CGI_ERR_FILE,
	CGI_ERR_RULE,
};

#define CHECK_LOGIN do {\
	if(con->login != 1) {\
		CGI_PRINTF("WARNING: NO LOGIN\n");\
		return CGI_ERR_NOLOGIN;\
	}\
} while (0)

char *cgi_mac2str(uint8_t *mac)
{
	static char str[20];

	memset(str, 0, sizeof(str));
	snprintf(str, sizeof(str), "%02X:%02X:%02X:%02X:%02X:%02X",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	return str;
}

int cgi_str2mac(char *str, unsigned char *mac)
{
	int i = 0, j = 0;
	unsigned char v = 0;

	for (i = 0; i < 17; i++) {
		if (str[i] >= '0' && str[i] <= '9') {
			v = str[i] - '0';
		} else if (str[i] >= 'a' && str[i] <= 'f') {
			v = str[i] - 'a' + 10;
		} else if (str[i] >= 'A' && str[i] <= 'F') {
			v = str[i] - 'A' + 10;
		} else if (str[i] == ':' || str[i] == '-' ||
					str[i] == ',' || str[i] == '\r' ||
					str[i] == '\n') {
			continue;
		} else if (str[i] == '\0') {
			return 0;
		} else {
			return -1;
		}
		if (j%2)
			mac[j/2] += v;
		else
			mac[j/2] = v*16;
		j++;
		if (j/2 > 5)
			break;
	}
	return 0;
}

struct json_object *get_app_json(struct host_app_dump_info *app)
{
	int i = 0;
	struct json_object *obj, *papp;
	char app_flag[HARF_MAX + 1] = {0,};

	papp = json_object_new_object();
	
	obj = json_object_new_int((int)app->appid);
	json_object_object_add(papp, "id", obj);
	
	obj = json_object_new_int((int)(app->down_speed/1024));
	json_object_object_add(papp, "speed", obj);

	obj = json_object_new_int((int)(app->up_speed/1024));
	json_object_object_add(papp, "up_speed", obj);

	obj = json_object_new_uint64(app->up_bytes/1024);
	json_object_object_add(papp, "up_bytes", obj);

	obj = json_object_new_uint64(app->down_bytes/1024);
	json_object_object_add(papp, "down_bytes", obj);

	obj = json_object_new_int((int)app->uptime);
	json_object_object_add(papp, "uptime", obj);

	obj = json_object_new_int((int)app->ontime);
	json_object_object_add(papp, "ontime", obj);

	memset(app_flag, 0, sizeof(app_flag));
	for (i = 0; i < HARF_MAX; i++) {
		app_flag[i] = \
			igd_test_bit(i, app->flag) ? 'T' : 'F';
	}
	obj = json_object_new_string(app_flag);
	json_object_object_add(papp, "flag", obj);

	return papp;
}
	
struct json_object *get_host_json(struct host_dump_info *host, char *callip)
{
	unsigned long speed;
	int i = 0;
	struct json_object *obj, *phost;
	char host_flag[HIRF_MAX + 3] = {0,}, *ptr = NULL;

	phost = json_object_new_object();

	obj= json_object_new_string(cgi_mac2str(host->mac));
	json_object_object_add(phost, "mac", obj);

	obj= json_object_new_string(
		host->ip[0].s_addr ? inet_ntoa(host->ip[0]) : "");
	json_object_object_add(phost, "ip", obj);

	speed = (host->ls.down && (host->down_speed \
		> host->ls.down*1024)) ? host->ls.down*1024 : host->down_speed;
	obj= json_object_new_int(speed/1024);
	json_object_object_add(phost, "speed", obj);

	speed = (host->ls.up && (host->up_speed \
		> host->ls.up*1024)) ? host->ls.up*1024 : host->up_speed;
	obj= json_object_new_int(speed/1024);
	json_object_object_add(phost, "up_speed", obj);

	obj= json_object_new_uint64(host->up_bytes/1024);
	json_object_object_add(phost, "up_bytes", obj);

	obj= json_object_new_uint64(host->down_bytes/1024);
	json_object_object_add(phost, "down_bytes", obj);

	obj= json_object_new_string(host->name);
	json_object_object_add(phost, "name", obj);

	obj = json_object_new_int((int)host->vender);
	json_object_object_add(phost, "vendor", obj);

	obj = json_object_new_int((int)host->os_type);
	json_object_object_add(phost, "ostype", obj);

	obj = json_object_new_int((int)host->uptime);
	json_object_object_add(phost, "uptime", obj);

	obj = json_object_new_int((int)host->ontime);
	json_object_object_add(phost, "ontime", obj);

	obj = json_object_new_int((int)host->mode);
	json_object_object_add(phost, "mode", obj);

	obj = json_object_new_int((int)host->pic);
	json_object_object_add(phost, "pic", obj);

	memset(host_flag, 0, sizeof(host_flag));
	host_flag[0] = 'F'; 
	for (i = 0; i < 1 /*IGD_HOST_IP_MX*/; i++) {
		if (host->ip[i].s_addr == 0)
			continue;
		ptr = inet_ntoa(host->ip[i]);
		if (ptr != NULL && callip != NULL \
				&& strcmp(ptr, callip) == 0) {
			host_flag[0] = 'T';
		}
	}
	for (i = 0; i < HIRF_MAX; i++) {
		host_flag[i + 1] = \
			igd_test_bit(i, host->flag) ? 'T' : 'F';
	}
	obj = json_object_new_string(host_flag);
	json_object_object_add(phost, "flag", obj);

	return phost;
}

static int cgi_get_host_sig(
	unsigned char *mac,
	struct sta_status *wifi24g,
	struct sta_status *wifi5g)
{
	int i;

	for (i = 0; i < wifi24g->num; i++) {
		if (!memcmp(mac, wifi24g->entry[i].Addr, 6))
			return wifi24g->entry[i].AvgRssi0;
	}
	for (i = 0; i < wifi5g->num; i++) {
		if (!memcmp(mac, wifi5g->entry[i].Addr, 6))
			return wifi5g->entry[i].AvgRssi0;
	}
	return 100;
}

int cgi_sys_main_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	int status = 0;
	char cmd[128] = {0}, *login = NULL;
	int ret = 0, i = 0, j = 0;
	int host_nr = 0, app_nr = 0;
	unsigned long down_speed = 0, up_speed = 0;
	struct json_object *obj, *ts, *t, *app, *apps;
	struct host_dump_info host[IGD_HOST_MX];
	struct host_app_dump_info host_app[IGD_HOST_APP_MX];
	struct qos_conf qos_config;
	struct if_statistics statis;
	struct iface_info ifinfo;
	struct sta_status wifi24g, wifi5g;

	login = con_value_get(con, "login");
	if (login && atoi(login))
		CHECK_LOGIN;
	if (mu_msg(QOS_PARAM_SHOW, NULL, 0, &qos_config, sizeof(qos_config)))
		return CGI_ERR_FAIL;

	obj = json_object_new_int(qos_config.down/8);
	json_object_object_add(response, "total_speed", obj);

	obj = json_object_new_int(qos_config.up/8);
	json_object_object_add(response, "total_upspeed", obj);

	status = 1;
	if (mu_msg(IF_MOD_IFACE_INFO, &status, sizeof(int), &ifinfo, sizeof(ifinfo)))
		return CGI_ERR_FAIL;
	if (!mu_msg(SYSTEM_MOD_DEV_CHECK, NULL, 0, &status, sizeof(int))) {
		if (ifinfo.net && !status) {
			snprintf(cmd, 128, "qos_rule.status.status=%d", !status);
			uuci_set(cmd);
		}
	}
	obj = json_object_new_boolean(ifinfo.net);
	json_object_object_add(response, "connected", obj);
	
	obj = json_object_new_int(sys_uptime());
	json_object_object_add(response, "ontime", obj);

	host_nr = mu_msg(IGD_HOST_DUMP, NULL, 0, host, sizeof(host));
	if (host_nr < 0) {
		CGI_PRINTF("dump host err, ret:%d\n", host_nr);
		return CGI_ERR_FAIL;
	}

	i = 0;
	if (mu_msg(WIFI_MOD_GET_STA_INFO,
		&i, sizeof(i), &wifi24g, sizeof(wifi24g))) {
		memset(&wifi24g, 0, sizeof(wifi24g));
	}
	i = 1;
	if (mu_msg(WIFI_MOD_GET_STA_INFO,
		&i, sizeof(i), &wifi5g, sizeof(wifi5g))) {
		memset(&wifi5g, 0, sizeof(wifi5g));
	}

	ts = json_object_new_array();
	for (i = 0; i < host_nr; i++) {
		if (!qos_config.enable)
			igd_clear_bit(HIRF_ISKING, host[i].flag);
		t = get_host_json(&host[i], con->ip_from);

		if (!igd_test_bit(HIRF_ONLINE, host[i].flag)
			|| !igd_test_bit(HIRF_WIRELESS, host[i].flag)) {
			obj = json_object_new_int(0);
		} else {
			obj = json_object_new_int(
				cgi_get_host_sig(host[i].mac, &wifi24g, &wifi5g));
		}
		json_object_object_add(t, "sig", obj);

		apps = json_object_new_array();
		app_nr = mu_msg(IGD_HOST_APP_DUMP,
			host[i].mac, 6, host_app, sizeof(host_app));
		if (app_nr >= 0) {
			for (j = 0; j < app_nr; j++) {
				if (!igd_test_bit(HARF_ONLINE, host_app[j].flag))
					continue;
				app = get_app_json(&host_app[j]);
				json_object_array_add(apps, app);
			}
		} else {
			CGI_PRINTF("dump app err, ret:%d\n", app_nr);
		}
		json_object_object_add(t, "apps", apps);
		json_object_array_add(ts, t);
		up_speed += host[i].up_speed;
		down_speed += host[i].down_speed;
	}

	up_speed /= 1024;
	down_speed /= 1024;
	get_if_statistics(1, &statis);
	ret = (int)(statis.in.all.speed/1024);
	obj = json_object_new_int(ret > down_speed ? ret : down_speed);
	json_object_object_add(response, "cur_speed", obj);
	ret = (int)(statis.out.all.speed/1024);
	obj = json_object_new_int(ret > up_speed ? ret : up_speed);
	json_object_object_add(response, "up_speed", obj);

 	obj = json_object_new_uint64(statis.in.all.bytes/1024);
	json_object_object_add(response, "down_bytes", obj);

 	obj = json_object_new_uint64(statis.out.all.bytes/1024);
	json_object_object_add(response, "up_bytes", obj);

	json_object_object_add(response, "terminals", ts);
	return 0;
}

int cgi_net_host_app_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *pmac, *pact, *pappid, *pver;
	unsigned char mac[6] = {0,};
	struct json_object *t = NULL, *tmp = NULL, *apps = NULL, *app = NULL, *obj = NULL;
	struct host_dump_info host;
	struct host_app_dump_info host_app[IGD_HOST_APP_MX];
	int app_nr = 0, i = 0;
	unsigned int appid = 0;

	pmac = con_value_get(con, "mac");
	pact = con_value_get(con, "act");
	pver = con_value_get(con, "ver");
	if (!pmac || cgi_str2mac(pmac, mac)) {
		CGI_PRINTF("input err, %p\n", pmac);
		return CGI_ERR_INPUT;
	}

	if (!pact || strstr(pact, "host")) {
		if (mu_msg(IGD_HOST_DUMP, mac, 6, &host, sizeof(host)) < 0) {
			CGI_PRINTF("get host fail\n");
			return CGI_ERR_FAIL;
		}
		t = get_host_json(&host, con->ip_from);

		obj = json_object_new_int(host.ls.down);
		json_object_object_add(t, "ls", obj);
		obj = json_object_new_int(host.ls.up);
		json_object_object_add(t, "ls_up", obj);

		if (!pver) { // for 1.0 app
			tmp = json_object_new_array();
			json_object_object_add(t, "lm", tmp);

			tmp = json_object_new_object();
			obj = json_object_new_array();
			json_object_object_add(tmp, "week", obj);

			obj = json_object_new_int(0);
			json_object_object_add(tmp, "sh", obj);

			obj = json_object_new_int(0);
			json_object_object_add(tmp, "sm", obj);

			obj = json_object_new_int(0);
			json_object_object_add(tmp, "eh", obj);

			obj = json_object_new_int(0);
			json_object_object_add(tmp, "em", obj);

			obj = json_object_new_int(0);
			json_object_object_add(tmp, "st", obj);

			obj = json_object_new_int(0);
			json_object_object_add(tmp, "ft", obj);

			json_object_object_add(t, "lt", tmp);
		}

		json_object_object_add(response, "host", t);
	}

	if (!pact || strstr(pact, "app")) {
		pappid = con_value_get(con, "appid");
		if (pappid)
			appid = (unsigned int)atoll(pappid);

		apps = json_object_new_array();
		app_nr = mu_msg(IGD_HOST_APP_DUMP, mac, 6, host_app, sizeof(host_app));
		if (app_nr < 0) {
			CGI_PRINTF("dump app err, ret:%d\n", app_nr);
			return CGI_ERR_FAIL;
		}

		for (i = 0; i < app_nr; i++) {
			if (appid && (appid != host_app[i].appid))
				continue;

			app = get_app_json(&host_app[i]);
			if (appid) {
				tmp = json_object_new_object();

				obj = json_object_new_int(host_app[i].lt.start_hour);
				json_object_object_add(tmp, "sh", obj);

				obj = json_object_new_int(host_app[i].lt.start_min);
				json_object_object_add(tmp, "sm", obj);

				obj = json_object_new_int(host_app[i].lt.end_hour);
				json_object_object_add(tmp, "eh", obj);

				obj = json_object_new_int(host_app[i].lt.end_min);
				json_object_object_add(tmp, "em", obj);

				json_object_object_add(app, "lt", tmp);
			}

			json_object_array_add(apps, app);
		}

		if (t == NULL)
			json_object_object_add(response, "apps", apps);
		else
			json_object_object_add(t, "apps", apps);
	}
	if (!t && !apps)
		return CGI_ERR_INPUT;
	return 0;
}

enum HOST_IF_TYPE {
	HIT_IP = 0,
	HIT_NAME,
	HIT_VENDER,
	HIT_OSTYPE,
	HIT_SPEED,
	HIT_BYTES,
	HIT_TIME,
	HIT_LS,

	HIT_MAX, //must last
};

int cgi_net_host_if_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	unsigned long speed;
	int host_nr, i, j;
	char if_data[HIT_MAX], host_flag[HIRF_MAX + 3] = {0,}, *pif_data, *ptr;
	struct json_object *phost, *obj, *ts;
	struct host_dump_info *host, hdi[HOST_MX];

	pif_data = con_value_get(con, "if");
	if (!pif_data)
		return CGI_ERR_INPUT;

	memset(if_data, 0, sizeof(if_data));
	for (i = 0; i < HIT_MAX; i++) {
		if (pif_data[i] == '\0')
			break;
		else if (pif_data[i] == 'T')
			if_data[i] = 1;
	}

	host_nr = mu_msg(IGD_HOST_DUMP, NULL, 0, hdi, sizeof(hdi));
	if (host_nr < 0) {
		CGI_PRINTF("dump host err, ret:%d\n", host_nr);
		return CGI_ERR_FAIL;
	}

	ts = json_object_new_array();
	for (j = 0; j < host_nr; j++) {
		host = &hdi[j];
		phost = json_object_new_object();
		obj= json_object_new_string(cgi_mac2str(host->mac));
		json_object_object_add(phost, "mac", obj);

		memset(host_flag, 0, sizeof(host_flag));
		host_flag[0] = 'F';
		for (i = 0; i < 1 /*HOST_NUM_MAX*/; i++) {
			if (host->ip[i].s_addr == 0)
				continue;
			ptr = inet_ntoa(host->ip[i]);
			if (ptr != NULL && con->ip_from != NULL \
					&& strcmp(ptr, con->ip_from) == 0) {
				host_flag[0] = 'T';
			}
		}
		for (i = 0; i < HIRF_MAX ; i++) {
			if (igd_test_bit(i, host->flag))
				host_flag[i + 1] = 'T';
			else
				host_flag[i + 1] = 'F';
		}
		obj = json_object_new_string(host_flag);
		json_object_object_add(phost, "flag", obj);

		if (if_data[HIT_IP]) {
			if (host->ip[0].s_addr)
				obj= json_object_new_string(inet_ntoa(host->ip[0]));
			else
				obj= json_object_new_string("");
			json_object_object_add(phost, "ip", obj);
		}
		if (if_data[HIT_NAME]) {
			obj= json_object_new_string(host->name);
			json_object_object_add(phost, "name", obj);
		}
		if (if_data[HIT_VENDER]) {
			obj = json_object_new_int(host->vender);
			json_object_object_add(phost, "vendor", obj);
		}
		if (if_data[HIT_OSTYPE]) {
			obj = json_object_new_int(host->os_type);
			json_object_object_add(phost, "ostype", obj);
		}

		if (if_data[HIT_SPEED]) {
			speed = (host->ls.down && (host->down_speed \
				> host->ls.down*1024)) ? host->ls.down*1024 : host->down_speed;
			obj= json_object_new_int(speed/1024);
			json_object_object_add(phost, "speed", obj);
			
			speed = (host->ls.up && (host->up_speed \
				> host->ls.up*1024)) ? host->ls.up*1024 : host->up_speed;
			obj= json_object_new_int(speed/1024);
			json_object_object_add(phost, "up_speed", obj);
		}
		if (if_data[HIT_BYTES]) {
			obj= json_object_new_int(host->up_bytes);
			json_object_object_add(phost, "up_bytes", obj);
			obj= json_object_new_int(host->down_bytes);
			json_object_object_add(phost, "down_bytes", obj);
		}
		if (if_data[HIT_TIME]) {
			obj = json_object_new_int((int)host->uptime);
			json_object_object_add(phost, "uptime", obj);
			obj = json_object_new_int((int)host->ontime);
			json_object_object_add(phost, "ontime", obj);
		}
		if (if_data[HIT_LS]) {
			obj = json_object_new_int(host->ls.down);
			json_object_object_add(phost, "ls", obj);
			obj = json_object_new_int(host->ls.up);
			json_object_object_add(phost, "ls_up", obj);
		}
		json_object_array_add(ts, phost);
	}
	json_object_object_add(response, "terminals", ts);
	return 0;
}

#define CON_GET_MAC(pmac, con, mac) do {\
	pmac = con_value_get(con, "mac");\
	if (!pmac || cgi_str2mac(pmac, (unsigned char *)mac)) {\
		CGI_PRINTF("mac is NULL\n");\
		return CGI_ERR_INPUT;\
	}\
} while(0)

#define CON_GET_ACT(pact, con, act) do {\
	pact = con_value_get(con, "act");\
	if (!pact) {\
		CGI_PRINTF("act is NULL\n");\
		return CGI_ERR_INPUT;\
	} else if (!strcmp(pact, "on") || !strcmp(pact, "add")) {\
		act = NLK_ACTION_ADD;\
	} else if (!strcmp(pact, "off") || !strcmp(pact, "del")) {\
		act = NLK_ACTION_DEL;\
	} else if (!strcmp(pact, "dump")) {\
		act = NLK_ACTION_DUMP;\
	} else if (!strcmp(pact, "mod")) {\
		act = NLK_ACTION_MOD;\
	} else {\
		CGI_PRINTF("act err, act:%s\n", pact);\
		return CGI_ERR_INPUT;\
	}\
} while(0)

#define CON_GET_INT(pdata, con, data, str) do {\
	pdata = con_value_get(con, str);\
	if (!pdata) {\
		CGI_PRINTF("%s is NULL\n", str);\
		return CGI_ERR_INPUT;\
	}\
	data = atoi(pdata);\
} while(0)

#define CON_GET_STR(pdata, con, data, str) do {\
	pdata = con_value_get(con, str);\
	if (!pdata || !strlen(pdata)) {\
		CGI_PRINTF("%s is NULL\n", str);\
		return CGI_ERR_INPUT;\
	}\
	arr_strcpy(data, pdata);\
} while(0)

#define CON_GET_CHECK_INT(pdata, con, data, str, max) do {\
	CON_GET_INT(pdata, con, data, str);\
	if (data > max){\
		CGI_PRINTF("%s > max(%d)\n", str, max);\
		return CGI_ERR_INPUT;\
	}\
}while(0)

int cgi_net_host_mode_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	struct host_set_info info;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);
	CON_GET_INT(ptr, con, info.v.mode, "mode");
	if (mu_msg(IGD_HOST_SET_MODE, &info, sizeof(info), NULL, 0) < 0)
		return CGI_ERR_FAIL;
	return 0;
}

static int cgi_get_time_comm(struct time_comm *t, connection_t *con)
{
	int i, st, et, nt;
	char *ptr;
	struct tm *ntime;

	CON_GET_CHECK_INT(ptr, con, t->start_hour, "sh", 23);
	CON_GET_CHECK_INT(ptr, con, t->start_min, "sm", 59);
	CON_GET_CHECK_INT(ptr, con, t->end_hour, "eh", 23);
	CON_GET_CHECK_INT(ptr, con, t->end_min, "em", 59);

	st = t->start_hour*60 + t->start_min;
	et = t->end_hour*60 + t->end_min;

	if (st == et)
		return CGI_ERR_INPUT;

	ptr = con_value_get(con, "week");
	if (!ptr) {
		CGI_PRINTF("input err, %p\n", ptr);
		return CGI_ERR_INPUT;
	}
	for (i = 0; i < 7; i++) {
		if (*(ptr + i) == '\0')
			break;
		if (*(ptr + i) == '1')
			CGI_BIT_SET(t->day_flags, i);
	}

	if (t->day_flags) {
		t->loop = 1;
		return 0;
	}
	t->loop = 0;

	ntime = get_tm();
	if (!ntime) {
		CGI_PRINTF("get time fail\n");
		return CGI_ERR_FAIL;
	}
	CGI_BIT_SET(t->day_flags, ntime->tm_wday);
	if (st > et)
		return 0;

	nt = ntime->tm_hour*60 + ntime->tm_min;
	if (et < nt) {
		CGI_PRINTF("time out\n");
		return CGI_ERR_TIMEOUT;
	}
	return 0;
}

int cgi_net_host_lmt_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr;
	struct host_set_info info;
	struct app_mod_dump *dump;
	int nr, i, j;
	struct json_object *obj, *lmt, *mt, *tmp;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);
	CON_GET_ACT(ptr, con, info.act);
	if (info.act == NLK_ACTION_DUMP) {
		dump = malloc(sizeof(*dump)*IGD_APP_MOD_TIME_MX);
		if (!dump)
			return CGI_ERR_MALLOC;
		nr = mu_msg(IGD_HOST_APP_MOD_ACTION, &info, sizeof(info),
			dump, IGD_APP_MOD_TIME_MX*sizeof(*dump));
		if (nr < 0) {
			free(dump);
			CGI_PRINTF("mu msg err, ret:%d\n", nr);
			return CGI_ERR_FAIL;
		}
		lmt = json_object_new_array();
		for (i = 0; i < nr; i++) {
			mt = json_object_new_object();
			
			obj = json_object_new_int(dump[i].time.start_hour);
			json_object_object_add(mt, "sh", obj);

			obj = json_object_new_int(dump[i].time.start_min);
			json_object_object_add(mt, "sm", obj);

			obj = json_object_new_int(dump[i].time.end_hour);
			json_object_object_add(mt, "eh", obj);

			obj = json_object_new_int(dump[i].time.end_min);
			json_object_object_add(mt, "em", obj);

			tmp = json_object_new_array();
			for (j = 0; j < 7; j++) {
				if (!dump[i].time.loop || 
					!CGI_BIT_TEST(dump[i].time.day_flags, j))
					continue;
				obj = json_object_new_int(j);
				json_object_array_add(tmp, obj);
			}
			json_object_object_add(mt, "week", tmp);

			tmp = json_object_new_array();
			for (j = 0; j < L7_MID_MX; j++) {
				if (!igd_test_bit(j, dump[i].mid_flag))
					continue;
				obj = json_object_new_int(j);
				json_object_array_add(tmp, obj);
			}
			json_object_object_add(mt, "mid", tmp);

			obj = json_object_new_int(dump[i].enable);
			json_object_object_add(mt, "enable", obj);

			json_object_array_add(lmt, mt);
		}
		json_object_object_add(response, "lmt", lmt);
		free(dump);
		return 0;
	}

	CON_GET_INT(ptr, con, info.v.app_mod.enable, "enable");
	nr = cgi_get_time_comm(&info.v.app_mod.time, con);
	if (nr) {
		CGI_PRINTF("get time com fail\n");
		return nr;
	}

	ptr = con_value_get(con, "mid");
	if (!ptr) {
		CGI_PRINTF("input err, %p\n", ptr);
		return CGI_ERR_INPUT;
	}
	for (i = 0; i < L7_MID_MX; i++) {
		if (*(ptr + i) == '\0')
			break;
		if (*(ptr + i) == '1')
			igd_set_bit(i, info.v.app_mod.mid_flag);
	}

	nr = mu_msg(IGD_HOST_APP_MOD_ACTION, &info, sizeof(info), NULL, 0);
	if (nr == -2)
		return CGI_ERR_FULL;
	else if (nr < 0)
		return CGI_ERR_FAIL;
	return 0;
}

int cgi_net_host_ls_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	struct host_set_info info;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);
	CON_GET_INT(ptr, con, info.v.ls.down, "speed");
	ptr = con_value_get(con, "up_speed");
	if (ptr)
		info.v.ls.up = atoi(ptr);

	if (mu_msg(IGD_HOST_SET_LIMIT_SPEED, &info, sizeof(info), NULL, 0) < 0)
		return CGI_ERR_FAIL;
	return 0;
}

int cgi_net_host_study_time_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr;
	struct host_set_info info;
	struct study_time_dump *dump;
	int nr = 0, i, j;
	struct json_object *t_arr, *t_obj, *t_day, *obj;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);
	CON_GET_ACT(ptr, con, info.act);

	if (info.act == NLK_ACTION_DUMP) {
		dump = malloc(sizeof(*dump)*IGD_STUDY_TIME_NUM);
		if (!dump)
			return CGI_ERR_MALLOC;
		nr = mu_msg(IGD_HOST_STUDY_TIME, &info, sizeof(info),
			dump, sizeof(*dump)*IGD_STUDY_TIME_NUM);
		if (nr < 0) {
			free(dump);
			CGI_PRINTF("mu msg err, ret:%d\n", nr);
			return CGI_ERR_FAIL;
		}
		t_arr = json_object_new_array();
		for (i = 0; i < nr; i ++) {
			t_obj = json_object_new_object();
			
			obj = json_object_new_int(dump[i].enable);
			json_object_object_add(t_obj, "enable", obj);

			obj = json_object_new_int(dump[i].time.start_hour);
			json_object_object_add(t_obj, "sh", obj);

			obj = json_object_new_int(dump[i].time.start_min);
			json_object_object_add(t_obj, "sm", obj);

			obj = json_object_new_int(dump[i].time.end_hour);
			json_object_object_add(t_obj, "eh", obj);

			obj = json_object_new_int(dump[i].time.end_min);
			json_object_object_add(t_obj, "em", obj);

			t_day = json_object_new_array();
			for (j = 0; j < 7; j++) {
				if (!dump[i].time.loop || 
					!CGI_BIT_TEST(dump[i].time.day_flags, j))
					continue;
				obj = json_object_new_int(j);
				json_object_array_add(t_day, obj);
			}
			json_object_object_add(t_obj, "week", t_day);
			json_object_array_add(t_arr, t_obj);
		}
		json_object_object_add(response, "study_time", t_arr);
		free(dump);
		return 0;
	}

	CON_GET_INT(ptr, con, info.v.study_time.enable, "enable");
	nr = cgi_get_time_comm(&info.v.study_time.time, con);
	if (nr) {
		CGI_PRINTF("get time com fail\n");
		return nr;
	}

	nr = mu_msg(IGD_HOST_STUDY_TIME, &info, sizeof(info), NULL, 0);
	if (nr == -2)
		return CGI_ERR_FULL;
	else if (nr < 0)
		return CGI_ERR_FAIL;
	return 0;
}

int cgi_net_host_king_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	struct host_set_info info;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);
	CON_GET_ACT(ptr, con, info.act);

	if (mu_msg(IGD_HOST_SET_KING, &info, sizeof(info), NULL, 0) < 0)
		return CGI_ERR_FAIL;

	return 0;
}

int cgi_net_host_black_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	struct host_set_info info;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);
	CON_GET_ACT(ptr, con, info.act);

	if (mu_msg(IGD_HOST_SET_BLACK, &info, sizeof(info), NULL, 0) < 0)
		return CGI_ERR_FAIL;

	return 0;
}

int cgi_net_app_black_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	struct host_set_info info;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);
	CON_GET_ACT(ptr, con, info.act);
	CON_GET_INT(ptr, con, info.appid, "appid");

	if (IGD_TEST_STUDY_URL(info.appid))
		return CGI_ERR_NOSUPPORT;
	if (info.appid == IGD_CHUYUN_APPID)
		return CGI_ERR_NOSUPPORT;

	if (mu_msg(IGD_HOST_SET_APP_BLACK, &info, sizeof(info), NULL, 0) < 0)
		return CGI_ERR_FAIL;
	return 0;
}

int cgi_net_host_nick_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	int len;
	char *ptr = NULL;
	struct host_set_info info;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);

	ptr = con_value_get(con, "nick");
	if (ptr) {
		len = strlen(ptr);
		if (len >= sizeof(info.v.name)) {
			return CGI_ERR_INPUT;
		} else if (len) {
			arr_strcpy(info.v.name, ptr);
		}
	}
	if (mu_msg(IGD_HOST_SET_NICK, &info, sizeof(info), NULL, 0) < 0)
		return CGI_ERR_FAIL;
	return 0;
}

int cgi_net_app_lt_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	struct host_set_info info;
	struct time_comm *t = &info.v.time;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);
	CON_GET_ACT(ptr, con, info.act);
	CON_GET_INT(ptr, con, info.appid, "appid");
	if (IGD_TEST_STUDY_URL(info.appid))
		return CGI_ERR_NOSUPPORT;
	if (info.appid == IGD_CHUYUN_APPID)
		return CGI_ERR_NOSUPPORT;

	if (info.act == NLK_ACTION_ADD) {
		CON_GET_CHECK_INT(ptr, con, t->start_hour, "sh", 23);
		CON_GET_CHECK_INT(ptr, con, t->start_min, "sm", 59);
		CON_GET_CHECK_INT(ptr, con, t->end_hour, "eh", 23);
		CON_GET_CHECK_INT(ptr, con, t->end_min, "em", 59);
		t->day_flags = 127;
		t->loop = 0;
	}

	if (mu_msg(IGD_HOST_SET_APP_LIMIT_TIME, &info, sizeof(info), NULL, 0) < 0)
		return CGI_ERR_FAIL;
	return 0;
}

int cgi_net_study_url_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	struct host_set_info info;
	struct study_url_dump *dump;
	char *ptr;
	int start, num, nr, i;
	struct json_object *obj, *study_url, *study_urls;

	CHECK_LOGIN;
	memset(&info, 0, sizeof(info));

	CON_GET_ACT(ptr, con, info.act);
	if (info.act == NLK_ACTION_DUMP) {
		CON_GET_INT(ptr, con, start, "start");
		CON_GET_INT(ptr, con, num, "num");

		dump = malloc(sizeof(*dump)*IGD_STUDY_URL_SELF_NUM);
		if (!dump)
			return CGI_ERR_MALLOC;
		nr = mu_msg(IGD_STUDY_URL_ACTION, &info, sizeof(info),
			dump, sizeof(*dump)*IGD_STUDY_URL_SELF_NUM);
		if (nr < 0) {
			free(dump);
			CGI_PRINTF("mu msg err, ret:%d\n", nr);
			return CGI_ERR_FAIL;
		}

		study_urls = json_object_new_array();
		for (i = 0; i < nr; i++) {
			if ((i + 1) < start)
				continue;
			study_url = json_object_new_object();

			obj = json_object_new_int(dump[i].id);
			json_object_object_add(study_url, "id", obj);

			obj = json_object_new_string(dump[i].name);
			json_object_object_add(study_url, "name", obj);

			obj = json_object_new_string(dump[i].url);
			json_object_object_add(study_url, "url", obj);

			json_object_array_add(study_urls, study_url);
			num--;
			if (num <= 0)
				break;
		}
		json_object_object_add(response, "study_url", study_urls);
		free(dump);
		return 0;
	}
	return CGI_ERR_INPUT;
}

int cgi_net_host_study_url_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr;
	int nr, i, self = 0, len;
	struct study_url_dump *dump;
	struct host_set_info info;
	struct json_object *obj, *urls;
	unsigned char dataup[1024] = {0,};

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_ACT(ptr, con, info.act);
	CON_GET_MAC(ptr, con, info.mac);

	if (info.act == NLK_ACTION_DUMP) {
		dump = malloc(sizeof(*dump)*IGD_STUDY_URL_MX);
		if (!dump)
			return CGI_ERR_MALLOC;
		nr = mu_msg(IGD_HOST_STUDY_URL_ACTION, &info, sizeof(info),
			dump, sizeof(*dump)*IGD_STUDY_URL_MX);
		if (nr < 0) {
			free(dump);
			CGI_PRINTF("mu msg err, ret:%d\n", nr);
			return CGI_ERR_FAIL;
		}
		urls = json_object_new_array();
		for (i = 0; i < nr; i ++) {
			obj = json_object_new_int(dump[i].id);
			json_object_array_add(urls, obj);
		}
		json_object_object_add(response, "study_url", urls);
		free(dump);
		return 0;
	}

	if (info.act == NLK_ACTION_DEL) {
		CON_GET_INT(ptr, con, info.v.surl.id, "id");
		if (!IGD_TEST_STUDY_URL(info.v.surl.id)) {
			CGI_PRINTF("url id err, %d\n", info.v.surl.id);
			return CGI_ERR_INPUT;
		}
		if (mu_msg(IGD_HOST_STUDY_URL_ACTION, &info, sizeof(info), NULL, 0))
			return CGI_ERR_FAIL;
		if (mu_msg(IGD_STUDY_URL_ACTION, &info, sizeof(info), NULL, 0))
			return CGI_ERR_FAIL;
	} else if (info.act == NLK_ACTION_ADD) {
		CON_GET_STR(ptr, con, info.v.surl.name, "name");
		CON_GET_STR(ptr, con, info.v.surl.url, "url");
		ptr = con_value_get(con, "id");
		if (ptr) {
			info.v.surl.id = atoi(ptr);
		} else {
			self = 1;
			nr = mu_msg(IGD_STUDY_URL_ACTION, &info, sizeof(info), NULL, 0);
			if (nr == -2)
				return CGI_ERR_FULL;
			else if (nr <= 0) {
				CGI_PRINTF("add fail %d\n", nr);
				return CGI_ERR_FAIL;
			}
			info.v.surl.id = nr;
		}
		nr = mu_msg(IGD_HOST_STUDY_URL_ACTION, &info, sizeof(info), NULL, 0);
		if (nr < 0) {
			CGI_PRINTF("add fail %d\n", nr);
			return CGI_ERR_FAIL;
		}
		if (self) {
			obj = json_object_new_int(info.v.surl.id);
			json_object_object_add(response, "id", obj);
		}
	}

	if (IGD_SELF_STUDY_URL(info.v.surl.id)) {
		if (info.act == NLK_ACTION_ADD) {
			CC_PUSH2(dataup, 2, CSO_REQ_SITE_CUSTOM);
			i = 8;
			CC_PUSH4(dataup, i, info.v.surl.id);
			i += 4;;
			len = strlen(info.v.surl.url);
			CC_PUSH1(dataup, i, len);
			i += 1;
			CC_PUSH_LEN(dataup, i, info.v.surl.url, len);
			i += len;
			len = strlen(info.v.surl.name);
			CC_PUSH1(dataup, i, len);
			i += 1;
			CC_PUSH_LEN(dataup, i, info.v.surl.name, len);
			i += len;
			CC_PUSH2(dataup, 0, i);
			CC_MSG_ADD(dataup, i);
		}
	} else {
		CC_PUSH2(dataup, 2, CSO_REQ_SITE_RECOMMAND);
		i = 8;
		CC_PUSH4(dataup, i, info.v.surl.id);
		i += 4;
		CC_PUSH1(dataup, i, (info.act == NLK_ACTION_ADD) ? 1 : 0);
		i += 1;
		CC_PUSH2(dataup, 0, i);
		CC_MSG_ADD(dataup, i);
	}
	return 0;
}

int cgi_net_host_del_history_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	struct host_set_info info;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);

	if (mu_msg(IGD_HOST_DEL_HISTORY, &info, sizeof(info), NULL, 0) < 0)
		return CGI_ERR_FAIL;

	if (mu_msg(NAT_MOD_MAC_DEL, info.mac, 6, NULL, 0) < 0)
		return CGI_ERR_FAIL;
	return 0;
}

int cgi_net_host_dbg_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	if (mu_msg(IGD_HOST_DBG_FILE, NULL, 0, NULL, 0) < 0)
		return CGI_ERR_FAIL;
	return 0;
}

int cgi_sys_rconf_ver_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char buf[512] = {0,}, *ptr = NULL;
	FILE *fp = NULL;
	struct json_object *rconf, *obj;

	fp = fopen(RCONF_CHECK, "r");
	if (!fp)
		return CGI_ERR_FAIL;

	rconf = json_object_new_object();
	while(1) {
		memset(buf, 0, sizeof(buf));
		if (!fgets(buf, sizeof(buf) - 1, fp))
			break;
		ptr = strrchr(buf, '\n');
		if (ptr)
			*ptr = '\0';
		ptr = strrchr(buf, '\r');
		if (ptr)
			*ptr = '\0';
		ptr = strchr(buf, ':');
		if (!ptr)
			continue;
		*ptr = '\0';
		obj = json_object_new_string(ptr + 1);
		json_object_object_add(rconf, buf, obj);
	}
	json_object_object_add(response, "rconf", rconf);
	return 0;
}

#define FIRMWARE_FILE "/etc/firmware"
int cgi_sys_firminfo_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char buf[512] = {0,}, *ptr = NULL;
	FILE *fp = NULL;
	struct json_object *firmware, *obj;

	fp = fopen(FIRMWARE_FILE, "r");
	if (!fp)
		return CGI_ERR_FAIL;

	firmware = json_object_new_object();
	while(1) {
		memset(buf, 0, sizeof(buf));
		if (!fgets(buf, sizeof(buf) - 1, fp))
			break;
		if (!memcmp(buf, "GPIO", 4))
			continue;
		ptr = strrchr(buf, '\n');
		if (ptr)
			*ptr = '\0';
		ptr = strrchr(buf, '\r');
		if (ptr)
			*ptr = '\0';
		if (!(ptr = strchr(buf, '=')) && !(ptr = strchr(buf, ':')))
			continue;
		if (*(ptr + 1) == '\0')
			continue;
		*ptr = '\0';
		ptr++;
		if (*ptr == ' ')
			ptr++;
		obj = json_object_new_string(ptr);
		json_object_object_add(firmware, buf, obj);
	}
	json_object_object_add(response, "firmware", firmware);
	return 0;
}

int cgi_net_host_url_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr, *ptype;
	struct host_set_info info;
	struct host_url_dump *dump;
	int nr, i;
	struct json_object *url_type, *obj;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);
	CON_GET_ACT(ptr, con, info.act);
	ptype = con_value_get(con, "type");
	if (!ptype) {
		CGI_PRINTF("type is null\n");
		return CGI_ERR_INPUT;
	} else if (!strcmp(ptype, "black")) {
		info.v.bw_url.type = RLT_URL_BLACK;
	} else if (!strcmp(ptype, "white")) {
		info.v.bw_url.type = RLT_URL_WHITE;
	} else {
		CGI_PRINTF("type is err, %s\n", ptype);
		return CGI_ERR_INPUT;
	}
	if (info.act == NLK_ACTION_DUMP) {
		dump = malloc(sizeof(*dump)*IGD_HOST_URL_MX);
		if (!dump)
			return CGI_ERR_MALLOC;
		nr = mu_msg(IGD_HOST_URL_ACTION, &info, sizeof(info),
			dump, IGD_HOST_URL_MX*sizeof(*dump));
		if (nr < 0) {
			free(dump);
			CGI_PRINTF("mu msg err, ret:%d\n", nr);
			return CGI_ERR_FAIL;
		}
		url_type = json_object_new_array();
		for (i = 0; i < nr; i++) {
			obj = json_object_new_string(dump[i].url);
			json_object_array_add(url_type, obj);
		}
		json_object_object_add(response, ptype, url_type);
		free(dump);
		return 0;
	}
	CON_GET_STR(ptr, con, info.v.bw_url.url, "url");
 	nr = mu_msg(IGD_HOST_URL_ACTION, &info, sizeof(info), NULL, 0);
	if ((nr == -2) && (info.act == NLK_ACTION_ADD))
		return CGI_ERR_FULL;
	if ((nr == -2) && (info.act == NLK_ACTION_DEL))
		return CGI_ERR_NONEXIST;
	else if (nr < 0)
		return CGI_ERR_FAIL;
	return 0;
}

int cgi_net_study_url_var_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *opt, grp_name[32];
	struct in_addr ip;
	struct host_set_info info;
	struct study_url_dump *dump;
	struct json_object *url_group, *url_obj[10], *obj_one, *obj;
	int nr, i, cid;

	opt = con_value_get(con, "opt");
	if (!opt)
		return CGI_ERR_INPUT;

	if (con && con->ip_from) {
		CGI_PRINTF("host ip:%s\n", con->ip_from);
		inet_aton(con->ip_from, &ip);
	} else {
		return CGI_ERR_FAIL;
	}

	nr = mu_msg(IGD_HOST_IP2MAC, &ip, sizeof(ip), info.mac, sizeof(info.mac));
	if (nr < 0)
		return CGI_ERR_FAIL;
	CGI_PRINTF("host mac:%s\n", cgi_mac2str(info.mac));

	info.act = NLK_ACTION_DUMP;
	dump = malloc(sizeof(*dump)*IGD_STUDY_URL_MX);
	if (!dump)
		return CGI_ERR_MALLOC;
	nr = mu_msg(IGD_HOST_STUDY_URL_ACTION, &info, sizeof(info),
		dump, sizeof(*dump)*IGD_STUDY_URL_MX);
	if (nr < 0) {
		free(dump);
		CGI_PRINTF("mu msg err, ret:%d\n", nr);
		return CGI_ERR_FAIL;
	}

	memset(url_obj, 0, sizeof(url_obj));
	url_group = json_object_new_object();
	for (i = 0; i < nr; i++) {
		cid = (dump[i].id/(L7_APP_MX/10) - L7_MID_HTTP*10);
		if (cid < 0 || cid > 10)
			continue;
		if (!url_obj[cid])
			url_obj[cid] = json_object_new_array();
		if (cid == 0) {
			obj_one = json_object_new_object();
			obj = json_object_new_int(dump[i].id);
			json_object_object_add(obj_one, "id", obj);
			obj = json_object_new_string(dump[i].name);
			json_object_object_add(obj_one, "name", obj);
			obj = json_object_new_string(dump[i].url);
			json_object_object_add(obj_one, "url", obj);
			json_object_array_add(url_obj[cid], obj_one);
		} else {
			obj_one = json_object_new_int(dump[i].id);
			json_object_array_add(url_obj[cid], obj_one);
		}
	}

	for (i = 0; i < 10; i++) {
		if (!url_obj[i])
			continue;
		snprintf(grp_name, sizeof(grp_name) - 1, "T_%d", i);
		json_object_object_add(url_group, grp_name, url_obj[i]);
	}
	json_object_object_add(response, opt, url_group);
	free(dump);
	return 0;
}

int cgi_net_host_push_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	struct host_set_info info;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);
	CON_GET_ACT(ptr, con, info.act);

	if (mu_msg(IGD_HOST_SET_ONLINE_PUSH, &info, sizeof(info), NULL, 0) < 0)
		return CGI_ERR_FAIL;

	return 0;
}

int cgi_net_new_push_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	struct host_set_info info, back;
	struct json_object *obj;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_ACT(ptr, con, info.act);

	if (mu_msg(IGD_HOST_SET_NEW_PUSH, &info, sizeof(info), &back, sizeof(back)) < 0)
		return CGI_ERR_FAIL;

	if (info.act == NLK_ACTION_DUMP) {
		obj = json_object_new_int(back.v.new_push);
		json_object_object_add(response, "new_push", obj);
	}
	return 0;
}

int cgi_net_host_pic_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	struct host_set_info info;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_MAC(ptr, con, info.mac);
	CON_GET_INT(ptr, con, info.v.pic, "pic");

	if (mu_msg(IGD_HOST_SET_PIC, &info, sizeof(info), NULL, 0) < 0)
		return CGI_ERR_FAIL;

	return 0;
}

int cgi_net_host_filter_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL, str[512];
	struct host_filter_info info, dump[IGD_HOST_FILTER_MX];
	struct json_object *arr, *obj, *tmp, *week;
	int i, j, nr;
	struct in_addr ip;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_ACT(ptr, con, info.act);

	if (info.act == NLK_ACTION_DUMP) {
		nr = mu_msg(IGD_HOST_SET_FILTER,
			&info, sizeof(info), dump, sizeof(dump));
		if (nr < 0)
			return CGI_ERR_FAIL;

		arr = json_object_new_array();
		for (i = 0; i < nr; i++) {
			obj = json_object_new_object();

			tmp = json_object_new_int(dump[i].enable);
			json_object_object_add(obj, "enable", tmp);

			tmp = json_object_new_int(dump[i].time.start_hour);
			json_object_object_add(obj, "sh", tmp);

			tmp = json_object_new_int(dump[i].time.start_min);
			json_object_object_add(obj, "sm", tmp);

			tmp = json_object_new_int(dump[i].time.end_hour);
			json_object_object_add(obj, "eh", tmp);

			tmp = json_object_new_int(dump[i].time.end_min);
			json_object_object_add(obj, "em", tmp);

			week = json_object_new_array();
			for (j = 0; j < 7; j++) {
				if (!dump[i].time.loop || 
					!CGI_BIT_TEST(dump[i].time.day_flags, j))
					continue;
				tmp = json_object_new_int(j);
				json_object_array_add(week, tmp);
			}
			json_object_object_add(obj, "week", week);

			tmp = json_object_new_int(dump[i].arg.target);
			json_object_object_add(obj, "type", tmp);

			tmp = json_object_new_int(dump[i].arg.l4.protocol);
			json_object_object_add(obj, "proto", tmp);

			tmp = json_object_new_int(dump[i].host.type);
			json_object_object_add(obj, "host_type", tmp);
			if (dump[i].host.type == INET_HOST_MAC) {
				snprintf(str, sizeof(str),
					"%02X:%02X:%02X:%02X:%02X:%02X",
					MAC_SPLIT(dump[i].host.mac));
				tmp = json_object_new_string(str);
				json_object_object_add(obj, "host_mac", tmp);
			} else if (dump[i].host.type == INET_HOST_STD) {
				ip.s_addr = dump[i].host.addr.start;
				tmp = json_object_new_string(inet_ntoa(ip));
				json_object_object_add(obj, "host_sip", tmp);

				ip.s_addr = dump[i].host.addr.end;
				tmp = json_object_new_string(inet_ntoa(ip));
				json_object_object_add(obj, "host_eip", tmp);
			}

			tmp = json_object_new_int(dump[i].arg.l4.type);
			json_object_object_add(obj, "l4_type", tmp);
			if ((dump[i].arg.l4.type == INET_L4_DST)
				|| (dump[i].arg.l4.type == INET_L4_ALL)) {
				tmp = json_object_new_int(dump[i].arg.l4.dst.start);
				json_object_object_add(obj, "l4_sdport", tmp);
			
				tmp = json_object_new_int(dump[i].arg.l4.dst.end);
				json_object_object_add(obj, "l4_edport", tmp);
			}
			if ((dump[i].arg.l4.type == INET_L4_SRC)
				|| (dump[i].arg.l4.type == INET_L4_ALL)) {
				tmp = json_object_new_int(dump[i].arg.l4.src.start);
				json_object_object_add(obj, "l4_ssport", tmp);
			
				tmp = json_object_new_int(dump[i].arg.l4.src.end);
				json_object_object_add(obj, "l4_esport", tmp);
			}

			json_object_array_add(arr, obj);
		}
		json_object_object_add(response, "filter", arr);
		return 0;
	} else if ((info.act == NLK_ACTION_ADD)
	 || (info.act == NLK_ACTION_DEL)) {
		nr = cgi_get_time_comm(&info.time, con);
		if (nr)
			return nr;
		CON_GET_INT(ptr, con, info.enable, "enable");
		CON_GET_INT(ptr, con, info.arg.target, "type");
		CON_GET_INT(ptr, con, info.arg.l4.protocol, "proto");
		CON_GET_INT(ptr, con, info.host.type, "host_type");
		CON_GET_INT(ptr, con, info.arg.l4.type, "l4_type");

		if (info.host.type == INET_HOST_MAC) {
			CON_GET_STR(ptr, con, str, "host_mac");
			if (cgi_str2mac(str, info.host.mac))
				return CGI_ERR_INPUT;
		} else if (info.host.type == INET_HOST_STD) {
			ptr = con_value_get(con, "host_sip");
			if (!ptr)
				return CGI_ERR_INPUT;
			info.host.addr.start = inet_addr(ptr);

			ptr = con_value_get(con, "host_eip");
			if (!ptr)
				return CGI_ERR_INPUT;
			info.host.addr.end = inet_addr(ptr);
		}

		if ((info.arg.l4.type == INET_L4_DST)
			|| (info.arg.l4.type == INET_L4_ALL)) {
			CON_GET_INT(ptr, con, info.arg.l4.dst.start, "l4_sdport");
			CON_GET_INT(ptr, con, info.arg.l4.dst.end, "l4_edport");
		}
		if ((info.arg.l4.type == INET_L4_SRC)
			|| (info.arg.l4.type == INET_L4_ALL)) {
			CON_GET_INT(ptr, con, info.arg.l4.src.start, "l4_ssport");
			CON_GET_INT(ptr, con, info.arg.l4.src.end, "l4_esport");
		}

		nr = mu_msg(IGD_HOST_SET_FILTER, &info, sizeof(info), NULL, 0);
		if (!nr)
			return 0;
		if (info.act == NLK_ACTION_ADD) {
			if (nr == -3)
				return CGI_ERR_FULL;
			else if (nr == -2)
				return CGI_ERR_EXIST;
		} else if (info.act == NLK_ACTION_DEL) {
			if (nr == -2)
				return CGI_ERR_NONEXIST;
		}
		return CGI_ERR_FAIL;
	}
	return CGI_ERR_INPUT;
}

int cgi_sys_led_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	int act = 0, led = 0;
	char *ptr = NULL;
	struct json_object *obj = NULL;

	CHECK_LOGIN;

	CON_GET_ACT(ptr, con, act);
	if (mu_msg(SYSTME_MOD_SET_LED, &act, sizeof(act), &led, sizeof(led)) < 0)
		return CGI_ERR_FAIL;
	if (act == NLK_ACTION_ADD)
		_ADD_SYS_LOG("路由器灯光开启");
	else if (act == NLK_ACTION_DEL)
		_ADD_SYS_LOG("路由器灯光关闭");
	if (act == NLK_ACTION_DUMP) {
		obj = json_object_new_int(led);
		json_object_object_add(response, "led", obj);
	}
	return 0;
}

int cgi_host_nat_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	int act = 0, i = 0, num = 0;
	struct igd_nat_param inp[IGD_NAT_MAX];
	struct json_object *object, *array, *t;

	CHECK_LOGIN;

	CON_GET_ACT(ptr, con, act);
	CON_GET_MAC(ptr, con, inp[0].mac);
	if (act == NLK_ACTION_DUMP) {
		num = mu_msg(NAT_MOD_DUMP, inp[0].mac, sizeof(inp[0].mac), inp, sizeof(inp));
		if (num < 0)
			return CGI_ERR_FAIL;
		array = json_object_new_array();
		for (i = 0; i < num; i++) {
 			object = json_object_new_object();

			t = json_object_new_int(inp[i].out_port);
			json_object_object_add(object, "out_port", t);

			t = json_object_new_int(inp[i].out_port_end);
			json_object_object_add(object, "out_port_end", t);

			t = json_object_new_int(inp[i].in_port);
			json_object_object_add(object, "in_port", t);

			t = json_object_new_int(inp[i].in_port_end);
			json_object_object_add(object, "in_port_end", t);

			t = json_object_new_int(inp[i].proto);
			json_object_object_add(object, "proto", t);

			t = json_object_new_int(inp[i].enable);
			json_object_object_add(object, "enable", t);

			json_object_array_add(array, object);
		}
		json_object_object_add(response, "nat", array);
	} else {
		CON_GET_INT(ptr, con, inp[0].out_port, "out_port");
		CON_GET_INT(ptr, con, inp[0].in_port, "in_port");
		CON_GET_INT(ptr, con, inp[0].proto, "proto");
		CON_GET_INT(ptr, con, inp[0].enable, "enable");

		ptr = con_value_get(con, "out_port_end");
		if (ptr) {
			inp[0].out_port_end = atoi(ptr);
		} else {
			inp[0].out_port_end = inp[0].out_port;
		}
		ptr = con_value_get(con, "in_port_end");
		if (ptr) {
			inp[0].in_port_end = atoi(ptr);
		} else {
			inp[0].in_port_end = inp[0].in_port;
		}
		if (inp[0].out_port_end < inp[0].out_port)
			return CGI_ERR_INPUT;
		if (inp[0].in_port_end < inp[0].in_port)
			return CGI_ERR_INPUT;
		if ((inp[0].in_port_end - inp[0].in_port)
			!= (inp[0].out_port_end - inp[0].out_port))
			return CGI_ERR_INPUT;

		if (act == NLK_ACTION_ADD) {
			num = mu_msg(NAT_MOD_ADD, &inp[0], sizeof(inp[0]), NULL, 0);
			if (num == -2) {
				return (act == NLK_ACTION_ADD) ? CGI_ERR_EXIST : CGI_ERR_NONEXIST;
			} else if (num == -3) {
				return CGI_ERR_FULL;
			} else if (num < 0) {
				return CGI_ERR_FAIL;
			}
		} else if (act == NLK_ACTION_DEL) {
			num = mu_msg(NAT_MOD_DEL, &inp[0], sizeof(inp[0]), NULL, 0);
			if (num == -2) {
				return (act == NLK_ACTION_ADD) ? CGI_ERR_EXIST : CGI_ERR_NONEXIST;
			} else if (num == -3) {
				return CGI_ERR_FULL;
			} else if (num < 0) {
				return CGI_ERR_FAIL;
			}
		} else if (act == NLK_ACTION_MOD) {
			if (mu_msg(NAT_MOD_DEL, &inp[0], sizeof(inp[0]), NULL, 0) < 0) {
				CGI_PRINTF("mod del fail\n");
				return CGI_ERR_FAIL;
			}
			if (mu_msg(NAT_MOD_ADD, &inp[0], sizeof(inp[0]), NULL, 0) < 0) {
				CGI_PRINTF("mod add fail\n");
				return CGI_ERR_FAIL;
			}
		} else {
			return CGI_ERR_INPUT;
		}

	}
	return 0;
}

int cgi_sys_account_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	struct sys_account info, pre;

	CHECK_LOGIN;

	if (connection_is_set(con)) {
		CON_GET_STR(ptr, con, info.user, "user");
		CON_GET_STR(ptr, con, info.password, "password");
		if (mu_msg(SYSTME_MOD_GET_ACCOUNT, NULL, 0, &pre, sizeof(pre)))
			return CGI_ERR_FAIL;
		if (!strcmp(pre.password, info.password))
			return 0;
		if (mu_msg(SYSTME_MOD_SET_ACCOUNT, &info, sizeof(info), NULL, 0))
			return CGI_ERR_FAIL;
		_ADD_SYS_LOG("用户修改当前登录密码");
		server_clean(srv);
	}
	return 0;
}

int cgi_sys_guest_account_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	int act, index = -1;
	struct sys_guest guest;
	struct json_object *obj, *array, *t;

	CHECK_LOGIN;
	if (connection_is_set(con)) {
		CON_GET_ACT(ptr, con, act);
		memset(&guest, 0x0, sizeof(guest));
		if (act == NLK_ACTION_DEL) {
			CON_GET_INT(ptr, con, guest.index, "index");
			if (mu_msg(SYSTEM_MOD_GUEST_SET, &guest, sizeof(guest), NULL, 0))
				return CGI_ERR_FAIL;
			return 0;
		} else if (act == NLK_ACTION_ADD)
			guest.index = -1;
		else if (act == NLK_ACTION_MOD)
			CON_GET_INT(ptr, con, guest.index, "index");
		else
			return CGI_ERR_INPUT;
		guest.invalid = 1;
		
		CON_GET_STR(ptr, con, guest.user, "user");
		CON_GET_STR(ptr, con, guest.password, "password");
		if (mu_msg(SYSTEM_MOD_GUEST_SET, &guest, sizeof(guest), NULL, 0))
			return CGI_ERR_FAIL;
		server_clean(srv);
		return 0;
	}
	array = json_object_new_array();
	for (index = 0; index < SYS_GUEST_MX; index++) {
		if (mu_msg(SYSTEM_MOD_GUEST_GET, &index, sizeof(int), &guest, sizeof(guest)))
			continue;
		t = json_object_new_object();
		obj = json_object_new_int(guest.index);
		json_object_object_add(t, "index", obj);
		
		obj = json_object_new_int(guest.readonly);
		json_object_object_add(t, "readonly", obj);
		
		obj = json_object_new_string(guest.user);
		json_object_object_add(t, "user", obj);
		
		obj = json_object_new_string(guest.password);
		json_object_object_add(t, "password", obj);
		
		json_object_array_add(array, t);
	}
	json_object_object_add(response, "guests", array);
	return 0;
}

//POST /protocol.csp?fname=system&opt=sys_cmd&function=set&cmd=xxxx
int cgi_system_cmdline_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *cmd;

	CHECK_LOGIN;

	if (!con->ip_from ||
		strcmp(con->ip_from, "127.0.0.1")) {
		return CGI_ERR_NOSUPPORT;
	}

	cmd = con_value_get(con, "cmd");
	if (!cmd || !strlen(cmd)) {
		CGI_PRINTF("%s is NULL\n", "cmd");
		return CGI_ERR_INPUT;
	}

	CGI_PRINTF("[%s]\n", cmd);
	return mu_msg(SYSTME_MOD_SYS_CMD, cmd, strlen(cmd) + 1, NULL, 0);
}

#define SYSTEM_APP_NUM  500
struct sys_app_info {
	uint32_t appid;
	unsigned long down_speed;
	unsigned long up_speed;
	uint64_t up_bytes;
	uint64_t down_bytes;
	unsigned long flag[BITS_TO_LONGS(HARF_MAX)];
};

static int get_appall_info(int get_num, struct json_object *response)
{
	int host_nr, app_nr, sys_app_nr, i, j, k;
	struct host_dump_info host[IGD_HOST_MX];
	struct host_app_dump_info host_app[IGD_HOST_APP_MX];
	struct sys_app_info *sys_app, *sa = NULL;
	struct json_object *t_arr, *t_obj, *obj;
	char flag[HARF_MAX + 1] = {0,};

	host_nr = mu_msg(IGD_HOST_DUMP, NULL, 0, host, sizeof(host));
	if (host_nr < 0) {
		CGI_PRINTF("dump host err, ret:%d\n", host_nr);
		return CGI_ERR_FAIL;
	}

	sys_app = malloc(sizeof(*sys_app) * SYSTEM_APP_NUM);
	if (!sys_app) {
		CGI_PRINTF("malloc fail, %d\n", sizeof(*sys_app) * SYSTEM_APP_NUM);
		return CGI_ERR_MALLOC;
	}
	sys_app_nr = 0;
	memset(sys_app, 0, sizeof(*sys_app) * SYSTEM_APP_NUM);

	for (i = 0; i < host_nr; i++) {
		app_nr = mu_msg(IGD_HOST_APP_DUMP,
			host[i].mac, sizeof(host[i].mac), host_app, sizeof(host_app));
		if (app_nr < 0) {
			CGI_PRINTF("dump app err, ret:%d, %s\n", 
				app_nr, cgi_mac2str(host[i].mac));
			continue;
		}
		for (j = 0; j < app_nr; j++) {
			if (sys_app_nr >= SYSTEM_APP_NUM)
				break;
			sa = NULL;
			for (k = 0; k < sys_app_nr; k++) {
				if (sys_app[k].appid == host_app[j].appid) {
					sa = &sys_app[k];
					break;
				}
			}
			if (!sa) {
				sa = &sys_app[sys_app_nr];
				sys_app_nr++;
			}
			sa->appid = host_app[j].appid;
			sa->up_speed += host_app[j].up_speed;
			sa->down_speed += host_app[j].down_speed;
			sa->up_bytes += host_app[j].up_bytes;
			sa->down_bytes += host_app[j].down_bytes;
			for (k = 0; k < BITS_TO_LONGS(HARF_MAX); k++)
				sa->flag[k] |= host_app[j].flag[k];
		}
	}

	if (get_num)
		return sys_app_nr;

	t_arr = json_object_new_array();
	for (i = 0; i < sys_app_nr; i++) {
		t_obj = json_object_new_object();

		obj = json_object_new_int(sys_app[i].appid);
		json_object_object_add(t_obj, "appid", obj);

		obj = json_object_new_int(sys_app[i].up_speed/1024);
		json_object_object_add(t_obj, "up_speed", obj);
		obj = json_object_new_int(sys_app[i].down_speed/1024);
		json_object_object_add(t_obj, "down_speed", obj);

		obj = json_object_new_uint64(sys_app[i].up_bytes/1024);
		json_object_object_add(t_obj, "up_bytes", obj);
		obj = json_object_new_uint64(sys_app[i].down_bytes/1024);
		json_object_object_add(t_obj, "down_bytes", obj);

		for (k = 0; k < HARF_MAX; k++)
			flag[k] = igd_test_bit(k, sys_app[i].flag) ? 'T' : 'F';
		flag[k] = '\0';

		obj = json_object_new_string(flag);
		json_object_object_add(t_obj, "flag", obj);

		json_object_array_add(t_arr, t_obj);
	}
	json_object_object_add(response, "sys_app", t_arr);
	free(sys_app);
	return 0;
}

int cgi_sys_app_info_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	CHECK_LOGIN;

	return get_appall_info(0, response);
}

int cgi_net_intercept_url_black_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	struct host_set_info info, url;
	struct host_dump_info host[IGD_HOST_MX];
	struct host_intercept_url_dump url_dump[IGD_HOST_INTERCEPT_URL_MX];
	int host_nr, i, url_nr, j;
	char *ptr, *pmac, paddr[64], key[64];
	struct json_object *t_arr, *t_obj, *obj;

	CHECK_LOGIN;

	memset(&info, 0, sizeof(info));
	CON_GET_ACT(ptr, con, info.act);
	if (info.act == NLK_ACTION_DUMP) {
		pmac = con_value_get(con, "mac");
		if (pmac)
			pmac = cgi_str2mac(pmac, info.mac) ? NULL : (char *)info.mac;

		host_nr = mu_msg(IGD_HOST_DUMP, pmac, pmac ? 6 : 0, host, sizeof(host));
		if (host_nr < 0) {
			CGI_PRINTF("dump host err, ret:%d\n", host_nr);
			return CGI_ERR_FAIL;
		} else if (!host_nr && pmac) {
			host_nr = 1;
		}

		t_arr = json_object_new_array();
		for (i = 0; i < host_nr; i++) {
			memcpy(url.mac, host[i].mac, sizeof(url.mac));
			url.act = NLK_ACTION_DUMP;
			url_nr = mu_msg(IGD_HOST_INTERCEPT_URL_BLACK_ACTION,
					&url, sizeof(url), url_dump, sizeof(url_dump));
			if (url_nr < 0)
				continue;
			for (j = 0; j < url_nr; j++) {
				t_obj = json_object_new_object();

				obj = json_object_new_string(cgi_mac2str(host[i].mac));
				json_object_object_add(t_obj, "mac", obj);

				obj = json_object_new_string(host[i].name);
				json_object_object_add(t_obj, "name", obj);

				obj = json_object_new_string(url_dump[j].url);
				json_object_object_add(t_obj, "url", obj);

				obj = json_object_new_int(url_dump[j].time);
				json_object_object_add(t_obj, "time", obj);

				obj = json_object_new_string(url_dump[j].type);
				json_object_object_add(t_obj, "type", obj);

				obj = json_object_new_string(url_dump[j].type_en);
				json_object_object_add(t_obj, "type_en", obj);

				obj = json_object_new_int(url_dump[j].sport);
				json_object_object_add(t_obj, "sport", obj);

				obj = json_object_new_int(url_dump[j].dport);
				json_object_object_add(t_obj, "dport", obj);

				obj = json_object_new_string(inet_ntoa(url_dump[j].saddr));
				json_object_object_add(t_obj, "saddr", obj);

				obj = json_object_new_string(inet_ntoa(url_dump[j].daddr));
				json_object_object_add(t_obj, "daddr", obj);

				json_object_array_add(t_arr, t_obj);
			}
		}
		json_object_object_add(response, "intercept_url_black", t_arr);
		return 0;
	}

	for (i = 0; i < 16; i++) {
		snprintf(key, sizeof(key), "mac_%d", i + 1);
		pmac = con_value_get(con, key);
		if (!pmac || cgi_str2mac(pmac, info.mac)) {
			CGI_PRINTF("mac is %p, i:%d\n", pmac, i);
			break;
		}
		snprintf(key, sizeof(key), "url_%d", i + 1);
		CON_GET_STR(ptr, con, info.v.intercept_url.url, key);

		snprintf(key, sizeof(key), "type_%d", i + 1);
		CON_GET_STR(ptr, con, info.v.intercept_url.type, key);

		snprintf(key, sizeof(key), "type_en_%d", i + 1);
		CON_GET_STR(ptr, con, info.v.intercept_url.type_en, key);

		snprintf(key, sizeof(key), "time_%d", i + 1);
		CON_GET_INT(ptr, con, info.v.intercept_url.time, key);

		snprintf(key, sizeof(key), "dport_%d", i + 1);
		CON_GET_INT(ptr, con, info.v.intercept_url.dport, key);

		snprintf(key, sizeof(key), "sport_%d", i + 1);
		CON_GET_INT(ptr, con, info.v.intercept_url.sport, key);

		snprintf(key, sizeof(key), "saddr_%d", i + 1);
		CON_GET_STR(ptr, con, paddr, key);
		inet_aton(paddr, &info.v.intercept_url.saddr);

		snprintf(key, sizeof(key), "daddr_%d", i + 1);
		CON_GET_STR(ptr, con, paddr, key);
		inet_aton(paddr, &info.v.intercept_url.daddr);

		url_nr = mu_msg(IGD_HOST_INTERCEPT_URL_BLACK_ACTION, &info, sizeof(info), NULL, 0);
		if (url_nr == -2)
			return CGI_ERR_FULL;
		else if (url_nr < 0)
			return CGI_ERR_FAIL;
	}
	return 0;
}

int cgi_net_abandon_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	return 0;
}

int cgi_net_app_queue_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	int i = 0;
	char *ptr, *pmac, *pqueue;
	unsigned char mid[L7_MID_MX];
	struct host_set_info info;
	struct json_object *t_arr, *t_obj;

	CHECK_LOGIN;

	memset(&mid, 0, sizeof(mid));
	memset(&info, 0, sizeof(info));

	CON_GET_ACT(ptr, con, info.act);
	pqueue = con_value_get(con, "queue");
	if (pqueue) {
		ptr = pqueue;
		while (ptr) {
			info.v.mid[i] = atoi(ptr);
			if (info.v.mid[i] < L7_MID_MX) {
				i++;
			} else {
				CGI_PRINTF("mid err, %s\n", pqueue);
				return CGI_ERR_INPUT;
			}
			ptr = strchr(ptr, ',');
			if (ptr)
				ptr++;
		}
	}
	pmac = con_value_get(con, "mac");
	if (pmac) {
		cgi_str2mac(pmac, info.mac);
		i = mu_msg(IGD_HOST_APP_MOD_QUEUE, &info, sizeof(info), mid, sizeof(mid));
	} else {
		i = mu_msg(IGD_APP_MOD_QUEUE, &info, sizeof(info), mid, sizeof(mid));
	}
	if (i < 0) {
		CGI_PRINTF("mac:%s, ret:%d\n", pmac ? pmac : "", i);
		return CGI_ERR_FAIL;
	}
	if (info.act != NLK_ACTION_DUMP)
		return 0;

	t_arr = json_object_new_array();
	for (i = 0; i < L7_MID_MX; i++) {
		if (!mid[i])
			continue;
		t_obj = json_object_new_int(mid[i]);
		json_object_array_add(t_arr, t_obj);
	}
	json_object_object_add(response, "queue", t_arr);
	return 0;
}

int cgi_up_ready_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	CHECK_LOGIN;

#ifdef FLASH_4_RAM_32
	system("up_ready.sh");
#endif
	return 0;
}

int pro_reset_first_login_handler(server_t *srv, connection_t *con, struct json_object *response)
{

#ifdef ALI_REQUIRE_SWITCH
	if (mu_msg(ALI_MOD_GET_RESET, NULL, 0, NULL, 0))
		return 0;
#endif
	return CGI_ERR_FAIL;
}

/*interface module cgi*/
int pro_net_wan_set_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int uid = 0;
	int mode = 0;
	int mtu = 0;
	char macstr[18];
	struct in_addr ip;
	struct host_info info;
	struct iface_conf config;
	struct json_object *obj = NULL;
	struct in_addr *dnsip = NULL;
	unsigned char mac[ETH_ALEN];
	
	char *mode_s = NULL,*mac_s = NULL, *ip_s = NULL;
	char *mask_s = NULL, *gw_s = NULL, *dns_s = NULL, *mtu_s = NULL;
	char *user_s = NULL, *passwd_s = NULL, *status = NULL, *dns1_s = NULL;
	char *channel_s = NULL, *ssid_s = NULL, *security_s = NULL, *key_s = NULL, *pppoe_server = NULL;
	char *speed_s = NULL;
	
	if(connection_is_set(con)) {
#ifdef ALI_REQUIRE_SWITCH
		int ali_flag = 0;
		if (mu_msg(ALI_MOD_GET_RESET, NULL, 0, NULL, 0))
			mu_msg(ALI_MOD_GET_RESET, &ali_flag, sizeof(ali_flag), NULL, 0);
#endif
		status = uci_getval("qos_rule", "status", "status");
		if (NULL == status)
			return CGI_ERR_FAIL;
		if (status[0] == '1') {
			free(status);
			CHECK_LOGIN;
		}
		mac_s = con_value_get(con, "mac");
		if (NULL == mac_s || checkmac(mac_s))
			return CGI_ERR_INPUT;
		parse_mac(mac_s, mac);
		mode_s = con_value_get(con, "mode");
		if (NULL == mode_s)
			return CGI_ERR_INPUT;
		mode = atoi(mode_s);
		if (mode < MODE_DHCP || mode > MODE_3G)
			return CGI_ERR_INPUT;
		uid = 1;
		memset(&config, 0x0, sizeof(config));
		if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &config, sizeof(config)))
			return CGI_ERR_FAIL;
		if (config.isbridge)
			return CGI_ERR_NOSUPPORT;
		memcpy(config.mac, mac, ETH_ALEN);

		speed_s = con_value_get(con, "speed");
		if (speed_s && strlen(speed_s)) {
			config.speed = atoi(speed_s);
			switch(config.speed) {
			case SPEED_DUPLEX_AUTO:
			case SPEED_DUPLEX_10H:
			case SPEED_DUPLEX_10F:
			case SPEED_DUPLEX_100H:
			case SPEED_DUPLEX_100F:
				break;
			default:
				return CGI_ERR_INPUT;
			}
		}
		switch (mode) {
		case MODE_DHCP:
			config.mode = MODE_DHCP;
			mtu_s = con_value_get(con, "mtu");
			if (mtu_s)
				config.dhcp.mtu = atoi(mtu_s);
			else if (!config.dhcp.mtu)
				config.dhcp.mtu = 1500;
			if (config.dhcp.mtu > 1500 || config.dhcp.mtu < 1000)
				return CGI_ERR_INPUT;
			dns_s = con_value_get(con, "dns");
			if (dns_s) {
				if (!strlen(dns_s))
					config.dhcp.dns[0].s_addr = 0;
				if (strlen(dns_s) > 0 && checkip(dns_s))
					return CGI_ERR_INPUT;
				inet_aton(dns_s, &config.dhcp.dns[0]);
			}
			dns1_s = con_value_get(con, "dns1");
			if (dns1_s) {
				if (!strlen(dns1_s))
					config.dhcp.dns[1].s_addr = 0;
				if (strlen(dns1_s) > 0 && checkip(dns1_s))
					return CGI_ERR_INPUT;
				inet_aton(dns1_s, &config.dhcp.dns[1]);
			}
			break;
		case MODE_PPPOE:
			config.mode = MODE_PPPOE;
			user_s = con_value_get(con, "user");
			passwd_s = con_value_get(con, "passwd");
			pppoe_server = con_value_get(con, "pppoe_server");
			if (NULL == user_s || NULL == passwd_s)
				return CGI_ERR_INPUT;
			if (!strlen(user_s) || (strlen(user_s) > sizeof(config.pppoe.user) - 1))
				return CGI_ERR_INPUT;
			if (!strlen(passwd_s) || (strlen(passwd_s) > sizeof(config.pppoe.passwd) - 1))
				return CGI_ERR_INPUT;
			if (pppoe_server) {
				if(strlen(pppoe_server) > (sizeof(config.pppoe.pppoe_server) - 1))
					return CGI_ERR_INPUT;
				arr_strcpy(config.pppoe.pppoe_server, pppoe_server);
			}	
			mtu_s = con_value_get(con, "mtu");
			if (mtu_s)
				config.pppoe.comm.mtu = atoi(mtu_s);
			else if (!config.pppoe.comm.mtu)
				config.pppoe.comm.mtu = 1480;
			if (config.pppoe.comm.mtu > 1492 || config.pppoe.comm.mtu < 1000)
				return CGI_ERR_INPUT;
			dns_s = con_value_get(con, "dns");
			if (dns_s) {
				if (!strlen(dns_s))
					config.pppoe.comm.dns[0].s_addr = 0;
				if (strlen(dns_s) > 0 && checkip(dns_s))
					return CGI_ERR_INPUT;
				inet_aton(dns_s, &config.pppoe.comm.dns[0]);
			}
			dns1_s = con_value_get(con, "dns1");
			if (dns1_s) {
				if (!strlen(dns1_s))
					config.pppoe.comm.dns[1].s_addr = 0;
				if (strlen(dns1_s) > 0 && checkip(dns1_s))
					return CGI_ERR_INPUT;
				inet_aton(dns1_s, &config.pppoe.comm.dns[1]);
			}
			arr_strcpy(config.pppoe.user, user_s);
			arr_strcpy(config.pppoe.passwd, passwd_s);
			break;
		case MODE_STATIC:
			ip_s = con_value_get(con, "ip");
			gw_s = con_value_get(con, "gw");
			mask_s = con_value_get(con, "mask");
			if (ip_s == NULL || gw_s == NULL || mask_s == NULL)
				return CGI_ERR_INPUT;
			if (checkip(ip_s) || checkip(gw_s) || checkip(mask_s))
				return CGI_ERR_INPUT;
			mtu_s = con_value_get(con, "mtu");
			if (mtu_s)
				config.statip.comm.mtu = atoi(mtu_s);
			else if (!config.statip.comm.mtu)
				config.statip.comm.mtu = 1500;
			if (config.statip.comm.mtu > 1500 || config.statip.comm.mtu < 1000)
				return CGI_ERR_INPUT;
			dns_s = con_value_get(con, "dns");
			if (dns_s) {
				if (!strlen(dns_s))
					config.statip.comm.dns[0].s_addr = 0;
				if (strlen(dns_s) > 0 && checkip(dns_s))
					return CGI_ERR_INPUT;
				inet_aton(dns_s, &config.statip.comm.dns[0]);
			}
			dns1_s = con_value_get(con, "dns1");
			if (dns1_s) {
				if (!strlen(dns1_s))
					config.statip.comm.dns[1].s_addr = 0;
				if (strlen(dns1_s) > 0 && checkip(dns1_s))
					return CGI_ERR_INPUT;
				inet_aton(dns1_s, &config.statip.comm.dns[1]);
			}	
			config.mode = MODE_STATIC;
			inet_aton(ip_s, &config.statip.ip);
			inet_aton(mask_s, &config.statip.mask);
			inet_aton(gw_s, &config.statip.gw);
			if (checkmask(ntohl(config.statip.mask.s_addr)))
				return CGI_ERR_INPUT;
			if (ip_check2(config.statip.ip.s_addr, config.statip.gw.s_addr, config.statip.mask.s_addr))
				return CGI_ERR_INPUT;
			break;
		case MODE_WISP:
			config.mode = MODE_WISP;
			channel_s = con_value_get(con, "channel");
			ssid_s = con_value_get(con, "ssid");
			security_s = con_value_get(con, "security");
			key_s = con_value_get(con, "key");
			key_s = (key_s && strlen(key_s)) ? key_s : "NONE";
			if (channel_s == NULL || ssid_s == NULL || security_s == NULL || key_s == NULL)
				return CGI_ERR_INPUT;
			if (atoi(channel_s) <= 0)
				return CGI_ERR_INPUT;
			if (!strlen(ssid_s) || (strlen(ssid_s) > sizeof(config.wisp.ssid) - 1))
				return CGI_ERR_INPUT;
			if (!strlen(security_s) || (strlen(security_s) > sizeof(config.wisp.security) - 1))
				return CGI_ERR_INPUT;
			if (!strlen(key_s) || (strlen(key_s) > sizeof(config.wisp.key) - 1))
				return CGI_ERR_INPUT;
			config.wisp.channel = atoi(channel_s);
			arr_strcpy(config.wisp.ssid, ssid_s);
			arr_strcpy(config.wisp.key, key_s);
			arr_strcpy(config.wisp.security, security_s);
			break;
		case MODE_3G:
			config.mode = MODE_3G;
			break;
		default:
			return CGI_ERR_INPUT;
		}
		if (mu_msg(IF_MOD_PARAM_SET, &config, sizeof(config), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	memset(&config, 0x0, sizeof(config));
	uid = 1;
	if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &config, sizeof(config)))
		return CGI_ERR_FAIL;
	obj = json_object_new_int(config.mode);
	json_object_object_add(response, "mode", obj);
	snprintf(macstr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(config.mac));
	obj = json_object_new_string(macstr);
	json_object_object_add(response, "mac", obj);
	obj = json_object_new_int(config.speed);
	json_object_object_add(response, "speed", obj);
	switch (config.mode) {
	case MODE_DHCP:
		mtu = config.dhcp.mtu;
		dnsip = config.dhcp.dns;
		break;
	case MODE_PPPOE:
		mtu = config.pppoe.comm.mtu;
		dnsip = config.pppoe.comm.dns;
		break;
	case MODE_STATIC:
		mtu = config.statip.comm.mtu;
		dnsip = config.statip.comm.dns;
		break;
	default:
		break;
	}
	if (mtu) {
		obj = json_object_new_int(mtu);
		json_object_object_add(response, "mtu", obj);
	}
	if (dnsip && dnsip[0].s_addr)
		obj = json_object_new_string(inet_ntoa(dnsip[0]));
	else 
		obj = json_object_new_string("");
	json_object_object_add(response, "dns", obj);
	if (dnsip && dnsip[1].s_addr)
		obj = json_object_new_string(inet_ntoa(dnsip[1]));
	else 
		obj = json_object_new_string("");
	json_object_object_add(response, "dns1", obj);
	
	obj = json_object_new_string(config.pppoe.user);
	json_object_object_add(response, "user", obj);
	obj = json_object_new_string(config.pppoe.passwd);
	json_object_object_add(response, "passwd", obj);
	obj = json_object_new_string(config.pppoe.pppoe_server);
	json_object_object_add(response, "pppoe_server", obj);
	if (config.statip.ip.s_addr)
		obj = json_object_new_string(inet_ntoa(config.statip.ip));
	else
		obj = json_object_new_string("");
	json_object_object_add(response, "ip", obj);
	if (config.statip.mask.s_addr)
		obj = json_object_new_string(inet_ntoa(config.statip.mask));
	else
		obj = json_object_new_string("");
	json_object_object_add(response, "mask", obj);
	if (config.statip.gw.s_addr)
		obj = json_object_new_string(inet_ntoa(config.statip.gw));
	else
		obj = json_object_new_string("");
	json_object_object_add(response, "gw", obj);
	
	obj = json_object_new_int(config.wisp.channel);
	json_object_object_add(response, "channel", obj);
	obj = json_object_new_string(config.wisp.ssid);
	json_object_object_add(response, "ssid", obj);
	obj = json_object_new_string(config.wisp.security);
	json_object_object_add(response, "security", obj);
	obj = json_object_new_string(config.wisp.key);
	json_object_object_add(response, "key", obj);

	obj = json_object_new_int(config.dhcp.mtu?config.dhcp.mtu:1500);
	json_object_object_add(response, "mtu_dhcp", obj);
	if (config.dhcp.dns[0].s_addr)
		obj = json_object_new_string(inet_ntoa(config.dhcp.dns[0]));
	else 
		obj = json_object_new_string("");
	json_object_object_add(response, "dns_dhcp", obj);
	if (config.dhcp.dns[1].s_addr)
		obj = json_object_new_string(inet_ntoa(config.dhcp.dns[1]));
	else 
		obj = json_object_new_string("");
	json_object_object_add(response, "dns1_dhcp", obj);

	obj = json_object_new_int(config.pppoe.comm.mtu?config.pppoe.comm.mtu:1480);
	json_object_object_add(response, "mtu_pppoe", obj);
	if (config.pppoe.comm.dns[0].s_addr)
		obj = json_object_new_string(inet_ntoa(config.pppoe.comm.dns[0]));
	else 
		obj = json_object_new_string("");
	json_object_object_add(response, "dns_pppoe", obj);
	if (config.pppoe.comm.dns[1].s_addr)
		obj = json_object_new_string(inet_ntoa(config.pppoe.comm.dns[1]));
	else 
		obj = json_object_new_string("");
	json_object_object_add(response, "dns1_pppoe", obj);

	obj = json_object_new_int(config.statip.comm.mtu?config.statip.comm.mtu:1500);
	json_object_object_add(response, "mtu_static", obj);
	if (config.statip.comm.dns[0].s_addr)
		obj = json_object_new_string(inet_ntoa(config.statip.comm.dns[0]));
	else 
		obj = json_object_new_string("");
	json_object_object_add(response, "dns_static", obj);
	if (config.statip.comm.dns[1].s_addr)
		obj = json_object_new_string(inet_ntoa(config.statip.comm.dns[1]));
	else 
		obj = json_object_new_string("");
	json_object_object_add(response, "dns1_static", obj);

	/*add host mac and raw mac show*/
	inet_aton(con->ip_from, &ip);
	if (!dump_host_info(ip, &info)) {
		snprintf(macstr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(info.mac));
		obj = json_object_new_string(macstr);
	} else {
		obj = json_object_new_string("");
	}
	json_object_object_add(response, "hostmac", obj);
	uid = 0;
	if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &config, sizeof(config))) {
		obj = json_object_new_string("");
	} else {
		memcpy(mac, config.mac, ETH_ALEN);
		mode = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
		mode += 1;
		mac[2] = (mode >> 24) & 0xff;
		mac[3] = (mode >> 16) & 0xff;
		mac[4] = (mode >> 8) & 0xff;
		mac[5] = mode & 0xff;
		snprintf(macstr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(mac));
		obj = json_object_new_string(macstr);
	}
	json_object_object_add(response, "rawmac", obj);
	return 0;
}


int pro_net_wan_info_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int uid = 1;
	char macstr[18];
	struct iface_info info;
	struct json_object* obj;

	if (connection_is_set(con))
		return CGI_ERR_INPUT;
	memset(&info, 0x0, sizeof(info));
	if (mu_msg(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &info, sizeof(info)))
		return CGI_ERR_FAIL;
	obj = json_object_new_int(info.net);
	json_object_object_add(response, "connected", obj);
	obj = json_object_new_int(info.link);
	json_object_object_add(response, "link", obj);
	obj = json_object_new_int(info.mode);
	json_object_object_add(response, "mode", obj);
	obj = json_object_new_string(inet_ntoa(info.ip));
	json_object_object_add(response, "ip", obj);
	obj = json_object_new_string(inet_ntoa(info.mask));
	json_object_object_add(response, "mask", obj);
	obj= json_object_new_string(inet_ntoa(info.gw));
	json_object_object_add(response, "gateway", obj);
	obj= json_object_new_string(inet_ntoa(info.dns[0]));
	json_object_object_add(response, "DNS1", obj);
	obj= json_object_new_string(inet_ntoa(info.dns[1]));
	json_object_object_add(response, "DNS2", obj);
	snprintf(macstr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(info.mac));
	obj= json_object_new_string(macstr);
	json_object_object_add(response, "mac", obj);
	obj= json_object_new_int(info.err);
	json_object_object_add(response, "reason", obj);
	return 0;
}

int pro_net_wan_account_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int option = 0;
	struct account_info info;
	struct json_object *obj;

	if (connection_is_set(con)) {
		if (mu_msg(IF_MOD_GET_ACCOUNT, &option, sizeof(int), &info, sizeof(info)))
			return CGI_ERR_FAIL;
		return 0;
	}

	option = 1;
	if (mu_msg(IF_MOD_GET_ACCOUNT, &option, sizeof(int), &info, sizeof(info)))
		return CGI_ERR_ACCOUNT_NOTREADY;
	obj= json_object_new_string(info.passwd);
	json_object_object_add(response, "passwd", obj);
	obj= json_object_new_string(info.user);
	json_object_object_add(response, "account", obj);
	return 0; 
}

int pro_net_wan_detect_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	struct json_object* obj;
	struct detect_info info;

	memset(&info, 0x0, sizeof(info));
	if (mu_msg(IF_MOD_WAN_DETECT, NULL, 0, &info, sizeof(info)))
		return CGI_ERR_FAIL;
	obj = json_object_new_int(info.link);
	json_object_object_add(response, "wan_link", obj);
	obj = json_object_new_int(info.mode);
	json_object_object_add(response, "mode", obj);
	obj = json_object_new_int(info.net);
	json_object_object_add(response, "connected", obj);
	return 0;
}

/*wifi module cgi*/
int pro_net_wifi_ap_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int ac = 0, ret = 0;
	struct wifi_conf config;
	struct json_object* obj;
	char *ssid = NULL, *passwd = NULL, *country = NULL;
	char *channel = NULL, *hidden = NULL, *bw = NULL;
	char *mode_s = NULL, *forward_s = NULL, *wmm_s = NULL;
	struct nlk_sys_msg nlk;
	char crypto[IGD_NAME_LEN];

	if (connection_is_set(con)) {
		CHECK_LOGIN;
		ssid = con_value_get(con, "ssid");
		passwd = con_value_get(con, "passwd");
		channel = con_value_get(con, "channel");
		hidden = con_value_get(con, "hidden");
		bw = con_value_get(con, "bw");
		country = con_value_get(con, "country");
		mode_s = con_value_get(con, "mode");
		forward_s = con_value_get(con, "noforward");
		wmm_s = con_value_get(con, "wmm");
		if (NULL == ssid ||(strlen(ssid) <= 0) || (strlen(ssid) > 31))
			return CGI_ERR_INPUT;
		memset(&config, 0x0, sizeof(config));
		if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
			return CGI_ERR_FAIL;
		if (channel) {
			config.channel = atoi(channel);
			if (config.channel < 0)
				return CGI_ERR_INPUT;
		}
		if (hidden) {
			config.hidssid = atoi(hidden);
			if (config.hidssid != 0 && config.hidssid != 1)
				return CGI_ERR_INPUT;
		}
		if (bw) {
			config.htbw = atoi(bw);
			if (config.htbw != 0 && config.htbw != 1)
				return CGI_ERR_INPUT;
		}
		if (country && country[0])
			config.country = atoi(country);
		if (strcmp(config.ssid, ssid))
			_ADD_SYS_LOG("更改无线名称");
		arr_strcpy(config.ssid, ssid);
		if (passwd == NULL ||!strlen(passwd)) {
			if (strcmp(config.encryption, "none"))
				_ADD_SYS_LOG("更改无线密码");
			arr_strcpy(config.encryption, "none");
		} else {
			if (strcmp(config.key, passwd))
				_ADD_SYS_LOG("更改无线密码");
			if (!strcmp(config.encryption, "none"))
				arr_strcpy(config.encryption, "psk2+ccmp");
			arr_strcpy(config.key, passwd);
		}
		if (wmm_s)
			config.wmmcapable = atoi(wmm_s);
		if (forward_s)
			config.nohostfoward = atoi(forward_s);
		if (mode_s) {
			if (!strcmp(mode_s, "bgn"))
				config.mode = MODE_802_11_BGN;
			else if (!strcmp(mode_s, "bg"))
				config.mode = MODE_802_11_BG;
			else if (!strcmp(mode_s, "b"))
				config.mode = MODE_802_11_B;
			else if (!strcmp(mode_s, "g"))
				config.mode = MODE_802_11_G;
			else if (!strcmp(mode_s, "n"))
				config.mode = MODE_802_11_N;
		}
		ret = mu_msg(WIFI_MOD_SET_AP, &config, sizeof(config), NULL, 0);
		if (ret == -2)
			return CGI_ERR_NOSUPPORT;
		else if (ret)
			return CGI_ERR_FAIL;

		memset(&nlk, 0x0, sizeof(nlk));
		nlk.comm.gid = NLKMSG_GRP_SYS;
		nlk.comm.mid = SYS_GRP_MID_WIFI;
		nlk.msg.wifi.type = SYS_WIFI_MSG_2G_UPLOAD;
		nlk_event_send(NLKMSG_GRP_SYS, &nlk, sizeof(nlk));
		return 0;
	}

	if (!mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config)))
		ac = 1;
	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
		return CGI_ERR_FAIL;
	obj = json_object_new_int(ac);
	json_object_object_add(response, "5G", obj);
	obj = json_object_new_string(config.ssid);
	json_object_object_add(response, "ssid", obj);
	obj = json_object_new_int(config.hidssid);
	json_object_object_add(response, "hidden", obj);
	obj = json_object_new_int(config.channel);
	json_object_object_add(response, "channel", obj);
	obj = json_object_new_int(config.htbw);
	json_object_object_add(response, "bw", obj);
	obj = json_object_new_int(config.country);
	json_object_object_add(response, "country", obj);

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
	case MODE_802_11_BGN:
		obj = json_object_new_string("bgn");
		break;
	default:
		obj = json_object_new_string("bgn");
		break;
	}
	json_object_object_add(response, "mode", obj);
	
	if (strstr(config.encryption, "psk+mixed"))
		ret = snprintf(crypto, sizeof(crypto) - 1, "WPA1PSKWPA2PSK/");
	else if (strstr(config.encryption, "psk2+"))
		ret = snprintf(crypto, sizeof(crypto) - 1, "WPA2PSK/");
	else if (strstr(config.encryption, "psk+"))
		ret = snprintf(crypto, sizeof(crypto) - 1, "WPA1PSK/");
	else
		arr_strcpy(crypto, "NONE");
	if (strstr(config.encryption, "tkip+ccmp"))
		ret += snprintf(crypto + ret, sizeof(crypto) - 1, "TKIPAES");
	else if (strstr(config.encryption, "tkip"))
		ret += snprintf(crypto + ret, sizeof(crypto) - 1, "TKIP");
	else if (strstr(config.encryption, "ccmp"))
		ret += snprintf(crypto + ret, sizeof(crypto) - 1, "AES");
	else
		arr_strcpy(crypto, "NONE");
	obj = json_object_new_string(crypto);
	json_object_object_add(response, "crypto", obj);

	obj = json_object_new_int(config.wmmcapable);
	json_object_object_add(response, "wmm", obj);
	obj = json_object_new_int(config.nohostfoward);
	json_object_object_add(response, "noforward", obj);
	
	if (!strncmp(config.encryption, "none", 4)) {
		obj= json_object_new_int(0);
		json_object_object_add(response, "security", obj);
		obj= json_object_new_string("");
		json_object_object_add(response, "passwd", obj);
		return 0; 
	}
	
	if (con->login == 1) {
		obj= json_object_new_int(1);
		json_object_object_add(response, "security", obj);
		obj= json_object_new_string(config.key);
		json_object_object_add(response, "passwd", obj);
	} else {
		obj= json_object_new_int(0);
		json_object_object_add(response, "security", obj);
		obj= json_object_new_string("");
		json_object_object_add(response, "passwd", obj);
	}
	return 0;
}

int pro_net_wifi_ap_5g_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int ret = 0;
	struct wifi_conf config;
	struct json_object* obj;
	char *ssid = NULL, *passwd = NULL, *country = NULL;
	char *channel = NULL, *hidden = NULL, *bw = NULL;
	char *mode_s = NULL, *forward_s = NULL, *wmm_s = NULL;
	struct nlk_sys_msg nlk;
	char crypto[IGD_NAME_LEN];

	if (connection_is_set(con)) {
		CHECK_LOGIN;
		ssid = con_value_get(con, "ssid");
		passwd = con_value_get(con, "passwd");
		channel = con_value_get(con, "channel");
		hidden = con_value_get(con, "hidden");
		bw = con_value_get(con, "bw");
		country = con_value_get(con, "country");
		mode_s = con_value_get(con, "mode");
		forward_s = con_value_get(con, "noforward");
		wmm_s = con_value_get(con, "wmm");
		if (NULL == ssid ||(strlen(ssid) <= 0) || (strlen(ssid) > 31))
			return CGI_ERR_INPUT;
		memset(&config, 0x0, sizeof(config));
		if (mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config)))
			return CGI_ERR_FAIL;
		if (channel) {
			config.channel = atoi(channel);
			#if 0
			switch(config.channel) {
			case 0:
			case 149:
			case 153:
			case 157:
			case 161:
				break;
			default:
				return CGI_ERR_INPUT;
			}
			#endif
		}
		if (hidden) {
			config.hidssid = atoi(hidden);
			if (config.hidssid != 0 && config.hidssid != 1)
				return CGI_ERR_INPUT;
		}
		if (bw) {
			config.htbw = atoi(bw);
			if (config.htbw != 0 && config.htbw != 1)
				return CGI_ERR_INPUT;
		}
		if (country && country[0])
			config.country = atoi(country);
		arr_strcpy(config.ssid, ssid);
		if (passwd == NULL ||!strlen(passwd))
			arr_strcpy(config.encryption, "none");
		else {
			arr_strcpy(config.encryption, "psk2+ccmp");
			arr_strcpy(config.key, passwd);
		}
		if (wmm_s)
			config.wmmcapable = atoi(wmm_s);
		if (forward_s)
			config.nohostfoward = atoi(forward_s);
		if (mode_s) {
			if (!strcmp(mode_s, "acan"))
				config.mode = MODE_802_11_ANAC;
			else if (!strcmp(mode_s, "an"))
				config.mode = MODE_802_11_AN;
			else if (!strcmp(mode_s, "ac"))
				config.mode = MODE_802_11_AANAC;
			else if (!strcmp(mode_s, "a"))
				config.mode = MODE_802_11_A;
			else if (!strcmp(mode_s, "n"))
				config.mode = MODE_802_11_5N;
			else if (!strcmp(mode_s, "n"))
				config.mode = MODE_802_11_5N;
		}
		ret = mu_msg(WIFI_MOD_SET_AP, &config, sizeof(config), NULL, 0);
		if (ret == -2)
			return CGI_ERR_NOSUPPORT;
		else if (ret)
			return CGI_ERR_FAIL;
	
		memset(&nlk, 0x0, sizeof(nlk));
		nlk.comm.gid = NLKMSG_GRP_SYS;
		nlk.comm.mid = SYS_GRP_MID_WIFI;
		nlk.msg.wifi.type = SYS_WIFI_MSG_5G_UPLOAD;
		nlk_event_send(NLKMSG_GRP_SYS, &nlk, sizeof(nlk));
		return 0;
	}

	if (mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config)))
		return CGI_ERR_FAIL;
	obj = json_object_new_string(config.ssid);
	json_object_object_add(response, "ssid", obj);
	obj = json_object_new_int(config.hidssid);
	json_object_object_add(response, "hidden", obj);
	obj = json_object_new_int(config.channel);
	json_object_object_add(response, "channel", obj);
	obj = json_object_new_int(config.htbw);
	json_object_object_add(response, "bw", obj);
	obj = json_object_new_int(config.country);
	json_object_object_add(response, "country", obj);

	obj = json_object_new_int(config.wmmcapable);
	json_object_object_add(response, "wmm", obj);
	obj = json_object_new_int(config.nohostfoward);
	json_object_object_add(response, "noforward", obj);

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
		obj = json_object_new_string("ac");
		break;
	}
	json_object_object_add(response, "mode", obj);
	
	if (strstr(config.encryption, "psk+mixed"))
		ret = snprintf(crypto, sizeof(crypto) - 1, "WPA1PSKWPA2PSK/");
	else if (strstr(config.encryption, "psk2+"))
		ret = snprintf(crypto, sizeof(crypto) - 1, "WPA2PSK/");
	else if (strstr(config.encryption, "psk+"))
		ret = snprintf(crypto, sizeof(crypto) - 1, "WPA1PSK/");
	else
		arr_strcpy(crypto, "NONE");
	if (strstr(config.encryption, "tkip+ccmp"))
		ret += snprintf(crypto + ret, sizeof(crypto) - 1, "TKIPAES");
	else if (strstr(config.encryption, "tkip"))
		ret += snprintf(crypto + ret, sizeof(crypto) - 1, "TKIP");
	else if (strstr(config.encryption, "ccmp"))
		ret += snprintf(crypto + ret, sizeof(crypto) - 1, "AES");
	else
		arr_strcpy(crypto, "NONE");
	obj = json_object_new_string(crypto);
	json_object_object_add(response, "crypto", obj);
	
	if (!strncmp(config.encryption, "none", 4)) {
		obj= json_object_new_int(0);
		json_object_object_add(response, "security", obj);
		obj= json_object_new_string("");
		json_object_object_add(response, "passwd", obj);
		return 0; 
	}
	
	if (con->login == 1) {
		obj= json_object_new_int(1);
		json_object_object_add(response, "security", obj);
		obj= json_object_new_string(config.key);
		json_object_object_add(response, "passwd", obj);
	} else {
		obj= json_object_new_int(0);
		json_object_object_add(response, "security", obj);
		obj= json_object_new_string("");
		json_object_object_add(response, "passwd", obj);
	}
	return 0;
}

int pro_net_wifi_lt_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	int oenable;
	char *ptr = NULL;
	unsigned char i = 0;
	int start, end, now;
	struct tm *tm = get_tm();
	struct wifi_conf config;
	struct time_comm *t = &config.tm;
	struct json_object* obj, *week, *day;
	
	memset(&config, 0, sizeof(config));
	if (connection_is_set(con)) {
		CHECK_LOGIN;
		if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
			return CGI_ERR_FAIL;
		memset(&config.tm, 0x0, sizeof(config.tm));
		ptr = con_value_get(con, "enable");
		if (!ptr) {
			CGI_PRINTF("input err, %p\n", ptr);
			return CGI_ERR_INPUT;
		}
		oenable = config.enable;
		config.enable = atoi(ptr);
		ptr = con_value_get(con, "time_on");
		if (!ptr) {
			CGI_PRINTF("input err, %p\n", ptr);
			return CGI_ERR_INPUT;
		}
		config.time_on = atoi(ptr);
		ptr = con_value_get(con, "week");
		if (!ptr) {
			CGI_PRINTF("input err, %p\n", ptr);
			return CGI_ERR_INPUT;
		}
		for (i = 0; i < 7; i++) {
			if (*(ptr + i) == '\0') {
				CGI_PRINTF("week err\n");
				break;
			}
			if (*(ptr + i) == '1')
				CGI_BIT_SET(t->day_flags, i);
		}
		CON_GET_CHECK_INT(ptr, con, t->start_hour, "sh", 23);
		CON_GET_CHECK_INT(ptr, con, t->start_min, "sm", 59);
		CON_GET_CHECK_INT(ptr, con, t->end_hour, "eh", 23);
		CON_GET_CHECK_INT(ptr, con, t->end_min, "em", 59);
		if (!t->day_flags && tm && config.time_on) {
			start = t->start_hour*60 + t->start_min;
			end = t->end_hour*60 + t->end_min;
			now = tm->tm_hour*60 + tm->tm_min;
			if ((start < end) && (now > end))
				return CGI_ERR_TIMEOUT;
		}
		if (t->day_flags)
			t->loop = 1;
		else {
			if (!tm && config.time_on)
				return CGI_ERR_TIMEOUT;
			if (tm && config.time_on)
				CGI_BIT_SET(t->day_flags, tm->tm_wday);
		}
		if (mu_msg(WIFI_MOD_SET_TIME, &config, sizeof(config), NULL, 0))
			return CGI_ERR_FAIL;
		if (!oenable && config.enable)
			_ADD_SYS_LOG("2.4G WIFI 开启");
		else if (oenable && !config.enable)
			_ADD_SYS_LOG("2.4G WIFI 关闭");
		else
			_ADD_SYS_LOG("2.4G WIFI定时开关设置生效");
		return 0;
	}

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
		return CGI_ERR_FAIL;
	obj = json_object_new_int(config.enable);
	json_object_object_add(response, "enable", obj);
	obj = json_object_new_int(config.time_on);
	json_object_object_add(response, "time_on", obj);
	week = json_object_new_array();
	if (t->loop) {
		for (i = 0; i < 7; i++) {
			if (!CGI_BIT_TEST(t->day_flags, i))
				continue;
			day = json_object_new_int(i);
			json_object_array_add(week, day);
		}
	}
	json_object_object_add(response, "week", week);
	obj = json_object_new_int(t->start_hour);
	json_object_object_add(response, "sh", obj);

	obj = json_object_new_int(t->start_min);
	json_object_object_add(response, "sm", obj);

	obj = json_object_new_int(t->end_hour);
	json_object_object_add(response, "eh", obj);

	obj = json_object_new_int(t->end_min);
	json_object_object_add(response, "em", obj);
	return 0;
}

int pro_net_wifi_lt_5g_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	int oenable;
	char *ptr = NULL;
	unsigned char i = 0;
	int start, end, now;
	struct tm *tm = get_tm();
	struct wifi_conf config;
	struct time_comm *t = &config.tm;
	struct json_object* obj, *week, *day;
	
	memset(&config, 0, sizeof(config));
	if (connection_is_set(con)) {
		CHECK_LOGIN;
		if (mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config)))
			return CGI_ERR_FAIL;
		memset(&config.tm, 0x0, sizeof(config.tm));
		ptr = con_value_get(con, "enable");
		if (!ptr) {
			CGI_PRINTF("input err, %p\n", ptr);
			return CGI_ERR_INPUT;
		}
		oenable = config.enable;
		config.enable = atoi(ptr);
		ptr = con_value_get(con, "time_on");
		if (!ptr) {
			CGI_PRINTF("input err, %p\n", ptr);
			return CGI_ERR_INPUT;
		}
		config.time_on = atoi(ptr);
		ptr = con_value_get(con, "week");
		if (!ptr) {
			CGI_PRINTF("input err, %p\n", ptr);
			return CGI_ERR_INPUT;
		}
		for (i = 0; i < 7; i++) {
			if (*(ptr + i) == '\0') {
				CGI_PRINTF("week err\n");
				break;
			}
			if (*(ptr + i) == '1')
				CGI_BIT_SET(t->day_flags, i);
		}
		CON_GET_CHECK_INT(ptr, con, t->start_hour, "sh", 23);
		CON_GET_CHECK_INT(ptr, con, t->start_min, "sm", 59);
		CON_GET_CHECK_INT(ptr, con, t->end_hour, "eh", 23);
		CON_GET_CHECK_INT(ptr, con, t->end_min, "em", 59);
		if (!t->day_flags && tm && config.time_on) {
			start = t->start_hour*60 + t->start_min;
			end = t->end_hour*60 + t->end_min;
			now = tm->tm_hour*60 + tm->tm_min;
			if ((start < end) && (now > end))
				return CGI_ERR_TIMEOUT;
		}
		if (t->day_flags)
			t->loop = 1;
		else {
			if (!tm && config.time_on)
				return CGI_ERR_TIMEOUT;
			if (tm && config.time_on)
				CGI_BIT_SET(t->day_flags, tm->tm_wday);
		}
		if (mu_msg(WIFI_MOD_SET_TIME, &config, sizeof(config), NULL, 0))
			return CGI_ERR_FAIL;
		if (!oenable && config.enable)
			_ADD_SYS_LOG("5G WIFI 开启");
		else if (oenable && !config.enable)
			_ADD_SYS_LOG("5G WIFI 关闭");
		else
			_ADD_SYS_LOG("5G WIFI定时开关设置生效");
		return 0;
	}

	if (mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config)))
		return CGI_ERR_FAIL;
	obj = json_object_new_int(config.enable);
	json_object_object_add(response, "enable", obj);
	obj = json_object_new_int(config.time_on);
	json_object_object_add(response, "time_on", obj);
	week = json_object_new_array();
	if (t->loop) {
		for (i = 0; i < 7; i++) {
			if (!CGI_BIT_TEST(t->day_flags, i))
				continue;
			day = json_object_new_int(i);
			json_object_array_add(week, day);
		}
	}
	json_object_object_add(response, "week", week);
	obj = json_object_new_int(t->start_hour);
	json_object_object_add(response, "sh", obj);

	obj = json_object_new_int(t->start_min);
	json_object_object_add(response, "sm", obj);

	obj = json_object_new_int(t->end_hour);
	json_object_object_add(response, "eh", obj);

	obj = json_object_new_int(t->end_min);
	json_object_object_add(response, "em", obj);
	return 0;
}

int pro_net_wifi_vap_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int len = 0;
	int enable = 0;
	char *rebind_s = NULL;
	char *hidden_s = NULL;
	char *vssid_s = NULL;
	char *enable_s = NULL;
	char *wechat_s = NULL;
	char *passwd_s = NULL;
	char *forward_s = NULL;
	struct wifi_conf config;
	struct json_object* obj;

	memset(&config, 0x0, sizeof(config));
	if (connection_is_set(con)) {
		CHECK_LOGIN;
		enable_s = con_value_get(con, "enable");
		vssid_s = con_value_get(con, "vssid");
		hidden_s = con_value_get(con, "hidden");
		wechat_s = con_value_get(con, "wechat");
		rebind_s = con_value_get(con, "rebind");
		passwd_s = con_value_get(con, "password");
		forward_s = con_value_get(con, "noforward");
		if (enable_s == NULL)
			return CGI_ERR_INPUT; 
		if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
			return CGI_ERR_FAIL;
		enable = atoi(enable_s);
		config.vap = enable;
		config.aliset = 0;
		if (vssid_s)
			arr_strcpy(config.vssid, vssid_s);
		if (hidden_s)
			config.hidvssid = atoi(hidden_s);
		else
			config.hidvssid = 0;
		if (wechat_s)
			config.wechat = atoi(wechat_s);
		if (rebind_s)
			config.rebind = atoi(rebind_s);
		if (forward_s)
			config.noapforward = atoi(forward_s);
		if (passwd_s) {
			len = strlen(passwd_s);
			if (len > 7 && len < 32) {
				if (!strcmp(config.vencryption, "none"))
					arr_strcpy(config.vencryption, "psk2+ccmp");
				arr_strcpy(config.vkey, passwd_s);
			} else if (len == 0) {
				arr_strcpy(config.vencryption, "none");
			} else {
				return CGI_ERR_INPUT;
			}
		} else {
			arr_strcpy(config.vencryption, "none");
		}
		if (mu_msg(WIFI_MOD_SET_VAP, &config, sizeof(config), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
		return CGI_ERR_FAIL;
	obj = json_object_new_int(config.vap);
	json_object_object_add(response, "enable", obj);
	obj = json_object_new_string(config.vssid);
	json_object_object_add(response, "vssid", obj);
	obj = json_object_new_int(config.hidvssid);
	json_object_object_add(response, "hidden", obj);
	obj = json_object_new_int(config.wechat);
	json_object_object_add(response, "wechat", obj);
	obj = json_object_new_int(config.rebind);
	json_object_object_add(response, "rebind", obj);
	if (con->login == 1)
		obj = json_object_new_string(config.vkey);
	else
		obj = json_object_new_string("");
	json_object_object_add(response, "password", obj);
	if (!strcmp(config.vencryption, "none"))
		obj = json_object_new_int(0);
	else
		obj = json_object_new_int(1);
	json_object_object_add(response, "security", obj);
	obj = json_object_new_int(config.noapforward);
	json_object_object_add(response, "noforward", obj);
	return 0;
}

int pro_net_wifi_vap_host_hander(server_t* srv, connection_t *con, struct json_object*response)
{
	int i = 0, nr = 0;
	char *mac_s = NULL;
	char *action_s = NULL;
	char macstr[18];
	struct iwguest ghost;
	struct host_info info[HOST_MX];
	struct json_object *obj, *vhosts, *host;
	
	if (connection_is_set(con)) {
		CHECK_LOGIN;
		mac_s = con_value_get(con, "mac");
		action_s = con_value_get(con, "action");
		if (!action_s || !mac_s || checkmac(mac_s))
			return CGI_ERR_INPUT;
		memset(&ghost, 0x0, sizeof(ghost));
		parse_mac(mac_s, ghost.mac);
		if (!strncmp(action_s, "add", 3)) {
			if (mu_msg(WIFI_MOD_VAP_HOST_ADD, &ghost, sizeof(ghost), NULL, 0))
				return CGI_ERR_FAIL;
		} else if (!strncmp(action_s, "del", 3)) {
			if (mu_msg(WIFI_MOD_VAP_HOST_DEL, &ghost, sizeof(ghost), NULL, 0))
				return CGI_ERR_FAIL;
		} else
			return CGI_ERR_INPUT;
		return 0;
	}
	memset(&info, 0x0, sizeof(info));
	nr = mu_msg(WIFI_MOD_VAP_HOST_DUMP, NULL, 0, info, sizeof(struct host_info) * HOST_MX);
	if (nr < 0)
		return CGI_ERR_FAIL;
	obj = json_object_new_int(nr);
	json_object_object_add(response, "count", obj);
	vhosts = json_object_new_array();
	for (i = 0; i < nr; i++) {
		if (info[i].pid != 2)
			continue;
		host = json_object_new_object();
		obj = json_object_new_int(info[i].vender);
		json_object_object_add(host, "vender", obj);
		obj = json_object_new_int(info[i].os_type);
		json_object_object_add(host, "ostype", obj);
		if (info[i].nick_name[0]) {
			obj = json_object_new_string(info[i].nick_name);
			json_object_object_add(host, "name", obj);
		} else {
			obj = json_object_new_string(info[i].name);
			json_object_object_add(host, "name", obj);
		}
		snprintf(macstr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(info[i].mac));
		obj = json_object_new_string(macstr);
		json_object_object_add(host, "mac", obj);
		obj = json_object_new_int(info[i].is_wifi);
		json_object_object_add(host, "wlist", obj);
		obj = json_object_new_int(info[i].pid);
		json_object_object_add(host, "pid", obj);
		json_object_array_add(vhosts, host);
		
	}
	json_object_object_add(response, "guests", vhosts);
	return 0;
}

int pro_net_wifi_vap_allowmin_hander(server_t* srv, connection_t *con, struct json_object*response)
{
	char *mac_s = NULL;
	struct iwguest ghost;

	if (connection_is_set(con)) {
		mac_s = con_value_get(con, "mac");
		if (!mac_s || checkmac(mac_s))
			return CGI_ERR_INPUT;
		parse_mac(mac_s, ghost.mac);
		if (mu_msg(WIFI_MOD_VAP_WECHAT_ALLOW, &ghost, sizeof(ghost), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	return 0;
}

int pro_net_wifi_wechat_host_hander(server_t* srv, connection_t *con, struct json_object*response)
{
	int i = 0, nr = 0;
	char *mac_s, *passid_s;
	struct iwguest ghost[HOST_MX];
	struct json_object *vhosts, *host;

	CHECK_LOGIN;
	memset(ghost, 0x0, sizeof(ghost));
	if (connection_is_set(con)) {
		mac_s = con_value_get(con, "mac");
		passid_s = con_value_get(con, "passid");
		if (!mac_s || checkmac(mac_s) || !passid_s)
			return CGI_ERR_INPUT;
		parse_mac(mac_s, ghost[0].mac);
		arr_strcpy(ghost[0].wechatid, passid_s);
		if (mu_msg(WIFI_MOD_VAP_HOST_ADD, &ghost, sizeof(struct iwguest), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	nr = mu_msg(WIFI_MOD_VAP_HOST_WECHAT_DUMP, NULL, 0, ghost, sizeof(struct iwguest) * HOST_MX);
	if (nr < 0)
		return CGI_ERR_FAIL;
	vhosts = json_object_new_array();
	for (i = 0; i < nr; i++) {
		host = json_object_new_string(ghost[i].wechatid);
		json_object_array_add(vhosts, host);
	}
	json_object_object_add(response, "passid", vhosts);
	return 0;
}

int pro_net_wifi_wechat_clean_hander(server_t* srv, connection_t *con, struct json_object*response)
{
	unsigned char ifindex = 0;

	CHECK_LOGIN;
	if (connection_is_set(con)) {
		if (mu_msg(WIFI_MOD_VAP_HOST_WECHAT_CLEAN, &ifindex, sizeof(ifindex), NULL, 0))
			return CGI_ERR_FAIL;
	}
	return 0;
}

int pro_net_txrate_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int txrate;
	char *txrates = NULL;
	struct wifi_conf config;
	struct json_object* obj;
	struct nlk_sys_msg nlk;

	if (connection_is_set(con)) {
		CHECK_LOGIN;
		txrates = con_value_get(con, "txrate");
		if (txrates == NULL)
			return CGI_ERR_INPUT; 
		txrate = atoi(txrates);
		if (txrate > 100 || txrate < 10)
			return CGI_ERR_INPUT;
		if (mu_msg(WIFI_MOD_SET_TXRATE, &txrate, sizeof(txrate), NULL, 0))
			return CGI_ERR_FAIL;
		if (txrate <= 30)
			_ADD_SYS_LOG("调节WIFI信号强度，当前强度为孕妇模式");
		else if (txrate > 65)
			_ADD_SYS_LOG("调节WIFI信号强度，当前强度为穿墙模式");
		else
			_ADD_SYS_LOG("调节WIFI信号强度，当前强度为均衡模式");
		memset(&nlk, 0x0, sizeof(nlk));
		nlk.comm.gid = NLKMSG_GRP_SYS;
		nlk.comm.mid = SYS_GRP_MID_WIFI;
		nlk.msg.wifi.type = SYS_WIFI_MSG_2G_UPLOAD;
		nlk_event_send(NLKMSG_GRP_SYS, &nlk, sizeof(nlk));
		return 0;
	}
	memset(&config, 0x0, sizeof(config));
	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)))
		return CGI_ERR_FAIL;
	obj = json_object_new_int(config.txrate);
	json_object_object_add(response, "txrate", obj);
	return 0;
}

int pro_net_txrate_5g_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int txrate;
	char *txrates = NULL;
	struct wifi_conf config;
	struct json_object* obj;

	if (connection_is_set(con)) {
		CHECK_LOGIN;
		txrates = con_value_get(con, "txrate");
		if (txrates == NULL)
			return CGI_ERR_INPUT; 
		txrate = atoi(txrates);
		if (txrate > 100 || txrate < 10)
			return CGI_ERR_INPUT;
		if (mu_msg(WIFI_MOD_SET_TXRATE_5G, &txrate, sizeof(txrate), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	memset(&config, 0x0, sizeof(config));
	if (mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config)))
		return CGI_ERR_FAIL;
	obj = json_object_new_int(config.txrate);
	json_object_object_add(response, "txrate", obj);
	return 0;
}

int pro_net_ap_list_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int i;
	int nr = 0;
	int scan = 1;
	struct json_object *obj;
	struct json_object *aps;
	struct json_object *ap;
	struct iwsurvey survey[IW_SUR_MX];

	//CHECK_LOGIN;
	if (connection_is_set(con))
		return CGI_ERR_FAIL;
	nr = mu_msg(WIFI_MOD_GET_SURVEY, &scan, sizeof(int), survey, sizeof(struct iwsurvey) * IW_SUR_MX);
	if (nr > 0)
		goto finish;
	usleep(8000000);
	scan = 0;
	nr = mu_msg(WIFI_MOD_GET_SURVEY, &scan, sizeof(int), survey, sizeof(struct iwsurvey) * IW_SUR_MX);
	if (nr <= 0)
		return CGI_ERR_FAIL;
finish:
	obj = json_object_new_int(nr);
	json_object_object_add(response, "count", obj);
	aps = json_object_new_array();
	for (i = 0; i < nr; i++) {
		ap = json_object_new_object();
		obj = json_object_new_int(survey[i].ch);
		json_object_object_add(ap, "channel", obj);
		obj = json_object_new_int(survey[i].signal);
		json_object_object_add(ap, "dbm", obj);
		obj = json_object_new_string(survey[i].ssid);
		json_object_object_add(ap, "ssid", obj);
		obj = json_object_new_string(survey[i].extch);
		json_object_object_add(ap, "extch", obj);
		obj = json_object_new_string(survey[i].security);
		json_object_object_add(ap, "security", obj);
		obj = json_object_new_string(survey[i].mode);
		json_object_object_add(ap, "mode", obj);
		obj = json_object_new_string(survey[i].bssid);
		json_object_object_add(ap, "bssid", obj);
		obj = json_object_new_int(survey[i].wps);
		json_object_object_add(ap, "wps", obj);
		json_object_array_add(aps,ap);
	}
	json_object_object_add(response, "aplist", aps);
	return 0;
}

int pro_net_ap_list_5g_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int i;
	int nr = 0;
	int scan = 1;
	struct json_object *obj;
	struct json_object *aps;
	struct json_object *ap;
	struct iwsurvey survey[IW_SUR_MX];

	//CHECK_LOGIN;
	if (connection_is_set(con))
		return CGI_ERR_FAIL;
	nr = mu_msg(WIFI_MOD_GET_SURVEY_5G, &scan, sizeof(int), survey, sizeof(struct iwsurvey) * IW_SUR_MX);
	if (nr > 0)
		goto finish;
	usleep(8000000);
	scan = 0;
	nr = mu_msg(WIFI_MOD_GET_SURVEY_5G, &scan, sizeof(int), survey, sizeof(struct iwsurvey) * IW_SUR_MX);
	if (nr <= 0)
		return CGI_ERR_FAIL;
finish:
	obj = json_object_new_int(nr);
	json_object_object_add(response, "count", obj);
	aps = json_object_new_array();
	for (i = 0; i < nr; i++) {
		ap = json_object_new_object();
		obj = json_object_new_int(survey[i].ch);
		json_object_object_add(ap, "channel", obj);
		obj = json_object_new_int(survey[i].signal);
		json_object_object_add(ap, "dbm", obj);
		obj = json_object_new_string(survey[i].ssid);
		json_object_object_add(ap, "ssid", obj);
		obj = json_object_new_string(survey[i].extch);
		json_object_object_add(ap, "extch", obj);
		obj = json_object_new_string(survey[i].security);
		json_object_object_add(ap, "security", obj);
		obj = json_object_new_string(survey[i].mode);
		json_object_object_add(ap, "mode", obj);
		obj = json_object_new_string(survey[i].bssid);
		json_object_object_add(ap, "bssid", obj);
		obj = json_object_new_int(survey[i].wps);
		json_object_object_add(ap, "wps", obj);
		json_object_array_add(aps,ap);
	}
	json_object_object_add(response, "aplist", aps);
	return 0;
}

int pro_net_wifi_acl_black_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int i;
	struct iwacl wacl;
	char *ifindex = NULL;
	char *list_enable = NULL;
	char *mac_num = NULL;
	char macstr[18];
	struct json_object *obj, *obj_arr;

	ifindex = con_value_get(con, "ifindex");
	if (!ifindex) {
		CGI_PRINTF("ifindex is NULL\n");
		return CGI_ERR_INPUT;
	}
	obj= json_object_new_string(ifindex);
	json_object_object_add(response, "ifindex", obj);
	
	if (!(strcmp(ifindex, "0"))) {
		if (mu_msg(WIFI_MOD_GET_ACL, NULL, 0, &wacl, sizeof(wacl)))
			return CGI_ERR_FAIL;
	}
	if (!(strcmp(ifindex, "1"))) {
		if (mu_msg(WIFI_MOD_GET_ACL_5G, NULL, 0, &wacl, sizeof(wacl)))
			return CGI_ERR_FAIL;
	}
		
	if (connection_is_set(con)) {
		CHECK_LOGIN;
		wacl.blist.nr = 0;
		memset(wacl.blist.list, 0x0, sizeof(wacl.blist.list));

		char *macList = NULL;
		char *macTemp = NULL;
		i = 0;
		char delims[] = ";";
		
		list_enable = con_value_get(con, "enable");
		if (!list_enable) {
			CGI_PRINTF("list_enable is NULL\n");
			return CGI_ERR_INPUT;
		}
		if (!(strcmp(list_enable, "1"))) {
			wacl.blist.enable = 1;
			wacl.wlist.enable = 0;
		}
		if (!(strcmp(list_enable, "0"))) {
			wacl.blist.enable = 0;
		}
			
		macList = con_value_get(con, "mac");

		if (!macList) {
			return CGI_ERR_INPUT;
		}
		if (!strcmp(macList, "empty")) {
			wacl.blist.nr = 0;
			if (!(strcmp(ifindex, "0"))) {
				if (mu_msg(WIFI_MOD_SET_ACL, &wacl, sizeof(wacl), NULL, 0))
					return CGI_ERR_FAIL;
			}
		
			if (!(strcmp(ifindex, "1"))) {
				if (mu_msg(WIFI_MOD_SET_ACL_5G, &wacl, sizeof(wacl), NULL, 0))
					return CGI_ERR_FAIL;
			}

			return 0;
		}
		macTemp = strtok(macList, delims);
		while (macTemp != NULL) {
			if(checkmac(macTemp) || 
				cgi_str2mac(macTemp, (unsigned char *)wacl.blist.list[i].mac)) {
					return CGI_ERR_INPUT;
			}
			wacl.blist.list[i].enable = 1;
			i++;
			wacl.blist.nr = i;
			macTemp = strtok( NULL, delims );
		}
		mac_num = con_value_get(con, "mac_num");
		if (!mac_num) {
			CGI_PRINTF("mac_num is NULL\n");
			return CGI_ERR_INPUT;
		}
		if (i != atoi(mac_num)) {
			return CGI_ERR_INPUT;
		}
				
		if (!(strcmp(ifindex, "0"))) {
			if (mu_msg(WIFI_MOD_SET_ACL, &wacl, sizeof(wacl), NULL, 0))
				return CGI_ERR_FAIL;
		}
		
		if (!(strcmp(ifindex, "1"))) {
			if (mu_msg(WIFI_MOD_SET_ACL_5G, &wacl, sizeof(wacl), NULL, 0))
				return CGI_ERR_FAIL;
		}

		return 0;
	}
	obj_arr= json_object_new_array();
	for (i = 0; (i < wacl.blist.nr) && (i < IW_LIST_MX); i++) {
		snprintf(macstr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(wacl.blist.list[i].mac));
		obj= json_object_new_string(macstr);		
		json_object_array_add(obj_arr, obj);
	}
	json_object_object_add(response, "mac", obj_arr);
	obj= json_object_new_int(wacl.blist.enable);
	json_object_object_add(response, "enable", obj);
	return 0;
}

int pro_net_wifi_acl_white_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int i;
	struct iwacl wacl;
	char *ifindex = NULL;
	char *list_enable = NULL;
	char *mac_num = NULL;
	char macstr[18];
	struct json_object *obj, *obj_arr;
	
	ifindex = con_value_get(con, "ifindex");
	if (!ifindex) {
		CGI_PRINTF("ifindex is NULL\n");
		return CGI_ERR_INPUT;
	}
	obj= json_object_new_string(ifindex);
	json_object_object_add(response, "ifindex", obj);	
	if (!(strcmp(ifindex, "0"))) {
		if (mu_msg(WIFI_MOD_GET_ACL, NULL, 0, &wacl, sizeof(wacl)))
			return CGI_ERR_FAIL;
	}
	if (!(strcmp(ifindex, "1"))) {
		if (mu_msg(WIFI_MOD_GET_ACL_5G, NULL, 0, &wacl, sizeof(wacl)))
			return CGI_ERR_FAIL;
	}
	if (connection_is_set(con)) {
		CHECK_LOGIN;
		wacl.wlist.nr = 0;
		memset(wacl.wlist.list, 0x0, sizeof(wacl.wlist.list));

		char *macList = NULL;
		char *macTemp = NULL;
		i = 0;
		char delims[] = ";";
			
		macList = con_value_get(con, "mac");
		if (!macList) {

			return CGI_ERR_INPUT;
		}

		list_enable = con_value_get(con, "enable");
		if (!list_enable) {
			CGI_PRINTF("list_enable is NULL\n");
			return CGI_ERR_INPUT;
		}
		if (!(strcmp(list_enable, "1"))) {
			wacl.wlist.enable = 1;
			wacl.blist.enable = 0;
		}
		if (!(strcmp(list_enable, "0"))) {
			wacl.wlist.enable = 0;
		}
		
		if (!strcmp(macList, "empty")) {
			wacl.wlist.nr = 0;
			if (!(strcmp(ifindex, "0"))) {
				if (mu_msg(WIFI_MOD_SET_ACL, &wacl, sizeof(wacl), NULL, 0))
					return CGI_ERR_FAIL;
			}
		
			if (!(strcmp(ifindex, "1"))) {
				if (mu_msg(WIFI_MOD_SET_ACL_5G, &wacl, sizeof(wacl), NULL, 0))
					return CGI_ERR_FAIL;
			}
			
			return 0;
		}

		macTemp = strtok(macList, delims);

		while (macTemp != NULL) {
			if(checkmac(macTemp) || 
				cgi_str2mac(macTemp, (unsigned char *)wacl.wlist.list[i].mac)) {
					return CGI_ERR_INPUT;
			}
			wacl.wlist.list[i].enable = 1;
			i++;
			wacl.wlist.nr = i;
			macTemp = strtok( NULL, delims );
		}
		mac_num = con_value_get(con, "mac_num");
		if (!mac_num) {
			CGI_PRINTF("mac_num is NULL\n");
			return CGI_ERR_INPUT;
		}
		if (i != atoi(mac_num)) {
			return CGI_ERR_INPUT;
		}
					
		ifindex = con_value_get(con, "ifindex");
		if (!ifindex) {
			CGI_PRINTF("ifindex is NULL\n");
			return CGI_ERR_INPUT;
		}
		if (!(strcmp(ifindex, "0"))) {
			if (mu_msg(WIFI_MOD_SET_ACL, &wacl, sizeof(wacl), NULL, 0))
				return CGI_ERR_FAIL;
		}
		
		if (!(strcmp(ifindex, "1"))) {
			if (mu_msg(WIFI_MOD_SET_ACL_5G, &wacl, sizeof(wacl), NULL, 0))
				return CGI_ERR_FAIL;
		}
	
		return 0;
	}
	obj_arr= json_object_new_array();
	for (i = 0; (i < wacl.wlist.nr) && (i < IW_LIST_MX); i++) {
		snprintf(macstr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(wacl.wlist.list[i].mac));
		obj= json_object_new_string(macstr);		
		json_object_array_add(obj_arr, obj);
	}
	json_object_object_add(response, "mac", obj_arr);
	obj= json_object_new_int(wacl.wlist.enable);
	json_object_object_add(response, "enable", obj);
	return 0;
}

int pro_net_channel_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int ret = 0;
	int channel = 0;
	char *channels = NULL;
	struct json_object* obj;
	struct nlk_sys_msg nlk;

	if (connection_is_set(con)) {
		CHECK_LOGIN;
		channels  = con_value_get(con, "channel");
		if (channels == NULL)
			return CGI_ERR_INPUT; 
		channel = atoi(channels);
		if (channel <= 0)
			return CGI_ERR_INPUT;
		if (mu_msg(WIFI_MOD_SET_CHANNEL, &channel, sizeof(channel), NULL, 0))
			return CGI_ERR_FAIL;
		memset(&nlk, 0x0, sizeof(nlk));
		nlk.comm.gid = NLKMSG_GRP_SYS;
		nlk.comm.mid = SYS_GRP_MID_WIFI;
		nlk.msg.wifi.type = SYS_WIFI_MSG_2G_UPLOAD;
		nlk_event_send(NLKMSG_GRP_SYS, &nlk, sizeof(nlk));
		return 0;
	}
	//if wifi disable,return CGI_ERR_NONEXIST
	ret = mu_msg(WIFI_MOD_GET_CHANNEL, NULL, 0, &channel, sizeof(channel));
	if (ret > 0)
		return CGI_ERR_NOSUPPORT;
	else if (ret < 0)
		return CGI_ERR_FAIL;
	else if (channel <= 0)
		return CGI_ERR_FAIL;
	obj = json_object_new_int(channel);
	json_object_object_add(response, "channel", obj);
	return 0; 
}

int pro_net_channel_5g_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int ret = 0;
	int channel = 0;
	char *channels = NULL;
	struct json_object* obj;
	struct nlk_sys_msg nlk;

	if (connection_is_set(con)) {
		CHECK_LOGIN;
		channels  = con_value_get(con, "channel");
		if (channels == NULL)
			return CGI_ERR_INPUT; 
		channel = atoi(channels);
		#if 0
		switch(channel) {
		case 149:
		case 153:
		case 157:
		case 161:
			break;
		default:
			return CGI_ERR_INPUT;
		}
		#endif
		if (mu_msg(WIFI_MOD_SET_CHANNEL_5G, &channel, sizeof(channel), NULL, 0))
			return CGI_ERR_FAIL;
		memset(&nlk, 0x0, sizeof(nlk));
		nlk.comm.gid = NLKMSG_GRP_SYS;
		nlk.comm.mid = SYS_GRP_MID_WIFI;
		nlk.msg.wifi.type = SYS_WIFI_MSG_5G_UPLOAD;
		nlk_event_send(NLKMSG_GRP_SYS, &nlk, sizeof(nlk));
		return 0;
	}
	//if wifi disable,return CGI_ERR_NONEXIST
	ret = mu_msg(WIFI_MOD_GET_CHANNEL_5G, NULL, 0, &channel, sizeof(channel));
	if (ret > 0)
		return CGI_ERR_NOSUPPORT;
	else if (ret < 0)
		return CGI_ERR_FAIL;
	obj = json_object_new_int(channel);
	json_object_object_add(response, "channel", obj);
	return 0; 
}

/*dnsmasq/dhcp module cgi*/
int pro_net_dhcpd_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int reboot = 1;
	int uid = 1;
	struct iface_conf ifc;
	struct dhcp_conf config;
	struct json_object* obj;
	char *enable_s = NULL;
	char *ip_s = NULL, *mask_s = NULL;
	char *start_s = NULL, *end_s = NULL;
	char *dns1_s = NULL, *dns2_s = NULL;
	char *time_s = NULL;
	char macstr[18];
	
	if (connection_is_set(con)) {
		CHECK_LOGIN;
		start_s = con_value_get(con, "start");
		end_s = con_value_get(con, "end");
		mask_s = con_value_get(con, "mask");
		ip_s = con_value_get(con, "ip");
		enable_s = con_value_get(con, "enable");
		dns1_s = con_value_get(con, "dns1");
		dns2_s = con_value_get(con, "dns2");
		time_s = con_value_get(con, "time");
		if (!ip_s || !mask_s || !start_s || !end_s)
			return CGI_ERR_INPUT;
		if (checkip(mask_s) || checkip(ip_s) || checkip(start_s) || checkip(end_s))
			return CGI_ERR_INPUT;
		memset(&config, 0x0, sizeof(config));
		if (dns1_s && strlen(dns1_s)) {
			if (checkip(dns1_s))
				return CGI_ERR_INPUT;
			inet_aton(dns1_s, &config.dns1);
		}
		if (dns2_s && strlen(dns2_s)) {
			if (checkip(dns2_s))
				return CGI_ERR_INPUT;
			inet_aton(dns2_s, &config.dns2);
		}
		memset(&ifc, 0x0, sizeof(ifc));
		if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &ifc, sizeof(ifc)))
			return CGI_ERR_FAIL;
		if (ifc.isbridge)
			return CGI_ERR_NOSUPPORT;
		uid = 0;
		memset(&ifc, 0x0, sizeof(ifc));
		if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &ifc, sizeof(ifc)))
			return CGI_ERR_FAIL;
		if (enable_s)
			config.enable = atoi(enable_s);
		else
			config.enable = 1;
		if (!time_s)
			config.time = 12;
		else
			config.time = atoi(time_s);
		if(config.time < 1 || config.time > 720)
			return CGI_ERR_INPUT;
		inet_aton(ip_s, &config.ip);
		inet_aton(mask_s, &config.mask);
		inet_aton(start_s, &config.start);
		inet_aton(end_s, &config.end);
		if (checkmask(ntohl(config.mask.s_addr)))
			return CGI_ERR_INPUT;
		if (ntohl(config.end.s_addr) < ntohl(config.start.s_addr))
			return CGI_ERR_INPUT;
		if (ip_check2(config.ip.s_addr, config.start.s_addr, config.mask.s_addr))
			return CGI_ERR_INPUT;
		if (ip_check2(config.ip.s_addr, config.end.s_addr, config.mask.s_addr))
			return CGI_ERR_INPUT;
		ifc.statip.ip = config.ip;
		ifc.statip.mask = config.mask;
		if (mu_msg(IF_MOD_PARAM_SET, &ifc,  sizeof(ifc), NULL, 0))
			return CGI_ERR_FAIL;
		if (mu_msg(UPNPD_SERVER_SET, NULL, 0, NULL, 0))
			return CGI_ERR_FAIL;
		_ADD_SYS_LOG("LAN口设置成功，当前IP地址为%s", ip_s);
		if (mu_msg(DNSMASQ_DHCP_SET, &config, sizeof(config), NULL, 0))
			return CGI_ERR_FAIL;
		obj= json_object_new_int(reboot);
		json_object_object_add(response, "reboot", obj);
		obj= json_object_new_int(config.time);
		json_object_object_add(response, "time", obj);
		return 0;
	}
	memset(&ifc, 0x0, sizeof(ifc));
	if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &ifc, sizeof(ifc)))
		return CGI_ERR_FAIL;
	if (mu_msg(DNSMASQ_DHCP_SHOW, NULL, 0, &config, sizeof(config)))
		return CGI_ERR_FAIL;
	obj= json_object_new_int(reboot);
	json_object_object_add(response, "reboot", obj);
	obj= json_object_new_string(inet_ntoa(config.start));
	json_object_object_add(response, "start", obj);
	obj= json_object_new_string(inet_ntoa(config.end));
	json_object_object_add(response, "end", obj);
	obj= json_object_new_string(inet_ntoa(config.ip));
	json_object_object_add(response, "ip", obj);
	obj= json_object_new_string(inet_ntoa(config.mask));
	json_object_object_add(response, "mask", obj);
	obj= json_object_new_int(config.enable);
	json_object_object_add(response, "enable", obj);
	obj= json_object_new_int(config.time);
	json_object_object_add(response, "time", obj);
	snprintf(macstr, sizeof(macstr), "%02X:%02X:%02X:%02X:%02X:%02X", MAC_SPLIT(ifc.mac));
	obj= json_object_new_string(macstr);
	json_object_object_add(response, "mac", obj);
	if (config.dns1.s_addr)
		obj = json_object_new_string(inet_ntoa(config.dns1));
	else
		obj = json_object_new_string("");
	json_object_object_add(response, "dns1", obj);
	if (config.dns2.s_addr)
		obj = json_object_new_string(inet_ntoa(config.dns2));
	else
		obj = json_object_new_string("");
	json_object_object_add(response, "dns2", obj);
	return 0;
}

int pro_net_static_dhcp_hander(server_t* srv, connection_t *con, struct json_object*response)
{
	int i;
	char macstr[32];
	struct in_addr ip;
	unsigned char mac[ETH_ALEN];
	struct ip_mac config[IP_MAC_MAX];
	struct json_object *obj;
	struct json_object *hosts;
	struct json_object *host;
	char *ip_s = NULL, *mac_s = NULL, *act_s = NULL;
	
	CHECK_LOGIN;
	if (mu_msg(DNSMASQ_SHOST_SHOW, NULL, 0, &config, sizeof(config)))
		return CGI_ERR_FAIL;
	if (connection_is_set(con)) {
		ip_s = con_value_get(con, "ip");
		mac_s = con_value_get(con, "mac");
		act_s = con_value_get(con, "act");
		if (!ip_s || !mac_s || !act_s)
			return CGI_ERR_INPUT;
		if (checkip(ip_s) || checkmac(mac_s))
			return CGI_ERR_INPUT;
		inet_aton(ip_s, &ip);
		memset(mac, 0x0, sizeof(mac));
		parse_mac(mac_s, mac);
		for (i = 0; i < IP_MAC_MAX; i++) {
			if (!config[i].ip.s_addr)
				break;
			if (ip.s_addr == config[i].ip.s_addr
				|| !memcmp(mac, config[i].mac, ETH_ALEN))
				break;
		}
		if (!strcmp(act_s , "add")) {
			if (i == IP_MAC_MAX)
				return CGI_ERR_FULL;
			config[i].ip.s_addr = ip.s_addr;
			memcpy(config[i].mac, mac, ETH_ALEN);
		} else if (!strcmp(act_s, "del")) {
			if (i == IP_MAC_MAX)
				return CGI_ERR_NONEXIST;
			memset(&config[i], 0x0, sizeof(struct ip_mac));
		} else
			return CGI_ERR_INPUT;
		if (mu_msg(DNSMASQ_SHOST_SET, &config, sizeof(config), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	hosts = json_object_new_array();
	for (i = 0; i < IP_MAC_MAX; i++) {
		if (!config[i].ip.s_addr)
			continue;
		host = json_object_new_object();
		obj = json_object_new_string(inet_ntoa(config[i].ip));
		json_object_object_add(host, "ip", obj);
		snprintf(macstr, sizeof(macstr) - 1, MAC_FORMART, MAC_SPLIT(config[i].mac));
		obj = json_object_new_string(macstr);
		json_object_object_add(host, "mac", obj);
		json_object_array_add(hosts, host);
	}
	json_object_object_add(response, "hosts", hosts);
	return 0;	
}

int pro_net_ipmac_bind_hander(server_t* srv, connection_t *con, struct json_object*response)
{
	int i;
	char macstr[32];
	struct in_addr ip;
	unsigned char mac[ETH_ALEN];
	struct ip_mac config[IP_MAC_MAX];
	struct json_object *obj;
	struct json_object *hosts;
	struct json_object *host;
	char *ip_s = NULL, *mac_s = NULL, *act_s = NULL;
	
	CHECK_LOGIN;
	if (mu_msg(DNSMASQ_IPMAC_SHOW, NULL, 0, &config, sizeof(config)))
		return CGI_ERR_FAIL;
	if (connection_is_set(con)) {
		ip_s = con_value_get(con, "ip");
		mac_s = con_value_get(con, "mac");
		act_s = con_value_get(con, "act");
		
		if (!ip_s || !mac_s || !act_s)
			return CGI_ERR_INPUT;
		if (checkip(ip_s) || checkmac(mac_s))
			return CGI_ERR_INPUT;
		inet_aton(ip_s, &ip);
		memset(mac, 0x0, sizeof(mac));
		parse_mac(mac_s, mac);
		for (i = 0; i < IP_MAC_MAX; i++) {
			if (!config[i].ip.s_addr)
				break;
			if (ip.s_addr == config[i].ip.s_addr
				|| !memcmp(mac, config[i].mac, ETH_ALEN))
				break;
		}
		if (!strcmp(act_s , "add")) {
			if (i == IP_MAC_MAX)
				return CGI_ERR_FULL;
			config[i].ip.s_addr = ip.s_addr;
			memcpy(config[i].mac, mac, ETH_ALEN);
		} else if (!strcmp(act_s, "del")) {
			if (i == IP_MAC_MAX)
				return CGI_ERR_NONEXIST;
			memset(&config[i], 0x0, sizeof(struct ip_mac));
		} else
			return CGI_ERR_INPUT;
		if (mu_msg(DNSMASQ_IPMAC_SET, &config, sizeof(config), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	hosts = json_object_new_array();
	for (i = 0; i < IP_MAC_MAX; i++) {
		if (!config[i].ip.s_addr)
			continue;
		host = json_object_new_object();
		obj = json_object_new_string(inet_ntoa(config[i].ip));
		json_object_object_add(host, "ip", obj);
		snprintf(macstr, sizeof(macstr) - 1, MAC_FORMART, MAC_SPLIT(config[i].mac));
		obj = json_object_new_string(macstr);
		json_object_object_add(host, "mac", obj);
		json_object_array_add(hosts, host);
	}
	json_object_object_add(response, "hosts", hosts);
	return 0;	
}

static void get_userid_md5(char *md5str)
{
	int i = 0;
	char str[128];
	unsigned char md5[64];
	unsigned int usrid;

	md5str[0] = 0;
	usrid = get_usrid();
	if (!usrid)
		return;

	snprintf(str, sizeof(str), "&*E#N@DER_$%u", usrid);
	get_md5_numbers((unsigned char *)str, md5, strlen(str));
	memset(str, 0x0, sizeof(str));
	for (i = 0; i < 16; i++)
		sprintf(&md5str[i*2], "%02X", md5[i]);	
}

static void get_login_account_md5(char *user, char *pwd, char *md5str)
{
	int i = 0;
	char str[128];
	unsigned char md5[64];
	
	snprintf(str, sizeof(str), "%s%s", user, pwd);
	get_md5_numbers((unsigned char *)str, md5, strlen(str));
	memset(str, 0x0, sizeof(str));
	for (i = 0; i < 16; i++)
		sprintf(&md5str[i*2], "%02X", md5[i]);
}


/*system module cgi*/
int pro_sys_login_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int i = 0;
	struct in_addr addr;
	struct host_info info;
	struct nlk_sys_msg nlk;
	char *usrid, userid_md5[64], account_md5[64];
	struct sys_account suser;
	struct sys_guest guest;

	if (!con->ip_from)
		return CGI_ERR_FAIL;

	usrid = con_value_get(con, "usrid");
	if (!usrid)
		return CGI_ERR_INPUT;

	get_userid_md5(userid_md5);
	if (!strncasecmp(userid_md5, usrid, 32))
		goto sucess;
	if (!mu_msg(SYSTME_MOD_GET_ACCOUNT, NULL, 0, &suser, sizeof(suser))) {
		get_login_account_md5(suser.user, suser.password, account_md5);
		if (!strncasecmp(account_md5, usrid, 32))
			goto sucess;
	}
	for (i = 0; i < SYS_GUEST_MX; i++) {
		if (mu_msg(SYSTME_MOD_GET_ACCOUNT, &i, sizeof(int), &guest, sizeof(guest)))
			continue;
		get_login_account_md5(guest.user, guest.password, account_md5);
		if (!strncasecmp(account_md5, usrid, 32))
			goto sucess;
	}
	goto fail;
sucess:
	if (con->ip_from) {
		_ADD_SYS_LOG("用户从%s登录", con->ip_from);
		server_list_add(con->ip_from, srv);
	}
	return 0;
fail:
	inet_aton(con->ip_from, &addr);
	if (!dump_host_info(addr, &info)) {
		memset(&nlk, 0x0, sizeof(nlk));
		nlk.comm.gid = NLKMSG_GRP_SYS;
		nlk.comm.mid = SYS_GRP_MID_ADMIN;
		memcpy(nlk.msg.admin.mac, info.mac, ETH_ALEN);
		nlk_event_send(NLKMSG_GRP_SYS, &nlk, sizeof(nlk));
	}
	return CGI_ERR_FAIL;

}

int pro_sys_setting_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	CHECK_LOGIN;
	int flag = SYSFLAG_RECOVER;
	char *action  = NULL, dataup[8];
	
	if (!connection_is_set(con))
		return CGI_ERR_INPUT; 
	action  = con_value_get(con, "action");
	if (!action)
		return CGI_ERR_FAIL;
	if (!strncmp(action, "default", 7)) {
		if (mu_msg(SYSTEM_MOD_SYS_DEFAULT, &flag, sizeof(int), NULL, 0))
			return CGI_ERR_FAIL;
		//send to cloud, unbind
		CC_PUSH2(dataup, 0, 8);
		CC_PUSH2(dataup, 2, CSO_REQ_ROUTER_RESET);
		CC_MSG_ADD(dataup, 8);
	} else if (!strncmp(action, "reboot", 6)) {
		if (mu_msg(SYSTEM_MOD_SYS_REBOOT, NULL, 0, NULL, 0))
			return CGI_ERR_FAIL;
	} else
		return CGI_ERR_INPUT; 
	return 0;
}

int pro_sys_deviceid_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	char tmpbuf[64];
	unsigned int id;
	struct json_object* obj;
	
	if (connection_is_set(con))
		return CGI_ERR_INPUT;
	id = get_devid();
	sprintf(tmpbuf, "%u", id);
	obj= json_object_new_string(tmpbuf);
	json_object_object_add(response, "deviceid", obj);
	return 0;
}

int pro_sys_sdevice_check_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int status = 0;
	struct json_object* obj;
	
	if (connection_is_set(con))
		return CGI_ERR_INPUT;
	if (mu_msg(SYSTEM_MOD_DEV_CHECK, NULL, 0, &status, sizeof(status)))
		return CGI_ERR_FAIL;
	obj= json_object_new_int(status);
	json_object_object_add(response, "config_status", obj);
	obj= json_object_new_string("CY_WiFi");
	json_object_object_add(response, "name", obj);
	return 0;
}

int pro_sys_firmup_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	CHECK_LOGIN;
	if (!connection_is_set(con))
		return CGI_ERR_INPUT;
	if (mu_msg(SYSTEM_MOD_FW_UPDATE, NULL, 0, NULL, 0))
		return CGI_ERR_FAIL;
	return 0;
}

static int check_firmare(void)
{
	struct image_header iheader;
	int len;
	int imagefd;
	int ret = -1;
	char *retstr;
	char firmname[32];

	retstr = read_firmware("VENDOR");
	if (!retstr)
		return -1;
	len = snprintf(firmname, sizeof(firmname), "%s_", retstr);
	retstr = read_firmware("PRODUCT");
	if (!retstr)
		return -1;
	snprintf(&firmname[len], sizeof(firmname) - len, "%s", retstr);
	imagefd = open(FW_LOCAL_FILE, O_RDONLY);
	if (imagefd < 0)
		return -1;
	len = read(imagefd, (void *)&iheader, sizeof(iheader));
	if (len != sizeof(iheader))
		goto error;
	if (strncmp((char *)iheader.ih_name, firmname, 15))
		goto error;
	ret = 0;
error:
	close(imagefd);
	return ret;
}

int pro_sys_local_firmup_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	CHECK_LOGIN;
	if (!connection_is_set(con))
		return CGI_ERR_INPUT;
	/*  check firmware valid */
	if (check_firmare())
		return CGI_ERR_FILE;
	if (mu_msg(SYSTME_MOD_LOCAL_UPDATE, NULL, 0, NULL, 0))
		return CGI_ERR_FAIL;
	return 0;
}

int pro_sys_upload_status_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int status = 0;
	struct json_object* obj;

	CHECK_LOGIN;
	if (connection_is_set(con))
		return CGI_ERR_INPUT;
	if (!access(FW_LOCAL_FILE, F_OK))
		status = 1;
	obj = json_object_new_int(status);
	json_object_object_add(response, "status", obj);
	return 0;
}

int pro_sys_firmstatus_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int nr = 0;
	char *flag = NULL;
	struct fw_info info;
	struct json_object* obj;
	char model[512];

	if (connection_is_set(con)) {
		flag = con_value_get(con, "flag");
		if((flag == NULL) || (strcmp(flag, "get") != 0))
			return CGI_ERR_INPUT;
		if (mu_msg(SYSTEM_MOD_FW_DOWNLOAD, NULL, 0, NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}

	while(1) {
		if (nr++ >= 16)
			break;
		memset(&info, 0x0, sizeof(info));
		mu_msg(SYSTEM_MOD_VERSION_CHECK, NULL, 0, &info, sizeof(struct fw_info));
		if (info.flag != FW_FLAG_NOFW)
			break;
		usleep(500000);
	}
	if (info.flag == FW_FLAG_FINISH)
		info.flag = FW_FLAG_NOFW;
	obj = json_object_new_int(info.flag);
	json_object_object_add(response, "status", obj);
	
	obj = json_object_new_int(info.speed);
	json_object_object_add(response, "speed", obj);
	
	obj = json_object_new_int(info.cur);
	json_object_object_add(response, "curl", obj);
	
	obj = json_object_new_int(atoi(info.size));
	json_object_object_add(response, "total", obj);
	
	obj = json_object_new_string(info.name);
	json_object_object_add(response, "name", obj);

	obj = json_object_new_string(info.time);
	json_object_object_add(response, "time", obj);
	
	obj = json_object_new_string(info.ver);
	json_object_object_add(response, "newfirm_version", obj);
	
	obj = json_object_new_string(info.desc);
	json_object_object_add(response, "manual", obj);

	obj = json_object_new_string(info.cur_ver);
	json_object_object_add(response, "localfirm_version", obj);

	flag = read_firmware("MODEL");
	if (flag && flag[0]) {
		arr_strcpy(model, flag);
	} else {
		nr = snprintf(model, sizeof(model), "%s", read_firmware("VENDOR"));
		nr += snprintf(&model[nr], sizeof(model) - nr, "_%s", read_firmware("PRODUCT"));
	}
	obj = json_object_new_string(model);
	json_object_object_add(response, "model", obj);

	obj = json_object_new_string(read_firmware("DATE") ? : "");
	json_object_object_add(response, "born", obj);

	unsigned long port_status = 0;
	mu_msg(SYSTEM_MOD_PORT_STATUS, NULL, 0, &port_status, sizeof(port_status));
	char bits[6] = {0};
	int i;
	for (i = 0; i < 5; i++) {
		if (port_status & (0x01 << i))
			bits[i] = '1';
		else
			bits[i] = '0';
	}
	obj = json_object_new_string(bits);
	json_object_object_add(response, "port_status", obj);
	return 0;
}

int pro_sys_local_config_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	CHECK_LOGIN;
	if (connection_is_set(con)) {
		if (access(CONFIG_LOCAL_FILE, F_OK))
			return CGI_ERR_FAIL;
		if (mu_msg(SYSTEM_MOD_CONFIG_SET, NULL, 0, NULL, 0))
			return CGI_ERR_FAIL;
	} else {
		if (mu_msg(SYSTEM_MOD_CONFIG_GET, NULL, 0, NULL, 0))
			return CGI_ERR_FAIL;
	}
	return 0;
}

int pro_sys_config_status_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int status = 0;
	struct json_object* obj;

	CHECK_LOGIN;
	if (connection_is_set(con))
		return CGI_ERR_INPUT;
	if (!access(CONFIG_LOCAL_FILE, F_OK))
		status = 1;
	obj = json_object_new_int(status);
	json_object_object_add(response, "status", obj);
	return 0;
}

int cgi_mtd_param_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	char value[32];
	char valname[32];
	char *valname_s;
	struct json_object* obj;
	
	CHECK_LOGIN;
	if (connection_is_set(con))
		return CGI_ERR_INPUT;
	valname_s = con_value_get(con, "valname");
	if (valname_s == NULL)
		return CGI_ERR_INPUT;
	arr_strcpy(valname, valname_s);
	if (mu_msg(SYSTME_MOD_MTD_PARAM, valname, sizeof(valname), value, sizeof(value)))
		return CGI_ERR_FAIL;
	obj = json_object_new_string(value);
	json_object_object_add(response, "value", obj);
	return 0;
}

/*QOS module cgi*/
int pro_net_qos_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	struct qos_conf config;
	struct json_object*obj;
	char *enable_s = NULL, *up_s = NULL, *down_s = NULL;
	
	if (connection_is_set(con)) {
		CHECK_LOGIN;
		enable_s = con_value_get(con, "enable");
		up_s = con_value_get(con, "up");
		down_s = con_value_get(con, "down");
		if (enable_s == NULL || up_s == NULL || down_s == NULL)
			return CGI_ERR_INPUT;

		memset(&config, 0, sizeof(config));
		if (enable_s[0] == '1')
			config.enable = 1;
		else if (enable_s[0] == '0')
			config.enable = 0;
		else
			return CGI_ERR_INPUT;
		config.up = atol(up_s);
		config.down = atol(down_s);
		
		if ((!config.up) || (!config.down)) {
			CGI_PRINTF("qos up/down err, %d, %d\n", config.up, config.down);
			return CGI_ERR_INPUT;
		}
		if (mu_msg(QOS_PARAM_SET, &config, sizeof(config), NULL, 0))
			return CGI_ERR_FAIL;
		if (config.enable)
			_ADD_SYS_LOG("QOS功能已启用");
		else
			_ADD_SYS_LOG("QOS功能已停用");
		return 0;
	}
	
	if (mu_msg(QOS_PARAM_SHOW, NULL, 0, &config, sizeof(config)))
		return CGI_ERR_FAIL;
	obj= json_object_new_int(config.enable);
	json_object_object_add(response, "enable", obj);
	obj= json_object_new_int(config.up);
	json_object_object_add(response, "up", obj);
	obj= json_object_new_int(config.down);
	json_object_object_add(response, "down", obj);
	return 0;
}

int pro_net_testspeed_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	char *ptr = NULL;
	struct tspeed_info info;
	struct json_object *obj = NULL;

	if (connection_is_set(con)) {
		if (mu_msg(QOS_TEST_SPEED, NULL, 0, NULL, 0))
			return CGI_ERR_FAIL;
	}

	ptr = con_value_get(con, "act");
	if (ptr && !strcmp(ptr, "off")) {
		if (mu_msg(QOS_TEST_BREAK, NULL, 0, NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	} else {
		if (mu_msg(QOS_SPEED_SHOW, NULL, 0, &info, sizeof(info)))
			return CGI_ERR_FAIL;
	}
	obj = json_object_new_int(info.flag);
	json_object_object_add(response, "flag", obj);
	if (info.flag == 1) {
		obj = json_object_new_int(info.ispeed);
		json_object_object_add(response, "ispeed", obj);
	} else if (info.flag == 2) {
		obj = json_object_new_int(info.down);
		json_object_object_add(response, "down_speed", obj);
		obj = json_object_new_int(info.up);
		json_object_object_add(response, "up_speed", obj);
		obj = json_object_new_int(info.delay);
		json_object_object_add(response, "delay", obj);
	}
	return 0;
}

/*url safe module cgi*/
int pro_net_rule_table_security_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int enable = 0;
	char *enable_s = NULL;
	struct json_object* obj;
	
	if (connection_is_set(con)) {
		CHECK_LOGIN;
		enable_s = con_value_get(con, "sercurity");
		if (!enable_s)
			return CGI_ERR_INPUT;
		enable = atoi(enable_s);
		if (mu_msg(URL_SAFE_MOD_SET_ENABLE, &enable, sizeof(enable), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	enable_s = uci_getval("qos_rule", "security", "status");
	if (!enable_s)
		return CGI_ERR_FAIL;
	enable = atoi(enable_s);
	obj= json_object_new_int(enable);
	json_object_object_add(response, "security", obj);
	free(enable_s);
	return 0;
}

int pro_net_url_safe_redirect_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	struct urlsafe_trust info; 
	char* ip = con_value_get(con,"ip");
	char* url = con_value_get(con,"url");

	if (NULL == ip || NULL == url)
		return CGI_ERR_INPUT;
	memset(&info, 0x0, sizeof(info));
	if (checkip(ip) || !strlen(url))
		return CGI_ERR_INPUT;
	inet_aton(ip, &info.ip);
	__arr_strcpy_end(info.url, (unsigned char *)url, sizeof(info.url), '\0');
	if (mu_msg(URL_SAFE_MOD_SET_IP, &info, sizeof(info), NULL, 0))
		return CGI_ERR_FAIL;
	return 0;
}

int cgi_net_advert_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int enable;
	char *enable_s = NULL;
	struct json_object* obj;
	
	if (connection_is_set(con)) {
		CHECK_LOGIN;
		enable_s = con_value_get(con, "enable");
		if (!enable_s)
			return CGI_ERR_INPUT;
		enable = atoi(enable_s);
		if (mu_msg(ADVERT_MOD_ACTION, &enable, sizeof(enable), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	if (mu_msg(ADVERT_MOD_DUMP, NULL, 0, &enable, sizeof(enable)))
		return CGI_ERR_FAIL;
	obj= json_object_new_int(enable);
	json_object_object_add(response, "enable", obj);
	return 0;
}

int cgi_net_vpn_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int len = 0;
	char *retstr;
	int connect = 0;
	struct vpn_info info;
	struct json_object* obj;
	
	if (connection_is_set(con)) {
		CHECK_LOGIN;
		retstr = con_value_get(con, "user");
		if (retstr == NULL)
			return CGI_ERR_INPUT;
		len = strlen(retstr);
		if (len >= sizeof(info.user))
			return CGI_ERR_INPUT;
		memset(&info, 0x0, sizeof(info));
		arr_strcpy(info.user, retstr);
		retstr = con_value_get(con, "password");
		if (retstr == NULL)
			return CGI_ERR_INPUT;
		len = strlen(retstr);
		if (len >= sizeof(info.password))
			return CGI_ERR_INPUT;
		arr_strcpy(info.password, retstr);
		if (mu_msg(VPN_MOD_PARAM_SET, &info, sizeof(info), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	if (mu_msg(VPN_MOD_STATUS_DUMP, NULL, 0, &info, sizeof(info)))
		return CGI_ERR_FAIL;
	if (info.ipcp.ip.s_addr)
		connect = 1;
	obj = json_object_new_int(connect);
	json_object_object_add(response, "connect", obj);
	obj = json_object_new_string(inet_ntoa(info.ipcp.ip));
	json_object_object_add(response, "ip", obj);
	obj = json_object_new_string(inet_ntoa(info.ipcp.mask));
	json_object_object_add(response, "mask", obj);
	obj = json_object_new_string(inet_ntoa(info.ipcp.gw));
	json_object_object_add(response, "gw", obj);
	obj = json_object_new_string(inet_ntoa(info.ipcp.dns[0]));
	json_object_object_add(response, "dns", obj);
	obj = json_object_new_string(inet_ntoa(info.ipcp.dns[1]));
	json_object_object_add(response, "dns1", obj);
	obj = json_object_new_string(info.user);
	json_object_object_add(response, "user", obj);
	obj = json_object_new_string(info.password);
	json_object_object_add(response, "password", obj);
	obj = json_object_new_string(info.callid);
	json_object_object_add(response, "cid", obj);
	obj = json_object_new_int(info.vpn_enable);
	json_object_object_add(response, "vpn_support", obj);
	return 0;
}

int cgi_net_tunnel_hander(server_t* srv, connection_t *con, struct json_object*response)
{
	char *retstr;
	int i, len = 0, action;
	struct tunnel_conf tcfg;
	struct sys_msg_ipcp info;
	struct json_object *obj;
	struct json_object *ts;
	struct json_object *t;

	memset(&tcfg, 0x0, sizeof(tcfg));
	if (connection_is_set(con)) {
		CHECK_LOGIN;
		CON_GET_ACT(retstr, con, action);
		if (action == NLK_ACTION_DEL) {
			retstr = con_value_get(con, "index");
			if (!retstr)
				return CGI_ERR_INPUT;
			tcfg.index = atoi(retstr);
			if (mu_msg(TUNNEL_MOD_SET, &tcfg, sizeof(tcfg), NULL, 0))
				return CGI_ERR_FAIL;
			return 0;
		}
		if (action == NLK_ACTION_ADD) {
			tcfg.index = -1;
		} else if (action == NLK_ACTION_MOD) {
			retstr = con_value_get(con, "index");
			if (!retstr)
				return CGI_ERR_INPUT;
			tcfg.index = atoi(retstr);
		} else {
			return CGI_ERR_INPUT;
		}

		retstr = con_value_get(con, "enable");
		if (retstr == NULL)
			return CGI_ERR_INPUT;
		tcfg.enable = atoi(retstr);
		
		retstr = con_value_get(con, "type");
		if (retstr == NULL)
			return CGI_ERR_INPUT;
		if (!strcmp(retstr, "pptp"))
			tcfg.type = PPPD_TYPE_TUNNEL_PPTP;
		else if (!strcmp(retstr, "l2tp"))
			tcfg.type = PPPD_TYPE_TUNNEL_L2TP;
		else
			return CGI_ERR_INPUT;
		retstr = con_value_get(con, "mppe");
		if (retstr == NULL)
			return CGI_ERR_INPUT;
		tcfg.mppe = atoi(retstr);
		retstr = con_value_get(con, "user");
		if (retstr == NULL)
			return CGI_ERR_INPUT;
		len = strlen(retstr);
		if (!len || len >= sizeof(tcfg.user))
			return CGI_ERR_INPUT;
		arr_strcpy(tcfg.user, retstr);
		retstr = con_value_get(con, "password");
		if (retstr == NULL)
			return CGI_ERR_INPUT;
		len = strlen(retstr);
		if (!len || len >= sizeof(tcfg.password))
			return CGI_ERR_INPUT;
		arr_strcpy(tcfg.password, retstr);

		retstr = con_value_get(con, "port");
		if (retstr == NULL)
			return CGI_ERR_INPUT;
		tcfg.port = atoi(retstr);
		if (!tcfg.port)
			return CGI_ERR_INPUT;
		
		retstr = con_value_get(con, "serverip");
		if (retstr == NULL)
			return CGI_ERR_INPUT;
		len = strlen(retstr);
		if (len > 0) {
			if (checkip(retstr))
				return CGI_ERR_INPUT;
			inet_aton(retstr, &tcfg.serverip);
		}
		retstr = con_value_get(con, "sdomain");
		if (retstr == NULL)
			return CGI_ERR_INPUT;
		len = strlen(retstr);
		if (len >= sizeof(tcfg.sdomain))
			return CGI_ERR_INPUT;
		arr_strcpy(tcfg.sdomain, retstr);
		if (!tcfg.serverip.s_addr && !tcfg.sdomain[0])
			return CGI_ERR_INPUT;
		if (mu_msg(TUNNEL_MOD_SET, &tcfg, sizeof(tcfg), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	
	ts = json_object_new_array();
	for (i = 0; i < TUNNEL_MX; i++) {
		if (mu_msg(TUNNEL_MOD_SHOW, &i, sizeof(int), &tcfg, sizeof(tcfg)))
			continue;
		if (mu_msg(TUNNEL_MOD_INFO, &i, sizeof(int), &info, sizeof(info)))
			continue;
		t = json_object_new_object();
		obj = json_object_new_int(tcfg.index);
		json_object_object_add(t, "index", obj);

		obj = json_object_new_int(tcfg.enable);
		json_object_object_add(t, "enable", obj);
		
		obj = json_object_new_int(tcfg.mppe);
		json_object_object_add(t, "mppe", obj);
		
		if (tcfg.type == PPPD_TYPE_TUNNEL_PPTP)
			obj = json_object_new_string("pptp");
		else if (tcfg.type == PPPD_TYPE_TUNNEL_L2TP)
			obj = json_object_new_string("l2tp");
		json_object_object_add(t, "type", obj);

		obj = json_object_new_int(tcfg.port);
		json_object_object_add(t, "port", obj);
		
		if (tcfg.serverip.s_addr)
			obj = json_object_new_string(inet_ntoa(tcfg.serverip));
		else
			obj = json_object_new_string("");
		json_object_object_add(t, "serverip", obj);
		
		obj = json_object_new_string(tcfg.sdomain);
		json_object_object_add(t, "sdomain", obj);
		
		obj = json_object_new_string(tcfg.user);
		json_object_object_add(t, "user", obj);
		
		obj = json_object_new_string(tcfg.password);
		json_object_object_add(t, "password", obj);
		
		obj = json_object_new_string(inet_ntoa(info.ip));
		json_object_object_add(t, "ip", obj);
		
		obj = json_object_new_string(inet_ntoa(info.mask));
		json_object_object_add(t, "mask", obj);
		
		obj = json_object_new_string(inet_ntoa(info.gw));
		json_object_object_add(t, "gw", obj);
		
		obj = json_object_new_string(inet_ntoa(info.dns[0]));
		json_object_object_add(t, "dns1", obj);
		
		obj = json_object_new_string(inet_ntoa(info.dns[1]));
		json_object_object_add(t, "dns2", obj);
		json_object_array_add(ts, t);
	}
	json_object_object_add(response, "tunnels", ts);
	return 0;
}

#define FILE_NAME "/tmp/ccapp/dnsprefer"
int cgi_sys_lanxun_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	struct json_object *obj=NULL;
	if(connection_is_set(con)){
		CHECK_LOGIN;
		char *value=con_value_get(con,"enabled");;
		if (value && strstr(value, "1")) {
			mu_msg(LANXUN_SET_OPEN,NULL, 0, NULL,0);
			return 0;
		}
		else if (value && strstr(value, "0")) {
			mu_msg(LANXUN_SET_CLOSE,NULL, 0, NULL,0);
			return 0;			
		}
		return 1;
	}

	char retstr[8];
	int status=-1;
	if (mu_msg(LANXUN_GET_STATUS, NULL, 0, &status, sizeof(status))){
		return CGI_ERR_FAIL;
	}
	sprintf(retstr,"%d",status);
	obj= json_object_new_string(retstr);
	json_object_object_add(response,"status",obj);
	
//read file
 	struct printbuf *pb=NULL;
	char buf[JSON_FILE_BUF_SIZE];

	struct json_tokener* tok = NULL;
	
	int fd, ret;
	if((fd = open(FILE_NAME, O_RDONLY)) < 0){
		goto RET;
	}
	if(!(pb = printbuf_new())) {
		goto RET;	
	}
	while((ret = read(fd, buf, JSON_FILE_BUF_SIZE)) > 0) {
		printbuf_memappend(pb, buf, ret);
	}
	close(fd);
	if(ret < 0) {
		goto RET;
	}
	tok = json_tokener_new();
	if(tok == NULL){
		goto RET;
	}
	obj = json_tokener_parse_ex(tok, pb->buf, -1);
	if(tok->err != json_tokener_success){
		goto RET;
	}
	json_object_object_add(response,"info",obj);
	if(NULL == obj || obj == 0){
		goto RET;
	}
//	json_object_object_add(response,"info",obj);
RET:
	if(pb){
		printbuf_free(pb);
	}
	if(tok){
		json_tokener_free(tok);
	}
	return 0;
}

int cgi_sys_telcom_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	struct json_object *obj=NULL;
	char *value;
	int disable = 0;
	if(connection_is_set(con)){
		CHECK_LOGIN;
		value = con_value_get(con,"disabled");
		if (value) {
			system("touch /etc/config/telcom");
			system("uci set telcom.sync=telcom_config");
			uci_setval("telcom","sync","disabled",value);
			return 0;
		} 
	} else {
		value = uci_getval("telcom", "sync", "disabled");
		if (value) {
			disable = atoi(value);
			free(value);
		}
		obj= json_object_new_int(disable);
		json_object_object_add(response,"disabled",obj);
		return 0;
	}
	return 1;
}


int cgi_sys_log_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	int i = 0, nr = 0;
	char retstr[IGD_NAME_LEN];
	struct log_desc info[SYS_LOG_MX_NR];
	struct json_object *obj, *logs, *log;
	
	if (connection_is_set(con))
		return CGI_ERR_INPUT;
	CHECK_LOGIN;
	char *download = con_value_get(con,"download");
	if (download) {
		nr = mu_msg(LOG_MOD_DOWNLOAD, NULL, 0, NULL, 0);
		if (nr < 0)
			return CGI_ERR_FAIL;
		return 0;
	}
	memset(&info, 0x0, sizeof(info));
	nr = mu_msg(LOG_MOD_DUMP, NULL, 0, info, sizeof(struct log_desc) * SYS_LOG_MX_NR);
	if (nr < 0)
		return CGI_ERR_FAIL;
	obj = json_object_new_int(nr);
	json_object_object_add(response, "count", obj);
	logs = json_object_new_array();
	for (i = 0; i < nr; i++) {
		log = json_object_new_object();
		snprintf(retstr, sizeof(retstr), "%lu", info[i].timer);
		obj = json_object_new_string(retstr);
		json_object_object_add(log, "time", obj);
		obj = json_object_new_string(info[i].desc);
		json_object_object_add(log, "event", obj);
		json_object_array_add(logs, log);
	}
	json_object_object_add(response, "logs", logs);
	return 0;
}

int pro_sys_retime_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	unsigned char i = 0;
	struct json_object* obj, *week, *day;
	struct sys_time tm;

	if (connection_is_set(con)) {
		CHECK_LOGIN;
		memset(&tm, 0x0, sizeof(tm));
		ptr = con_value_get(con, "time_on");
		if (!ptr) {
			CGI_PRINTF("input err, %p\n", ptr);
			return CGI_ERR_INPUT;
		}
		tm.enable = atoi(ptr);
		ptr = con_value_get(con, "week");
		if (!ptr) {
			CGI_PRINTF("input err, %p\n", ptr);
			return CGI_ERR_INPUT;
		}
		for (i = 0; i < 7; i++) {
			if (*(ptr + i) == '\0') {
				CGI_PRINTF("week err\n");
				break;
			}
			if (*(ptr + i) == '1')
				CGI_BIT_SET(tm.day, i);
		}
		ptr = con_value_get(con, "loop");
		if (!ptr) {
			CGI_PRINTF("input err, %p\n", ptr);
			return CGI_ERR_INPUT;
		}
		tm.loop = atoi(ptr);
		CON_GET_CHECK_INT(ptr, con, tm.hour, "hour", 23);
		CON_GET_CHECK_INT(ptr, con, tm.min, "min", 59);
		if (mu_msg(SYSTME_MOD_TIME_REBOOT, &tm, sizeof(tm), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	if (mu_msg(SYSTME_MOD_TIME_REBOOT, NULL, 0, &tm, sizeof(tm)))
		return CGI_ERR_FAIL;
	obj = json_object_new_int(tm.enable);
	json_object_object_add(response, "time_on", obj);
	obj = json_object_new_int(tm.loop);
	json_object_object_add(response, "loop", obj);
	week = json_object_new_array();
	for (i = 0; i < 7; i++) {
		if (!CGI_BIT_TEST(tm.day, i))
			continue;
		day = json_object_new_int(i);
		json_object_array_add(week, day);
	}
	json_object_object_add(response, "week", week);
	obj = json_object_new_int(tm.hour);
	json_object_object_add(response, "hour", obj);
	obj = json_object_new_int(tm.min);
	json_object_object_add(response, "min", obj);
	return 0;
}

int pro_sys_timezone_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *tz = NULL;
	char *date = NULL;
	struct sys_timezone timezone;
	
	memset(&timezone, 0, sizeof(timezone));
	if (connection_is_set(con)) {		
		CHECK_LOGIN;
		tz = con_value_get(con, "tz");
		if (NULL == tz) {
			return CGI_ERR_INPUT;
		}
		strncpy(timezone.tz, tz, 15);	
		date = con_value_get(con, "date");
		if (date) {
			strncpy(timezone.tz_time, date, 31);		
		}
		if (mu_msg(SYSTEM_MOD_TIMEZONE_SET, &timezone, sizeof(timezone), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;		
	}
	if (mu_msg(SYSTEM_MOD_TIMEZONE_GET, NULL, 0, &timezone, sizeof(timezone)))
			return CGI_ERR_FAIL;
	struct json_object *obj;
	obj= json_object_new_string(timezone.tz);
	json_object_object_add(response, "tz", obj);
	obj= json_object_new_string(timezone.tz_time);
	json_object_object_add(response, "date", obj);	
	return 0;
}

int pro_usb_login_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	int num = 0;
	char *ptr = NULL, **array=NULL, passwd[64], str[128];
	struct sys_account info;

	CON_GET_STR(ptr, con, info.user, "user");
	if (strcmp(info.user, "admin"))
		return CGI_ERR_INPUT;

	if (!connection_is_set(con)) {
		ptr = con_value_get(con, "password");
		if (uuci_get("system.account.usb_password", &array, &num))
			memset(passwd, 0, sizeof(passwd));
		else {
			arr_strcpy(passwd, array[0]);
			uuci_get_free(array, num);
		}
		if ((!ptr && !passwd[0]) 
			|| (ptr && !strcmp(passwd, ptr))) {
			if (con->ip_from)
				server_list_add(con->ip_from, srv);
			return 0;
		}
		return CGI_ERR_FAIL;
	} else {
		CHECK_LOGIN;
		ptr = con_value_get(con, "password");
		if (!ptr) {
			uuci_set("system.account.usb_password=");
		} else {
			snprintf(str, sizeof(str),
				"system.account.usb_password=%s", ptr);
			uuci_set(str);
		}
		server_clean(srv);
	}
	return 0;
}

#ifdef DISKM_SUPPORT
#include <ftw.h>

#define STORAGE_PATH_PREFIX  "/data/"
#define STORAGE_PATH_CHECK(path) do { \
	if (strncmp(path, STORAGE_PATH_PREFIX, strlen(STORAGE_PATH_PREFIX))) \
		return CGI_ERR_INPUT; \
	if (strstr(path, "/../")) \
		return CGI_ERR_INPUT; \
} while(0)

#define STORAGE_SCANNING_MAX 2000
static int storage_scanning_num = 0;

enum STORAGE_FILE_TYPE {
	SFT_ALL = 0,
	SFT_OTHER,
	SFT_MUSIC,
	SFT_VIDEO,
	SFT_PIC,
	SFT_TXT,
	SFT_PACK,
	SFT_MAX,
};

struct storage_classify_info {
	unsigned long long nr;
	unsigned long long size;
};

static void *cgi_storage_ptr = NULL;

static char *music_suffix[] = {".mp3", ".wav", ".cda",
	".aiff", ".au", ".mid", ".wma", ".ra", ".vqf",
	".ogg", ".aac",	".ape", ".flac", ".wv", NULL};

static char *video_suffix[] = {".avi", ".mp4", ".mov", 
	".asf", ".wma", ".3gp", ".mpg", ".swf", ".flv", 
	".rm", ".mkv", ".rmvb", ".wmv", ".mpeg", ".asf",
	".navi", NULL};

static char *pic_suffix[] = {".bmp", ".gif", ".jpeg",
	".jpg", ".png", NULL};

static char *txt_suffix[] = {".txt", ".doc", ".docx", 
	".wps", ".pdf", ".ppt", ".xls", NULL};

static char *pack_suffix[] = {".7z", ".rar", ".zip",
	".gz", ".tar", NULL};

int file_check(char *arr[], char *name)
{
	int i = 0;

	while (arr[i]) {
		if(!strcasecmp(name, arr[i]))
			return 1;
		i++;
	}
	return 0;
}

static void prefix_encode(
	void *dst, int dlen, void *src, void *chars)
{
	char *d = dst;
	unsigned char *s = src;

	while ((*s != '\0') && (dlen >= 2)) {
		if (strchr(chars, *s)) {
			*d++ = '\\';
			dlen--;
		}
		*d++ = *s++;
		dlen--;
	}
	*d = '\0';
}

static int find_dir(const char *name, const struct stat *sb, int flag)
{
	char *ptr;
	struct storage_classify_info *sci = cgi_storage_ptr;
	struct storage_classify_info *tmp;

	if (!S_ISREG(sb->st_mode))
		return 0;

	if (!sci)
		return -1;
	if (storage_scanning_num > STORAGE_SCANNING_MAX) {
		CGI_PRINTF("too much exit\n");
		return -1;
	}
	storage_scanning_num++;
	ptr = strrchr(name, '.');
	if (!ptr) {
		tmp = &sci[SFT_OTHER];
	} else if (file_check(music_suffix, ptr)) {
		tmp = &sci[SFT_MUSIC];
	} else if (file_check(video_suffix, ptr)) {
		tmp = &sci[SFT_VIDEO];
	} else if (file_check(pic_suffix, ptr)) {
		tmp = &sci[SFT_PIC];
	} else if (file_check(txt_suffix, ptr)) {
		tmp = &sci[SFT_TXT];
	} else if (file_check(pack_suffix, ptr)) {
		tmp = &sci[SFT_PACK];
	} else {
		tmp = &sci[SFT_OTHER];
	}

	tmp->nr++;
	tmp->size += (unsigned long long)sb->st_size;

	sci[SFT_ALL].nr++;
	sci[SFT_ALL].size += (unsigned long long)sb->st_size;
	return 0;
}

int cgi_storage_info(server_t *srv, connection_t *con, struct json_object *response)
{
	void *info;
	int i, len;
	struct storage_classify_info sci[SFT_MAX];
	struct json_object *arr, *sci_obj, *sci_arr, *obj;
	disk_proto_t *diskinfo = NULL;

	memset(sci, 0, sizeof(sci));

	if(disk_api_call(MSG_DISK_INFO, NULL, 0, &info, &len) == DISK_FAILURE) {
		CGI_PRINTF("disk_api_call Failed\n");
		len = 0;
	}
	diskinfo = (disk_proto_t *)info;

	arr = json_object_new_array();
	while (len > 0) {
		if ((diskinfo->ptype &0xFF) == 99) {
			diskinfo++;
			len -= sizeof(disk_proto_t);
			continue;
		}

		storage_scanning_num = 0;
		cgi_storage_ptr = sci;
		if (ftw(diskinfo->partition.part_info.mntpoint, find_dir, 32) < 0)
			CGI_PRINTF("ftw failed\n");
		cgi_storage_ptr = NULL;

		sci_obj = json_object_new_object();

		obj = json_object_new_string(diskinfo->devname);
		json_object_object_add(sci_obj, "name", obj);

		obj = json_object_new_uint64(diskinfo->total);
		json_object_object_add(sci_obj, "total", obj);

		obj = json_object_new_uint64(diskinfo->used);
		json_object_object_add(sci_obj, "used", obj);

		obj = json_object_new_string(diskinfo->partition.part_info.mntpoint);
		json_object_object_add(sci_obj, "path", obj);

		for (i = SFT_ALL; i < SFT_MAX; i++) {
			sci_arr = json_object_new_array();

			obj = json_object_new_uint64(sci[i].nr);
			json_object_array_add(sci_arr, obj);

			obj = json_object_new_uint64(sci[i].size);
			json_object_array_add(sci_arr, obj);

			switch (i) {
			case SFT_ALL:
				json_object_object_add(sci_obj, "file", sci_arr);
				break;
			case SFT_OTHER:
				json_object_object_add(sci_obj, "other", sci_arr);
				break;
			case SFT_MUSIC:
				json_object_object_add(sci_obj, "music", sci_arr);
				break;
			case SFT_VIDEO:
				json_object_object_add(sci_obj, "video", sci_arr);
				break;
			case SFT_PIC:
				json_object_object_add(sci_obj, "pic", sci_arr);
				break;
			case SFT_TXT:
				json_object_object_add(sci_obj, "txt", sci_arr);
				break;
			case SFT_PACK:
				json_object_object_add(sci_obj, "pack", sci_arr);
				break;
			}
		}
		json_object_array_add(arr, sci_obj);

		diskinfo++;
		len -= sizeof(disk_proto_t);
	}

	json_object_object_add(response, "info", arr);
	if (info)
		free(info);
	return 0;
}

struct storage_kind_info {
	char path[4096];
	unsigned long long size;
};

struct storage_kind_dump {
	int start;
	int num;
	char **match;
	struct storage_kind_info *ski;
};

static int find_dir_classify(const char *name, const struct stat *sb, int flag)
{
	char *ptr;
	struct storage_kind_dump *skd = cgi_storage_ptr;

	if (!skd)
		return -1;
	if (storage_scanning_num > STORAGE_SCANNING_MAX) {
		CGI_PRINTF("too much exit\n");
		return -1;
	}
	storage_scanning_num++;

	if (skd->num <= 0)
		return 1;
	if (!S_ISREG(sb->st_mode))
		return 0;
	ptr = strrchr(name, '.');
	if (!ptr)
		return 0;
	if (!file_check(skd->match, ptr))
		return 0;
	if (skd->start > 1) {
		skd->start--;
		return 0;
	}
	skd->ski[skd->num - 1].size = (unsigned long long)sb->st_size;
	arr_strcpy(skd->ski[skd->num - 1].path, name);
	skd->num--;
	return 0;
}

int cgi_storage_file(server_t *srv, connection_t *con, struct json_object *response)
{
	int nr, i, path_len;
	char *path, *ptr;
	struct storage_kind_dump skd;
	struct json_object *arr, *skd_obj, *obj;

	CON_GET_INT(ptr, con, skd.num, "num");
	CON_GET_INT(ptr, con, skd.start, "start");
	path = con_value_get(con, "path");
	if (!path)
		return CGI_ERR_INPUT;
	ptr = con_value_get(con, "if");
	if (!ptr)
		return CGI_ERR_INPUT;
	if (!strcmp(ptr, "music")) {
		skd.match = music_suffix;
	} else if (!strcmp(ptr, "video")) {
		skd.match = video_suffix;
	} else if (!strcmp(ptr, "pic")) {
		skd.match = pic_suffix;
	} else if (!strcmp(ptr, "txt")) {
		skd.match = txt_suffix;
	} else if (!strcmp(ptr, "pack")) {
		skd.match = pack_suffix;
	} else {
		CGI_PRINTF("if err, %s\n", ptr);
		return CGI_ERR_INPUT;
	}
	if (skd.num <= 0)
		return CGI_ERR_INPUT;

	skd.ski = malloc(skd.num*sizeof(struct storage_kind_info));
	if (!skd.ski)
		return CGI_ERR_MALLOC;

	nr = skd.num;
	cgi_storage_ptr = &skd;
	storage_scanning_num = 0;
	if (ftw(path, find_dir_classify, 32) < 0)
		CGI_PRINTF("ftw failed\n");
	cgi_storage_ptr = NULL;

	path_len = strlen(path);

	arr = json_object_new_array();
	for (i = nr; i > skd.num; i--) {
		skd_obj = json_object_new_object();

		obj = json_object_new_string(skd.ski[i - 1].path + path_len);
		json_object_object_add(skd_obj, "path", obj);

		obj = json_object_new_uint64((unsigned long long)skd.ski[i - 1].size);
		json_object_object_add(skd_obj, "size", obj);

		json_object_array_add(arr, skd_obj);
	}
	json_object_object_add(response, "file", arr);
	free(skd.ski);
	return 0;
}

int cgi_storage_dir(server_t *srv, connection_t *con, struct json_object *response)
{
	DIR *dir;
	struct dirent *d;
	int i = 0, start, num, read_dir = 1, dir_flag;
	char path[4096], *ptr, str[4096];
	struct json_object *arr = NULL, *dir_obj, *obj;
	struct stat st;

	CON_GET_INT(ptr, con, num, "num");
	CON_GET_INT(ptr, con, start, "start");
	CON_GET_STR(ptr, con, path, "path");
	if (num < 0 || start < 0)
		return CGI_ERR_INPUT;

	STORAGE_PATH_CHECK(path);

read_file:
	dir = opendir(path);
	if (!dir)
		return CGI_ERR_INPUT;

	arr = arr ? arr : json_object_new_array();
	while (1) {
		d = readdir(dir);
		if (!d)
			break;
		if (!strcmp(".", d->d_name))
			continue;
		if (!strcmp("..", d->d_name))
			continue;
		snprintf(str, sizeof(str), "%s/%s", path, d->d_name);
		if (lstat(str, &st)) {
			CGI_PRINTF("stat fail, [%s]\n", str);
			continue;
		}
		dir_flag = S_ISDIR(st.st_mode) ? 1 : 0;
		if (dir_flag != read_dir)
			continue;

		i++;
		if (i < start)
			continue;
		if (i >= (num + start))
			break;
		dir_obj = json_object_new_object();

		obj = json_object_new_string(d->d_name);
		json_object_object_add(dir_obj, "name", obj);

		if (dir_flag)
			obj = json_object_new_string("D");
		else if (S_ISREG(st.st_mode))
			obj = json_object_new_string("F");
		else if (S_ISLNK(st.st_mode))
			obj = json_object_new_string("L");
		else
			obj = json_object_new_string("");
		json_object_object_add(dir_obj, "type", obj);

		obj = json_object_new_uint64((unsigned long long)st.st_size);
		json_object_object_add(dir_obj, "size", obj);

		obj = json_object_new_uint64((unsigned long long)st.st_ctime);
		json_object_object_add(dir_obj, "time", obj);

		json_object_array_add(arr, dir_obj);
	}
	closedir(dir);

	if (read_dir) {
		read_dir = 0;
		goto read_file;
	}
	json_object_object_add(response, "dir", arr);
	return 0;
}

int cgi_storage_act(server_t *srv, connection_t *con, struct json_object *response)
{
	int i = 0;
	char *act, *s, *d, str[4096], src[1024], dst[1024], *ptr;
	struct igd_sys_task_set ists;
	struct igd_sys_task_get istg;
	struct json_object *obj;

	act = con_value_get(con, "act");
	if (!act)
		return CGI_ERR_INPUT;
	if (!strcmp(act, "mv") 
		|| (!strcmp(act, "cp"))) {
		for (i = 1; i <= 15; i++) {
			snprintf(str, sizeof(str), "s%d", i);
			s = con_value_get(con, str);
			if (!s)
				break;
			STORAGE_PATH_CHECK(s);
			prefix_encode(src, sizeof(src), s, "$");

			snprintf(str, sizeof(str), "d%d", i);
			d = con_value_get(con, str);
			if (!d)
				break;
			STORAGE_PATH_CHECK(d);
			prefix_encode(dst, sizeof(dst), d, "$");

			snprintf(ists.cmd, sizeof(ists.cmd), "%s \"%s\" \"%s\"",
				(act[0] == 'm') ? "mv" : "cp -rf", src, dst);

			ists.timeout = 0;
			arr_strcpy(ists.name, act);
			if (mu_msg(SYSTEM_MOD_TASK_ADD, &ists, sizeof(ists), NULL, 0)) {
				CGI_PRINTF("[%s], %m\n", ists.cmd);
				return CGI_ERR_FAIL;
			}
		}
	} else if (!strcmp(act, "rm")) {
		for (i = 1; i <= 15; i++) {
			snprintf(str, sizeof(str), "s%d", i);
			s = con_value_get(con, str);
			if (!s)
				break;
			STORAGE_PATH_CHECK(s);
			prefix_encode(src, sizeof(src), s, "$");

			snprintf(ists.cmd, sizeof(ists.cmd), "rm -r \"%s\"", src);
			ists.timeout = 0;
			arr_strcpy(ists.name, act);
			if (mu_msg(SYSTEM_MOD_TASK_ADD, &ists, sizeof(ists), NULL, 0)) {
				CGI_PRINTF("[%s], %m\n", ists.cmd);
				return CGI_ERR_FAIL;
			}
		}
	} else if (!strcmp(act, "touch")
		|| (!strcmp(act, "mkdir"))) {
		s = con_value_get(con, "path");
		if (!s)
			return CGI_ERR_INPUT;
		STORAGE_PATH_CHECK(s);
		if (!access(s, F_OK))
			return CGI_ERR_EXIST;
		prefix_encode(src, sizeof(src), s, "$");

		snprintf(str, sizeof(str), "%s \"%s\"", act, src);
		if (system(str)) {
			CGI_PRINTF("[%s], %m\n", ists.cmd);
			return CGI_ERR_FAIL;
		}
		return 0;
	} else if (!strcmp(act, "dump")) {
		ptr = con_value_get(con, "info");
		if (!ptr)
			return CGI_ERR_INPUT;
		if (mu_msg(SYSTEM_MOD_TASK_DUMP, ptr, strlen(ptr) + 1, &istg, sizeof(istg)))
			return CGI_ERR_NONEXIST;

		obj = json_object_new_int((istg.finish_nr*100)/istg.nr);
		json_object_object_add(response, "sch", obj);

		obj = json_object_new_string(ptr);
		json_object_object_add(response, "sch_info", obj);
		return 0;
	} else {
		return CGI_ERR_INPUT;
	}

	if (!i)
		return CGI_ERR_INPUT;
	return 0;
}

int cgi_storage_size(server_t *srv, connection_t *con, struct json_object *response)
{
	char *path, cmd[4096], str[4096];
	unsigned long long size;
	struct json_object *obj;

	path = con_value_get(con, "path");
	if (!path)
		return CGI_ERR_INPUT;

	STORAGE_PATH_CHECK(path);

	if (access(path, F_OK))
		return CGI_ERR_NONEXIST;

	snprintf(cmd, sizeof(cmd), "du -s \"%s\"", path);

	if (shell_printf(cmd, str, sizeof(str)) < 0)
		return CGI_ERR_FAIL;

	size = atoll(str) * 1024;
	obj = json_object_new_uint64(size);

	json_object_object_add(response, "size", obj);
	return 0;
}
#endif

int cgi_sys_storage(server_t *srv, connection_t *con, struct json_object *response)
{
#ifdef DISKM_SUPPORT
	char *ptr = NULL;

	CHECK_LOGIN;

	ptr = con_value_get(con, "type");
	if (!ptr)
		return CGI_ERR_INPUT;

	if (!strcmp(ptr, "info")) {
		return cgi_storage_info(srv, con, response);
	} else if (!strcmp(ptr, "file")) {
		return cgi_storage_file(srv, con, response);
	} else if (!strcmp(ptr, "dir")) {
		return cgi_storage_dir(srv, con, response);
	} else if (!strcmp(ptr, "act")) {
		return cgi_storage_act(srv, con, response);
	} else if (!strcmp(ptr, "size")) {
		return cgi_storage_size(srv, con, response);
	} else {
		return CGI_ERR_INPUT;
	}
#else
	return CGI_ERR_NOSUPPORT;
#endif
}

int cgi_command_log(server_t *srv, connection_t *con, struct json_object *response)
{
	int act;
	char *ptr = NULL;
	
	CHECK_LOGIN;
	
	CON_GET_ACT(ptr, con, act);
	if (act == NLK_ACTION_ADD) {
		system("command_log.sh /tmp/command_log");
		system("ln -s /tmp/command_log  /www/command_log");
	} else if (act == NLK_ACTION_DEL) {
		system("rm /www/command_log");
		system("rm /tmp/command_log");
	}
	return 0;
}

int get_firewall_parm(connection_t *con, struct firewall *info)
{
	char *ptr = NULL;

	CON_GET_INT(ptr, con, info->is_dos_open, "is_dos_open");
	CON_GET_INT(ptr, con, info->is_flood_open, "is_flood_open");
	return 0;
}

int firewall_set(connection_t *con, struct firewall *pre, struct firewall *info)
{
	if (get_firewall_parm(con, info))
		return CGI_ERR_INPUT;
	if (!memcmp(pre, info, sizeof(*pre)))
		return 0;
	if (mu_msg(IF_MOD_FIREWALL_SET, info, sizeof(*info), NULL, 0))
		return CGI_ERR_FAIL;
	return 0;
}

int get_firewall_info(struct firewall *info)
{
	if (mu_msg(IF_MOD_FIREWALL_GET, NULL, 0, info, sizeof(*info)))
		return -1;
	return 0;
}

int cgi_firewall_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	struct firewall info, pre;
	struct json_object *obj;

	CHECK_LOGIN;
	memset(&pre, 0x00, sizeof(pre));
	if (get_firewall_info(&pre))
		return CGI_ERR_FAIL;
	if (connection_is_set(con)) {
		memset(&info, 0x00, sizeof(info));
		firewall_set(con, &pre, &info);
		return 0;
	}
	obj = json_object_new_int(pre.is_dos_open);
	json_object_object_add(response, "is_dos_open", obj);
	obj = json_object_new_int(pre.is_flood_open);
	json_object_object_add(response, "is_flood_open", obj);
	return 0;
}

int get_cpuusage(void)
{
	FILE *fp = NULL;
	int  total  = 0, idle   = 0;
	int  idle1  = 0, idle2  = 0;
	int  total1 = 0, total2 = 0;
	int  nice, user, system, iowait, irq, softirq, s, g; 

	fp = fopen("/proc/stat", "r");
	if (!fp) {
		CGI_PRINTF("fopen fail\n");
		return -1;
	}
	if (9 != fscanf(fp, "cpu %d %d %d %d %d %d %d %d %d",
			&nice, &user, &system, &idle1,
			&iowait, &irq, &softirq, &s, &g)) {
		CGI_PRINTF("fscanf one, fail\n");
		goto err;
	}
	total1 = nice + user + system + idle1 + iowait + irq + softirq + s + g;
	usleep(1000000);
	fseek(fp, 0x0, SEEK_SET);
	if (9 != fscanf(fp, "cpu %d %d %d %d %d %d %d %d %d",
			&nice, &user, &system, &idle2,
			&iowait, &irq, &softirq, &s, &g)) {
		CGI_PRINTF("fscanf two, fail\n");
		goto err;
	}
	total2 = nice + user + system + idle2 + iowait + irq + softirq + s + g;
	total = total2 - total1;
	idle  = idle2  - idle1;
	fclose(fp);
	return (100 * (total - idle)) / total;
err:
	if (fp)
		fclose(fp);
	return -1;
}

int pro_saas_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	struct json_object *obj;
	struct sysinfo sys;
	struct iface_info info;
	struct iface_conf wan_conf;
	int uid = 1;
	char str[64];
	struct if_statistics statis;

	CHECK_LOGIN;

	sysinfo(&sys);

	obj = json_object_new_int(sys.freeram);
	json_object_object_add(response, "freemem", obj);

	obj = json_object_new_int(sys.totalram);
	json_object_object_add(response, "totalram", obj);

	obj = json_object_new_int(sys.uptime);
	json_object_object_add(response, "runtime", obj);

	memset(&info, 0x0, sizeof(info));
	if (mu_msg(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &info, sizeof(info)))
		return CGI_ERR_FAIL;

	obj = json_object_new_int(info.mode);
	json_object_object_add(response, "mode", obj);

	obj= json_object_new_string(inet_ntoa(info.dns[0]));
	json_object_object_add(response, "DNS1", obj);
	obj= json_object_new_string(inet_ntoa(info.dns[1]));
	json_object_object_add(response, "DNS2", obj);

	read_mac(info.mac);
	snprintf(str, sizeof(str),
		"%02X:%02X:%02X:%02X:%02X:%02X", MAC_SPLIT(info.mac));
	obj= json_object_new_string(str);
	json_object_object_add(response, "mac", obj);

	get_if_statistics(1, &statis);
	obj = json_object_new_int((int)(statis.in.all.speed/1024));
	json_object_object_add(response, "down_speed", obj);
	obj = json_object_new_int((int)(statis.out.all.speed/1024));
	json_object_object_add(response, "up_speed", obj);

 	obj = json_object_new_uint64(statis.in.all.bytes/1024);
	json_object_object_add(response, "down_bytes", obj);
 	obj = json_object_new_uint64(statis.out.all.bytes/1024);
	json_object_object_add(response, "up_bytes", obj);

	get_if_statistics(0, &statis);
	obj = json_object_new_int((int)(statis.in.all.speed/1024));
	json_object_object_add(response, "lan_down_speed", obj);
	obj = json_object_new_int((int)(statis.out.all.speed/1024));
	json_object_object_add(response, "lan_up_speed", obj);

 	obj = json_object_new_uint64(statis.in.all.bytes/1024);
	json_object_object_add(response, "lan_down_bytes", obj);
 	obj = json_object_new_uint64(statis.out.all.bytes/1024);
	json_object_object_add(response, "lan_up_bytes", obj);

	uid = 1;
	memset(&wan_conf, 0, sizeof(wan_conf));
	if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &wan_conf, sizeof(wan_conf)) ||
		(wan_conf.mode != MODE_PPPOE)) {
		wan_conf.pppoe.user[0] = '\0';
	}
	wan_conf.pppoe.user[sizeof(wan_conf.pppoe.user) - 1] = '\0';

	obj = json_object_new_string(wan_conf.pppoe.user);
	json_object_object_add(response, "pppoe", obj);

	obj = json_object_new_int(get_appall_info(1, NULL));
	json_object_object_add(response, "appnum", obj);

	obj = json_object_new_string(read_firmware("CURVER") ? : "");
	json_object_object_add(response, "curver", obj);

	obj = json_object_new_int(get_cpuusage());
	json_object_object_add(response, "cpuused", obj);

	return 0;
}

void bitmap_format(unsigned long bitmap, char *pbitmap)
{
	int i = 0;
	unsigned char tmp = 0;

	for (i = 0; i < 6; i++) {
		tmp = bitmap >> (5 - i) & 0x1;
		sprintf(pbitmap + i, "%d", tmp);
	}
}

int cgi_link_status_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	unsigned long bitmap;
	char *pwanpid = NULL;
	struct json_object *obj;
	char pbitmap[7] = {0};

	if (get_link_status(&bitmap))
		return CGI_ERR_FAIL;
	CGI_PRINTF("bitmap=%lu\n", bitmap);
	pwanpid = read_firmware("WANPID");
	if (!pwanpid)
		return CGI_ERR_FAIL;
	if (get_link_status(&bitmap))
		return CGI_ERR_FAIL;
	bitmap_format(bitmap, pbitmap);
	obj = json_object_new_string(pbitmap);
	json_object_object_add(response, "bitmap", obj);
	obj = json_object_new_string(pwanpid);
	json_object_object_add(response, "pwanpid", obj);
	return 0;
}

int cgi_upnpd_switch_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	int flag = 0;
	struct json_object *obj;
	int upnpd_state = -1;

	if (connection_is_set(con)) {
		CON_GET_INT(ptr, con, flag, "flag");
		if (mu_msg(UPNPD_STATE_SET, &flag, sizeof(flag), NULL, 0))
			return CGI_ERR_FAIL;
		return 0;
	}
	mu_msg(UPNPD_STATE_GET, NULL, 0, &upnpd_state, sizeof(upnpd_state));
	obj = json_object_new_int(upnpd_state);
	json_object_object_add(response, "upnpd_state", obj);
	return 0;
}

int cgi_route_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *ptr = NULL;
	int act = 0, i = 0, num = 0;
	struct igd_route_param irp[ROUTE_MAX];
	struct json_object *object, *array, *t;

	CHECK_LOGIN;
	if (connection_is_set(con)) {
		memset(irp, 0x0, sizeof(irp));
		CON_GET_ACT(ptr, con, act);
		CON_GET_STR(ptr, con, irp[0].dst_ip, "dst_ip");
		CON_GET_STR(ptr, con, irp[0].gateway, "gateway");
		CON_GET_STR(ptr, con, irp[0].netmask, "netmask");
		CON_GET_INT(ptr, con, irp[0].dev_flag, "dev_flag");
		/* CON_GET_INT(ptr, con, irp[0].is_reject, "is_reject"); */
		CON_GET_INT(ptr, con, irp[0].enable, "enable");
		if (act == NLK_ACTION_ADD) {
			num = mu_msg(ROUTE_MOD_ADD, &irp[0], sizeof(irp[0]), NULL, 0);
			if (num == -2) {
				return CGI_ERR_EXIST;
			} else if (num == -3) {
				return CGI_ERR_FULL;
			} else if (num == -4) {
				return CGI_ERR_RULE;
			} else if (num < 0) {
				return CGI_ERR_FAIL;
			}
		}
		else if (act == NLK_ACTION_DEL) {
			num = mu_msg(ROUTE_MOD_DEL, &irp[0], sizeof(irp[0]), NULL, 0);
			if (num == -2) {
				return CGI_ERR_NONEXIST;
			} else if (num == -3) {
				return CGI_ERR_RULE;
			} else if (num < 0) {
				return CGI_ERR_FAIL;
			}
		}
		else if (act == NLK_ACTION_MOD) {
			CON_GET_STR(ptr, con, irp[1].dst_ip, "org_dst_ip");
			CON_GET_STR(ptr, con, irp[1].gateway, "org_gateway");
			CON_GET_STR(ptr, con, irp[1].netmask, "org_netmask");
			CON_GET_INT(ptr, con, irp[1].dev_flag, "org_dev_flag");
			/* CON_GET_INT(ptr, con, irp[1].is_reject, "is_reject"); */
			CON_GET_INT(ptr, con, irp[1].enable, "org_enable");
			if (mu_msg(ROUTE_MOD_DEL, &irp[1], sizeof(irp[1]), NULL, 0) < 0) {
				CGI_PRINTF("mod del fail\n");
				return CGI_ERR_FAIL;
			}
			num = mu_msg(ROUTE_MOD_ADD, &irp[0], sizeof(irp[0]), NULL, 0);
			if (num == -2) {
				return CGI_ERR_EXIST;
			} else if (num == -3) {
				return CGI_ERR_FULL;
			} else if (num == -4) {
				return CGI_ERR_RULE;
			} else if (num < 0) {
				return CGI_ERR_FAIL;
			}
		}
		else
			return CGI_ERR_INPUT;
		return 0;
	}
	num = mu_msg(ROUTE_MOD_DUMP, NULL, 0, irp, sizeof(irp));
	if (num < 0)
		return CGI_ERR_FAIL;
	array = json_object_new_array();
	for (i = 0; i < num; i++) {
 		object = json_object_new_object();

		t = json_object_new_string(irp[i].dst_ip);
		json_object_object_add(object, "dst_ip", t);

		t = json_object_new_string(irp[i].gateway);
		json_object_object_add(object, "gateway", t);

		t = json_object_new_string(irp[i].netmask);
		json_object_object_add(object, "netmask", t);

		t = json_object_new_int(irp[i].dev_flag);
		json_object_object_add(object, "dev_flag", t);
/* 
		t = json_object_new_int(irp[i].is_reject);
		json_object_object_add(object, "is_reject", t);
*/
		t = json_object_new_int(irp[i].enable);
		json_object_object_add(object, "enable", t);

		json_object_array_add(array, object);
	}
	json_object_object_add(response, "route", array);
	return 0;
}
int cgi_net_ddns_handler(server_t *srv, connection_t *con, struct json_object *response)
{
	char *userid = NULL;
	char *pwd = NULL;
	char *enable = NULL;
	struct json_object *obj;
	
	CHECK_LOGIN;
	if (connection_is_set(con)) {
		struct ddns_info ddnsif;
		memset(&ddnsif,0,sizeof(ddnsif));
		enable = con_value_get(con, "enable");
		if (strcmp(enable, "1") == 0) {
			userid = con_value_get(con, "userid");
			pwd  = con_value_get(con, "pwd");
			ddnsif.enable = 1;
			if (NULL == userid || NULL == pwd)
				return CGI_ERR_INPUT;
			strcpy(ddnsif.userid,userid);
			strcpy(ddnsif.pwd,pwd);
			if(mu_msg(DNSMASQ_DDNS_SET, &ddnsif, sizeof(ddnsif), NULL, 0))
				return CGI_ERR_FAIL;
		} else if (strcmp(enable, "0") == 0) {
			ddnsif.enable = 0;
			if (mu_msg(DNSMASQ_DDNS_SET, &ddnsif, sizeof(ddnsif), NULL, 0))
				return CGI_ERR_FAIL;
		}
		return 0;
	}
	
	struct ddns_status ddnsst;
	memset(&ddnsst, 0, sizeof(ddnsst));
	if (mu_msg(DNSMASQ_DDNS_SHOW, NULL, 0, &ddnsst, sizeof(ddnsst)))
				return CGI_ERR_FAIL;	
	if (ddnsst.dif.enable == 0) {
		obj=json_object_new_string("0");
		json_object_object_add(response, "enable", obj);
		obj=json_object_new_string(ddnsst.dif.userid);
		json_object_object_add(response, "userid", obj);
		obj=json_object_new_string(ddnsst.dif.pwd);
		json_object_object_add(response, "pwd", obj);
		return 0;
	}		
	obj=json_object_new_string("1");
	json_object_object_add(response, "enable", obj);
	obj=json_object_new_string(ddnsst.dif.userid);
	json_object_object_add(response, "userid", obj);
	obj=json_object_new_string(ddnsst.dif.pwd);
	json_object_object_add(response, "pwd", obj);
	obj= json_object_new_string(ddnsst.basic);
	json_object_object_add(response, "status", obj);	
	obj= json_object_new_string(ddnsst.domains);
	json_object_object_add(response, "domains", obj);
	return 0;
}

