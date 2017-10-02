#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "igd_lib.h"
#include "igd_host.h"
#include "uci.h"
#include "uci_fn.h"
#include "host_type.h"
#include "igd_cloud.h"

#if 1 //def FLASH_4_RAM_32
#define IGD_HOST_DBG(fmt,args...) do {}while(0)
#else
#define IGD_HOST_DBG(fmt,args...) do { \
	igd_log("/tmp/igd_host_dbg", "DBG:%s[%d,%ld]:"fmt, \
		__FUNCTION__, __LINE__, sys_uptime(),##args); \
} while(0)
#endif

#ifdef FLASH_4_RAM_32
#define IGD_HOST_ERR(fmt,args...) do{}while(0)
#else
#define IGD_HOST_ERR(fmt,args...) do { \
	igd_log("/tmp/igd_host_err", "ERR:%s[%d,%ld]:"fmt, \
		__FUNCTION__, __LINE__, sys_uptime(), ##args); \
} while(0)
#endif

extern int set_host_info(struct in_addr ip, char *nick_name, char *name, unsigned char os_type, uint16_t vender);

#define IGD_HOST_INPUT_CHECK(data, num, len) \
	if (sizeof(*data)*num != len) { \
		IGD_HOST_ERR("input err, %d,%d\n", sizeof(*data), len); \
		return -1; \
	}

#define TEST_HOST_FLAG(bit) igd_test_bit(bit, igd_host_flag)
#define SET_HOST_FLAG(bit) igd_set_bit(bit, igd_host_flag)
#define CLEAR_HOST_FLAG(bit) igd_clear_bit(bit, igd_host_flag)

static unsigned char igd_host_num = 0;
static struct list_head igd_host_list = LIST_HEAD_INIT(igd_host_list);
static unsigned long igd_host_flag[BITS_TO_LONGS(HF_MAX)];
static struct list_head igd_study_url_list = LIST_HEAD_INIT(igd_study_url_list);
static unsigned long igd_study_url_flag[BITS_TO_LONGS(IGD_STUDY_URL_SELF_NUM) + 1];
static struct igd_host_global_info igd_host_global;
static unsigned char igd_host_ua_num = 0;
static struct list_head igd_host_ua_list = LIST_HEAD_INIT(igd_host_ua_list);
static unsigned char igd_host_filter_num = 0;
static struct list_head igd_host_filter_list = LIST_HEAD_INIT(igd_host_filter_list);

/*host param save file*/
#define IGD_HOST_SAVE_FILE  "host"

/*config section*/
#define IGD_HOST_GLOBAL_INFO  "global_rule"
#define IGD_HOST_STUDY_URL  "study_url"
#define IGD_HOST_FILTER    "filter"

/*HGSF: host global save flag*/
#define HGSF_KING        "king"
#define HGSF_NEW_PUSH    "new_push"

/*HSF: host save flag*/
#define HSF_MODE            "mode"
#define HSF_LS              "ls"
#define HSF_BLACK           "black"
#define HSF_NICK            "nick"
#define HSF_ONLINE_PUSH     "online_push"
#define HSF_PIC             "pic"
#define HSF_ALLOW_URL       "allow_url"
#define HSF_STUDY_TIME      "study_time"
#define HSF_URL_BLACK       "url_black"
#define HSF_URL_WHITE       "url_white"
#define HSF_APP_BLACK       "app_black"
#define HSF_APP_LT          "app_lt"
#define HSF_APP_LMT         "app_lmt"
#define HSF_INTERCEPT_URL   "intercept_url"
#define HSF_APP_QUEUE       "app_queue"
#define HSF_APP_WHITE       "app_white"

static long get_nettime(void)
{
	return (long)time(NULL);
}

static void set_uptime(struct uptime_record_info *t)
{
	struct tm *tm = get_tm();

	if (tm) {
		t->flag = UPTIME_NET;
		t->time = get_nettime();
	} else {
		t->flag = UPTIME_SYS;
		t->time = sys_uptime();
	}
}

static void add_oltime(struct time_record_info *t)
{
	long st = sys_uptime(), nt = get_nettime();
	unsigned long time = t->all;

	if (!t->up.time) {
		IGD_HOST_ERR("%d,%lu\n",
			t->up.flag, t->up.time);
	} else if (t->up.flag == UPTIME_SYS) {
		t->all += (st - t->up.time);
	} else if (t->up.flag == UPTIME_NET) {
		t->all += (nt - t->up.time);
	} else {
		IGD_HOST_ERR("%d,%lu\n",
			t->up.flag, t->up.time);
	}
	if (time > t->all) {
		IGD_HOST_ERR("%d,%lu, %lu,%lu,%ld,%ld\n",
			t->up.flag, t->up.time, t->all, time, st, nt);
		t->all = time;
	}
}

static char *mac_to_str(unsigned char *mac)
{
	static char str[18];

	snprintf(str, sizeof(str), 
		"%02X%02X%02X%02X%02X%02X", MAC_SPLIT(mac));
	return str;
}

static int str_to_mac(char *str, unsigned char *mac)
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

static int str_filter(char *str, int len)
{
	int i = 0;

	for (i = 0; (i < len) && str && str[i]; i++) {
		if (!(isdigit(str[i]) || isalpha(str[i]) ||
			str[i] == ' ' || str[i] == '-' || str[i] == '_')) {
			str[i] = '\0';
			return 1;
		}
	}
	return 0;
}

static char *time_to_str(struct time_comm *t)
{
	static char str[25];

	snprintf(str, sizeof(str), "%02d:%02d,%02d:%02d,%d,%d",
		t->start_hour, t->start_min, t->end_hour, t->end_min,
		t->day_flags, t->loop);

	return str;
}

static int timeout_check(struct time_comm *time)
{
	int st, et, nt, nw, pw;
	struct tm *t = get_tm();

	if (!t || time->loop)
		return 0;
	st = time->start_hour*60 + time->start_min;
	et = time->end_hour*60 + time->end_min;
	nt = t->tm_hour*60 + t->tm_min;

	nw = t->tm_wday;
	pw = (nw > 0) ? (nw - 1) : 6;
	if ((nw < 0) || (nw > 6))
		return 0;
	if ((pw < 0) || (pw > 6))
		return 0;

	if (st < et) {
		if ((nt >= et) ||
			!(time->day_flags & (1 << nw)))
			return 1;
	} else if (st > et) {
		if (time->day_flags & (1 << pw)) {
			if (nt >= et)
				return 1;
		} else if (time->day_flags & (1 << nw)) {
			return 0;
		} else {
			return 1;
		}
	}
	return 0;
}

static char *mid_to_str(unsigned long *mid)
{
	int i = 0;
	static char str[L7_MID_MX + 1];

	for (i = 0; i < L7_MID_MX; i++)
		str[i] = igd_test_bit(i, mid) ? '1' : '0';
	str[i] = 0;
	return str;
}

static int set_ua_info(int nr, struct ua_data *ua)
{
	struct nlk_msg_comm nlk;
	if (!nr || !ua || nr > UA_DATA_MX)
		return IGD_ERR_DATA_ERR;
	memset(&nlk, 0x0, sizeof(nlk));
	nlk.action = NLK_ACTION_ADD;
	nlk.obj_nr = nr;
	nlk.obj_len = sizeof(struct ua_data);
	return nlk_data_send(NLK_MSG_UA, &nlk, sizeof(nlk), ua, nr * sizeof(struct ua_data));
}

static time_t tomorrow_time = 0;
static void day_change(void)
{
	time_t now_time;
	struct tm tm_now;
	struct nlk_msg_comm nlk;

	now_time = get_nettime();
	if ((tomorrow_time == 0) || \
			((now_time - tomorrow_time) > 25*60*60)) {
		localtime_r(&now_time, &tm_now);
		tm_now.tm_hour = 0;
		tm_now.tm_min = 0;
		tm_now.tm_sec = 0;
		tomorrow_time = mktime(&tm_now) + 24*60*60 - 11;
	}

	if (now_time > tomorrow_time) {
		IGD_HOST_DBG("day change-----%d\n", now_time);
		tomorrow_time = 0;
		memset(&nlk, 0, sizeof(nlk));
		nlk_head_send(NLK_MSG_DAY, &nlk, sizeof(nlk));
		system("echo 3 > /proc/sys/vm/drop_caches");
	}
}

#define HOST_BASE_FUNCTION "-------------above is base function-----------------------"

static int igd_host_app_save_check(
	struct host_info_record *host, unsigned int appid, int del)
{
	struct uci_package *pkg;
	struct uci_context *ctx;
	struct uci_element *se, *oe;
	struct uci_section *s;
	char str[128], appstr[32], flag = 0;

	snprintf(appstr, sizeof(appstr), "_%u", appid);
	snprintf(str, sizeof(str), "%s%s",
		mac_to_str(host->mac), appid ? appstr : "");

	ctx = uci_alloc_context();
	if (0 != uci_load(ctx, IGD_HOST_SAVE_FILE, &pkg)) {
		uci_free_context(ctx);
		IGD_HOST_ERR("uci load %s fail\n", IGD_HOST_SAVE_FILE);
		return -1;
	}
	uci_foreach_element(&pkg->sections, se) {
		if (strcmp(se->name, str))
			continue;
		flag = 1;
		s = uci_to_section(se);
		uci_foreach_element(&s->options, oe) {
			flag = 2;
			break;
		}
		break;
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx);

	if ((flag == 0) && (del == 0)) {
		snprintf(str, sizeof(str), "%s.%s%s=%s",
			IGD_HOST_SAVE_FILE, mac_to_str(host->mac),
			appid ? appstr : "", appid ? "APP" : "MAC");
	} else if (((flag == 1) && (del == 1))
		|| ((del == -1) && flag)) {
		snprintf(str, sizeof(str), "%s.%s%s=",
			IGD_HOST_SAVE_FILE, mac_to_str(host->mac),
			appid ? appstr : "");
	} else {
		return 0;
	}

	if (uuci_set(str) != 0) {
		IGD_HOST_ERR("set fail, %s\n", str);
		return -1;
	}
	IGD_HOST_DBG("%s\n", str);
	return 0;
}

static int igd_host_app_save_add(
	struct host_info_record *host, unsigned int appid, char *flag, const char *fmt, ...)
{
	int len = -1, value_len;
	va_list ap;
	char *cmd;

	#define CMD_SIZE_MX 8192
	cmd = malloc(CMD_SIZE_MX);
	if (!cmd) {
		IGD_HOST_ERR("malloc fail\n");
		goto err;
	}

	len = snprintf(cmd, CMD_SIZE_MX, "%s.%s",
		IGD_HOST_SAVE_FILE, mac_to_str(host->mac));
	if (appid)
		len += snprintf(cmd + len, CMD_SIZE_MX - len, "_%u", appid);
	len += snprintf(cmd + len, CMD_SIZE_MX - len, ".%s=", flag);

	va_start(ap, fmt);
	value_len = vsnprintf(cmd + len, CMD_SIZE_MX - len, fmt, ap);
	va_end(ap);

	if (value_len)
		igd_host_app_save_check(host, appid, 0);

	IGD_HOST_DBG("uuci:%s\n", cmd);
	len = uuci_set(cmd);
	if (len < 0) {
		IGD_HOST_ERR("uuci set fail, %d, %s\n", len, cmd);
		goto err;
	}
	if (!value_len)
		igd_host_app_save_check(host, appid, 1);
err:
	if (cmd)
		free(cmd);
	return (len < 0) ? -1 : 0;
}

static int igd_host_app_save_del(
	struct host_info_record *host, unsigned int appid, char *flag)
{
	return igd_host_app_save_add(host, appid, flag, "");
}

static int igd_global_save_add(
	char *flag, const char *fmt, ...)
{
	int len;
	va_list ap;
	char cmd[512];

	len = snprintf(cmd, sizeof(cmd), "%s.%s.%s=",
		IGD_HOST_SAVE_FILE, IGD_HOST_GLOBAL_INFO, flag);

	va_start(ap, fmt);
	vsnprintf(cmd + len, sizeof(cmd) - len, fmt, ap);
	va_end(ap);

	IGD_HOST_DBG("uuci:%s\n", cmd);
	if (uuci_set(cmd) < 0) {
		IGD_HOST_ERR("uuci set fail, %s\n", cmd);
		return -1;
	}
	return 0;
}

static int igd_global_save_del(char *flag)
{
	return igd_global_save_add(flag, "");
}

static int igd_study_url_save_del(struct study_url_record *sur)
{
	char cmd[512];

	snprintf(cmd, sizeof(cmd), "%s.%s.%u=",
		IGD_HOST_SAVE_FILE, IGD_HOST_STUDY_URL, sur->id);

	IGD_HOST_DBG("uuci:%s\n", cmd);
	if (uuci_set(cmd) < 0) {
		IGD_HOST_ERR("uuci set fail\n");
		return -1;
	}
	return 0;
}

static int igd_study_url_save_add(struct study_url_record *sur)
{
	char cmd[512];

	snprintf(cmd, sizeof(cmd), "%s.%s.%u=%s,%s",
		IGD_HOST_SAVE_FILE, IGD_HOST_STUDY_URL, sur->id, sur->name, sur->url);

	IGD_HOST_DBG("uuci:%s\n", cmd);
	if (uuci_set(cmd) < 0) {
		IGD_HOST_ERR("uuci set fail, %s\n", cmd);
		return -1;
	}
	return 0;
}

#define HOST_SAVE_FUNCTION "-------------above is save function-----------------------"

static struct study_url_record *igd_find_study_url_by_url(char *url)
{
	struct study_url_record *sur;

	list_for_each_entry(sur, &igd_study_url_list, list) {
		if (!strcmp(sur->url, url))
			return sur;
	}
	return NULL;
}

static struct study_url_record *igd_find_study_url_by_id(unsigned int id)
{
	struct study_url_record *sur;

	list_for_each_entry(sur, &igd_study_url_list, list) {
		if (sur->id == id)
			return sur;
	}
	return NULL;
}

static void igd_del_study_url(
	struct study_url_record *sur)
{
	if (sur->name)
		free(sur->name);
	if (sur->url)
		free(sur->url);
	if (sur->info)
		free(sur->info);
	list_del(&sur->list);
	free(sur);
}

static void igd_free_study_url(void)
{
	struct study_url_record *sur, *_sur;

	list_for_each_entry_safe(sur, _sur, &igd_study_url_list, list) {
		if (IGD_SELF_STUDY_URL(sur->id))
			continue;
		igd_del_study_url(sur);
	}
}

static struct study_url_record *igd_add_study_url(
	unsigned int id, char *name, char *url, char *info)
{
	struct study_url_record *sur;

	sur = malloc(sizeof(*sur));
	if (!sur) {
		IGD_HOST_ERR("malloc fail, %d\n", sizeof(*sur));
		return NULL;
	}
	sur->id = id;
	sur->used = 0;
	sur->name = name ? strdup(name) : NULL;
	sur->url = url ? strdup(url) : NULL;
	sur->info = info ? strdup(info) : NULL;
	list_add(&sur->list, &igd_study_url_list);
	return sur;
}

static int igd_study_url_alloc_id(void)
{
	int i = 0;

	for (i = 0; i < IGD_STUDY_URL_SELF_NUM; i++) {
		if (!igd_test_bit(i, igd_study_url_flag))
			break;			
	}
	if (i >= IGD_STUDY_URL_SELF_NUM) {
		IGD_HOST_ERR("self study url is full\n");
		return -1;
	}
	igd_set_bit(i, igd_study_url_flag);
	return i + IGD_STUDY_URL_SELF_ID_MIN;
}

static void igd_study_url_free_id(unsigned int id)
{
	int i = id - IGD_STUDY_URL_SELF_ID_MIN;

	if ((i < 0) || (i >= IGD_STUDY_URL_SELF_NUM)) {
		IGD_HOST_ERR("id err, %u\n", id);
		return;
	}
	igd_clear_bit(i, igd_study_url_flag);
}

#define STUDY_URL_FUNCTION "-------------above is study url function-----------------------"

static struct host_ip_info *igd_host_find_ip(
	struct host_info_record *host, struct in_addr ip)
{
	struct host_ip_info *host_ip;

	list_for_each_entry(host_ip, &host->ip_list, list) {
		if (host_ip->ip.s_addr == ip.s_addr)
			return host_ip;
	}
	return NULL;
}

static int igd_host_check_ip(
	struct host_info_record *host)
{
	struct host_ip_info *host_ip;

	list_for_each_entry(host_ip, &host->ip_list, list) {
		return 1;
	}
	return 0;
}

static int igd_host_add_ip(
	struct host_info_record *host, struct in_addr ip)
{
	struct host_ip_info *host_ip;

	host_ip = igd_host_find_ip(host, ip);
	if (host_ip)
		return 0;
	host_ip = malloc(sizeof(*host_ip));
	if (!host_ip) {
		IGD_HOST_ERR("malloc fail, %d\n", sizeof(*host_ip));
		return -1;
	}
	host_ip->ip.s_addr = ip.s_addr;
	list_add(&host_ip->list, &host->ip_list);
	host->num.ip++;
	if (host->num.ip > IGD_HOST_IP_MX) {
		IGD_HOST_ERR("ip too much, MAC:%s, num:%d\n",
			mac_to_str(host->mac), host->num.ip);
		host_ip = list_entry(host->ip_list.prev, typeof(*host_ip), list);
		list_del(&host_ip->list);
		free(host_ip);
		host->num.ip--;
	}
	return 0;
}

static int igd_host_del_ip(
	struct host_info_record *host, struct in_addr ip)
{
	struct host_ip_info *host_ip;

	host_ip = igd_host_find_ip(host, ip);
	if (!host_ip) {
		IGD_HOST_ERR("no find ip, %s\n", inet_ntoa(ip));
		return -1;
	}
	list_del(&host_ip->list);
	free(host_ip);
	host->num.ip--;
	return 0;
}

#define HOST_IP_FUNCTION "-------------above is ip function-----------------------"

static struct host_app_record *igd_host_find_app(
	struct host_info_record *host, unsigned int appid)
{
	struct host_app_record *app;

	list_for_each_entry(app, &host->app_list, list) {
		if (app->appid == appid)
			return app;
	}
	return NULL;
}

static int igd_host_check_app(
	struct host_info_record *host, unsigned int appid)
{
	int app_nr, i;
	struct host_ip_info *ip;
	struct app_conn_info *app;

	list_for_each_entry(ip, &host->ip_list, list) {
		app = dump_host_app(ip->ip, &app_nr);
		if (!app)
			continue;
		for (i = 0; i < app_nr; i++) {
			if (!app[i].conn_cnt)
				continue;
			if (app[i].appid == appid)
				break;
		}
		free(app);
		if (i < app_nr)
			return 1;
	}
	return 0;
}

static struct host_app_record *igd_host_add_app(
	struct host_info_record *host, unsigned int appid)
{
	int mid;
	struct host_app_record *app;

	mid = L7_GET_MID(appid);
	if ((mid < 0) || (mid >= L7_MID_MX)) {
		IGD_HOST_ERR("mid:%d, err\n", mid);
		return NULL;
	}

	app = malloc(sizeof(*app));
	if (!app) {
		IGD_HOST_ERR("malloc fail, %d\n", sizeof(*app));
		return NULL;
	}
	memset(app, 0, sizeof(*app));
	app->appid = appid;
	list_add(&app->list, &host->app_list);
	host->num.app++;
	return app;
}

static int igd_host_del_app(
	struct host_info_record *host, unsigned int appid)
{
	struct host_app_record *app = NULL, *_app;

	if (appid) {
		app = igd_host_find_app(host, appid);
	} else if (host->num.app >= IGD_HOST_APP_MX) {
		IGD_HOST_DBG("full del, %d\n", host->num.app);
		list_for_each_entry(_app, &host->app_list, list) {
			if (!igd_test_bit(HARF_ONLINE, _app->flag)) {
				app = _app;
				break;
			}
		}
	} else {
		return 0;
	}
	if (!app) {
		IGD_HOST_ERR("del fail, appid:%d, num:%d\n",
			appid, host->num.app);
		return -1;
	}
	igd_host_app_save_check(host, app->appid, -1);
	list_del(&app->list);
	free(app);
	host->num.app--;
	return 0;
}

static int igd_host_part_app(unsigned int appid)
{
	return (appid == IGD_CHUYUN_APPID) \
		|| (appid == IGD_REDIRECT_STUDY_URL_APPID);
}

#define HOST_APP_FUNCTION "-------------above is app function-----------------------"

static void igd_host_del_log(
	struct host_info_record *host, struct host_log_record *log)
{
	list_del(&log->list);
	free(log);
	host->num.log--;
}

static struct host_log_record *igd_host_add_log(
	struct host_info_record *host, struct host_log_record *in)
{
	struct host_log_record *log;

	log = malloc(sizeof(*log));
	if (!log) {
		IGD_HOST_ERR("malloc fail, %d\n", sizeof(*log));
		return NULL;
	}
	memcpy(log, in, sizeof(*in));
	list_add(&log->list, &host->log_list);
	host->num.log++;
	return log;
}

#define IGD_HOST_CHECK_LOG_NUM(host) do { \
	if (host->num.log > IGD_HOST_LOG_MX) { \
		IGD_HOST_DBG("full, %d,%d\n", \
			host->num.log, IGD_HOST_LOG_MX); \
		igd_host_log_upload(host); \
	} \
} while(0)

static int igd_host_log_filter(
	struct host_info_record *host, struct host_log_record *log)
{
	struct host_log_record *hlr, *_hlr = log;

	if (log->type != HLT_APP)
		return 0;
	if (IGD_TEST_STUDY_URL(log->v.appid))
		return 0;

	if (!log->oltime) {
		hlr = NULL;
		list_for_each_entry_continue_reverse(_hlr, &host->log_list, list) {
			if (_hlr->type != HLT_APP)
				continue;
			if (log->v.appid != _hlr->v.appid)
				continue;
			hlr = _hlr;
			break;
		}
		if (!hlr) {
			if (sys_uptime() < (log->time + 20))
				return 1;
		} else if (!hlr->oltime) {
			IGD_HOST_ERR("no down log, %s,%u\n", 
					mac_to_str(host->mac), log->v.appid);
		} else if (hlr->oltime < 20) {
			igd_set_bit(HLF_UP, hlr->flag);
			igd_set_bit(HLF_UP, log->flag);
		}
	} else if (log->oltime < 20) {
		if (!igd_test_bit(HLF_UP, log->flag))
			IGD_HOST_ERR("filter fail, %s,%u\n", 
					mac_to_str(host->mac), log->v.appid);
		else
			igd_set_bit(HLF_UP, log->flag);
	}
	return 0;
}

static void igd_host_log_upload(struct host_info_record *host)
{
	int i, len, nr;
	unsigned char *buf;
	struct host_log_record *log, *_log;
	char value[32];

	buf = malloc(IGD_HOST_LOG_MX*50 + 22);
	if (!buf) {
		IGD_HOST_ERR("malloc fail, %d\n", IGD_HOST_LOG_MX*50 + 22);
		return;
	}
	CC_PUSH2(buf, 2, CSO_NTF_ROUTER_APP_ONOFF);
	i = 8;
	CC_PUSH1(buf, i, 12);
	i += 1;
	CC_PUSH_LEN(buf, i, mac_to_str(host->mac), 12);
	i += 12;
	nr = 0;
	//CC_PUSH1(buf, i, nr); last add
	i += 1;
	list_for_each_entry_safe_reverse(log, _log, &host->log_list, list) {
		if (igd_host_log_filter(host, log))
			continue;

		if (igd_test_bit(HLF_UP, log->flag)) {
			if (igd_test_bit(HLF_CLEAR, log->flag))
				igd_host_del_log(host, log);
			continue;
		}

		memset(value, 0, sizeof(value));
		switch (log->type) {
		case HLT_APP:
			snprintf(value, sizeof(value), "%u", log->v.appid);
			break;
		case HLT_HOST:
			value[0] = 0;
			break;
		case HLT_MODE:
			snprintf(value, sizeof(value), "%d", log->v.mode);
			break;
		case HLT_SELF_URL:
			snprintf(value, sizeof(value), "%s", log->v.url_name);
			break;
		default:
			IGD_HOST_ERR("type:%d, nonsupport\n", log->type);
			break;
		}
		CC_PUSH4(buf, i, log->oltime);
		i += 4;
		CC_PUSH4(buf, i, get_nettime() - sys_uptime() + log->time);
		i += 4;
		CC_PUSH1(buf, i, log->type);
		i += 1;
		len = strlen(value);
		CC_PUSH1(buf, i, len);
		i += 1;
		if (len > 0) {
			CC_PUSH_LEN(buf, i, value, len);
			i += len;
		}
		nr++;
		IGD_HOST_DBG("log:%s,%lu,%lu,%lu\n", (len > 0) ? \
			value : host->base.name, log->time, log->oltime, log->flag[0]);
		if (igd_test_bit(HLF_CLEAR, log->flag)) {
			igd_host_del_log(host, log);
		} else {
			igd_set_bit(HLF_UP, log->flag);
		}
	}
	if (nr > 0) {
		IGD_HOST_DBG("%s,%d,%d-----SEND-----\n", mac_to_str(host->mac), nr, i);
		CC_PUSH1(buf, 21, nr);
		CC_PUSH2(buf, 0, i);
		CC_CALL_ADD(buf, i);
	}
	free(buf);
}

static void igd_host_log(
	struct host_info_record *host, unsigned char up)
{
	struct host_log_record *hlr, *_hlr, log;

	IGD_HOST_CHECK_LOG_NUM(host);

	memset(&log, 0, sizeof(log));
	log.type = HLT_HOST;
	log.time = sys_uptime();
	if (up) {
		igd_host_add_log(host, &log);
		return;
	} else {
		list_for_each_entry_safe(hlr, _hlr, &host->log_list, list) {
			if (hlr->type != HLT_HOST)
				continue;
			if (hlr->oltime)
				break;
			igd_set_bit(HLF_CLEAR, log.flag);
			igd_set_bit(HLF_CLEAR, hlr->flag);
			if (log.time == hlr->time) {
				log.time += 1;
				log.oltime = 1;
			} else if (log.time < hlr->time) {
				log.oltime = 1;
				IGD_HOST_ERR("time err, %lu,%lu\n", log.time, hlr->time);
			} else {
				log.oltime = (log.time - hlr->time);
			}
			igd_host_add_log(host, &log);
			return;
		}
	}
	IGD_HOST_ERR("log record fail, %d,%s\n", up, mac_to_str(host->mac));
}

static void igd_host_app_log(
	struct host_info_record *host,
	struct host_app_record *app, unsigned char up)
{
	struct host_log_record *hlr, *_hlr, *add_log, *pre_log = NULL, log;
	struct study_url_record *sur;

	IGD_HOST_CHECK_LOG_NUM(host);

	memset(&log, 0, sizeof(log));
	log.time = sys_uptime();
	if (!IGD_SELF_STUDY_URL(app->appid)) {
		log.type = HLT_APP;
		log.v.appid = app->appid;
	} else {
		sur = igd_find_study_url_by_id(app->appid);
		if (!sur) {
			IGD_HOST_ERR("no find, %u\n", app->appid);
			return;
		}
		log.type = HLT_SELF_URL;
		arr_strcpy(log.v.url_name, sur->name); 
	}

	if (up) {
		add_log = igd_host_add_log(host, &log);
	} else {
		add_log = NULL;
		list_for_each_entry_safe(hlr, _hlr, &host->log_list, list) {
			if (hlr->type == HLT_HOST)
				break;
			if (hlr->type != log.type)
				continue;
			if (((hlr->type == HLT_APP)
					&& (hlr->v.appid != log.v.appid))
				|| ((hlr->type == HLT_SELF_URL)
					&& (strcmp(hlr->v.url_name, log.v.url_name)))) {
				continue;
			}
			if (!add_log) {
				if (hlr->oltime)
					break;
				igd_set_bit(HLF_CLEAR, log.flag);
				igd_set_bit(HLF_CLEAR, hlr->flag);
				if (log.time == hlr->time) {
					igd_host_del_log(host, hlr);
					return;
				} else if (log.time < hlr->time) {
					log.oltime = 1;
					IGD_HOST_ERR("time err, %lu,%lu\n", log.time, hlr->time);
				} else {
					log.oltime = (log.time - hlr->time);
				}
				pre_log = hlr;
				add_log = igd_host_add_log(host, &log);
			} else if (hlr->oltime) {
				add_log->oltime += hlr->oltime;
				igd_host_del_log(host, hlr);
			} else {
				igd_host_del_log(host, pre_log);
				pre_log = hlr;
			}
		}
	}
	if (add_log)
		return;
	IGD_HOST_ERR("log record fail, %d,%s,%u\n",
		up, mac_to_str(host->mac), app->appid);
}

static void igd_host_mode_log(
	struct host_info_record *host, unsigned char mode)
{
	struct host_log_record log;

	IGD_HOST_CHECK_LOG_NUM(host);

	memset(&log, 0, sizeof(log));
	log.type = HLT_MODE;
	log.v.mode = mode;
	log.time = sys_uptime();
	igd_set_bit(HLF_CLEAR, log.flag);
	igd_host_add_log(host, &log);
}

#define HOST_LOG_FUNCTION "-------------above is log function-----------------------"

static struct host_filter_list *
	igd_host_find_filter(struct host_filter_info *info)
{
	struct host_filter_list *hfl;

	list_for_each_entry(hfl, &igd_host_filter_list, list) {
		if (memcmp(&info->time, &hfl->time, sizeof(hfl->time)))
			continue;
		if (memcmp(&info->host, &hfl->host, sizeof(hfl->host)))
			continue;
		if (memcmp(&info->arg, &hfl->arg, sizeof(hfl->arg)))
			continue;
		return hfl;
	}
	return NULL;
}

#define HOST_FILTER_FUNCTION "-------------above is filter function-----------------------"

static void *igd_host_find_rule(
	struct host_info_record *host, unsigned char type)
{
	struct rule_info_list *rule;

	list_for_each_entry(rule, &host->rule_list, list) {
		if (rule->type == type)
			return rule->rule;
	}
	return NULL;
}

static int igd_host_del_rule(
	struct host_info_record *host, unsigned char type)
{
	struct rule_info_list *rule;

	list_for_each_entry(rule, &host->rule_list, list) {
		if (rule->type != type)
			continue;
		list_del(&rule->list);
		free(rule->rule);
		free(rule);
		return 0;
	}
	return -1;
}

static void *igd_host_new_rule(
	struct host_info_record *host, unsigned char type)
{
	int size = 0;
	struct rule_info_list *rule;

	rule = malloc(sizeof(*rule));
	if (!rule) {
		IGD_HOST_ERR("malloc fail, %d\n", sizeof(*rule));
		return NULL;
	}
	memset(rule, 0, sizeof(*rule));
	rule->type = type;
	switch (type) {
	case RLT_BLACK:
		size = sizeof(int);
		break;
	case RLT_SPEED:
		size = sizeof(struct limit_speed_info);
		break;
	case RLT_STUDY_URL:
		size = sizeof(struct allow_study_url_info);
		break;
	case RLT_STUDY_TIME:
		size = sizeof(struct study_time_info);
		break;
	case RLT_URL_BLACK:
	case RLT_URL_WHITE:
		size = sizeof(struct host_url_info);
		break;
	case RLT_APP_BLACK:
		size = sizeof(struct app_black_info);
		break;
	case RLT_INTERCEPT_URL:
		size = sizeof(struct host_intercept_url_info);
		break;
	case RLT_APP_MOD_QUEUE:
		size = sizeof(struct app_mod_queue);
		break;
	case RLT_APP_WHITE:
		size = sizeof(struct app_white_info);
		break;
	default:
		break;
	}
	if (!size) {
		free(rule);
		IGD_HOST_ERR("type err, %d,%d\n", type, size);
		return NULL;
	}
	rule->rule = malloc(size);
	if (!rule->rule) {
		free(rule);
		IGD_HOST_ERR("malloc fail, %d\n", size);
		return NULL;
	}
	memset(rule->rule, 0, size);
	list_add(&rule->list, &host->rule_list);
	return rule->rule;
}

static void igd_host_app_online_refresh_mid(
	struct host_info_record *host, struct host_app_record *app)
{
	int mid = 0;

	mid = L7_GET_MID(app->appid);
	if ((mid < 0) || (mid >= L7_MID_MX)) {
		IGD_HOST_ERR("mid:%d, err\n", mid);
		return;
	}
	if (host->mid[mid].up_time)
		return;
	host->mid[mid].up_time = sys_uptime();
}

static void igd_host_app_offline_refresh_mid(
	struct host_info_record *host, struct host_app_record *app_in)
{
	int mid;
	struct host_app_record *app;

	mid = L7_GET_MID(app_in->appid);
	if ((mid < 0) || (mid >= L7_MID_MX)) {
		IGD_HOST_ERR("mid:%d, err\n", mid);
		return;
	}
	if (!host->mid[mid].up_time)
		return;
	list_for_each_entry(app, &host->app_list, list) {
		if (!igd_test_bit(HARF_ONLINE, app->flag))
			continue;
		if (mid == L7_GET_MID(app->appid))
			return;
	}
	host->mid[mid].all_time += \
		sys_uptime() - host->mid[mid].up_time;
	host->mid[mid].up_time = 0;
}

static void igd_host_online_refresh_app(
	struct host_info_record *host, struct in_addr addr)
{
	int app_nr, i;
	struct app_conn_info *ip_info;
	struct host_app_record *app;

	ip_info = dump_host_app(addr, &app_nr);
	if (!ip_info)
		return;
	for (i = 0; i < app_nr; i++) {
		if (!ip_info[i].conn_cnt)
			continue;
		app = igd_host_find_app(host, ip_info[i].appid);
		if (!app)
			continue;
		if (igd_test_bit(HARF_ONLINE, app->flag))
			continue;
		igd_set_bit(HARF_ONLINE, app->flag);
		set_uptime(&app->time.up);
		igd_host_app_online_refresh_mid(host, app);
		igd_host_app_log(host, app, 1);
	}
	free(ip_info);
}

static void igd_host_offline_refresh_app(
	struct host_info_record *host, struct in_addr addr)
{
	int app_nr, i, online;
	struct host_ip_info *ip;
	struct app_conn_info *ip_info;
	struct host_app_record *app;

	list_for_each_entry(app, &host->app_list, list) {
		if (!igd_test_bit(HARF_ONLINE, app->flag))
			continue;
		online = 0;
		list_for_each_entry(ip, &host->ip_list, list) {
			ip_info = dump_host_app(ip->ip, &app_nr);
			if (!ip_info)
				continue;
			for (i = 0; i < app_nr; i++) {
				if (ip_info[i].appid != app->appid)
					continue;
				if (ip_info[i].conn_cnt) {
					online = 1;
					break;
				}
			}
			free(ip_info);
			if (online)
				break;
		}
		if (!online) {
			igd_clear_bit(HARF_ONLINE, app->flag);
			add_oltime(&app->time);
			ip_info = dump_host_app(addr, &app_nr);
			if (ip_info) {
				for (i = 0; i < app_nr; i++) {
					if (ip_info[i].appid != app->appid)
						continue;
					app->bytes.up = ip_info[i].send.bytes;
					app->bytes.down = ip_info[i].rcv.bytes;
					break;
				}
				free(ip_info);
			}
			igd_host_app_offline_refresh_mid(host, app);
			igd_host_app_log(host, app, 0);
		}
	}
}

static int igd_host_wifi_vap(
	struct host_info_record *host, struct in_addr addr)
{
	struct host_info info;

	igd_clear_bit(HIRF_VAP, host->flag);
	igd_clear_bit(HIRF_VAP_ALLOWED, host->flag);
	igd_clear_bit(HIRF_WIRELESS, host->flag);
	igd_clear_bit(HIRF_WIRELESS_5G, host->flag);

	if (dump_host_info(addr, &info))
		return -1;

	if (info.is_wifi)
		igd_set_bit(HIRF_WIRELESS, host->flag);
	if (info.pid > WIFI_5G_BASIC_ID)
		igd_set_bit(HIRF_WIRELESS_5G, host->flag);
	if ((info.is_wifi) && 
		((info.pid == 2) || (info.pid == (WIFI_5G_BASIC_ID + 2)))) {
		igd_set_bit(HIRF_VAP, host->flag);
	}
	return 0;
}

static void igd_host_info_upload(struct host_info_record *host, int type)
{
	int len, i = 0;
	char full_name[128];
	unsigned char buf[512];

	if (!TEST_HOST_FLAG(HF_HISTORY))
		return;

	CC_PUSH2(buf, 2, CSO_NTF_ROUTER_TERMINAL);
	i = 8;
	CC_PUSH1(buf, i, host->base.os_type);
	i += 1;
	CC_PUSH1(buf, i, 12);
	i += 1;
	CC_PUSH_LEN(buf, i, mac_to_str(host->mac), 12);
	i += 12;
	len = strlen(host->base.name);
	CC_PUSH1(buf, i, len);
	i += 1;
	if (len > 0) {
		CC_PUSH_LEN(buf, i, host->base.name, len);
		i += len;
	}
	memset(full_name, 0, sizeof(full_name));
	if (igd_test_bit(HIRF_NICK_NAME, host->flag)) {
		arr_strcpy(full_name, host->base.nick);
	} else {
		arr_strcpy(full_name, \
			get_vender_name(host->base.vender, host->base.name));
	}
	len = strlen(full_name);
	CC_PUSH1(buf, i, len);
	i += 1;
	if (len > 0) {
		CC_PUSH_LEN(buf, i, full_name, len);
		i += len;
	}
	CC_PUSH1(buf, i, type);
	i += 1;
	CC_PUSH2(buf, i, host->base.vender);
	i += 2;
	CC_PUSH2(buf, 0, i);

	IGD_HOST_DBG("host upload:%s, mac:%s, os_type:%d, type:%d, vender:%d\n",
		full_name, mac_to_str(host->mac), host->base.os_type, type, host->base.vender);

	CC_CALL_ADD(buf, i);
}

static void igd_host_app_info_upload(struct nlk_msg_l7 *app)
{
	int i = 0;
	unsigned char buf[512] = {0,};

	if (!TEST_HOST_FLAG(HF_HISTORY))
		return;

	if (L7_GET_MID(app->appid) == L7_MID_HTTP)
		return;

	CC_PUSH2(buf, 2, CSO_NTF_ROUTER_APP);
	i = 8;
	CC_PUSH1(buf, i, 12);
	i += 1;
	CC_PUSH_LEN(buf, i, mac_to_str(app->mac), 12);
	i += 12;
	CC_PUSH1(buf, i, 1);
	i += 1;
	CC_PUSH4(buf, i, app->appid);
	i += 4;
	CC_PUSH2(buf, 0, i);
	CC_CALL_ADD(buf, i);
}

static struct allow_study_url_list *igd_host_find_surl_id(
	struct allow_study_url_info *info, unsigned int url_id)
{
	struct allow_study_url_list *url;

	if (!info)
		return NULL;

	list_for_each_entry(url, &info->url_list, list) {
		if (url->url_id == url_id)
			return url;
	}
	return NULL;
}

static void igd_host_del_surl_id(
	struct allow_study_url_list *url)
{
	list_del(&url->list);
	free(url);
}

static struct allow_study_url_list *igd_host_add_surl_id(
	struct allow_study_url_info *info, unsigned int url_id)
{
	struct allow_study_url_list *url;

	url = malloc(sizeof(*url));
	if (!url) {
		IGD_HOST_ERR("malloc fail, %d\n", sizeof(*url));
		return NULL;
	}
	url->url_id = url_id;
	list_add(&url->list, &info->url_list);
	return url;
}

static struct study_time_list *igd_host_find_study_time(
	struct study_time_info *info, struct time_comm *time)
{
	struct study_time_list *t;

	if (!info)
		return NULL;

	list_for_each_entry(t, &info->time_list, list) {
		if (!memcmp(&t->time, time, sizeof(*time)))
			return t;
	}
	return NULL;
}

static void igd_host_del_study_time(
	struct study_time_list *t)
{
	list_del(&t->list);
	free(t);
}

static struct study_time_list *igd_host_add_study_time(
	struct study_time_info *info, struct study_time_dump *time)
{
	struct study_time_list *t;

	t = malloc(sizeof(*t));
	if (!t) {
		IGD_HOST_ERR("malloc fail, %d\n", sizeof(*t));
		return NULL;
	}
	t->enable = time->enable;
	memcpy(&t->time, &time->time, sizeof(time->time));	
	list_add(&t->list, &info->time_list);
	return t;
}

static struct host_url_list *igd_host_find_bw_url(
	struct host_url_info *info, char *url)
{
	struct host_url_list *u;

	if (!info)
		return NULL;

	list_for_each_entry(u, &info->url_list, list) {
		if (!strcmp(u->url, url))
			return u;
	}
	return NULL;
}

static void igd_host_del_bw_url(
	struct host_url_list *u)
{
	list_del(&u->list);
	free(u->url);
	free(u);
}

static struct host_url_list *igd_host_add_bw_url(
	struct host_url_info *info, char *url)
{
	struct host_url_list *u;

	u = malloc(sizeof(*u));
	if (!u) {
		IGD_HOST_ERR("malloc fail, %d\n", sizeof(*u));
		return NULL;
	}
	u->url = strdup(url);
	if (!u->url) {
		free(u);
		IGD_HOST_ERR("strdup fail, %s\n", url);
		return NULL;
	}
	list_add(&u->list, &info->url_list);
	return u;
}

static struct app_mod_black_list *igd_host_find_app_mod(
	struct app_black_info *info, struct app_mod_dump *mod)
{
	struct app_mod_black_list *ambl;

	if (!info)
		return NULL;

	list_for_each_entry(ambl, &info->mod_list, list) {
		if (!memcmp(&mod->time, &ambl->time, sizeof(mod->time)))
			return ambl;
	}
	return NULL;
}

static void igd_host_del_app_mod(
	struct app_mod_black_list *info)
{
	list_del(&info->list);
	free(info);
}

static struct app_mod_black_list *igd_host_add_app_mod(
	struct app_black_info *info, struct app_mod_dump *mod)
{
	struct app_mod_black_list *ambl;

	ambl = malloc(sizeof(*ambl));
	if (!ambl) {
		IGD_HOST_ERR("malloc fail, %d\n", sizeof(*ambl));
		return NULL;
	}
	ambl->enable = mod->enable;
	memcpy(&ambl->time, &mod->time, sizeof(ambl->time));
	memcpy(ambl->mid_flag, mod->mid_flag, sizeof(ambl->mid_flag));
	list_add(&ambl->list, &info->mod_list);
	return ambl;
}

static struct host_intercept_url_list *igd_host_find_intercept_url(
	struct host_intercept_url_info *info, char *url)
{
	struct host_intercept_url_list *hiul;

	if (!info)
		return NULL;

	list_for_each_entry(hiul, &info->url_list, list) {
		if (!strcmp(hiul->url, url))
			return hiul;
	}
	return NULL;
}

static void igd_host_del_intercept_url(
	struct host_intercept_url_list *info)
{
	list_del(&info->list);
	free(info->url);
	free(info->type);
	free(info->type_en);
	free(info);
}

static struct host_intercept_url_list *igd_host_add_intercept_url(
	struct host_intercept_url_info *info, struct host_intercept_url_dump *url)
{
	struct host_intercept_url_list *hiul;

	hiul = malloc(sizeof(*hiul));
	if (!hiul) {
		IGD_HOST_ERR("malloc fail, %d\n", sizeof(*hiul));
		goto err;
	}
	hiul->url = strdup(url->url);
	hiul->type = strdup(url->type);
	hiul->type_en = strdup(url->type_en);
	if (!hiul->url || !hiul->type || !hiul->type_en) {
		IGD_HOST_ERR("dump fail, %s,%s,%s\n",
			url->url, url->type, hiul->type_en);
		goto err;
	}

	hiul->time = url->time;
	hiul->dport = url->dport;
	hiul->sport = url->sport;
	hiul->daddr = url->daddr;
	hiul->saddr = url->saddr;
	list_add(&hiul->list, &info->url_list);
	return hiul;

err:
	if (hiul->url)
		free(hiul->url);
	if (hiul->type)
		free(hiul->type);
	if (hiul->type_en)
		free(hiul->type_en);
	if (hiul)
		free(hiul);
	return NULL;
}

static int igd_host_filter_app_mod_queue_del(
	struct app_mod_queue *amq)
{
	int i, r;

	for (i = 0; i < L7_MID_MX; i++) {
		if (amq->rule_id[i] <= 0)
			continue;
		r = unregister_app_qos(amq->rule_id[i]);
		if (r < 0) {
			IGD_HOST_ERR("del fail, %d\n", amq->rule_id[i]);
			return -1;
		}
		amq->rule_id[i] = -1;
	}
	return 0;
}

static int igd_host_filter_app_mod_queue_add(
	unsigned char *mac, struct app_mod_queue *amq)
{
	int i, r, app[10];
	struct inet_host host;

	memset(&host, 0, sizeof(host));
	if (!mac) {
		host.type = INET_HOST_NONE;
	} else {
		host.type = INET_HOST_MAC;
		memcpy(host.mac, mac, 6);
	}

	for (i = 0; i < L7_MID_MX; i++) {
		if (!amq->mid[i])
			continue;
		app[0] = amq->mid[i]*L7_APP_MX;
		r = register_app_qos(&host, i + 1, 1, app);
		if (r < 0) {
			IGD_HOST_ERR("del fail, %d\n", amq->rule_id[i]);
			return -1;
		}
		amq->rule_id[i] = r;
	}
	return 0;
}

#define HOST_RULE_FUNCTION "-------------above is rule function-----------------------"

static struct host_info_record *igd_find_host(unsigned char *mac)
{
	struct host_info_record *host;

	list_for_each_entry(host, &igd_host_list, list) {
		if (!memcmp(host->mac, mac, 6))
			return host;
	}
	return NULL;
}

static struct host_info_record *igd_add_host(unsigned char *mac)
{
	unsigned char tmp[6];
	struct host_info_record *host;

	memset(tmp, 0, 6);
	if (!memcmp(tmp, mac, 6)) {
		IGD_HOST_DBG("mac 0\n");
		return NULL;
	}
	memset(tmp, 0xFF, 6);
	if (!memcmp(tmp, mac, 6)) {
		IGD_HOST_DBG("mac 0xFF\n");
		return NULL;
	}

	host = malloc(sizeof(*host));
	if (!host) {
		IGD_HOST_ERR("malloc fail, %d\n", sizeof(*host));
		return NULL;
	}
	memset(host, 0, sizeof(*host));
	igd_host_num++;
	memcpy(host->mac, mac, 6);
	INIT_LIST_HEAD(&host->ip_list);
	INIT_LIST_HEAD(&host->app_list);
	INIT_LIST_HEAD(&host->rule_list);
	INIT_LIST_HEAD(&host->log_list);
	host->base.mode = HM_FREE;
	host->base.vender = get_vender(mac);
	host->mid[L7_MID_GAME].clock_gap = 2*60*60;
	list_add(&host->list, &igd_host_list);
	return host;
}

static void igd_free_host(struct host_info_record *host)
{
	struct igd_host_global_info *ihgi = &igd_host_global;
	struct host_ip_info *ip, *_ip;
	struct host_app_record *app, *_app;
	struct host_log_record *log, *_log;
	struct rule_info_list *rule, *_rule;
	int *black_id, ret;
	struct limit_speed_info *lsi;
	struct allow_study_url_info *asui;
	struct allow_study_url_list *asul, *_asul;
	struct study_time_info *sti;
	struct study_time_list *stl, *_stl;
	struct host_url_info *hui;
	struct host_url_list *hul, *_hul;
	struct app_black_info *abi;
	struct app_mod_black_list *ambl, *_ambl;
	struct study_url_record *sur;
	struct host_intercept_url_info *hiui;
	struct host_intercept_url_list *hiul, *_hiul;
	struct app_mod_queue *amq;
	struct app_white_info *awi;

	if (igd_test_bit(HIRF_ISKING, host->flag)) {
		if (memcmp(host->mac, ihgi->king_mac, 6)) {
			IGD_HOST_ERR("king err, %s,%s\n", 
				mac_to_str(host->mac), mac_to_str(ihgi->king_mac));
		} else {
			if (ihgi->king_rule_id > 0) {
				ret = unregister_qos_rule(ihgi->king_rule_id);
				if (ret < 0)
					IGD_HOST_ERR("del king fail, %d\n", ret);
				ihgi->king_rule_id = -1;
			}
			igd_global_save_del(HGSF_KING);
			memset(ihgi->king_mac, 0, sizeof(ihgi->king_mac));
		}
	}

	list_for_each_entry_safe(ip, _ip, &host->ip_list, list) {
		list_del(&ip->list);
		free(ip);
	}
	list_for_each_entry_safe(app, _app, &host->app_list, list) {
		igd_host_app_save_check(host, app->appid, -1);
		list_del(&app->list);
		free(app);
	}
	list_for_each_entry_safe(log, _log, &host->log_list, list) {
		igd_host_del_log(host, log);
	}

	list_for_each_entry_safe(rule, _rule, &host->rule_list, list) {
		switch (rule->type) {
		case RLT_BLACK:
			black_id = rule->rule;
			if (*black_id > 0) {
				ret = unregister_net_rule(*black_id);
				if (ret < 0)
					IGD_HOST_ERR("del black_id fail, %d\n", ret);
			}
			break;
		case RLT_SPEED:
			lsi = rule->rule;
			if (lsi->rule_id > 0) {
				ret = unregister_rate_rule(lsi->rule_id);
				if (ret < 0)
					IGD_HOST_ERR("del lsi fail, %d\n", ret);
			}
			break;
		case RLT_STUDY_URL:
			asui = rule->rule;
			if (asui->black_id > 0) {
				ret = unregister_net_rule(asui->black_id);
				if (ret < 0)
					IGD_HOST_ERR("del asui black fail, %d\n", ret);
			}
			if (asui->rule_id > 0) {
				ret = unregister_http_rule(URL_MID_PASS, asui->rule_id);
				if (ret < 0)
					IGD_HOST_ERR("del asui url fail, %d\n", ret);
			}
			if (asui->grp_id > 0) {
				ret = del_group(URL_GRP, asui->grp_id);
				if (ret < 0)
					IGD_HOST_ERR("del asui grp fail, %d\n", ret);
			}
			if (asui->redirect_id > 0) {
				ret = unregister_http_rule(URL_MID_REDIRECT, asui->redirect_id);
				if (ret < 0)
					IGD_HOST_ERR("del asui redirect fail, %d\n", ret);
			}
			
			list_for_each_entry_safe(asul, _asul, &asui->url_list, list) {
				if (!IGD_SELF_STUDY_URL(asul->url_id))
					continue;
				sur = igd_find_study_url_by_id(asul->url_id);
				if (!sur)
					continue;
				sur->used--;
				if (sur->used > 0)
					continue;
				igd_study_url_save_del(sur);
				igd_study_url_free_id(sur->id);
				igd_del_study_url(sur);
				list_del(&asul->list);
				free(asul);
			}
			break;
		case RLT_STUDY_TIME:
			sti = rule->rule;
			list_for_each_entry_safe(stl, _stl, &sti->time_list, list) {
				list_del(&stl->list);
				free(stl);
			}
			break;
		case RLT_URL_BLACK:
		case RLT_URL_WHITE:
			hui = rule->rule;
			if (hui->rule_id > 0) {
				ret = unregister_http_rule(
					(rule->type == RLT_URL_BLACK) ? 
					URL_MID_DROP : URL_MID_PASS, hui->rule_id);
				if (ret < 0)
					IGD_HOST_ERR("del url black rule fail, %d\n", ret);
			}
			if (hui->grp_id > 0) {
				ret = del_group(URL_GRP, hui->grp_id);
				if (ret < 0)
					IGD_HOST_ERR("del url black grp fail, %d\n", ret);
			}
			list_for_each_entry_safe(hul, _hul, &hui->url_list, list) {
				igd_host_del_bw_url(hul);
			}
			break;
		case RLT_APP_BLACK:
			abi = rule->rule;
			if (abi->rule_id > 0) {
				ret = unregister_app_filter(abi->rule_id);
				if (ret < 0)
					IGD_HOST_ERR("del abi fail, %d\n", ret);
			}
			list_for_each_entry_safe(ambl, _ambl, &abi->mod_list, list) {
				igd_host_del_app_mod(ambl);
			}
			break;
		case RLT_INTERCEPT_URL:
			hiui = rule->rule;
			if (hiui->rule_id > 0) {
				ret = unregister_http_rule(URL_MID_DROP, hiui->rule_id);
				if (ret < 0)
					IGD_HOST_ERR("del hiui rule fail, %d\n", ret);
			}
			if (hiui->grp_id > 0) {
				ret = del_group(URL_GRP, hiui->grp_id);
				if (ret < 0)
					IGD_HOST_ERR("del hiui grp fail, %d\n", ret);
			}
			list_for_each_entry_safe(hiul, _hiul, &hiui->url_list, list) {
				igd_host_del_intercept_url(hiul);
			}
			break;
		case RLT_APP_MOD_QUEUE:
			amq = rule->rule;
			igd_host_filter_app_mod_queue_del(amq);
			break;
		case RLT_APP_WHITE:
			awi = rule->rule;
			if (awi->rule_id > 0) {
				ret = unregister_app_filter(awi->rule_id);
				if (ret < 0)
					IGD_HOST_ERR("del awi fail, %d\n", ret);
			}
			break;
		default:
			IGD_HOST_ERR("nonsupport, %d\n", rule->type);
			break;
		}

		list_del(&rule->list);
		free(rule->rule);
		free(rule);
	}

	igd_host_app_save_check(host, 0, -1);
}

static int igd_del_host(unsigned char *mac)
{
	struct host_info_record *host = NULL, *_host;

	if (mac) {
		host = igd_find_host(mac);
	} else if (igd_host_num >= IGD_HOST_MX) {
		IGD_HOST_DBG("full del, %d\n", igd_host_num);
		list_for_each_entry(_host, &igd_host_list, list) {
			if (!igd_test_bit(HIRF_ONLINE, _host->flag)) {
				host = _host;
				break;
			}
		}
	} else {
		return 0;
	}
	if (!host) {
		IGD_HOST_ERR("no find, mac:%p, num:%d\n",
			mac, igd_host_num);
		return -1;
	}
	if (igd_test_bit(HIRF_ONLINE, host->flag)) {
		IGD_HOST_ERR("is online, mac:%p, num:%d\n",
			mac, igd_host_num);
		return -1;
	}

	igd_free_host(host);
	list_del(&host->list);
	free(host);
	igd_host_num--;
	return 0;
}

#define HOST_FUNCTION "-------------above is host function-----------------------"

static int igd_host_filter_black_del(int id)
{
	return (id <= 0) ? 0 : unregister_net_rule(id);
}

static int igd_host_filter_black_add(unsigned char *mac)
{
	struct inet_host host;
	struct net_rule_api arg;
	
	memset(&host, 0, sizeof(host));
	host.type = INET_HOST_MAC;
	memcpy(host.mac, mac, ETH_ALEN);
	memset(&arg, 0, sizeof(arg));
	arg.target = NF_TARGET_DENY;
	return register_net_rule(&host, &arg);
}

static int igd_host_filter_speed_del(int id)
{
	return (id <= 0) ? 0 : unregister_rate_rule(id);
}

static int igd_host_filter_speed_add(
	unsigned char *mac, unsigned long up, unsigned long down)
{
	struct inet_host host;
	struct rate_rule_api arg;
	
	memset(&host, 0, sizeof(host));
	host.type = INET_HOST_MAC;
	memcpy(host.mac, mac, ETH_ALEN);
	memset(&arg, 0, sizeof(arg));
	arg.up = (unsigned short)up;
	arg.down = (unsigned short)down;
	return register_rate_rule(&host, &arg);
}

static int igd_host_filter_king_del(int id)
{
	return (id <= 0) ? 0 : unregister_qos_rule(id);
}

static int igd_host_filter_king_add(unsigned char *mac)
{
	struct inet_host host;
	struct qos_rule_api arg;
	
	memset(&host, 0, sizeof(host));
	host.type = INET_HOST_MAC;
	memcpy(host.mac, mac, ETH_ALEN);
	memset(&arg, 0, sizeof(arg));
	arg.queue = 2;
	return register_qos_rule(&host, &arg);
}

static int igd_host_filter_grp_del(int id)
{
	return (id <= 0) ? 0 : del_group(URL_GRP, id);
}

static int igd_host_filter_study_grp_add(struct host_info_record *host)
{
	char grp_name[128];
	struct inet_url url[IGD_URL_GRP_PER_MX];
	struct study_url_record *sur;
	struct allow_study_url_info *url_info;
	int nr = 0, f1, f2;

	url_info = igd_host_find_rule(host, RLT_STUDY_URL);
	list_for_each_entry(sur, &igd_study_url_list, list) {
		f1 = IGD_SELF_STUDY_URL(sur->id) ? 1 : 0;
		f2 = igd_host_find_surl_id(url_info, sur->id) ? 1 : 0;
		if (f1 != f2)
			continue;
		url[nr].id = sur->id;
		arr_strcpy(url[nr].url,\
			memcmp(sur->url, "www.", 4) ? sur->url : sur->url + 4);
		nr++;
		if (nr >= IGD_URL_GRP_PER_MX)
			break;
	}
	if (!nr)
		return 0;
	snprintf(grp_name, sizeof(grp_name) - 1,
		"%s_%s", IGD_HOST_STUDY_URL, mac_to_str(host->mac));
	return add_url_grp2(grp_name, nr, url);
}

static int igd_host_filter_bw_url_grp_add(
	struct host_info_record *host, unsigned char type)
{
	struct host_url_info *url_info;
	struct host_url_list *url_list;
	int nr = 0;
	char grp_name[128];
	char url[IGD_HOST_URL_MX][IGD_URL_LEN];

	url_info = igd_host_find_rule(host, type);
	if (!url_info)
		return 0;

	list_for_each_entry(url_list, &url_info->url_list, list) {
		arr_strcpy(url[nr], url_list->url);
		nr++;
	}
	if (!nr)
		return 0;
	snprintf(grp_name, sizeof(grp_name) - 1, "%s_%s",
		(type == RLT_URL_BLACK) ? HSF_URL_BLACK : 
		HSF_URL_WHITE, mac_to_str(host->mac));
	return add_url_grp(grp_name, nr, url);
}

static int igd_host_filter_url_del(int mid, int id)
{
	return (id <= 0) ? 0 : unregister_http_rule(mid, id);
}

static int igd_host_filter_url_add(
	unsigned char *mac, int grp, int mid, int extra)
{
	struct inet_host rule_host;
	struct http_rule_api rule_url;

	memset(&rule_host, 0, sizeof(rule_host));
	rule_host.type = INET_HOST_MAC;
	memcpy(rule_host.mac, mac, 6);

	memset(&rule_url, 0, sizeof(rule_url));
	rule_url.l3.type = INET_L3_URLID;
	rule_url.l3.gid = grp;
	rule_url.mid = mid;
	rule_url.prio = 0;
	if (extra) 
		igd_set_bit(URL_RULE_STUDY_BIT, &rule_url.flags);
	return register_http_rule(&rule_host, &rule_url);
}

static int igd_host_filter_redirect_add(unsigned char *mac)
{
	struct inet_host host;
	struct http_rule_api rule;
	char dns[256];

	if (CC_CALL_RCONF_INFO(RCONF_FLAG_URLSTUDY, dns, sizeof(dns))) {
		IGD_HOST_ERR("read fail, %s,%s\n", RCONF_FLAG_URLSTUDY, dns);
		arr_strcpy(dns, "hao.wiair.com");
	}

	memset(&host, 0, sizeof(host));
	host.type = INET_HOST_MAC;
	memcpy(host.mac, mac, 6);

	memset(&rule, 0, sizeof(rule));
	rule.l3.type = INET_L3_NONE;
	rule.mid = URL_MID_REDIRECT;
	rule.prio = 1;
	snprintf(rule.rd.url, sizeof(rule.rd.url), "http://%s/study.html?dm=%s",
		dns, read_firmware("DOMAIN"));
	return register_http_rule(&host, &rule);
}

#define HOST_FILTER_FUNCTION "-------------above is filter function-----------------------"

static int igd_host_check_study_time(
	struct host_info_record *host)
{
	struct tm *t = get_tm();
	struct study_time_info *time_info;
	struct study_time_list *time_list;

	time_info = igd_host_find_rule(host, RLT_STUDY_TIME);
	if (!time_info || !t)
		return 0;

	list_for_each_entry(time_list, &time_info->time_list, list) {
		if (!time_list->enable)
			continue;
		if (igd_time_cmp(t, &time_list->time))
			return 1;
	}
	return 0;
}

static int igd_host_study_rule(
	struct host_info_record *host, int change)
{
	int ret, f_study, f_time, f_instudy_pre, f_instudy_later;
	struct allow_study_url_info *rule;

	f_study = (host->base.mode == HM_STUDY) ? 1 : 0;
	f_time = f_study ? igd_host_check_study_time(host) : 0;
	rule = igd_host_find_rule(host, RLT_STUDY_URL);
	f_instudy_pre = igd_test_bit(HTRF_INSTUDY, host->tmp) ? 1 : 0;

	if (f_study) {
		/*
		IGD_HOST_DBG("time:%d, grp_id:%d, rule_id:%d,"
		" redirect_id:%d, black_id:%d\n", f_time,
		rule ? rule->grp_id : -1, rule ? rule->rule_id : -1,
		rule ? rule->redirect_id : -1, rule ? rule->black_id : -1);
		*/
	}

	if (rule && (change || !f_study || (f_study && !f_time))) {
		if (rule->black_id > 0) {
			ret = igd_host_filter_black_del(rule->black_id);
			if (ret < 0)
				IGD_HOST_ERR("del black fail, %d\n", ret);
			else
				rule->black_id = -1;
		}
		if (rule->rule_id > 0) {
			ret = igd_host_filter_url_del(URL_MID_PASS, rule->rule_id);
			if (ret < 0)
				IGD_HOST_ERR("del url fail, %d\n", ret);
			else
				rule->rule_id = -1;
		}
		if (rule->grp_id > 0) {
			ret = igd_host_filter_grp_del(rule->grp_id);
			if (ret < 0)
				IGD_HOST_ERR("del grp fail, %d\n", ret);
			else
				rule->grp_id = -1;
		}
		if (rule->redirect_id > 0) {
			ret = igd_host_filter_url_del(URL_MID_REDIRECT, rule->redirect_id);
			if (ret < 0)
				IGD_HOST_ERR("del redirect fail, %d\n", ret);
			else
				rule->redirect_id = -1;
		}
		if (list_empty(&rule->url_list)) {
			igd_host_del_rule(host, RLT_STUDY_URL);
			rule = NULL;
		}
		if (igd_test_bit(HTRF_INSTUDY, host->tmp))
			igd_clear_bit(HTRF_INSTUDY, host->tmp);
	}

	if (f_study && f_time 
		&& !igd_test_bit(HIRF_INBLACK, host->flag)) {
		if (!rule) {
			rule = igd_host_new_rule(host, RLT_STUDY_URL);
			if (!rule)
				return -1;
			INIT_LIST_HEAD(&rule->url_list);
		}
		if (rule->grp_id <= 0) {
			rule->grp_id = igd_host_filter_study_grp_add(host);
			if (rule->grp_id < 0)
				IGD_HOST_ERR("add grp fail\n", rule->grp_id);
		}
		if (rule->grp_id > 0) {
			if (rule->rule_id <= 0) {
				rule->rule_id = igd_host_filter_url_add(
					host->mac, rule->grp_id, URL_MID_PASS, 1);
				if (rule->rule_id < 0)
					IGD_HOST_ERR("add url fail\n", rule->rule_id);
			}
		}
		if (rule->redirect_id <= 0) {
			rule->redirect_id = igd_host_filter_redirect_add(host->mac);
			if (rule->redirect_id < 0)
				IGD_HOST_ERR("add redirect fail\n", rule->redirect_id);
		}
		if (rule->black_id <= 0) {
			rule->black_id = igd_host_filter_black_add(host->mac);
			if (rule->black_id < 0)
				IGD_HOST_ERR("add black fail\n", rule->black_id);
		}
		if (!igd_test_bit(HTRF_INSTUDY, host->tmp))
			igd_set_bit(HTRF_INSTUDY, host->tmp);
	}
	f_instudy_later = igd_test_bit(HTRF_INSTUDY, host->tmp) ? 1 : 0;
	if (f_instudy_pre == f_instudy_later)
		return 0;
	if (f_instudy_later)
		igd_host_mode_log(host, HM_STUDY);
	else
		igd_host_mode_log(host, HM_FREE);
	return 0;
}

static int igd_host_url_rule(
	struct host_info_record *host, unsigned char type)
{
	int ret;
	struct host_url_info *url_info;

	url_info = igd_host_find_rule(host, type);
	if (!url_info)
		return 0;

	ret = igd_host_filter_url_del(
		(type == RLT_URL_BLACK) ? 
		URL_MID_DROP : URL_MID_PASS, url_info->rule_id);
	if (ret < 0) {
		IGD_HOST_ERR("del rule fail, %d,%d\n", ret, type);
		return -1;
	}
	url_info->rule_id = -1;

	ret = igd_host_filter_grp_del(url_info->grp_id);
	if (ret < 0) {
		IGD_HOST_ERR("del grp fail, %d,%d\n", ret, type);
		return -1;
	}
	url_info->grp_id = -1;

	url_info->grp_id = igd_host_filter_bw_url_grp_add(host, type);
	if (url_info->grp_id < 0) {
		IGD_HOST_ERR("add grp fail, %d,%d\n", url_info->grp_id, type);
		return -1;
	} else if (url_info->grp_id > 0) {
		url_info->rule_id = igd_host_filter_url_add(
			host->mac, url_info->grp_id,
			(type == RLT_URL_BLACK) ? URL_MID_DROP : URL_MID_PASS,
			0);
		if (url_info->rule_id < 0) {
			IGD_HOST_ERR("add rule fail, %d,%d\n", url_info->rule_id, type);
			return -1;
		}
	}
	return 0;
}

static int igd_host_app_mod_rule(
	struct host_info_record *host,
	struct app_black_info *abi, int *appid, int *nr)
{
	int i;
	struct tm *t = get_tm();
	struct app_mod_black_list *ambl;
	unsigned long mid_flag[BITS_TO_LONGS(L7_MID_MX)];
	unsigned long pre_flag = 0, now_flag = 0;

	if (!abi || !t)
		return 0;

	memset(mid_flag, 0, sizeof(mid_flag));

	if (HM_CLASSIFY == host->base.mode) {
		list_for_each_entry(ambl, &abi->mod_list, list) {
			if (!ambl->enable)
				continue;
			if (!igd_time_cmp(t, &ambl->time))
				continue;
			for (i = 0; i < BITS_TO_LONGS(L7_MID_MX); i++)
				mid_flag[i] |= ambl->mid_flag[i];
		}

		for (i = 0; i < L7_MID_MX; i++) {
			if (!igd_test_bit(i, mid_flag))
				continue;
			appid[*nr] = i*L7_APP_MX;
			(*nr)++;
		}
	}

	if (!memcmp(abi->mid_old, mid_flag, sizeof(abi->mid_old)))
		return 0;

	for (i = 0; i < BITS_TO_LONGS(L7_MID_MX); i++) {
		now_flag |= mid_flag[i];
		pre_flag |= abi->mid_old[i];
	}
	if ((!!now_flag) != (!!pre_flag)) {
		IGD_HOST_DBG("%d\n", !!now_flag);
		if (now_flag) {
			igd_host_mode_log(host, HM_CLASSIFY);
			igd_set_bit(HTRF_INCLASSIFY, host->tmp);
		} else {
			igd_clear_bit(HTRF_INCLASSIFY, host->tmp);
			igd_host_mode_log(host, HM_FREE);
		}
	}
	memcpy(abi->mid_old, mid_flag, sizeof(abi->mid_old));
	return 1;
}

static int igd_host_app_black_rule(
	struct host_info_record *host, int change)
{
	struct inet_host ih;
	struct app_filter_rule rule;
	struct host_app_record *app;
	struct app_black_info *abi;
	struct tm *t = get_tm();
	int ret;

	memset(&rule, 0, sizeof(rule));

	abi = igd_host_find_rule(host, RLT_APP_BLACK);

	change += igd_host_app_mod_rule(
		host, abi, rule.appid, &rule.nr);

	list_for_each_entry(app, &host->app_list, list) {
		if (igd_test_bit(HARF_INBLACK, app->flag)
			|| (t && igd_time_cmp(t, &app->lt))) {
			rule.appid[rule.nr] = app->appid;
			rule.nr++;
			if (!app->lt.loop)
				change++;
			app->lt.loop = 1;
			continue;
		}
		if (app->lt.loop)
			change++;
		app->lt.loop = 0;
	}

	if (rule.nr <= 0 || change) {
		if (abi && (abi->rule_id > 0)) {
			ret = unregister_app_filter(abi->rule_id);
			if (ret < 0) {
				IGD_HOST_ERR("del fail, %d\n", ret);
				return -1;
			}
			abi->rule_id = -1;
		}
	}

	if (rule.nr <= 0) {
		if (abi && list_empty(&abi->mod_list))
			igd_host_del_rule(host, RLT_APP_BLACK);
		return 0;
	} else if (!change) {
		return 0;
	}

	if (!abi) {
		abi = igd_host_new_rule(host, RLT_APP_BLACK);
		if (!abi)
			return -1;
		INIT_LIST_HEAD(&abi->mod_list);
	}
	IGD_HOST_DBG("change, %d,%d,%s\n", change,
		abi->rule_id, mac_to_str(host->mac));

	memset(&ih, 0, sizeof(ih));
	ih.type = INET_HOST_MAC;
	memcpy(ih.mac, host->mac, 6);

	rule.type = NF_TARGET_DENY;
	rule.prio = -1;
	ret = register_app_filter(&ih, &rule);
	if (ret < 0) {
		IGD_HOST_ERR("add fail, %d\n", ret);
		return -1;
	}
	abi->rule_id = ret;
	return 0;
}

static int igd_intercept_url_rule(struct host_info_record *host)
{
	struct host_intercept_url_info *hiui;
	struct host_intercept_url_list *hiul;
	char url[IGD_HOST_URL_MX][IGD_URL_LEN];
	char grp_name[64], *url_s, *url_e;
	int nr = 0, ret;

	hiui = igd_host_find_rule(host, RLT_INTERCEPT_URL);
	if (!hiui)
		return 0;

	ret = igd_host_filter_url_del(URL_MID_DROP, hiui->rule_id);
	if (ret < 0) {
		IGD_HOST_ERR("del rule fail, %d\n", ret);
		return -1;
	}
	hiui->rule_id = -1;

	ret = igd_host_filter_grp_del(hiui->grp_id);
	if (ret < 0) {
		IGD_HOST_ERR("del grp fail, %d\n", ret);
		return -1;
	}
	hiui->grp_id = -1;

	memset(url, 0, IGD_HOST_URL_MX*IGD_URL_LEN);
	list_for_each_entry(hiul, &hiui->url_list, list) {
		url_s = hiul->url;
		if (!memcmp(url_s, "http://", 7))
			url_s += 7;
		if (!memcmp(url_s, "www.", 4))
			url_s += 4;
		url_e = strchr(url_s, '/');
		if (url_e)
			*url_e = '\0';
		arr_strcpy(url[nr], url_s);
		if (url_e)
			*url_e = '/';
		nr++;
	}

	if (nr <= 0)
		return 0;

	snprintf(grp_name, sizeof(grp_name) - 1, "%s_%s",
		HSF_INTERCEPT_URL, mac_to_str(host->mac));
	ret = add_url_grp(grp_name, nr, url);
	if (ret < 0) {
		IGD_HOST_ERR("add fail, %d\n", ret);
		return -1;
	}
	hiui->grp_id = ret;

	ret = igd_host_filter_url_add(host->mac,
		hiui->grp_id, URL_MID_DROP, 0);
	if (ret < 0) {
		IGD_HOST_ERR("add fail, %d\n", ret);
		return -1;
	}
	hiui->rule_id = ret;
	return 0;
}

static int igd_host_app_white_rule(
	struct host_info_record *host, int change)
{
	int ret;
	struct inet_host ih;
	struct app_filter_rule rule;
	struct app_white_info *awi;
	struct host_app_record *app;

	awi = igd_host_find_rule(host, RLT_APP_WHITE);
	if (awi && (awi->rule_id > 0)) {
		ret = unregister_app_filter(awi->rule_id);
		if (ret < 0) {
			IGD_HOST_ERR("del fail, %d\n", ret);
			return -1;
		}
		awi->rule_id = -1;
	}

	memset(&ih, 0, sizeof(ih));
	memset(&rule, 0, sizeof(rule));

	list_for_each_entry(app, &host->app_list, list) {
		if (!igd_test_bit(HARF_INWHITE, app->flag))
			continue;
		rule.appid[rule.nr] = app->appid;
		rule.nr++;
		if (rule.nr >= APP_RULE_MAX)
			break;
	}

	if (rule.nr <= 0) {
		if (awi)
			igd_host_del_rule(host, RLT_APP_WHITE);
		return 0;
	}

	if (!awi) {
		awi = igd_host_new_rule(host, RLT_APP_WHITE);
		if (!awi)
			return -1;
	}

	ih.type = INET_HOST_MAC;
	memcpy(ih.mac, host->mac, 6);

	rule.type = NF_TARGET_ACCEPT;
	rule.prio = -2;
	ret = register_app_filter(&ih, &rule);
	if (ret < 0) {
		IGD_HOST_ERR("add fail, %d\n", ret);
		return -1;
	}
	awi->rule_id = ret;
	return 0;
}

#define HOST_RULE_ACTION_FUNCTION "-------------above is rule action function-----------------------"

static int igd_host_online(struct nlk_host *nlk, int len)
{
	struct host_info_record *host;

	IGD_HOST_INPUT_CHECK(nlk, 1, len);
	IGD_HOST_DBG("MAC:%s, ip:%s\n",	
		mac_to_str(nlk->mac), inet_ntoa(nlk->addr));

	host = igd_find_host(nlk->mac);
	if (!host) {
		igd_del_host(NULL);
		host = igd_add_host(nlk->mac);
		if (!host) {
			IGD_HOST_ERR("add host fail\n");
			return -1;
		}
	}

	igd_clear_bit(HTRF_DHCP, host->tmp);
	igd_clear_bit(HTRF_LOAD, host->tmp);

	list_move(&host->list, &igd_host_list);
	igd_host_add_ip(host, nlk->addr);
	if (set_host_info(nlk->addr, host->base.nick,
		host->base.name, host->base.os_type, host->base.vender)) {
		IGD_HOST_ERR("set host info fail, %s\n", inet_ntoa(nlk->addr));
	}

	if (!igd_test_bit(HIRF_ONLINE, host->flag))
		igd_host_log(host, 1);

	igd_host_online_refresh_app(host, nlk->addr);
	if (igd_test_bit(HIRF_ONLINE, host->flag)) {
		IGD_HOST_DBG("host already online, MAC:%s, ip:%s\n",
			mac_to_str(nlk->mac), inet_ntoa(nlk->addr));
		return 0;
	}
	set_uptime(&host->time.up);
	igd_set_bit(HIRF_ONLINE, host->flag);

	igd_host_wifi_vap(host, nlk->addr);

	if (igd_test_bit(HIRF_ONLINE_PUSH, host->flag)) {
		igd_host_info_upload(host, HOST_UPLOAD_EVERY_PUSH);
	} else if (igd_test_bit(HIRF_VAP, host->flag)) {
		igd_host_info_upload(host, HOST_UPLOAD_NO_PUSH);
	} else if (igd_test_bit(HTRF_NEW, host->tmp)) {
		if (igd_host_global.new_push)
			igd_host_info_upload(host, HOST_UPLOAD_FIRST_PUSH);
		igd_clear_bit(HTRF_NEW, host->tmp);
	} else {
		igd_host_info_upload(host, HOST_UPLOAD_NO_PUSH);
	}
	return 0;
}

static int igd_host_offline(struct nlk_host *nlk, int len)
{
	struct host_info_record *host;

	IGD_HOST_INPUT_CHECK(nlk, 1, len);
	IGD_HOST_DBG("MAC:%s, ip:%s\n",
		mac_to_str(nlk->mac), inet_ntoa(nlk->addr));

	host = igd_find_host(nlk->mac);
	if (!host) {
		IGD_HOST_ERR("no find host, MAC:%s, ip:%s\n",
			mac_to_str(nlk->mac), inet_ntoa(nlk->addr));
		return -1;
	}
	igd_host_del_ip(host, nlk->addr);
	igd_host_offline_refresh_app(host, nlk->addr);
	if (igd_host_check_ip(host)) {
		IGD_HOST_DBG("host is online\n");
		return 0;
	}
	if (!igd_test_bit(HIRF_ONLINE, host->flag)) {
		IGD_HOST_ERR("host already offline, MAC:%s, ip:%s\n",
			mac_to_str(nlk->mac), inet_ntoa(nlk->addr));
		return 0;
	}
	igd_clear_bit(HIRF_ONLINE, host->flag);
	host->bytes.up = nlk->pkt[0].bytes;
	host->bytes.down = nlk->pkt[1].bytes;
	add_oltime(&host->time);
	list_move_tail(&host->list, &igd_host_list);
	igd_host_log(host, 0);
	return 0;
}

static int igd_host_app_online(struct nlk_msg_l7 *nlk, int len)
{
	struct host_info_record *host;
	struct host_app_record *app;

	IGD_HOST_INPUT_CHECK(nlk, 1, len);
	IGD_HOST_DBG("MAC:%s, ip:%s, appid:%d\n",
		mac_to_str(nlk->mac), inet_ntoa(nlk->addr), nlk->appid);

	if (igd_host_part_app(nlk->appid))
		return 0;

	host = igd_find_host(nlk->mac);
	if (!host) {
		IGD_HOST_ERR("no find host, MAC:%s, ip:%s, appid:%d\n",
			mac_to_str(nlk->mac), inet_ntoa(nlk->addr), nlk->appid);
		return -1;
	}
	if (!igd_test_bit(HIRF_ONLINE, host->flag)) {
		IGD_HOST_DBG("host is offline\n");
		return 0;
	}

	app = igd_host_find_app(host, nlk->appid);
	if (!app) {
		igd_host_del_app(host, 0);
		app = igd_host_add_app(host, nlk->appid);
		if (!app) {
			IGD_HOST_ERR("add app fail\n");
			return -1;
		}
	}

	list_move(&app->list, &host->app_list);
	if (igd_test_bit(HARF_ONLINE, app->flag)) {
		IGD_HOST_DBG("app already online, MAC:%s, ip:%s, appid:%d\n",
			mac_to_str(nlk->mac), inet_ntoa(nlk->addr), nlk->appid);
		return 0;
	}
	set_uptime(&app->time.up);
	igd_set_bit(HARF_ONLINE, app->flag);
	igd_host_app_online_refresh_mid(host, app);
	igd_host_app_info_upload(nlk);
	igd_host_app_log(host, app, 1);
	return 0;
}

static int igd_host_app_offline(struct nlk_msg_l7 *nlk, int len)
{
	struct host_info_record *host;
	struct host_app_record *app;

	IGD_HOST_INPUT_CHECK(nlk, 1, len);
	IGD_HOST_DBG("MAC:%s, ip:%s, appid:%d\n",
		mac_to_str(nlk->mac), inet_ntoa(nlk->addr), nlk->appid);

	if (igd_host_part_app(nlk->appid))
		return 0;

	host = igd_find_host(nlk->mac);
	if (!host) {
		IGD_HOST_ERR("no find host, MAC:%s, ip:%s, appid:%d\n",
			mac_to_str(nlk->mac), inet_ntoa(nlk->addr), nlk->appid);
		return -1;
	}
	if (!igd_test_bit(HIRF_ONLINE, host->flag)) {
		IGD_HOST_DBG("host is offline\n");
		return 0;
	}
	if (igd_host_check_app(host, nlk->appid)) {
		IGD_HOST_DBG("app is online\n");
		return 0;
	}
	app = igd_host_find_app(host, nlk->appid);
	if (!app) {
		IGD_HOST_ERR("no find app, MAC:%s, ip:%s, appid:%d\n",
			mac_to_str(nlk->mac), inet_ntoa(nlk->addr), nlk->appid);
		return -1;
	}
	list_move_tail(&app->list, &host->app_list);
	if (!igd_test_bit(HARF_ONLINE, app->flag)) {
		IGD_HOST_DBG("already off, MAC:%s, ip:%s, appid:%d\n",
			mac_to_str(nlk->mac), inet_ntoa(nlk->addr), nlk->appid);
		return 0;
	}
	igd_clear_bit(HARF_ONLINE, app->flag);
	add_oltime(&app->time);
	app->bytes.up = nlk->pkt[0].bytes;
	app->bytes.down = nlk->pkt[1].bytes;
	igd_host_app_offline_refresh_mid(host, app);
	igd_host_app_log(host, app, 0);
	return 0;
}

static int igd_study_url_load(int reload)
{
	char *buf, *p, *n;
	unsigned char pwd[16] = {
		0xcc,0xfc,0x78,0x66,0x35,0x32,0x97,0xfc,0x78,0x99};
	unsigned int id = 0;
	struct host_info_record *host;

	IGD_HOST_DBG("reload:%d\n", reload);

	igd_free_study_url();
	buf = malloc(IGD_STUDY_FILE_SIZE);
	if (!buf) {
		IGD_HOST_ERR("malloc fail, %d\n", IGD_STUDY_FILE_SIZE);
		return -1;
	}
	memset(buf, 0, IGD_STUDY_FILE_SIZE);
	if (igd_aes_dencrypt(IGD_STUDY_URL_FILE, 
		(unsigned char *)buf, IGD_STUDY_FILE_SIZE, pwd) <= 0) {
		IGD_HOST_ERR("aes study url fail\n");
		goto err;
	}

	p = buf;
	for (n = buf; *n != '\0'; n++) {
		if (*n == ',') {
			*n = '\0';
			id = atoi(p);
			p = n + 1;
		} else if (*n == ';') {
			*n = '\0';
			if (!id) {
				IGD_HOST_ERR("id err\n");
				goto err;
			}
			if (igd_find_study_url_by_url(p)) {
				IGD_HOST_ERR("url is exist, %s\n", p);
			} else {
				igd_add_study_url(id, NULL, p, NULL);
			}
			p = n + 1;
		} else if (*n == '\r' || *n == '\n') {
			id = 0;
			p = n + 1;
		}
	}

err:
	remove(IGD_STUDY_URL_FILE);

	if (buf)
		free(buf);
	if (!reload)
		return 0;

	list_for_each_entry(host, &igd_host_list, list) {
		igd_host_study_rule(host, 1);
	}
	return 0;
}

static int igd_study_url_dns_update(void)
{
	struct host_info_record *host;

	list_for_each_entry(host, &igd_host_list, list) {
		igd_host_study_rule(host, 1);
	}
	return 0;
}

static int igd_host_ua_check(struct ua_data *rule)
{
	if (rule->start_len <=0)
		return -1;
	if (rule->vender <= 0)
		return -1;
	if (rule->name_len > 0)
		return 0;
	if (!rule->end && !rule->len)
		return -1;
	return 0;
}

static int igd_host_ua_load(void)
{
	int i = 0, j = 0, nr = 0;
	unsigned char tmp[32] = {0};
	unsigned char *buf = NULL;
	struct ua_data ua[UA_DATA_MX];
	unsigned char pwd[16] = {0xcc, 0xfc, 0x78, 0x66, 0x35, 0x32, 0x97, 0xfc, 0x78, 0x99};

	buf = malloc(IGD_HOST_UA_FILE_SIZE + 1);
	if (!buf) {
		IGD_HOST_ERR("malloc fail, %d\n", IGD_HOST_UA_FILE_SIZE);
		return -1;
	}

	memset(buf, 0, IGD_HOST_UA_FILE_SIZE + 1);
	if (igd_aes_dencrypt(IGD_HOST_UA_FILE, buf, IGD_HOST_UA_FILE_SIZE, pwd) <= 0) {
		IGD_HOST_ERR("aes dencrypt ua fail\n");
		goto err;
	}

	while (1) {
		memset(&ua[i], 0x0, sizeof(struct ua_data));
		nr = __arr_strcpy_end((unsigned char *)ua[i].match_start, &buf[j], sizeof(ua[i].match_start) - 1, ',');
		if (!nr)
			break;
		ua[i].start_len = strlen(ua[i].match_start);
		j += (nr + 1);
		nr = __arr_strcpy_end((unsigned char *)ua[i].host_name, &buf[j], sizeof(ua[i].host_name) - 1, ',');
		ua[i].name_len = strlen(ua[i].host_name);
		j += (nr + 1);
		nr = __arr_strcpy_end((unsigned char *)ua[i].match_end, &buf[j], sizeof(ua[i].match_end) - 1, ',');
		ua[i].end_len = strlen(ua[i].match_end);
		j += (nr + 1);
		nr = arr_strcpy_end(tmp, &buf[j], ',');
		if (strlen((char *)tmp) > 0)
			ua[i].skip = atoi((char *)tmp);
		j += (nr + 1);
		nr = arr_strcpy_end(tmp, &buf[j], ',');
		if (strlen((char *)tmp) > 0)
			ua[i].end = *tmp;
		j += (nr + 1);
		nr = arr_strcpy_end(tmp, &buf[j], ',');
		if (strlen((char *)tmp) > 0)
			ua[i].len = atoi((char *)tmp);
		j += (nr + 1);
		nr = arr_strcpy_end(tmp, &buf[j], '\n');
		if (strlen((char *)tmp) > 0)
			ua[i].vender = atoi((char *)tmp);
		j += (nr + 1);

		if (igd_host_ua_check(&ua[i]) < 0) {
			IGD_HOST_ERR("***bad ua rule,start:[%s:%d],name:[%s:%d],"
				"end:[%s:%d],skip:[%d],endchar:[%d],len:[%d]\n",
				ua[i].match_start, ua[i].start_len, ua[i].host_name,
				ua[i].name_len, ua[i].match_end, ua[i].end_len,
				ua[i].skip, ua[i].end, ua[i].len);
			continue;
		}

		i++;
		if (i >= UA_DATA_MX)
			break;
	}
	if (i > 0) {
		IGD_HOST_DBG("ua num:%d\n", i);
		set_ua_info(i, ua);
	}
err:
	if (buf)
		free(buf);
	remove(IGD_HOST_UA_FILE);
	return 0;
}

static int igd_host_vender_update(void)
{
	struct host_info_record *host;

	IGD_HOST_DBG("%d\n", sys_uptime());

	if (read_vender() < 0) {
		IGD_HOST_ERR("vender update fail\n");
		return -1;
	}

	list_for_each_entry(host, &igd_host_list, list) {
		host->base.vender = get_vender(host->mac);
	}
	return 0;
}

static int igd_vender_name(unsigned short *id, int len, char *rbuf, int rlen)
{
	if (!id || len != sizeof(unsigned short) || !rbuf || !rlen)
		return -1;

	strncpy(rbuf, get_vender_name((unsigned short)*id, NULL), rlen - 1);
	return 0;
}

static int igd_host_name_update(struct ua_msg *ua, int len)
{
	struct host_info_record *host;

	IGD_HOST_INPUT_CHECK(ua, 1, len);
	host = igd_find_host(ua->mac);
	if (!host) {
		IGD_HOST_ERR("no find, %s\n", mac_to_str(ua->mac));
		return -1;
	}
	IGD_HOST_DBG("%s,%s %d,%d\n", ua->ua, host->base.name,
		ua->vender, host->base.vender);

	if (!strcmp(host->base.name, ua->ua)
		&& (host->base.vender == ua->vender)) {
		return 0;
	}

	if (ua->ua[0])
		arr_strcpy(host->base.name, ua->ua);
	if (ua->vender)
		host->base.vender = ua->vender;
	igd_host_info_upload(host, HOST_UPLOAD_INFO_UPTATE);
	return 0;
}

static int igd_host_os_type_update(struct ua_msg *ua, int len)
{
	struct host_info_record *host;

	IGD_HOST_INPUT_CHECK(ua, 1, len);
	host = igd_find_host(ua->mac);
	if (!host) {
		IGD_HOST_ERR("no find, %s\n", mac_to_str(ua->mac));
		return -1;
	}
	IGD_HOST_DBG("%d,%d\n", ua->ua[0], host->base.os_type);
	if (host->base.os_type == (uint8_t)ua->ua[0])
		return 0;

	if (ua->ua[0])
		host->base.os_type = (uint8_t)ua->ua[0];
	igd_host_info_upload(host, HOST_UPLOAD_INFO_UPTATE);
	return 0;
}

static int igd_host_ua_collect(struct ua_msg *ua, int len)
{
	struct host_ua_record *hur;

	IGD_HOST_INPUT_CHECK(ua, 1, len);

	IGD_HOST_DBG("%d, ua:%s\n", igd_host_ua_num, ua->ua);

	if (igd_host_ua_num > IGD_HOST_UA_MX)
		return 0;

	list_for_each_entry(hur, &igd_host_ua_list, list) {
		if (!strcmp(ua->ua, hur->ua))
			return 0;
	}
	hur = malloc(sizeof(*hur));
	if (!hur) {
		IGD_HOST_ERR("malloc fail, %d\n", sizeof(*hur));
		return -1;
	}
	hur->ua = strdup(ua->ua);
	if (!hur->ua) {
		free(hur);
		IGD_HOST_ERR("strdup fail, %s\n", ua->ua);
		return -1;
	}
	list_add(&hur->list, &igd_host_ua_list);
	igd_host_ua_num++;
	return 0;
}

static int igd_host_dhcp_info(struct host_dhcp_trait *ops, int len)
{
	struct host_info_record *host;

	IGD_HOST_INPUT_CHECK(ops, 5, len);

	host = igd_find_host(ops[4].op);
	if (!host) {
		host = igd_add_host(ops[4].op);
		if (!host) {
			return -1;
		}
		igd_set_bit(HTRF_NEW, host->tmp);
		list_move_tail(&host->list, &igd_host_list);
	}
	if (host->base.os_type == 0)
		host->base.os_type = get_host_os_type(&ops[0]);
	if (host->base.os_type == 0)
		host->base.os_type = get_host_os_type(&ops[1]);
	if (host->base.os_type == 0) {
		if (!memcmp(ops[2].op, "MSFT", 4))
			host->base.os_type = OS_PC_WINDOWS;
		else if (!memcmp(ops[2].op, "dhcpcd", 6))
			host->base.os_type = OS_PHONE_ANDROID;
	}
	if (!host->base.name[0]) {
		str_filter((char *)ops[3].op, sizeof(ops[3].op));
		if (!memcmp((char *)ops[3].op, "android", 7)) {
			memcpy(host->base.name, (char *)ops[3].op, 14);
			host->base.name[14] = '\0';
		} else if (ops[3].op[0]) {
			arr_strcpy(host->base.name, (char *)ops[3].op);
		}
	}
	if (!igd_test_bit(HIRF_ONLINE, host->flag) && \
		!host->time.up.time) {
		igd_set_bit(HTRF_DHCP, host->tmp);
	}
	IGD_HOST_DBG("mac:%s, maker:%d, os_type:%d, host_name:%s\n", 
		mac_to_str(host->mac), host->base.vender, host->base.os_type, host->base.name);
	return 0;
}

static int igd_host_dbg_file(char *flag, int len)
{
	FILE *fp = NULL;
	struct host_info_record *host;
	struct host_app_record *app;
	struct host_ip_info *host_ip;
	struct rule_info_list *rule;
	int i = 0;

	fp = fopen("/tmp/igd_host_file", "wb");
	if (fp) {
		fprintf(fp, "---------------------\n");
		fprintf(fp, "HOST num:%d, flag:%lu, time:%ld\n",
			igd_host_num, igd_host_flag[0], sys_uptime());
		list_for_each_entry(host, &igd_host_list, list) {
			fprintf(fp, "MAC:%s\n", mac_to_str(host->mac));
			fprintf(fp, "--FLAG:%lu, tmp:%lu, app:%d, ip:%d\n",
				host->flag[0], host->tmp[0], host->num.app, host->num.ip);
			fprintf(fp, "--mode:%d, name:%s, nick:%s, os_type:%d, vender:%d\n", 
				host->base.mode, host->base.name, host->base.nick,
				host->base.os_type, host->base.vender);
			fprintf(fp, "--time:%d,%lu,%lu,%lu\n",
				host->time.up.flag, host->time.up.time, host->time.all, host->time.old);
			fprintf(fp, "--bytes:%llu,%llu,%llu,%llu\n",
				host->bytes.up, host->bytes.down, host->bytes.old_up, host->bytes.old_down);

			fprintf(fp, "--ip:");
			list_for_each_entry(host_ip, &host->ip_list, list) {
				fprintf(fp, "%s,", inet_ntoa(host_ip->ip));
			}
			fprintf(fp, "\n");
			for (i = 0; i < L7_MID_MX; i++) {
				fprintf(fp, "--mid(%d):%lu,%lu,%lu,%lu,%lu\n", i, host->mid[i].up_time,
					host->mid[i].all_time, host->mid[i].clock_gap,
					host->mid[i].clock_last, host->mid[i].upload_time);
			}
			list_for_each_entry(app, &host->app_list, list) {
				fprintf(fp, "---appid:%d, FLAG:%lu\n", app->appid, app->flag[0]);
				fprintf(fp, "-----time:%d,%lu,%lu,%lu\n",
					app->time.up.flag, app->time.up.time, app->time.all, app->time.old);
				fprintf(fp, "-----bytes:%llu,%llu,%llu,%llu\n",
					app->bytes.up, app->bytes.down, app->bytes.old_up, app->bytes.old_down);
			}

			list_for_each_entry(rule, &host->rule_list, list) {
				fprintf(fp, "---rule:%d, info:%p\n", rule->type, rule->rule);
			}
		}
		fclose(fp);
	}

	struct host_log_record *log;
	fp = fopen("/tmp/igd_host_file", "ab+");
	if (fp) {
		fprintf(fp, "---------------------\n");
		list_for_each_entry(host, &igd_host_list, list) {
			fprintf(fp, "MAC:%s, %d\n", mac_to_str(host->mac), host->num.log);
			list_for_each_entry_reverse(log, &host->log_list, list) {
				fprintf(fp, "type:[%d]", log->type);
				switch (log->type) {
				case HLT_APP:
					fprintf(fp, ",appid:%u", log->v.appid);
					break;
				case HLT_HOST:
					fprintf(fp, ",host:%s", host->base.name);
					break;
				case HLT_MODE:
					fprintf(fp, ",mode:%d", log->v.mode);
					break;
				case HLT_SELF_URL:
					fprintf(fp, ",url_name:%s", log->v.url_name);
					break;
				default:
					break;
				}
				fprintf(fp, "\n---:time:%lu,%lu,flag:%lu\n",
					log->time, log->oltime, log->flag[0]);
			}
		}
		fclose(fp);
	}

	struct study_url_record *sur;
	fp = fopen("/tmp/igd_host_file", "ab+");
	if (fp) {
		fprintf(fp, "---------------------\n");
		list_for_each_entry_reverse(sur, &igd_study_url_list, list) {
			fprintf(fp, "%d,%d,%s,%s,%s\n", sur->id, sur->used,
				sur->name ? sur->name : "",
				sur->url ? sur->url : "",
				sur->info ? sur->info : "");
		}
		fclose(fp);
	}

	dbg_vender();

	list_for_each_entry(host, &igd_host_list, list) {
		igd_host_log_upload(host);
	}
	return 0;
}

static void igd_host2dump(
	struct host_info_record *host,
	struct host_dump_info *info, unsigned long time_diff)
{
	struct host_ip_info *host_ip;
	struct host_info ip_info;
	struct limit_speed_info *ls;

	memcpy(info->mac, host->mac, 6);
	memcpy(info->flag, host->flag, sizeof(info->flag));
	if (igd_test_bit(HIRF_NICK_NAME, host->flag))
		arr_strcpy(info->name, host->base.nick);
	else
		arr_strcpy(info->name, host->base.name);
	info->mode = host->base.mode;
	info->os_type = host->base.os_type;
	info->vender = host->base.vender;
	info->pic = host->base.pic;

	if ((host->time.up.flag == UPTIME_SYS) && (time_diff)) {
		host->time.up.flag = UPTIME_NET;
		host->time.up.time += time_diff;
	}
	info->uptime = host->time.up.time;

	ls = igd_host_find_rule(host, RLT_SPEED);
	if (ls)
		memcpy(&info->ls, ls, sizeof(*ls));
	if (!igd_test_bit(HIRF_ONLINE, host->flag)) {
		info->up_bytes = host->bytes.up;
		info->down_bytes = host->bytes.down;
		info->ontime = 0;
	} else {
		info->ontime = ((host->time.up.flag == UPTIME_SYS) ? \
			sys_uptime() : get_nettime()) - host->time.up.time;

		list_for_each_entry(host_ip, &host->ip_list, list) {
			if (dump_host_info(host_ip->ip, &ip_info))
				continue;
			info->ip[0] = host_ip->ip;
			info->up_speed += ip_info.send.speed;
			info->down_speed += ip_info.rcv.speed;
			info->up_bytes += ip_info.send.bytes;
			info->down_bytes += ip_info.rcv.bytes;
		}
	}
}

static int igd_host_dump(
	unsigned char *mac, int mac_len,
	struct host_dump_info *info, int info_len)
{
	int num, i = 0;
	struct host_info_record *host;
	struct tm *tm = get_tm();
	unsigned long time_diff = 0;

	if (tm)
		time_diff = get_nettime() - sys_uptime();

	if (mac)
		IGD_HOST_INPUT_CHECK(mac, 6, mac_len);
	num = mac ? 1 : IGD_HOST_MX;
	IGD_HOST_INPUT_CHECK(info, num, info_len);

	list_for_each_entry(host, &igd_host_list, list) {
		if (num <= i)
			break;
		if (!igd_test_bit(HIRF_ONLINE, host->flag))
			continue;
		if (igd_test_bit(HTRF_DHCP, host->tmp))
			continue;
		if (igd_test_bit(HTRF_LOAD, host->tmp))
			continue;
		if (mac && memcmp(mac, host->mac, 6))
			continue;
		igd_host2dump(host, &info[i], time_diff);
		i++;
	}

	list_for_each_entry_reverse(host, &igd_host_list, list) {
		if (num <= i)
			break;
		if (igd_test_bit(HIRF_ONLINE, host->flag))
			continue;
		if (igd_test_bit(HTRF_DHCP, host->tmp))
			continue;
		if (igd_test_bit(HTRF_LOAD, host->tmp))
			continue;
		if (mac && memcmp(mac, host->mac, 6))
			continue;
		igd_host2dump(host, &info[i], time_diff);
		i++;
	}
	return mac ? (i ? 0 : -1) : i;
}

static int igd_host_app2off(
	struct host_app_record *app, unsigned long *app_mod)
{
	int mid;

	if (!igd_test_bit(HARF_ONLINE, app->flag))
		return 0;
	if (igd_test_bit(HARF_INBLACK, app->flag))
		return 1;

	mid = L7_GET_MID(app->appid);
	if (mid <= 0 || mid >= L7_MID_MX) {
		IGD_HOST_ERR("appid err, %u\n", app->appid);
		return 0;
	}
	if (igd_test_bit(mid, app_mod))
		return 1;
	return 0;
}

static void igd_host_app2dump(
	struct host_info_record *host,
	struct host_app_record *app,
	struct host_app_dump_info *info, unsigned long time_diff)
{
	int app_nr, i;
	struct host_ip_info *ip;
	struct app_conn_info *ip_info;

	info->appid = app->appid;

	if ((app->time.up.flag == UPTIME_SYS) && (time_diff)) {
		app->time.up.flag = UPTIME_NET;
		app->time.up.time += time_diff;
	}
	info->uptime = app->time.up.time;

	info->lt = app->lt;
	memcpy(info->flag, app->flag, sizeof(info->flag));
	if (!igd_test_bit(HARF_ONLINE, app->flag)) {
		info->up_bytes = app->bytes.up;
		info->down_bytes = app->bytes.down;
		info->ontime = 0;
	} else {
		info->ontime = ((app->time.up.flag == UPTIME_SYS) ? \
			sys_uptime() : get_nettime()) - app->time.up.time;

		list_for_each_entry(ip, &host->ip_list, list) {
			ip_info = dump_host_app(ip->ip, &app_nr);
			if (!ip_info)
				continue;
			for (i = 0; i < app_nr; i++) {
				if (ip_info[i].appid != app->appid)
					continue;
				info->up_bytes += ip_info[i].send.bytes;
				info->down_bytes += ip_info[i].rcv.bytes;
				if (!ip_info[i].conn_cnt)
					break;
				info->up_speed += ip_info[i].send.speed;
				info->down_speed += ip_info[i].rcv.speed;
				break;
			}
			free(ip_info);
		}
	}
}

static int igd_host_app_dump(
	unsigned char *mac, int mac_len,
	struct host_app_dump_info *info, int info_len)
{
	int num = 0;
	struct host_info_record *host;
	struct host_app_record *app;
	struct app_black_info *abi;
	unsigned long app_mod[BITS_TO_LONGS(L7_MID_MX)];
	struct tm *tm = get_tm();
	unsigned long time_diff = 0;

	if (tm)
		time_diff = get_nettime() - sys_uptime();

	IGD_HOST_INPUT_CHECK(mac, 6, mac_len);
	IGD_HOST_INPUT_CHECK(info, IGD_HOST_APP_MX, info_len);

	host = igd_find_host(mac);
	if (!host)
		return -1;

	if (igd_test_bit(HIRF_INBLACK, host->flag)) {
		memset(app_mod, 0xFF, sizeof(app_mod));
	} else if (igd_test_bit(HTRF_INSTUDY, host->tmp)) {
		memset(app_mod, 0xFF, sizeof(app_mod));
		igd_clear_bit(L7_MID_HTTP, app_mod);
	} else if (igd_test_bit(HTRF_INCLASSIFY, host->tmp)) {
		abi = igd_host_find_rule(host, RLT_APP_BLACK);
		memcpy(app_mod, abi->mid_old, sizeof(app_mod));
	} else {
		memset(app_mod, 0, sizeof(app_mod));
		igd_set_bit(L7_MID_HTTP, app_mod);
	}

	list_for_each_entry(app, &host->app_list, list) {
		if (!igd_test_bit(HARF_ONLINE, app->flag))
			continue;
		if (igd_host_part_app(app->appid))
			continue;
		igd_host_app2dump(host, app, &info[num], time_diff);
		if (igd_host_app2off(app, app_mod))
			igd_clear_bit(HARF_ONLINE, info[num].flag);
		num++;
	}

	list_for_each_entry_reverse(app, &host->app_list, list) {
		if (igd_test_bit(HARF_ONLINE, app->flag))
			continue;
		if (igd_host_part_app(app->appid))
			continue;
		igd_host_app2dump(host, app, &info[num], time_diff);
		if (igd_host_app2off(app, app_mod))
			igd_clear_bit(HARF_ONLINE, info[num].flag);
		num++;
	}
	return num;
}

#define PULL_HISTORY(data, data_len) do {\
	if (start + data_len > end) \
		goto err; \
	memcpy(data, start, data_len); \
	start += data_len; \
}while(0)

static int igd_host_add_history(unsigned char *data, int len)
{
	int i = 0, j = 0;
	unsigned char *start = data, *end = data + len;
	unsigned char host_nr, os_type, mac_len, strmac[13], mac[6], name_len, app_nr;
	unsigned int appid, uptime;
	unsigned short vender = 0;
	struct host_info_record *host;
	struct host_app_record *app;

	PULL_HISTORY(&host_nr, sizeof(host_nr));
	if (!host_nr) {
		SET_HOST_FLAG(HF_HISTORY);
		IGD_HOST_DBG("parse over\n");
		return 0;
	}

	for (i = 0; i < host_nr; i++) {
		if (igd_host_num > IGD_HOST_MX) {
			IGD_HOST_DBG("full, return\n");
			return 0;
		}
		PULL_HISTORY(&os_type, sizeof(os_type));
		PULL_HISTORY(&mac_len, sizeof(mac_len));
		if (mac_len != 12) {
			IGD_HOST_ERR("mac len err, %d\n", mac_len);
			goto err;
		}
		memset(strmac, 0, sizeof(strmac));
		memset(mac, 0, sizeof(mac));
		PULL_HISTORY(strmac, mac_len);
		if (str_to_mac((char *)strmac, mac) != 0) {
			IGD_HOST_ERR("mac err, %s\n", strmac);
			goto err;
		}

		host = igd_find_host(mac);
		if (!host) {
			host = igd_add_host(mac);
			if (!host)
				goto err;
			list_move_tail(&host->list, &igd_host_list);
		}
		igd_clear_bit(HTRF_DHCP, host->tmp);
		igd_clear_bit(HTRF_LOAD, host->tmp);
		igd_clear_bit(HTRF_NEW, host->tmp);

		if (host->base.os_type == 0)
			host->base.os_type = os_type;
		PULL_HISTORY(&name_len, sizeof(name_len));
		if (name_len > sizeof(host->base.name)) {
			IGD_HOST_ERR("name len err, %d\n", name_len);
			goto err;
		}
		PULL_HISTORY(host->base.name, name_len);
		PULL_HISTORY(&app_nr, sizeof(app_nr));
		for (j = 0; j < app_nr; j++) {
			PULL_HISTORY(&appid, sizeof(appid));
			PULL_HISTORY(&uptime, sizeof(uptime));
			appid = ntohl(appid);
			uptime = ntohl(uptime);
			IGD_HOST_DBG("appid:%d, uptime:%u\n", appid, uptime);
			if ((!appid || !host->time.up.time) && \
					!igd_test_bit(HIRF_ONLINE, host->flag)) {
				host->time.up.flag = UPTIME_NET;
				host->time.up.time = uptime;
				continue;
			}
			if (!appid)
				continue;
			app = igd_host_find_app(host, appid);
			if (!app) {
				app = igd_host_add_app(host, appid);
				if (!app)
					continue;
				list_move_tail(&app->list, &host->app_list);
			}
			if (!app->time.up.time) {
				app->time.up.flag = UPTIME_NET;
				app->time.up.time = uptime;
			}
		}
		PULL_HISTORY(&vender, sizeof(vender));
		if (!host->base.vender)
			host->base.vender = ntohs(vender);
		IGD_HOST_DBG("HISTORY:mac:%s, name:%s, os:%d, vender:%d\n",
			mac_to_str(host->mac), host->base.name, host->base.os_type, host->base.vender);
	}
	return 0;
err:
	IGD_HOST_ERR("parse err\n");
	return -1;
}

#define IGD_HOST_SET_CHECK(host, info, len) do {\
	IGD_HOST_INPUT_CHECK(info, 1, len); \
	host = igd_find_host(info->mac); \
	if (!host) { \
		IGD_HOST_ERR("no find, MAC:%s\n", mac_to_str(info->mac)); \
		return -1; \
	}\
} while(0)

static int igd_host_del_history(struct host_set_info *info, int len)
{
	int i;
	unsigned char buf[32];

	IGD_HOST_INPUT_CHECK(info, 1, len);

	if (igd_del_host(info->mac))
		return -1;
	CC_PUSH2(buf, 2, CSO_NTF_DEL_HISTORY);
	i = 8;
	CC_PUSH1(buf, i, 12);
	i += 1;
	CC_PUSH_LEN(buf, i, mac_to_str(info->mac), 12);
	i += 12;
	CC_PUSH2(buf, 0, i);
	CC_CALL_ADD(buf, i);
	return 0;
}

static int igd_host_ip2mac(
	struct in_addr *ip, int len, unsigned char *mac, int mac_len)
{
	struct host_info_record *host;
	struct host_ip_info *host_ip;

	IGD_HOST_INPUT_CHECK(ip, 1, len);
	IGD_HOST_INPUT_CHECK(mac, 6, mac_len);

	list_for_each_entry(host, &igd_host_list, list) {
		list_for_each_entry(host_ip, &host->ip_list, list) {
			if (host_ip->ip.s_addr == ip->s_addr) {
				memcpy(mac, host->mac, 6);
				return 0;
			}
		}
	}
	return -1;
}

static int igd_host_set_nick(struct host_set_info *info, int len)
{
	struct host_info_record *host;

	IGD_HOST_SET_CHECK(host, info, len);

	if (info->v.name[0]) {
		arr_strcpy(host->base.nick, info->v.name);
		igd_set_bit(HIRF_NICK_NAME, host->flag);
		return igd_host_app_save_add(host, 0, HSF_NICK, "%s", host->base.nick);
	} else {
		memset(host->base.nick, 0, sizeof(host->base.nick));
		igd_clear_bit(HIRF_NICK_NAME, host->flag);
		return igd_host_app_save_del(host, 0, HSF_NICK);
	}
}

static int igd_host_set_online_push(struct host_set_info *info, int len)
{
	struct host_info_record *host;

	IGD_HOST_SET_CHECK(host, info, len);
	if (info->act == NLK_ACTION_ADD) {
		igd_set_bit(HIRF_ONLINE_PUSH, host->flag);
		return igd_host_app_save_add(host, 0, HSF_ONLINE_PUSH, "1");
	} else if (info->act == NLK_ACTION_DEL) {
		igd_clear_bit(HIRF_ONLINE_PUSH, host->flag);
		return igd_host_app_save_del(host, 0, HSF_ONLINE_PUSH);
	} else {
		return -1;
	}
	return 0;
}

static int igd_host_set_pic(struct host_set_info *info, int len)
{
	struct host_info_record *host;

	IGD_HOST_SET_CHECK(host, info, len);
	if (host->base.pic == info->v.pic)
		return 0;
	host->base.pic = info->v.pic;
	if (host->base.pic)
		return igd_host_app_save_add(host, 0, HSF_PIC, "%d", host->base.pic);
	else
		return igd_host_app_save_del(host, 0, HSF_PIC);
}

static int igd_host_set_black(struct host_set_info *info, int len)
{
	int *black_id, ret;
	struct host_info_record *host;

	IGD_HOST_SET_CHECK(host, info, len);

	black_id = igd_host_find_rule(host, RLT_BLACK);
	IGD_HOST_DBG("%d,%d\n", info->act, black_id ? *black_id : -1);
	if ((info->act == NLK_ACTION_ADD) && !black_id) {
		black_id = igd_host_new_rule(host, RLT_BLACK);
		if (!black_id)
			return -1;
		ret = igd_host_filter_black_add(host->mac);
		if (ret < 0) {
			IGD_HOST_ERR("add fail, %d\n", ret);
			igd_host_del_rule(host, RLT_BLACK);
			return -1;
		}
		*black_id = ret;
		igd_set_bit(HIRF_INBLACK, host->flag);
		igd_host_app_save_add(host, 0, HSF_BLACK, "1");
	} else if ((info->act == NLK_ACTION_DEL) && black_id) {
		ret = igd_host_filter_black_del(*black_id);
		if (ret < 0) {
			IGD_HOST_ERR("del fail, %d\n", ret);
			return -1;
		}
		igd_host_del_rule(host, RLT_BLACK);
		igd_clear_bit(HIRF_INBLACK, host->flag);
		igd_host_app_save_del(host, 0, HSF_BLACK);
	}
	igd_host_study_rule(host, 1);
	return 0;
}

static int igd_host_set_limit_speed(struct host_set_info *info, int len)
{
	int ret;
	struct host_info_record *host;
	struct limit_speed_info *ls, *rule;

	IGD_HOST_SET_CHECK(host, info, len);

	ls = &info->v.ls;
	rule = igd_host_find_rule(host, RLT_SPEED);
	if (ls->up || ls->down) {
		if (!rule) {
			rule = igd_host_new_rule(host, RLT_SPEED);
			if (!rule)
				return -1;
		}
		ret = igd_host_filter_speed_del(rule->rule_id);
		if (ret < 0) {
			IGD_HOST_ERR("del fail, %d\n", ret);
			return -1;
		}
		ret = igd_host_filter_speed_add(host->mac, ls->up, ls->down);
		if (ret < 0) {
			IGD_HOST_ERR("add fail, %d\n", ret);
			igd_clear_bit(HIRF_LIMIT_SPEED, host->flag);
			igd_host_del_rule(host, RLT_SPEED);
			return -1;
		}
		rule->rule_id = ret;
		rule->up = ls->up;
		rule->down = ls->down;
		igd_set_bit(HIRF_LIMIT_SPEED, host->flag);
		return igd_host_app_save_add(host, 0, HSF_LS, "%lu,%lu", ls->down, ls->up);
	} else if (!rule) {
		IGD_HOST_DBG("no rule\n");
		return 0;
	} else {
		ret = igd_host_filter_speed_del(rule->rule_id);
		if (ret < 0) {
			IGD_HOST_ERR("del fail, %d\n", ret);
			return -1;
		}
		igd_host_del_rule(host, RLT_SPEED);
		igd_clear_bit(HIRF_LIMIT_SPEED, host->flag);
		return igd_host_app_save_del(host, 0, HSF_LS);
	}
}

static int igd_host_set_mode(struct host_set_info *info, int len)
{
	struct host_info_record *host;

	IGD_HOST_SET_CHECK(host, info, len);
	if (info->v.mode >= HM_MAX) {
		IGD_HOST_ERR("mode err, %d,%d\n", info->v.mode, HM_MAX);
		return -1;
	}
	host->base.mode = info->v.mode;

	igd_host_study_rule(host, 1);
	igd_host_app_black_rule(host, 1);

	if (host->base.mode == HM_FREE) {
		return igd_host_app_save_del(host, 0, HSF_MODE);
	} else {
		return igd_host_app_save_add(host, 0, HSF_MODE, "%d", host->base.mode);
	}
}

static int igd_host_set_king(struct host_set_info *info, int len)
{
	int ret;
	struct host_info_record *host;
	struct igd_host_global_info *ihgi = &igd_host_global;

	IGD_HOST_INPUT_CHECK(info, 1, len);

	ret = igd_host_filter_king_del(ihgi->king_rule_id);
	if (ret < 0) {
		IGD_HOST_ERR("del fail, %d\n", ret);
		return -1;
	}
	ihgi->king_rule_id = -1;
	host = igd_find_host(ihgi->king_mac);
	if (host)
		igd_clear_bit(HIRF_ISKING, host->flag);

	if (info->act == NLK_ACTION_ADD) {
		host = igd_find_host(info->mac);
		if (!host) {
			IGD_HOST_ERR("no find, %d\n", ret);
			return -1;
		}
		ret = igd_host_filter_king_add(info->mac);
		if (ret < 0) {
			IGD_HOST_ERR("add fail, %d\n", ret);
			return -1;
		}
		ihgi->king_rule_id = ret;
		igd_set_bit(HIRF_ISKING, host->flag);
		memcpy(ihgi->king_mac, info->mac, 6);
		return igd_global_save_add(HGSF_KING, "%s",
			mac_to_str(ihgi->king_mac));
	} else if (info->act == NLK_ACTION_DEL) {
		memset(ihgi->king_mac, 0, 6);
		return igd_global_save_del(HGSF_KING);
	} else {
		return -1;
	}
	return 0;
}

static int igd_host_set_new_push(
	struct host_set_info *info, int len,
	struct host_set_info *back, int b_len)
{
	struct igd_host_global_info *ihgi = &igd_host_global;

	IGD_HOST_INPUT_CHECK(info, 1, len);
	IGD_HOST_INPUT_CHECK(back, 1, b_len);

	if (info->act == NLK_ACTION_ADD) {
		ihgi->new_push = 1;
		return igd_global_save_del(HGSF_NEW_PUSH);
	} else if (info->act == NLK_ACTION_DEL) {
		ihgi->new_push = 0;
		return igd_global_save_add(HGSF_NEW_PUSH, "%d", 0);
	} else if (info->act == NLK_ACTION_DUMP) {
		back->v.new_push = ihgi->new_push;
	} else {
		return -1;
	}
	return 0;
}

static void igd_study_url2dump(
	struct study_url_record *sur, struct study_url_dump *dump)
{
	dump->id = sur->id;
	if (sur->name)
		arr_strcpy(dump->name, sur->name);
	if (sur->url)
		arr_strcpy(dump->url, sur->url);
	if (sur->info)
		arr_strcpy(dump->info, sur->info);
}


static int igd_study_url_act(
	struct host_set_info *info, int len,
	struct study_url_dump *dump, int d_len)
{
	int nr = 0;
	struct study_url_record *sur;
	struct study_url_dump *url = &info->v.surl;

	IGD_HOST_INPUT_CHECK(info, 1, len);
	if (info->act == NLK_ACTION_DUMP) {
		IGD_HOST_INPUT_CHECK(dump, IGD_STUDY_URL_SELF_NUM, d_len);

		list_for_each_entry(sur, &igd_study_url_list, list) {
			if (!IGD_SELF_STUDY_URL(sur->id))
				continue;
			igd_study_url2dump(sur, &dump[nr]);
			nr++;
		}
		return nr;
	} else if (info->act == NLK_ACTION_ADD) {
		sur = igd_find_study_url_by_url(url->url);
		if (sur) {
			if (!IGD_SELF_STUDY_URL(sur->id))
				return sur->id;
			if (sur->name)
				free(sur->name);
			sur->name = strdup(url->name);
		} else {
			nr = igd_study_url_alloc_id();
			if (nr < 0) {
				IGD_HOST_ERR("study url is full\n");
				return -2; // is full
			}
			sur = igd_add_study_url(
				(unsigned int)nr, url->name, url->url, NULL);
			if (!sur)
				return -1;
		}
		sur->used++;
		igd_study_url_save_add(sur);
		return sur->id;
	} else if (info->act == NLK_ACTION_DEL) {
		if (!IGD_SELF_STUDY_URL(url->id))
			return 0;
		sur = igd_find_study_url_by_id(url->id);
		if (!sur)
			return 0;
		sur->used--;
		if (sur->used > 0)
			return 0;
		igd_study_url_save_del(sur);
		igd_study_url_free_id(sur->id);
		igd_del_study_url(sur);
		return 0;
	} else {
		return -1;
	}
	return 0;
}

static int igd_host_study_url_act(
	struct host_set_info *info, int len,
	struct study_url_dump *dump, int d_len)
{
	char *str;
	struct host_info_record *host;
	struct study_url_record *sur;
	struct allow_study_url_info *url_info;
	struct allow_study_url_list *url_list;
	int nr = 0, i, f1, f2;

	IGD_HOST_SET_CHECK(host, info, len);

	url_info = igd_host_find_rule(host, RLT_STUDY_URL);
	if (info->act == NLK_ACTION_DUMP) {
		IGD_HOST_INPUT_CHECK(dump, IGD_STUDY_URL_MX, d_len);
		list_for_each_entry(sur, &igd_study_url_list, list) {
			if (sur->id == IGD_REDIRECT_STUDY_URL_APPID)
				continue;
			f1 = IGD_SELF_STUDY_URL(sur->id) ? 1 : 0;
			f2 = igd_host_find_surl_id(url_info, sur->id) ? 1 : 0;
			if (f1 != f2)
				continue;
			for (i = 0; i < nr; i++) {
				if (dump[i].id == sur->id)
					break;
			}
			if (i >= nr) {
				igd_study_url2dump(sur, &dump[nr]);
				nr++;
			}
			if (nr >= IGD_STUDY_URL_MX)
				break;
		}
		return nr;
	}
	f1 = (info->act == NLK_ACTION_ADD) ? 1 : 0;
	f2 = IGD_SELF_STUDY_URL(info->v.surl.id) ? 1 : 0;
	url_list = igd_host_find_surl_id(url_info, info->v.surl.id);
	if (f1 != f2) {
		if (!url_list)
			return -1;
		igd_host_del_surl_id(url_list);
	} else {
		if (url_list)
			return 0;
		if (!url_info) {
			url_info = igd_host_new_rule(host, RLT_STUDY_URL);
			if (!url_info)
				return -1;
			INIT_LIST_HEAD(&url_info->url_list);
		}
		if (!igd_host_add_surl_id(url_info, info->v.surl.id))
			return -1;
	}

	str = malloc(10*IGD_STUDY_URL_MX);
	if (!str) {
		IGD_HOST_ERR("malloc fail, %d\n", 10*IGD_STUDY_URL_MX);
		return -1;
	}
	nr = 0;
	str[0] = 0;
	list_for_each_entry(url_list, &url_info->url_list, list) {
		nr += snprintf(str + nr, 10*IGD_STUDY_URL_MX - nr,
			"%u,", url_list->url_id);
	}
	if (!str[0]) {
		igd_host_app_save_del(host, 0, HSF_ALLOW_URL);
	} else {
		igd_host_app_save_add(host, 0, HSF_ALLOW_URL, "%s", str);
	}
	free(str);

	igd_host_study_rule(host, 1);
	return 0;
}

static int igd_host_study_time_act(
	struct host_set_info *info, int len,
	struct study_time_dump *dump, int d_len)
{
	char *str;
	int nr = 0, l = 0, change = 0;
	struct host_info_record *host;
	struct study_time_info *time_info;
	struct study_time_list *time_list, *_time_list;
	struct study_time_dump *st = &info->v.study_time;

	IGD_HOST_SET_CHECK(host, info, len);

	time_info = igd_host_find_rule(host, RLT_STUDY_TIME);
	if (info->act == NLK_ACTION_DUMP) {
		IGD_HOST_INPUT_CHECK(dump, IGD_STUDY_TIME_NUM, d_len);
		if (!time_info)
			return 0;
		list_for_each_entry_safe(time_list, _time_list, &time_info->time_list, list) {
			if (timeout_check(&time_list->time)) {
				change = 1;
				time_info->time_num--;
				igd_host_del_study_time(time_list);
				continue;
			}
			dump[nr].enable = time_list->enable;
			memcpy(&dump[nr].time, &time_list->time, sizeof(dump[nr].time));
			nr++;
			if (nr >= IGD_STUDY_TIME_NUM)
				break;
		}
		if (change) {
			IGD_HOST_DBG("is change\n");
			goto save;
		}
		return nr;
	}

	time_list = igd_host_find_study_time(time_info, &st->time);
	if (info->act == NLK_ACTION_DEL) {
		if (!time_list)
			return -1;
		igd_host_del_study_time(time_list);
		time_info->time_num--;
	} else if (info->act == NLK_ACTION_ADD) {
		if (time_list) {
			if (time_list->enable == st->enable)
				return 0;
			time_list->enable = st->enable;
		} else {
			if (!time_info) {
				time_info = igd_host_new_rule(host, RLT_STUDY_TIME);
				if (!time_info)
					return -1;
				INIT_LIST_HEAD(&time_info->time_list);
			}
			if (time_info->time_num > IGD_STUDY_TIME_NUM)
				return -2;
			if (!igd_host_add_study_time(time_info, st))
				return -1;
			time_info->time_num++;
		}
	} else {
		return -1;
	}

save:
	igd_host_study_rule(host, 1);

	str = malloc(23*IGD_STUDY_TIME_NUM);
	str[0] = 0;
	list_for_each_entry(time_list, &time_info->time_list, list) {
		l += snprintf(str + l, 23*IGD_STUDY_TIME_NUM - l,
			"%s,%d;", time_to_str(&time_list->time), time_list->enable);
	}
	if (!str[0]) {
		igd_host_app_save_del(host, 0, HSF_STUDY_TIME);
	} else {
		igd_host_app_save_add(host, 0, HSF_STUDY_TIME, "%s", str);
	}
	free(str);

	if (time_info->time_num <= 0) {
		if (list_empty(&time_info->time_list))
			igd_host_del_rule(host, RLT_STUDY_TIME);
		else
			IGD_HOST_ERR("list no empty\n");
	}
	return change ? nr : 0;
}

static int igd_host_url_act(
	struct host_set_info *info, int len,
	struct host_url_dump *dump, int d_len)
{
	char *str;
	int nr = 0;
	struct host_info_record *host;
	struct host_url_dump *url = &info->v.bw_url;
	struct host_url_info *url_info;
	struct host_url_list *url_list, *_url_list;

	IGD_HOST_SET_CHECK(host, info, len);
	
	url_info = igd_host_find_rule(host, url->type);
	if (info->act == NLK_ACTION_DUMP) {
		IGD_HOST_INPUT_CHECK(dump, IGD_HOST_URL_MX, d_len);
		if (!url_info)
			return 0;
		list_for_each_entry(url_list, &url_info->url_list, list) {
			dump[nr].type = url->type;
			arr_strcpy(dump[nr].url, url_list->url);
			nr++;
			if (nr >= IGD_HOST_URL_MX)
				break;
		}
		return nr;
	}

	url_list = igd_host_find_bw_url(url_info, url->url);
	if (info->act == NLK_ACTION_DEL) {
		if (!url_list)
			return -2;
		igd_host_del_bw_url(url_list);
		url_info->url_num--;
	} else if (info->act == NLK_ACTION_ADD) {
		if (url_list)
			return 0;
		if (!url_info) {
			url_info = igd_host_new_rule(host, url->type);
			if (!url_info)
				return -1;
			INIT_LIST_HEAD(&url_info->url_list);
		}
		if (url_info->url_num > IGD_HOST_URL_MX)
			return -2;
		if (!igd_host_add_bw_url(url_info, url->url))
			return -1;
		url_info->url_num++;
	} else if (info->act == NLK_ACTION_CLEAN) {
		if (!url_info)
			return 0;
		list_for_each_entry_safe(url_list, _url_list, &url_info->url_list, list) {
			igd_host_del_bw_url(url_list);
			url_info->url_num--;
		}
	} else {
		return -1;
	}
	igd_host_url_rule(host, url->type);

	str = malloc(IGD_URL_LEN*IGD_HOST_URL_MX);
	nr = 0;
	str[0] = 0;
	list_for_each_entry(url_list, &url_info->url_list, list) {
		nr += snprintf(str + nr, IGD_URL_LEN*IGD_HOST_URL_MX - nr,
			"%s;;", url_list->url);
	}
	if (!str[0]) {
		igd_host_app_save_del(host, 0,
			(url->type == RLT_URL_BLACK) ? HSF_URL_BLACK : HSF_URL_WHITE);
	} else {
		igd_host_app_save_add(host, 0,
			(url->type == RLT_URL_BLACK) ? HSF_URL_BLACK : HSF_URL_WHITE, "%s", str);
	}
	free(str);
	if (url_info->url_num <= 0) {
		if (list_empty(&url_info->url_list))
			igd_host_del_rule(host, url->type);
		else
			IGD_HOST_ERR("list no empty, %d\n", url->type);
	}
	return 0;
}

static int igd_host_set_app_black(struct host_set_info *info, int len)
{
	struct host_info_record *host;
	struct host_app_record *app = NULL;

	IGD_HOST_SET_CHECK(host, info, len);

	if (info->act != NLK_ACTION_CLEAN) {
		app = igd_host_find_app(host, info->appid);
		if (!app) {
			igd_host_del_app(host, 0);
			app = igd_host_add_app(host, info->appid);
			if (!app) {
				IGD_HOST_ERR("add app fail\n");
				return -1;
			}
			list_move_tail(&app->list, &host->app_list);
		}
	}
	if (info->act == NLK_ACTION_ADD) {
		igd_set_bit(HARF_INBLACK, app->flag);
		igd_host_app_save_add(host, app->appid, HSF_APP_BLACK, "1");
	} else if (info->act == NLK_ACTION_DEL) {
		igd_clear_bit(HARF_INBLACK, app->flag);
		igd_host_app_save_del(host, app->appid, HSF_APP_BLACK);
	} else if (info->act == NLK_ACTION_CLEAN) {
		list_for_each_entry(app, &host->app_list, list) {
			if (igd_test_bit(HARF_INBLACK, app->flag)) {
				igd_clear_bit(HARF_INBLACK, app->flag);
				igd_host_app_save_del(host, app->appid, HSF_APP_BLACK);
			}
		}
	} else {
		return -1;
	}
	igd_host_app_black_rule(host, 1);
	return 0;
}

static int igd_host_set_app_limit_time(struct host_set_info *info, int len)
{
	struct host_info_record *host;
	struct host_app_record *app;
	struct time_comm *t = &info->v.time;

	IGD_HOST_SET_CHECK(host, info, len);

	app = igd_host_find_app(host, info->appid);
	if (!app) {
		IGD_HOST_ERR("no find %u\n", info->appid);
		return -1;
	}

	memcpy(&app->lt, t, sizeof(app->lt));
	if (info->act == NLK_ACTION_ADD) {
		igd_set_bit(HARF_LIMIT_TIME, app->flag);
		igd_host_app_save_add(host, app->appid, HSF_APP_LT,
			"%s;", time_to_str(t));
	} else if (info->act == NLK_ACTION_DEL) {
		igd_clear_bit(HARF_LIMIT_TIME, app->flag);
		igd_host_app_save_del(host, app->appid, HSF_APP_LT);
	} else {
		return -1;
	}
	igd_host_app_black_rule(host, 1);
	return 0;
}

static int igd_host_app_mod_act(
	struct host_set_info *info, int len,
	struct app_mod_dump *dump, int d_len)
{
	char *str;
	int nr = 0, l = 0, change = 0;
	struct host_info_record *host;
	struct app_black_info *abi;
	struct app_mod_black_list *ambl, *_ambl;

	IGD_HOST_SET_CHECK(host, info, len);

	abi = igd_host_find_rule(host, RLT_APP_BLACK);
	if (info->act == NLK_ACTION_DUMP) {
		IGD_HOST_INPUT_CHECK(dump, IGD_APP_MOD_TIME_MX, d_len);
		if (!abi)
			return 0;
		list_for_each_entry_safe(ambl, _ambl, &abi->mod_list, list) {
			if (timeout_check(&ambl->time)) {
				change = 1;
				abi->num--;
				igd_host_del_app_mod(ambl);
				continue;
			}
			dump[nr].enable = ambl->enable;
			memcpy(&dump[nr].time, &ambl->time, sizeof(dump[nr].time));
			memcpy(dump[nr].mid_flag, ambl->mid_flag, sizeof(dump[nr].mid_flag));
			nr++;
		}
		if (change)
			goto save;
		return nr;
	}
	ambl = igd_host_find_app_mod(abi, &info->v.app_mod);
	if (info->act == NLK_ACTION_DEL) {
		if (!ambl)
			return -1;
		igd_host_del_app_mod(ambl);
		abi->num--;
	} else if (info->act == NLK_ACTION_ADD) {
		if (ambl) {
			if (ambl->enable == info->v.app_mod.enable &&
				!memcmp(ambl->mid_flag, info->v.app_mod.mid_flag, sizeof(ambl->mid_flag))) {
				return 0;
			}
			ambl->enable = info->v.app_mod.enable;
			memcpy(ambl->mid_flag, info->v.app_mod.mid_flag, sizeof(ambl->mid_flag));
		} else {
			if (!abi) {
				abi = igd_host_new_rule(host, RLT_APP_BLACK);
				if (!abi)
					return -1;
				INIT_LIST_HEAD(&abi->mod_list);
			}
			if (abi->num > IGD_APP_MOD_TIME_MX)
				return -2;
			if (!igd_host_add_app_mod(abi, &info->v.app_mod))
				return -1;
			abi->num++;
		}
	} else {
		return -1;
	}

save:
	str = malloc(100*IGD_APP_MOD_TIME_MX);
	str[0] = 0;
	list_for_each_entry(ambl, &abi->mod_list, list) {
		l += snprintf(str + l, 100*IGD_APP_MOD_TIME_MX - l,
			"%s,%d,%s;", time_to_str(&ambl->time),
			ambl->enable, mid_to_str(ambl->mid_flag));
	}
	if (!str[0]) {
		igd_host_app_save_del(host, 0, HSF_APP_LMT);
	} else {
		igd_host_app_save_add(host, 0, HSF_APP_LMT, "%s", str);
	}
	free(str);

	igd_host_app_black_rule(host, 1);
	return 0;
}

static void igd_host_intercept_url2dump(
	struct host_intercept_url_list *hiul, struct host_intercept_url_dump *dump)
{
	if (hiul->url)
		arr_strcpy(dump->url, hiul->url);
	if (hiul->type)
		arr_strcpy(dump->type, hiul->type);
	if (hiul->type_en)
		arr_strcpy(dump->type_en, hiul->type_en);
	dump->time = hiul->time;
	dump->dport = hiul->dport;
	dump->sport = hiul->sport;
	dump->daddr = hiul->daddr;
	dump->saddr = hiul->saddr;
}

static int igd_host_intercept_url_black_act(
	struct host_set_info *info, int len,
	struct host_intercept_url_dump *dump, int d_len)
{
	char *str;
	int nr = 0, str_len;
	struct host_info_record *host;
	struct host_intercept_url_info *hiui;
	struct host_intercept_url_list *hiul;
	struct host_intercept_url_dump *hiud = &info->v.intercept_url;

	IGD_HOST_SET_CHECK(host, info, len);

	hiui = igd_host_find_rule(host, RLT_INTERCEPT_URL);
	if (info->act == NLK_ACTION_DUMP) {
		IGD_HOST_INPUT_CHECK(dump, IGD_HOST_INTERCEPT_URL_MX, d_len);
		if (!hiui)
			return 0;
		list_for_each_entry(hiul, &hiui->url_list, list) {
			igd_host_intercept_url2dump(hiul, &dump[nr]);
			nr++;
		}
		return nr;
	}

	hiul = igd_host_find_intercept_url(hiui, hiud->url);
	if (info->act == NLK_ACTION_DEL) {
		if (!hiul)
			return -1;
		igd_host_del_intercept_url(hiul);
		hiui->url_num--;
	} else if (info->act == NLK_ACTION_ADD) {
		if (hiul)
			return 0;
		if (!hiui) {
			hiui = igd_host_new_rule(host, RLT_INTERCEPT_URL);
			if (!hiui)
				return -1;
			INIT_LIST_HEAD(&hiui->url_list);
		}
		if (hiui->url_num > IGD_HOST_INTERCEPT_URL_MX)
			return -2;
		if (!igd_host_add_intercept_url(hiui, hiud))
			return -1;
		hiui->url_num++;
	} else {
		return -1;
	}

	igd_intercept_url_rule(host);

	str_len = (IGD_URL_LEN + 200)*IGD_HOST_INTERCEPT_URL_MX;
	str = malloc(str_len);
	nr = 0;
	str[0] = 0;
	list_for_each_entry(hiul, &hiui->url_list, list) {
		nr += snprintf(str + nr, str_len - nr,
			"%s@%s@%s@%lu,%s:%d,", hiul->url, hiul->type, hiul->type_en,
			hiul->time, inet_ntoa(hiul->saddr),	hiul->sport);
		nr += snprintf(str + nr, str_len - nr,
			"%s:%d;;", inet_ntoa(hiul->daddr), hiul->dport);
	}
	if (!str[0]) {
		igd_host_app_save_del(host, 0, HSF_INTERCEPT_URL);
	} else {
		igd_host_app_save_add(host, 0, HSF_INTERCEPT_URL, "%s", str);
	}
	free(str);
	if (hiui->url_num <= 0) {
		if (list_empty(&hiui->url_list))
			igd_host_del_rule(host, RLT_INTERCEPT_URL);
		else
			IGD_HOST_ERR("list no empty\n");
	}
	return 0;
}

static int igd_host_app_mod_queue(
	struct host_set_info *info, int len,
	unsigned char *dump, int d_len)
{
	int i, l = 0;
	char str[512] = {0};
	struct host_info_record *host;
	struct app_mod_queue *amq;

	IGD_HOST_SET_CHECK(host, info, len);

	amq = igd_host_find_rule(host, RLT_APP_MOD_QUEUE);
	if (info->act == NLK_ACTION_DUMP) {
		IGD_HOST_INPUT_CHECK(dump, L7_MID_MX, d_len);
		if (!amq) {
			memset(dump, 0, d_len);
		} else {
			memcpy(dump, amq->mid, d_len);
		}
	} else if (info->act == NLK_ACTION_DEL) {
		if (!amq)
			return 0;
		if (igd_host_filter_app_mod_queue_del(amq))
			return -1;
		igd_host_del_rule(host, RLT_APP_MOD_QUEUE);
		igd_host_app_save_del(host, 0, HSF_APP_QUEUE);
	} else if (info->act == NLK_ACTION_ADD) {
		if (amq) {
			if (igd_host_filter_app_mod_queue_del(amq))
				return -1;
		} else {
			amq = igd_host_new_rule(host, RLT_APP_MOD_QUEUE);
			if (!amq)
				return -2;
		}
		memcpy(amq->mid, info->v.mid, sizeof(amq->mid));
		if (igd_host_filter_app_mod_queue_add(host->mac, amq))
			return -1;

		for (i = 0; i < L7_MID_MX; i++) {
			if (!amq->mid[i])
				continue;
			l += snprintf(str + l, sizeof(str) - l, "%d,", amq->mid[i]);
		}

		if (str[0]) {
			igd_host_app_save_add(host, 0, HSF_APP_QUEUE, str);
		} else {
			igd_host_del_rule(host, RLT_APP_MOD_QUEUE);
			igd_host_app_save_del(host, 0, HSF_APP_QUEUE);
		}
	} else {
		IGD_HOST_ERR("%d\n", info->act);
		return -1;
	}
	return 0;
}

static int igd_app_mod_queue(
	struct host_set_info *info, int len,
	unsigned char *dump, int d_len)
{
	int i, l = 0;
	char str[512] = {0};
	struct igd_host_global_info *ihgi = &igd_host_global;

	IGD_HOST_INPUT_CHECK(info, 1, len);
	if (info->act == NLK_ACTION_DUMP) {
		IGD_HOST_INPUT_CHECK(dump, L7_MID_MX, d_len);
		memcpy(dump, ihgi->amq.mid, d_len);
		return 0;
	}

	if (igd_host_filter_app_mod_queue_del(&ihgi->amq))
		return -1;
	if (info->act == NLK_ACTION_DEL) {
		memset(&ihgi->amq, 0, sizeof(ihgi->amq));
	} else if (info->act == NLK_ACTION_ADD) {
		memcpy(&ihgi->amq.mid, info->v.mid, sizeof(ihgi->amq.mid));
		if (igd_host_filter_app_mod_queue_add(NULL, &ihgi->amq))
			return -1;
		for (i = 0; i < L7_MID_MX; i++) {
			if (!ihgi->amq.mid[i])
				continue;
			l += snprintf(str + l, sizeof(str) - l, "%d,", ihgi->amq.mid[i]);
		}
	} else {
		IGD_HOST_ERR("%d\n", info->act);
		return -1;
	}
	if (str[0]) {
		igd_global_save_add(HSF_APP_QUEUE, str);
	} else {
		igd_global_save_del(HSF_APP_QUEUE);
	}
	return 0;
}

static int igd_host_set_app_white(struct host_set_info *info, int len)
{
	struct host_info_record *host;
	struct host_app_record *app = NULL;

	IGD_HOST_SET_CHECK(host, info, len);

	if (info->act != NLK_ACTION_CLEAN) {
		app = igd_host_find_app(host, info->appid);
		if (!app) {
			igd_host_del_app(host, 0);
			app = igd_host_add_app(host, info->appid);
			if (!app) {
				IGD_HOST_ERR("add app fail\n");
				return -1;
			}
			list_move_tail(&app->list, &host->app_list);
		}
	}
	if (info->act == NLK_ACTION_ADD) {
		igd_set_bit(HARF_INWHITE, app->flag);
		igd_host_app_save_add(host, app->appid, HSF_APP_WHITE, "1");
	} else if (info->act == NLK_ACTION_DEL) {
		igd_clear_bit(HARF_INWHITE, app->flag);
		igd_host_app_save_del(host, app->appid, HSF_APP_WHITE);
	} else if (info->act == NLK_ACTION_CLEAN) {
		list_for_each_entry(app, &host->app_list, list) {
			if (igd_test_bit(HARF_INWHITE, app->flag)) {
				igd_clear_bit(HARF_INWHITE, app->flag);
				igd_host_app_save_del(host, app->appid, HSF_APP_WHITE);
			}
		}
	} else {
		return -1;
	}
	igd_host_app_white_rule(host, 1);
	return 0;
}

static int igd_host_vap_allow(struct host_set_info *info, int len)
{
	struct host_info_record *host;

	IGD_HOST_SET_CHECK(host, info, len);

	if (info->act == NLK_ACTION_ADD) {
		igd_set_bit(HIRF_VAP_ALLOWED, host->flag);
	} else {
		igd_clear_bit(HIRF_VAP_ALLOWED, host->flag);
	}
	return 0;
}

static int igd_host_set_ali_black(struct host_set_info *info, int len)
{
	int ret, i = 0;
	struct host_info_record *host;
	static int ali_black_id[IGD_HOST_MX] = {0,};
	static unsigned char mac[IGD_HOST_MX][7] = {{0,},};

	IGD_HOST_SET_CHECK(host, info, len);

	for (i = 0; i < IGD_HOST_MX; i++) {
		if (mac[i][6] == 0)
			continue;
		if (!memcmp(mac[i], host->mac, 6))
			break;
	}
	if (i >= IGD_HOST_MX) {
		for (i = 0; i < IGD_HOST_MX; i++) {
			if (mac[i][6] == 0) {
				mac[i][6] = 1;
				memcpy(mac[i], host->mac, 6);
				break;
			}
		}
		if (i >= IGD_HOST_MX)
			return -1;
	}

	IGD_HOST_DBG("%d,%d, %s\n",
		info->act, ali_black_id[i], mac_to_str(host->mac));
	if ((info->act == NLK_ACTION_ADD) && !ali_black_id[i]) {
		ret = igd_host_filter_black_add(host->mac);
		if (ret < 0) {
			IGD_HOST_ERR("add fail, %d\n", ret);
			return -1;
		}
		ali_black_id[i] = ret;
	} else if ((info->act == NLK_ACTION_DEL) && ali_black_id[i]) {
		ret = igd_host_filter_black_del(ali_black_id[i]);
		if (ret < 0) {
			IGD_HOST_ERR("del fail, %d\n", ret);
			return -1;
		}
		mac[i][6] = 0;
		ali_black_id[i] = 0;
	}
	return 0;
}

static int igd_host_set_filter(
	struct host_filter_info *info, int len,
	struct host_filter_info *dump, int d_len)
{
	int i = 0;
	struct host_filter_list *hfl = NULL;
	char str[4096];

	IGD_HOST_INPUT_CHECK(info, 1, len);

	if (info->act == NLK_ACTION_DUMP) {
		IGD_HOST_INPUT_CHECK(dump, IGD_HOST_FILTER_MX, d_len);
		list_for_each_entry(hfl, &igd_host_filter_list, list) {
			dump[i].enable = hfl->enable;
			dump[i].time = hfl->time;
			dump[i].host = hfl->host;
			dump[i].arg = hfl->arg;
			i++;
			if (i >= IGD_HOST_FILTER_MX)
				break;
		}
		return i;
	} else if (info->act == NLK_ACTION_DEL) {
		hfl = igd_host_find_filter(info);
		if (!hfl)
			return -2;
		if ((hfl->rule_id > 0) &&
			(unregister_net_rule(hfl->rule_id) < 0)) {
			IGD_HOST_ERR("del filter fail, %d\n", hfl->rule_id);
			return -1;
		}
		igd_host_filter_num--;
		list_del(&hfl->list);
		snprintf(str, sizeof(str), "%s.%s.%s=%hhd,%hhd,%hhd,%hhd:%hhd,%hhd:%hhd;"
			"%hd,%d,%X,%X,%02X:%02X:%02X:%02X:%02X:%02X,%d,%hd-%hd,%hd-%hd,%hd",
			IGD_HOST_SAVE_FILE, IGD_HOST_GLOBAL_INFO, IGD_HOST_FILTER,
			hfl->enable, hfl->time.loop, hfl->time.day_flags, hfl->time.start_hour,
			hfl->time.start_min, hfl->time.end_hour, hfl->time.end_min,
			hfl->arg.target, hfl->host.type, hfl->host.addr.start,
			hfl->host.addr.end, MAC_SPLIT(hfl->host.mac), hfl->arg.l4.type,
			hfl->arg.l4.src.start, hfl->arg.l4.src.end, hfl->arg.l4.dst.start,
			hfl->arg.l4.dst.end, hfl->arg.l4.protocol);
		uuci_del_list(str);
		free(hfl);
		return 0;
	} else if (info->act == NLK_ACTION_ADD) {
		if (igd_host_filter_num >= IGD_HOST_FILTER_MX)
			return -3;
		hfl = igd_host_find_filter(info);
		if (hfl)
			return -2;
		hfl = malloc(sizeof(*hfl));
		if (!hfl)
			return -1;
		memset(hfl, 0, sizeof(*hfl));
		hfl->enable = info->enable;
		hfl->time = info->time;
		hfl->host = info->host;
		hfl->arg = info->arg;
		list_add(&hfl->list, &igd_host_filter_list);
		igd_host_filter_num++;

		snprintf(str, sizeof(str), "%s.%s.%s=%hhd,%hhd,%hhd,%hhd:%hhd,%hhd:%hhd;"
			"%hd,%d,%X,%X,%02X:%02X:%02X:%02X:%02X:%02X,%hd,%hd-%hd,%hd-%hd,%hd",
			IGD_HOST_SAVE_FILE, IGD_HOST_GLOBAL_INFO, IGD_HOST_FILTER,
			hfl->enable, hfl->time.loop, hfl->time.day_flags, hfl->time.start_hour,
			hfl->time.start_min, hfl->time.end_hour, hfl->time.end_min,
			hfl->arg.target, hfl->host.type, hfl->host.addr.start,
			hfl->host.addr.end, MAC_SPLIT(hfl->host.mac), hfl->arg.l4.type,
			hfl->arg.l4.src.start, hfl->arg.l4.src.end, hfl->arg.l4.dst.start,
			hfl->arg.l4.dst.end, hfl->arg.l4.protocol);
		uuci_add_list(str);
		return 0;
	}
	return -1;
}

#define HOST_CALLBACK_FUNCTION "-------------above is callback function-----------------------"

static int igd_host_day_change(void)
{
	int i, len;
	char *dump;
	struct host_info_record *host;
	struct time_record_info time_info;
	struct host_app_record *app;
	struct host_set_info set_info;

	IGD_HOST_DBG("--------new day\n");

	list_for_each_entry(host, &igd_host_list, list) {
		memset(&host->bytes, 0, sizeof(host->bytes));
		time_info = host->time;
		if (igd_test_bit(HIRF_ONLINE, host->flag))
			add_oltime(&time_info);
		host->time.old = time_info.all;
		for (i = 0; i < L7_MID_MX; i++) {
			if (host->mid[i].up_time)
				host->mid[i].up_time = sys_uptime();
			host->mid[i].all_time = 0;
			host->mid[i].clock_last = 0;
			host->mid[i].upload_time = 0;
		}
		list_for_each_entry(app, &host->app_list, list) {
			memset(&app->bytes, 0, sizeof(app->bytes));
			time_info = app->time;
			if (igd_test_bit(HARF_ONLINE, app->flag))
				add_oltime(&time_info);
			app->time.old = time_info.all;
		}

		memcpy(set_info.mac, host->mac, 6);
		set_info.act = NLK_ACTION_DUMP;

		len = sizeof(struct app_mod_dump) * IGD_APP_MOD_TIME_MX;
		dump = malloc(len);
		if (!dump) {
			IGD_HOST_ERR("malloc fail, %d\n", len);
			continue;
		}
		igd_host_app_mod_act(&set_info,
			sizeof(set_info), (struct app_mod_dump *)dump, len);
		free(dump);

		len = sizeof(struct study_time_dump) * IGD_STUDY_TIME_NUM;
		dump = malloc(len);
		if (!dump) {
			IGD_HOST_ERR("malloc fail, %d\n", len);
			continue;
		}
		igd_host_study_time_act(&set_info,
			sizeof(set_info), (struct study_time_dump *)dump, len);
		free(dump);
	}
	return 0;
}

static void igd_host_mid_timer(struct host_info_record *host)
{
	int i, j, len;
	unsigned long time;
	struct host_app_mod_info *clk;
	unsigned char buf[128] = {0,};
	char *name;

	for (i = 0; i < L7_MID_MX; i++) {
		clk = &host->mid[i];
		if (!clk->clock_gap)
			continue;
		time = clk->all_time;
		if (clk->up_time)
			time += sys_uptime() - clk->up_time;
		if (clk->clock_gap + clk->clock_last > time)
			continue;
		time = time/60;
		clk->clock_last = time*60;

		CC_PUSH2(buf, 2, CSO_NTF_GAME_TIME);
		j = 8;
		CC_PUSH1(buf, j, i);
		j += 1;
		CC_PUSH2(buf, j, time);
		j += 2;
		CC_PUSH1(buf, j, 12);
		j += 1;
		CC_PUSH_LEN(buf, j, mac_to_str(host->mac), 12);
		j += 12;
		name = host->base.nick[0] ? host->base.nick : \
			get_vender_name(host->base.vender, host->base.name);
		len = strlen(name);
		CC_PUSH1(buf, j, len);
		j += 1;
		if (len > 0) {
			CC_PUSH_LEN(buf, j, name, len);
			j += len;
		}
		CC_PUSH2(buf, 0, j);
		CC_CALL_ADD(buf, j);
	}
}

static void igd_host_flow_timer(struct host_info_record *host, struct tm *nt)
{
	long time;
	int mid, i, nr = 0, j;
	unsigned char *buf;
	struct host_app_record *app;
	int64_t up_bytes, down_bytes;
	int64_t mid_up_bytes[L7_MID_MX], mid_down_bytes[L7_MID_MX];
	struct host_app_dump_info app_dump;
	struct time_record_info time_info;
	struct host_dump_info host_dump;

	buf = malloc((IGD_HOST_APP_MX + L7_MID_MX + 1)*16 + 32);
	if (!buf) {
		IGD_HOST_ERR("malloc fail, %d\n", (IGD_HOST_APP_MX + L7_MID_MX + 1)*12 + 20);
		return;
	}

	memset(mid_up_bytes, 0, sizeof(mid_up_bytes));
	memset(mid_down_bytes, 0, sizeof(mid_down_bytes));

	CC_PUSH2(buf, 2, CSO_NTF_ROUTER_UPDOWN);
	j = 8;
	CC_PUSH1(buf, j, 12);
	j += 1;
	CC_PUSH_LEN(buf, j, mac_to_str(host->mac), 12);
	j += 12;
	CC_PUSH1(buf, j, 1); // reboot flag
	j += 1;
	//CC_PUSH1(buf, j, 0); // num
	j += 1;

	list_for_each_entry(app, &host->app_list, list) {
		time_info = app->time;
		if (igd_test_bit(HARF_ONLINE, app->flag))
			add_oltime(&time_info);
		if (time_info.all >= time_info.old) {
			time = (time_info.all - time_info.old)/60;
		} else {
			time = 0;
			IGD_HOST_DBG("ERR,APP:%u, time:%lu,%lu\n",
				app->appid, time_info.all, time_info.old);
		}
		memset(&app_dump, 0, sizeof(app_dump));
		igd_host_app2dump(host, app, &app_dump, 0);
		if (app_dump.up_bytes >= app->bytes.old_up) {
			up_bytes = (app_dump.up_bytes - app->bytes.old_up)/1024;
		} else {
			up_bytes = 0;
			IGD_HOST_DBG("ERR,APP:%u, up_bytes:%llu,%llu\n",
				app->appid, app_dump.up_bytes, app->bytes.old_up);
		}
		if (app_dump.down_bytes >= app->bytes.old_down) {
			down_bytes = (app_dump.down_bytes - app->bytes.old_down)/1024;
		} else {
			down_bytes = 0;
			IGD_HOST_DBG("ERR,APP:%u, down_bytes:%llu,%llu\n",
				app->appid, app_dump.down_bytes, app->bytes.old_down);
		}

		if (!time && !up_bytes && !down_bytes)
			continue;

		app->time.old += (time*60);
		app->bytes.old_up += (up_bytes*1024);
		app->bytes.old_down += (down_bytes*1024);

		CC_PUSH2(buf, j, L7_GET_MID(app->appid));
		j += 2;
		CC_PUSH4(buf, j, app->appid);
		j += 4;
		CC_PUSH2(buf, j, (unsigned short)((time > 60) ? 60 : time));
		j += 2;
		CC_PUSH4(buf, j, (unsigned int)down_bytes);
		j += 4;
		CC_PUSH4(buf, j, (unsigned int)up_bytes);
		j += 4;
		nr++;
		IGD_HOST_DBG("APP:%d,%ld(min),%lld(KB),%lld(KB)\n",
			app->appid, time, down_bytes, up_bytes);
		mid = L7_GET_MID(app->appid);
		if ((mid < 0) || (mid >= L7_MID_MX)) {
			IGD_HOST_ERR("mid:%d, appid:%u, err\n", mid, app->appid);
			continue;
		}
		mid_up_bytes[mid] += up_bytes;
		mid_down_bytes[mid] += down_bytes;
	}

	for (i = 0; i < L7_MID_MX; i++) {
		time = host->mid[i].all_time;
		if (host->mid[i].up_time)
			time += (sys_uptime() - host->mid[i].up_time);
		if (time >= host->mid[i].upload_time) {
			time = (time - host->mid[i].upload_time)/60;
		} else {
			time = 0;
			IGD_HOST_DBG("ERR,MID:%d, time:%ld,%lu\n",
				i, time, host->mid[i].upload_time);
		}
		if (!time && !mid_up_bytes[i] && !mid_down_bytes[i])
			continue;

		host->mid[i].upload_time += (time*60);
		CC_PUSH2(buf, j, i);
		j += 2;
		CC_PUSH4(buf, j, 0);
		j += 4;
		CC_PUSH2(buf, j, (unsigned short)((time > 60) ? 60 : time));
		j += 2;
		CC_PUSH4(buf, j, (unsigned int)mid_down_bytes[i]);
		j += 4;
		CC_PUSH4(buf, j, (unsigned int)mid_up_bytes[i]);
		j += 4;
		nr++;
		IGD_HOST_DBG("MID:%d,%ld(min),%lld(KB),%lld(KB)\n",
			i, time, mid_down_bytes[i], mid_up_bytes[i]);
	}
	time_info = host->time;
	if (igd_test_bit(HIRF_ONLINE, host->flag))
		add_oltime(&time_info);
	if (time_info.all >= time_info.old) {
		time = (time_info.all - time_info.old)/60;
	} else {
		time = 0;
		IGD_HOST_DBG("ERR,HOST:%s, time:%lu,%lu\n",
			mac_to_str(host->mac), time_info.all, time_info.old);
	}
	memset(&host_dump, 0, sizeof(host_dump));
	igd_host2dump(host, &host_dump, 0);
	if (host_dump.up_bytes >= host->bytes.old_up) {
		up_bytes = (host_dump.up_bytes - host->bytes.old_up)/1024;
	} else {
		up_bytes = 0;
		IGD_HOST_DBG("ERR,HOST:%s, up_bytes:%llu,%llu\n",
			mac_to_str(host->mac), host_dump.up_bytes, host->bytes.old_up);
	}
	if (host_dump.down_bytes >= host->bytes.old_down) {
		down_bytes = (host_dump.down_bytes - host->bytes.old_down)/1024;
	} else {
		down_bytes = 0;
		IGD_HOST_DBG("ERR,HOST:%s, up_bytes:%llu,%llu\n",
			mac_to_str(host->mac), host_dump.down_bytes, host->bytes.old_down);
	}

	if (time || up_bytes || down_bytes) {
		host->time.old += (time*60);
		host->bytes.old_up += (up_bytes*1024);
		host->bytes.old_down += (down_bytes*1024);
		CC_PUSH2(buf, j, 0);
		j += 2;
		CC_PUSH4(buf, j, 0);
		j += 4;
		CC_PUSH2(buf, j, (unsigned short)((time > 60) ? 60 : time));
		j += 2;
		CC_PUSH4(buf, j, (unsigned int)down_bytes);
		j += 4;
		CC_PUSH4(buf, j, (unsigned int)up_bytes);
		j += 4;
		nr++;
		IGD_HOST_DBG("HOST:%s,%ld(min),%lld(KB),%lld(KB)\n",
			mac_to_str(host->mac), time, down_bytes, up_bytes);
	}
	if (nr > 253) {
		IGD_HOST_ERR("nr err, %d\n", nr);
	} else if (nr > 0) {
		CC_PUSH1(buf, 22, nr);
		CC_PUSH2(buf, 0, j);
		CC_CALL_ADD(buf, j);
		IGD_HOST_DBG("%s,%d-----SEND-----%d:%d\n",
			mac_to_str(host->mac), nr,
			nt ? nt->tm_hour : -1,
			nt ? nt->tm_min : -1);
	}
	free(buf);
}

static void igd_host_ua_timer(void)
{
	char *buf;
	int len;
	struct host_ua_record *hur, *_hur;

	buf = malloc(IGD_HOST_UA_MX*128 + 10);
	if (!buf) {
		IGD_HOST_ERR("malloc fail, %d\n", IGD_HOST_UA_MX*128);
		return;
	}
	CC_PUSH2(buf, 2, CSO_NTF_ROUTER_MMODEL);
	len = 8;
	len += 2; // ua len
	list_for_each_entry_safe(hur, _hur, &igd_host_ua_list, list) {
		len += snprintf(buf + len, IGD_HOST_UA_MX*128 + 10 - len,
			"%s\n", hur->ua);
		list_del(&hur->list);
		free(hur->ua);
		free(hur);
		igd_host_ua_num--;
	}
	if (len > 10) {
		IGD_HOST_DBG("ua upload, %d\n", len);
		CC_PUSH2(buf, 8, len - 10);
		CC_PUSH2(buf, 0, len);
		CC_CALL_ADD(buf, len);
	}
	free(buf);
}

static void igd_host_filter_timer(void)
{
	struct tm *t = get_tm();
	struct host_filter_list *hfl = NULL;

	if (!t)
		return;

	list_for_each_entry(hfl, &igd_host_filter_list, list) {
		if (!hfl->enable) {
			if (hfl->rule_id > 0) {
				if (unregister_net_rule(hfl->rule_id) < 0)
					IGD_HOST_ERR("del filter fail, %d\n", hfl->rule_id);
				else
					hfl->rule_id = -1;
			}
			continue;
		}
		if (!igd_time_cmp(t, &hfl->time))
			continue;
		if (hfl->rule_id > 0)
			continue;
		hfl->rule_id = register_net_rule(&hfl->host, &hfl->arg);
		if (hfl->rule_id < 0)
			IGD_HOST_ERR("add filter fail, %d\n", hfl->rule_id);
	}
}

static long timer_1h = 1;
static void igd_host_timer(void *data)
{
	int bit[2];
	struct timeval tv;
	struct host_info_record *host;
	long sys_time = sys_uptime();
	struct tm *net_time = get_tm();
	unsigned char buf[10];

	if (!TEST_HOST_FLAG(HF_HISTORY)) {
		bit[0] = ICFT_ONLINE;
		bit[1] = NLK_ACTION_DUMP;
		if (mu_call(IGD_CLOUD_FLAG, bit, sizeof(bit), NULL, 0) == 1) {
			CC_PUSH2(buf, 0, 8);
			CC_PUSH2(buf, 2, CSO_REQ_ROUTER_RECORD);
			CC_CALL_ADD(buf, 8);
		}
	}

	if (!TEST_HOST_FLAG(HF_STUDY_URL)) {
		if (uuci_set(IGD_HOST_SAVE_FILE"."IGD_HOST_STUDY_URL"=url"))
			IGD_HOST_ERR("%s fail\n", IGD_HOST_STUDY_URL);
		else
			SET_HOST_FLAG(HF_STUDY_URL);
	}

	if (!TEST_HOST_FLAG(HF_GLOBAL_INFO)) {
		if (uuci_set(IGD_HOST_SAVE_FILE"."IGD_HOST_GLOBAL_INFO"=rule"))
			IGD_HOST_ERR("%s fail\n", IGD_HOST_GLOBAL_INFO);
		else
			SET_HOST_FLAG(HF_GLOBAL_INFO);
	}

	day_change();
	if (!timer_1h) {
		timer_1h = sys_time;
		igd_host_ua_timer();

		CC_PUSH2(buf, 0, 8);
		CC_PUSH2(buf, 2, CSO_NTF_ROUTER_ONLINE);
		CC_CALL_ADD(buf, 8);
	}

	list_for_each_entry(host, &igd_host_list, list) {
		if (sys_time > (60*60 + timer_1h)) {
			timer_1h = 0;
			igd_host_log_upload(host);
			igd_host_flow_timer(host, net_time);
		}

		if (igd_test_bit(HIRF_INBLACK, host->flag))
			continue;
		igd_host_mid_timer(host);
		igd_host_study_rule(host, 0);
		igd_host_app_black_rule(host, 0);
	}

	igd_host_filter_timer();

	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if (!schedule(tv, igd_host_timer, NULL)) {
		IGD_HOST_ERR("schedule err\n");
	}
}

static int igd_load_study_url(struct uci_section *s)
{
	struct uci_element *oe = NULL;
	struct uci_option *o = NULL;
	int i;
	char *name, *url;
	unsigned int id;

	SET_HOST_FLAG(HF_STUDY_URL);

	uci_foreach_element(&s->options, oe) {
		o = uci_to_option(oe);
		IGD_HOST_DBG("\t op:%s:%s\n", oe->name, o->v.string);
		id = (unsigned int)atoll(oe->name);
		name = o->v.string;
		url = strchr(o->v.string, ',');
		*url++ = '\0';
		igd_add_study_url(id, name, url, NULL);
		if (!IGD_SELF_STUDY_URL(id))
			continue;
		i = id - IGD_STUDY_URL_SELF_ID_MIN;
		if ((i < 0) || (i >= IGD_STUDY_URL_SELF_NUM))
			IGD_HOST_ERR("id err, %u\n", id);
		else
			igd_set_bit(i, igd_study_url_flag);
	}
	return 0;
}

static int igd_load_app_queue(
	struct host_info_record *host,
	char *str, struct app_mod_queue *in_amq)
{
	int i = 0;
	char *ptr = str;
	struct app_mod_queue *amq = in_amq;

	if (!amq) {
		amq = igd_host_new_rule(host, RLT_APP_MOD_QUEUE);
		if (!amq)
			return -1;
	}

	while (ptr) {
		amq->mid[i] = atoi(ptr);
		if (amq->mid[i] < L7_MID_MX)
			i++;
		else
			IGD_HOST_ERR("%s\n", str);
		ptr = strchr(ptr, ',');
		if (ptr)
			ptr++;
	}
	return 0;
}

static int igd_load_host_filter(struct uci_option *o)
{
	struct uci_element *e = NULL;
	struct host_filter_list *hfl = NULL;

	uci_foreach_element(&o->v.list, e) {
		hfl = malloc(sizeof(*hfl));
		if (!hfl) {
			IGD_HOST_ERR("malloc fail\n");
			return -1;
		}
		memset(hfl, 0, sizeof(*hfl));
		if (23 != sscanf(e->name, "%hhd,%hhd,%hhd,%hhd:%hhd,%hhd:%hhd;"
			"%hd,%d,%X,%X,%hhX:%hhX:%hhX:%hhX:%hhX:%hhX,%hd,%hd-%hd,%hd-%hd,%hd",
			&hfl->enable, &hfl->time.loop, &hfl->time.day_flags, &hfl->time.start_hour,
			&hfl->time.start_min, &hfl->time.end_hour, &hfl->time.end_min,
			&hfl->arg.target, &hfl->host.type, &hfl->host.addr.start,
			&hfl->host.addr.end, MAC_SPLIT(&hfl->host.mac), &hfl->arg.l4.type,
			&hfl->arg.l4.src.start, &hfl->arg.l4.src.end, &hfl->arg.l4.dst.start,
			&hfl->arg.l4.dst.end, &hfl->arg.l4.protocol)) {
			IGD_HOST_ERR("load fail:[%s]\n", e->name);
			free(hfl);
			return -1;
		}
		list_add(&hfl->list, &igd_host_filter_list);
		igd_host_filter_num++;
	}
	return 0;
}

static int igd_load_global_info(struct uci_section *s)
{
	struct uci_element *oe = NULL;
	struct uci_option *o = NULL;
	struct igd_host_global_info *ihgi = &igd_host_global;

	SET_HOST_FLAG(HF_GLOBAL_INFO);

	uci_foreach_element(&s->options, oe) {
		o = uci_to_option(oe);
		IGD_HOST_DBG("\t op:%s:%s\n", oe->name, o->v.string);
		if (!strcmp(oe->name, HGSF_NEW_PUSH)) {
			ihgi->new_push = atoi(o->v.string);
		} else if (!strcmp(oe->name, HGSF_KING)) {
			str_to_mac(o->v.string, ihgi->king_mac);
		} else if (!strcmp(oe->name, HSF_APP_QUEUE)) {
			igd_load_app_queue(NULL, o->v.string, &ihgi->amq);
		} else if (!strcmp(oe->name, IGD_HOST_FILTER)) {
			igd_load_host_filter(o);
		} else {
			IGD_HOST_ERR("host don't support:%s=%s\n", oe->name, o->v.string);
		}
	}
	return 0;
}

static int igd_load_host_ls(
	struct host_info_record *host, char *str)
{
	char *ptr;
	struct limit_speed_info *ls;

	ls = igd_host_new_rule(host, RLT_SPEED);
	if (!ls)
		return -1;
	ls->down = (unsigned long)atoll(str);
	ptr = strchr(str, ',');
	if (ptr)
		ls->up = (unsigned long)atoll(ptr + 1);
	ls->rule_id = igd_host_filter_speed_add(
		host->mac, ls->up, ls->down);
	if (ls->rule_id < 0) {
		IGD_HOST_ERR("add ls fail, %d\n", ls->rule_id);
		igd_host_del_rule(host, RLT_SPEED);
		return -1;
	}
	igd_set_bit(HIRF_LIMIT_SPEED, host->flag);
	return 0;
}

static int igd_load_host_black(
	struct host_info_record *host, char *str)
{
	int *pint;

	pint = igd_host_new_rule(host, RLT_BLACK);
	if (!pint)
		return -1;
	*pint = igd_host_filter_black_add(host->mac);
	if (*pint < 0) {
		IGD_HOST_ERR("add black fail, %d\n", *pint);
		igd_host_del_rule(host, RLT_BLACK);
		return -1;
	}
	igd_set_bit(HIRF_INBLACK, host->flag);
	igd_host_study_rule(host, 1);
	return 0;
}

static int igd_load_host_allow_url(
	struct host_info_record *host, char *str)
{
	int id;
	char *ptr;
	struct allow_study_url_info *url_info;
	struct allow_study_url_list *url_list;

	ptr = str;
	url_info = igd_host_new_rule(host, RLT_STUDY_URL);
	if (!url_info)
		return -1;
	INIT_LIST_HEAD(&url_info->url_list);

	while (1) {
		id = ptr[0] ? atoi(ptr) : 0;
		if (!id)
			break;
		if (IGD_TEST_STUDY_URL(id)) {
			url_list = igd_host_find_surl_id(url_info, id);
			if (!url_list)
				url_list = igd_host_add_surl_id(url_info, id);
		}
		ptr = strchr(ptr, ',');
		if (!ptr)
			break;
		ptr++;
	}
	return 0;
}

static int igd_load_host_study_time(
	struct host_info_record *host, char *str)
{
	char *ptr = str;
	struct study_time_dump st;
	struct study_time_info *time_info;
	struct study_time_list *time_list;

	time_info = igd_host_new_rule(host, RLT_STUDY_TIME);
	if (!time_info)
		return -1;
	INIT_LIST_HEAD(&time_info->time_list);

	while (1) {
		if (sscanf(ptr, "%hhd:%hhd,%hhd:%hhd,%hhd,%hhd,%hhd;",
					&st.time.start_hour, &st.time.start_min, &st.time.end_hour,
					&st.time.end_min, &st.time.day_flags, &st.time.loop, &st.enable) != 7) {
			IGD_HOST_ERR("str err, %s\n", str);
		} else if ((st.time.start_hour > 23) || (st.time.start_min > 60)
			|| (st.time.end_hour > 23) || (st.time.end_hour > 60)
			|| (st.time.day_flags > 127)) {
			IGD_HOST_ERR("data err, %s;\n", time_to_str(&st.time));
		} else {
			time_list = igd_host_find_study_time(time_info, &st.time);
			if (!time_list) {
				time_list = igd_host_add_study_time(time_info, &st);
				if (time_list)
					time_info->time_num++;
			}
		}
		ptr = strchr(ptr, ';');
		if (!ptr || !ptr[1])
			break;
		ptr++;
	}
	return 0;
}

static int igd_load_host_url_black_white(
	struct host_info_record *host, unsigned char type, char *str)
{
	char *s = str, *n;
	struct host_url_info *url_info;
	struct host_url_list *url_list;

	url_info = igd_host_new_rule(host, type);
	if (!url_info)
		return -1;
	INIT_LIST_HEAD(&url_info->url_list);

	while (1) {
		n = strstr(s, ";;");
		if (n)
			*n = '\0';
		url_list = igd_host_find_bw_url(url_info, s);
		if (!url_list) {
			url_list = igd_host_add_bw_url(url_info, s);
			if (url_list)
				url_info->url_num++;
		}
		if (!n)
			break;
		s = n + 2;
		if (*s == '\0')
			break;
	}
	igd_host_url_rule(host, type);
	return 0;
}

static int igd_load_host_app_limit_time(
	struct host_app_record *app, char *str)
{
	struct time_comm t;

	if (sscanf(str, "%hhd:%hhd,%hhd:%hhd,%hhd,%hhd;",
				&t.start_hour, &t.start_min, &t.end_hour, &t.end_min,
				&t.day_flags, &t.loop) != 6) {
		IGD_HOST_ERR("str err, %s\n", str);
	} else if ((t.start_hour > 23) || (t.start_min > 60)
		|| (t.end_hour > 23) || (t.end_hour > 60)
		|| (t.day_flags > 127)) {
		IGD_HOST_ERR("data err, %s;\n", time_to_str(&t));
	} else {
		memcpy(&app->lt, &t, sizeof(t));
		app->lt.loop = 0;
		igd_set_bit(HARF_LIMIT_TIME, app->flag);
		return 0;
	}
	return -1;
}

static int igd_load_host_app_mod(
	struct host_info_record *host, char *str)
{
	char *ptr = str, mid[128];
	struct app_mod_dump mod;
	struct app_black_info *abi;
	struct app_mod_black_list *ambl;
	struct time_comm *t = &mod.time;
	int i;

	abi = igd_host_new_rule(host, RLT_APP_BLACK);
	if (!abi)
		return -1;
	INIT_LIST_HEAD(&abi->mod_list);

	while (1) {
		memset(mid, 0, sizeof(mid));
		memset(&mod, 0, sizeof(mod));
		if (sscanf(ptr, "%hhd:%hhd,%hhd:%hhd,%hhd,%hhd,%hhd,%32[^;];",
					&t->start_hour, &t->start_min, &t->end_hour, &t->end_min,
					&t->day_flags, &t->loop, &mod.enable, mid) != 8) {
			IGD_HOST_ERR("str err, %s\n", str);
		} else if ((t->start_hour > 23) || (t->start_min > 60)
			|| (t->end_hour > 23) || (t->end_hour > 60)
			|| (t->day_flags > 127) || !mid[0]) {
			IGD_HOST_ERR("data err, %s,%s;\n", time_to_str(t), mid);
		} else {
			for (i = 0; i < L7_MID_MX; i++) {
				if (mid[i] == '1')
					igd_set_bit(i, mod.mid_flag);
			}
			ambl = igd_host_find_app_mod(abi, &mod);
			if (!ambl) {
				ambl = igd_host_add_app_mod(abi, &mod);
				if (ambl)
					abi->num++;
			}
		}
		ptr = strchr(ptr, ';');
		if (!ptr || !ptr[1])
			break;
		ptr++;
	}
	return 0;
}

static int igd_load_host_intercept_url(
	struct host_info_record *host, char *str)
{
	struct host_intercept_url_info *hiui;
	struct host_intercept_url_dump hiud;
	char *ptr = str;
	char url[128], saddr[32], daddr[32];

	hiui = igd_host_new_rule(host, RLT_INTERCEPT_URL);
	if (!hiui)
		return -1;
	INIT_LIST_HEAD(&hiui->url_list);

	while (1) {
		memset(&hiud, 0, sizeof(hiud));
		memset(&url, 0, sizeof(url));
		memset(&saddr, 0, sizeof(saddr));
		memset(&daddr, 0, sizeof(daddr));
		if (8 != sscanf(ptr, "%127[^@]@%31[^@]@%31[^@]@%lu,%31[^:]:%hd,%31[^:]:%hd;;",
			url, hiud.type, hiud.type_en, &hiud.time, saddr, &hiud.sport, daddr, &hiud.dport)) {
			memset(&hiud, 0, sizeof(hiud));
			memset(&url, 0, sizeof(url));
			memset(&saddr, 0, sizeof(saddr));
			memset(&daddr, 0, sizeof(daddr));
		}
		if (!url[0] && (7 != sscanf(ptr, "%127[^@]@%31[^@]@%lu,%31[^:]:%hd,%31[^:]:%hd;;",
				url, hiud.type, &hiud.time, saddr, &hiud.sport, daddr, &hiud.dport))) {
			url[0] = 0;
			IGD_HOST_ERR("str err, %s\n", str);
		}
		if (url[0]) {
			arr_strcpy(hiud.url, url);
			inet_aton(saddr, &hiud.saddr);
			inet_aton(daddr, &hiud.daddr);
			igd_host_add_intercept_url(hiui, &hiud);
		}
		ptr = strstr(ptr, ";;");
		if (!ptr || !ptr[2])
			break;
		ptr += 2;
	}
	igd_intercept_url_rule(host);
	return 0;
}

static int igd_load_host_info(
	unsigned char *mac, unsigned int appid, struct uci_section *s)
{
	struct uci_option *o = NULL;
	struct uci_element *oe = NULL;
	struct host_info_record *host = NULL;
	struct host_app_record *app = NULL;

	host = igd_find_host(mac);
	if (!host) {
		host = igd_add_host(mac);
		if (!host)
			return -1;
		igd_set_bit(HTRF_LOAD, host->tmp);
	}

	if (appid) {
		app = igd_host_find_app(host, appid);
		if (!app)
			app = igd_host_add_app(host, appid);
		if (!app)
			return -1;
		app->time.up.flag = UPTIME_SYS;
	}

	IGD_HOST_DBG("mac:%s, appid:%u, %p,%p\n",
		mac_to_str(mac), appid, host, app);

	uci_foreach_element(&s->options, oe) {
		o = uci_to_option(oe);
		IGD_HOST_DBG("\t op:%s:%s\n", oe->name, o->v.string);
		if (!strcmp(oe->name, HSF_MODE)) {
			host->base.mode = atoi(o->v.string);
			if (host->base.mode >= HM_MAX)
				host->base.mode = HM_FREE;
		} else if (!strcmp(oe->name, HSF_LS)) {
			igd_load_host_ls(host, o->v.string);
		} else if (!strcmp(oe->name, HSF_BLACK)) {
			igd_load_host_black(host, o->v.string);
		} else if (!strcmp(oe->name, HSF_NICK)) {
			igd_set_bit(HIRF_NICK_NAME, host->flag);
			arr_strcpy(host->base.nick, o->v.string);
		} else if (!strcmp(oe->name, HSF_ONLINE_PUSH)) {
			igd_set_bit(HIRF_ONLINE_PUSH, host->flag);
		} else if (!strcmp(oe->name, HSF_PIC)) {
			host->base.pic = atoi(o->v.string);
		} else if (!strcmp(oe->name, HSF_ALLOW_URL)) {
			igd_load_host_allow_url(host, o->v.string);
		} else if (!strcmp(oe->name, HSF_STUDY_TIME)) {
			igd_load_host_study_time(host, o->v.string);
		} else if (!strcmp(oe->name, HSF_URL_BLACK)) {
			igd_load_host_url_black_white(host, RLT_URL_BLACK, o->v.string);
		} else if (!strcmp(oe->name, HSF_URL_WHITE)) {
			igd_load_host_url_black_white(host, RLT_URL_WHITE, o->v.string);
		} else if (!strcmp(oe->name, HSF_APP_BLACK)) {
			if (app)
				igd_set_bit(HARF_INBLACK, app->flag);
		} else if (!strcmp(oe->name, HSF_APP_LT)) {
			if (app)
				igd_load_host_app_limit_time(app, o->v.string);
		} else if (!strcmp(oe->name, HSF_APP_LMT)) {
			igd_load_host_app_mod(host, o->v.string);
		} else if (!strcmp(oe->name, HSF_INTERCEPT_URL)) {
			igd_load_host_intercept_url(host, o->v.string);
		} else if (!strcmp(oe->name, HSF_APP_QUEUE)) {
			igd_load_app_queue(host, o->v.string, NULL);
		} else if (!strcmp(oe->name, HSF_APP_WHITE)) {
			if (app)
				igd_set_bit(HARF_INWHITE, app->flag);
		}
	}
	return 0;
}

static int igd_host_load(void)
{
	struct uci_package *pkg = NULL;
	struct uci_context *ctx = NULL;
	struct uci_element *se = NULL;
	struct uci_section *s = NULL;
	unsigned char mac[6] = {0,};
	char *ptr = NULL;
	unsigned int appid = 0;

	ctx = uci_alloc_context();
	if (0 != uci_load(ctx, IGD_HOST_SAVE_FILE, &pkg)) {
		uci_free_context(ctx);
		IGD_HOST_ERR("uci load %s fail\n", IGD_HOST_SAVE_FILE);
		return -1;
	}

	uci_foreach_element(&pkg->sections, se) {
		s = uci_to_section(se);
		IGD_HOST_DBG("load, str:%s\n", se->name);
		if (!strcmp(se->name, IGD_HOST_STUDY_URL)) {
			igd_load_study_url(s);
		} else if (!strcmp(se->name, IGD_HOST_GLOBAL_INFO)) {
			igd_load_global_info(s);
		} else if (!str_to_mac(se->name, mac)) {
			ptr = strchr(se->name, '_');
			appid = ptr ? atoi(ptr + 1) : 0;
			igd_load_host_info(mac, appid, s);
		} else {
			IGD_HOST_ERR("don't support, str:%s\n", se->name);
		}
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx);
	return 0;
}

static int igd_host_run(void)
{
	struct host_info_record *host;
	struct allow_study_url_info *url_info;
	struct allow_study_url_list *url_list;
	struct study_url_record *url_all;

	list_for_each_entry(host, &igd_host_list, list) {
		url_info = igd_host_find_rule(host, RLT_STUDY_URL);
		if (!url_info)
			continue;
		list_for_each_entry(url_list, &url_info->url_list, list) {
			url_all = igd_find_study_url_by_id(url_list->url_id);
			if (!url_all) {
				IGD_HOST_ERR("no find, %u\n", url_list->url_id);
				continue;
			}
			url_all->used++;
		}
	}
	host = igd_find_host(igd_host_global.king_mac);
	if (!host) {
		host = igd_add_host(igd_host_global.king_mac);
		if (host)
			igd_set_bit(HTRF_LOAD, host->tmp);
	}
	if (host) {
		igd_set_bit(HIRF_ISKING, host->flag);
		igd_host_global.king_rule_id = igd_host_filter_king_add(host->mac);
		if (igd_host_global.king_rule_id < 0)
			IGD_HOST_ERR("add king fail, %d\n", igd_host_global.king_rule_id);
	}
	return 0;
}

static int igd_host_init(void)
{
	struct timeval tv;

	memset(&igd_host_global, 0, sizeof(igd_host_global));
	igd_host_global.new_push = 1;

	read_vender();
	igd_study_url_load(0);
	igd_host_ua_load();
	igd_host_load();
	igd_host_run();

	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if (!schedule(tv, igd_host_timer, NULL)) {
		IGD_HOST_ERR("igd_host_timer err\n");
	}
	return 0;
}

int igd_host_call(MSG_ID msgid, void *data, int d_len, void *back, int b_len)
{
	switch (msgid) {
	case IGD_HOST_INIT:
		return igd_host_init();
	case IGD_HOST_DAY_CHANGE:
		return igd_host_day_change();
	case IGD_HOST_ONLINE:
		return igd_host_online(data, d_len);
	case IGD_HOST_OFFLINE:
		return igd_host_offline(data, d_len);
	case IGD_HOST_APP_ONLINE:
		return igd_host_app_online(data, d_len);
	case IGD_HOST_APP_OFFLINE:
		return igd_host_app_offline(data, d_len);
	case IGD_HOST_DBG_FILE:
		return igd_host_dbg_file(data, d_len);
	case IGD_HOST_DUMP:
		return igd_host_dump(data, d_len, back, b_len);
	case IGD_HOST_APP_DUMP:
		return igd_host_app_dump(data, d_len, back, b_len);
	case IGD_HOST_ADD_HISTORY:
		return igd_host_add_history(data, d_len);
	case IGD_HOST_DEL_HISTORY:
		return igd_host_del_history(data, d_len);
	case IGD_STUDY_URL_UPDATE:
		return igd_study_url_load(1);
	case IGD_STUDY_URL_DNS_UPDATE:
		return igd_study_url_dns_update();
	case IGD_HOST_UA_UPDATE:
		return igd_host_ua_load();
	case IGD_HOST_VENDER_UPDATE:
		return igd_host_vender_update();
	case IGD_HOST_IP2MAC:
		return igd_host_ip2mac(data, d_len, back, b_len);
	case IGD_VENDER_NAME:
		return igd_vender_name(data, d_len, back, b_len);
	case IGD_HOST_NAME_UPDATE:
		return igd_host_name_update(data, d_len);
	case IGD_HOST_OS_TYPE_UPDATE:
		return igd_host_os_type_update(data, d_len);
	case IGD_HOST_UA_COLLECT:
		return igd_host_ua_collect(data, d_len);
	case IGD_HOST_DHCP_INFO:
		return igd_host_dhcp_info(data, d_len);
	case IGD_HOST_SET_NICK:
		return igd_host_set_nick(data, d_len);
	case IGD_HOST_SET_ONLINE_PUSH:
		return igd_host_set_online_push(data, d_len);
	case IGD_HOST_SET_PIC:
		return igd_host_set_pic(data, d_len);
	case IGD_HOST_SET_BLACK:
		return igd_host_set_black(data, d_len);
	case IGD_HOST_SET_LIMIT_SPEED:
		return igd_host_set_limit_speed(data, d_len);
	case IGD_HOST_SET_MODE:
		return igd_host_set_mode(data, d_len);
	case IGD_HOST_SET_KING:
		return igd_host_set_king(data, d_len);
	case IGD_HOST_SET_NEW_PUSH:
		return igd_host_set_new_push(data, d_len, back, b_len);
	case IGD_STUDY_URL_ACTION:
		return igd_study_url_act(data, d_len, back, b_len);
	case IGD_HOST_STUDY_URL_ACTION:
		return igd_host_study_url_act(data, d_len, back, b_len);
	case IGD_HOST_STUDY_TIME:
		return igd_host_study_time_act(data, d_len, back, b_len);
	case IGD_HOST_URL_ACTION:
		return igd_host_url_act(data, d_len, back, b_len);
	case IGD_HOST_SET_APP_BLACK:
		return igd_host_set_app_black(data, d_len);
	case IGD_HOST_SET_APP_LIMIT_TIME:
		return igd_host_set_app_limit_time(data, d_len);
	case IGD_HOST_APP_MOD_ACTION:
		return igd_host_app_mod_act(data, d_len, back, b_len);
	case IGD_HOST_INTERCEPT_URL_BLACK_ACTION:
		return igd_host_intercept_url_black_act(data, d_len, back, b_len);
	case IGD_HOST_APP_MOD_QUEUE:
		return igd_host_app_mod_queue(data, d_len, back, b_len);
	case IGD_APP_MOD_QUEUE:
		return igd_app_mod_queue(data, d_len, back, b_len);
	case IGD_HOST_SET_APP_WHITE:
		return igd_host_set_app_white(data, d_len);
	case IGD_HOST_VAP_ALLOW:
		return igd_host_vap_allow(data, d_len);
	case IGD_HOST_SET_ALI_BLACK:
		return igd_host_set_ali_black(data, d_len);
	case IGD_HOST_SET_FILTER:
		return igd_host_set_filter(data, d_len, back, b_len);

	default:
		IGD_HOST_ERR("msgid:%d nonsupport\n", msgid);
		return -1;
	}
	return -1;
}
