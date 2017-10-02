#include "uci.h"
#include "igd_lib.h"
#include "igd_system.h"
#include "igd_wifi.h"
#include "igd_cloud.h"
#include "igd_host.h"
#include <linux/wireless.h>
#include <string.h>

static int sockfd;
static int vhostnum;
static int host_flag;
static int time_flag;
static int surn[IW_IF_MX];
static int scannng[IW_IF_MX];
static int wifi_lock[IW_IF_MX];
static struct wifi_conf cfg[IW_IF_MX];
static struct schedule_entry *wifi_vap[IW_IF_MX];
static struct schedule_entry *wifi_time[IW_IF_MX];
static struct schedule_entry *wifi_ap[IW_IF_MX];
static struct schedule_entry *wifi_acl[IW_IF_MX];
static struct schedule_entry *wifi_site[IW_IF_MX];
static struct iwacl wacl[IW_IF_MX];
static struct list_head vhost;

extern int get_sysflag(unsigned char bit);
static char *ssid2gust(char *ssid);
static int wifi_set_txrate(struct wifi_conf *config);
static int wifi_set_hidden(int hidden, char *dev);
static int wifi_set_security(int ifindex, char *encryption, char *key, char *dev, char *ssid);
static int wifi_set_ssid(char *ssid, char *dev);
static int wifi_set_channel(struct wifi_conf *config);
static int wifi_disconn_all(struct wifi_conf *config);
static int wifi_set_htbw(struct wifi_conf *config);
static int wifi_set_mode(struct wifi_conf *config);
static int set_iwcmd(char *cmd, char *dev);
static int wifi_set_vap(struct wifi_conf *config);
static int wifi_set_2040scan(struct wifi_conf *config);
static int wifi_vap_host_clean(unsigned char ifindex);
static int wifi_set_country(struct wifi_conf *config);
static int wifi_set_wmm(int enable, char *dev);
static int wifi_set_apforward(struct wifi_conf *config);
static int wifi_set_hostforward(int enable, char *dev);

static int wifi_get_conf(int ifindex, struct wifi_conf *config)
{
	if (!cfg[ifindex].valid)
		return -1;
	memcpy(config, &cfg[ifindex], sizeof(*config));
	return 0;
}

static struct wifi_conf *wifi_get_dev(char *device)
{
	loop_for_each(0, IW_IF_MX) {
		if (!cfg[i].valid)
			continue;
		if (!strcmp(cfg[i].devname, device))
			return &cfg[i];			
	} loop_for_each_end();
	return NULL;
}

static void wifi_set_led(void)
{
	loop_for_each(0, IW_IF_MX) {
		if (!cfg[i].valid)
			continue;
		if (wifi_lock[i])
			led_act(i ? LED_WIFI5G : LED_WIFI24G, LED_ACT_OFF);
	}loop_for_each_end();

	loop_for_each(0, IW_IF_MX) {
		if (!cfg[i].valid)
			continue;
		if (!wifi_lock[i])
			led_act(i ? LED_WIFI5G : LED_WIFI24G, LED_ACT_FLASH);
	}loop_for_each_end();

}

static void vap_host_cb(void *data)
{
	int i;
	unsigned char buf[32];

	if (!host_flag) {
		time_flag = 0;
		return;
	}
	CC_PUSH2(buf, 2, CSO_REQ_VISITOR_MAC);
	i = 8;
	CC_PUSH1(buf, i, 0);
	i += 1;
	CC_PUSH2(buf, i, 0);
	i += 2;
	CC_PUSH1(buf, i, 0);
	i += 1;
	CC_PUSH2(buf, 0, i);
	CC_CALL_ADD(buf, i);
	host_flag = 0;
	time_flag = 0;
}

static void wifi_down(struct wifi_conf *config)
{
	if (wifi_lock[config->ifindex])
		return;
	/*scaning not support wifi set*/
	if (scannng[config->ifindex])
		return;
	if (config->vap)
		exec_cmd("ifconfig %s down", config->vapname);
	exec_cmd("ifconfig %s down", config->apname);
	wifi_lock[config->ifindex] = 1;
	wifi_set_led();
}

static void wifi_up(struct wifi_conf *config)
{
	if (!wifi_lock[config->ifindex])
		return;
	/*scaning not support wifi set*/
	if (scannng[config->ifindex])
		return;
	exec_cmd("ifconfig %s up", config->apname);
	wifi_set_country(config);
	wifi_set_mode(config);
	wifi_set_txrate(config);
	wifi_set_htbw(config);
	wifi_set_channel(config);
	wifi_set_wmm(config->wmmcapable, config->apname);
	wifi_set_hostforward(config->nohostfoward, config->apname);
	wifi_set_hidden(config->hidssid, config->apname);
	wifi_set_security(config->ifindex, config->encryption,
					config->key, config->apname,
					config->ssid);
	if (config->vap) {
		wifi_set_vap(config);
	} else {
		wifi_set_channel(config);
		wifi_set_2040scan(config);
	}
	wifi_lock[config->ifindex] = 0;
	wifi_set_led();
}

static int wifi_time_cmp(struct tm *t, struct time_comm *m)
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

static void wifi_time_cb(void *data)
{
	int ret = 0;
	struct timeval tv;
	struct tm *t = NULL;
	struct wifi_conf *apcfg;

	apcfg = (struct wifi_conf *)data;
	wifi_time[apcfg->ifindex] = NULL;
	if (!apcfg->enable) {
		wifi_down(apcfg);
		if (scannng[apcfg->ifindex])
			goto next;
		return;
	}
	if (!apcfg->time_on) {
		wifi_up(apcfg);
		if (scannng[apcfg->ifindex])
			goto next;
		return;
	}
	t = get_tm();
	if (!t) {
		wifi_up(apcfg);
		goto next;
	}
	ret = wifi_time_cmp(t, &apcfg->tm);
	if (ret > 0) {
		/*int the disable time*/
		wifi_down(apcfg);
	} else {
		/*not in the disable time*/
		wifi_up(apcfg);
	}
	if (ret < 0 && !scannng[apcfg->ifindex]) {
		memset(&apcfg->tm, 0x0, sizeof(apcfg->tm));
		apcfg->time_on = 0;
		return;
	}
next:
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	wifi_time[apcfg->ifindex] = schedule(tv, wifi_time_cb, data);
	return;
}

static void wifi_vap_cb(void *data)
{
	struct wifi_conf *config = (struct wifi_conf *)data;
	
	if (wifi_lock[config->ifindex])
		goto out;
	if (config->vap) {
		exec_cmd("ifconfig %s up", config->vapname);
		wifi_set_wmm(config->wmmcapable, config->vapname);
		wifi_set_hostforward(config->nohostfoward, config->vapname);
		wifi_set_hidden(config->hidvssid, config->vapname);
		wifi_set_security(config->ifindex, config->vencryption,
						config->vkey, config->vapname,
						config->vssid);
		wifi_set_apforward(config);
		wifi_set_channel(config);
		wifi_set_2040scan(config);
		if (strcmp(config->vencryption, "none"))
			wifi_vap_host_clean(config->ifindex);
	} else {
		exec_cmd("ifconfig %s down", config->vapname);
	}
out:
	wifi_vap[config->ifindex] = NULL;
}

static void wifi_ap_cb(void *data)
{
	struct wifi_conf *config = (struct wifi_conf *)data;

	if (wifi_lock[config->ifindex])
		goto out;
	wifi_disconn_all(config);
	wifi_set_country(config);
	wifi_set_mode(config);
	wifi_set_htbw(config);
	wifi_set_channel(config);
	wifi_set_wmm(config->wmmcapable, config->apname);
	wifi_set_hostforward(config->nohostfoward, config->apname);
	wifi_set_hidden(config->hidssid, config->apname);
	wifi_set_security(config->ifindex, config->encryption,
					config->key, config->apname,
					config->ssid);
	if (config->vap) {
		wifi_set_vap(config);
	} else {
		wifi_set_channel(config);
		wifi_set_2040scan(config);
	}
out:
	wifi_ap[config->ifindex] = NULL;
}

static void _wifi_acl_cb(void *data, int vap)
{
	int nr = 0;
	struct acl_list *list;
	struct iwacl *acl;
	struct wifi_conf *apcfg;
	char cmd[(IW_LIST_MX * 18) + 100] = {0};

	acl = (struct iwacl *)data;
	apcfg = &cfg[acl->ifindex];
	wifi_acl[acl->ifindex] = NULL;
	snprintf(cmd, sizeof(cmd), "ACLClearAll=1");
	set_iwcmd(cmd, vap ? apcfg->vapname : apcfg->apname);
	if (acl->blist.enable) {
		list = &acl->blist;
		snprintf(cmd, sizeof(cmd), "AccessPolicy=2");
		set_iwcmd(cmd, vap ? apcfg->vapname : apcfg->apname);
	} else if (acl->wlist.enable) {
		list = &acl->wlist;
		snprintf(cmd, sizeof(cmd), "AccessPolicy=1");
		set_iwcmd(cmd, vap ? apcfg->vapname : apcfg->apname);
	} else {
		snprintf(cmd, sizeof(cmd), "AccessPolicy=0");
		set_iwcmd(cmd, vap ? apcfg->vapname : apcfg->apname);
		return;
	}
	nr = snprintf(cmd, sizeof(cmd), "iwpriv %s set ACLAddEntry=\"",
						vap ? apcfg->vapname : apcfg->apname);
	loop_for_each(0, list->nr) {
		if (!list->list[i].enable)
			continue;
		nr += snprintf(cmd + nr, sizeof(cmd) - nr,
			"%02x:%02x:%02x:%02x:%02x:%02x;", MAC_SPLIT(list->list[i].mac));
	} loop_for_each_end();
	if (cmd[nr - 1] == '=')
		return;
	cmd[nr - 1] = '\"';
	cmd[nr] = '\0';
	system(cmd);
	return;
}

static void wifi_acl_cb(void *data)
{
	_wifi_acl_cb(data, 0);
	_wifi_acl_cb(data, 1);
}

static int set_iwcmd(char *cmd, char *dev)
{
	char cmdline[128];
	snprintf(cmdline, sizeof(cmdline), "iwpriv %s set %s", dev, cmd);
	return system(cmdline);
}

static int wifi_get_sta(int ifindex, struct sta_status *info)
{
	int i = 0;
	char rssi = 0;
	struct iwreq iwr;
	
	strncpy(iwr.ifr_name, cfg[ifindex].apname, IFNAMSIZ);
	iwr.u.data.pointer = (char *)info;
	iwr.u.data.length = sizeof(*info);

	if(ioctl(sockfd, RTPRIV_IOCTL_GET_MAC_TABLE_STRUCT, &iwr) < 0)
		return -1;
	if (info->num <= 0)
		return -1;
	if (info->num > IW_STA_MX)
		info->num = IW_STA_MX;
	for (i = 0; i < info->num; i++) {
		rssi = info->entry[i].AvgRssi0;
		if (rssi >= -50)
			info->entry[i].AvgRssi0 = 100;
		else if (rssi >= -80)
			info->entry[i].AvgRssi0 = (unsigned short)(24 + ((rssi + 80) * 26)/10);
		else if (rssi >= -90)
			info->entry[i].AvgRssi0 = (unsigned short)(((rssi + 90) * 26)/10);
		else
			info->entry[i].AvgRssi0 = 0;	
	}
	return 0;
}

static int get_iwsurvey(struct iwsurvey *survey, char *buf)
{
	int j, i = 0;
	int cnt = 0;
	int nr = 0;
	char *cur = NULL;
	char *ptr = NULL;
	char *point = NULL;
	char line[1024];
	char channel[4], bssid[20], security[23];
	char signal[9], mode[16], ext_ch[7], net_type[3], wps[5];
	
	if (!strlen(buf))
		return 0;
	cur = buf;
	memset(survey, 0x0, sizeof(*survey) * IW_SUR_MX);
	while (nr < strlen(buf)) {
		point = strchr(cur, '\n');
		if (NULL == point)
			break;
		memset(line, 0x0, sizeof(line));
		__arr_strcpy_end((unsigned char *)line, (unsigned char *)cur, sizeof(line) -1, '\n');
		cur = point + 1;
		nr += strlen(line) + 1;
		if (!strlen(line) || strlen(line) < 4 || !strncmp(line, "Ch  SSID", 8)
			|| strstr(line, "get_site_survey:"))
			continue;
		ptr = line;
		j = sscanf(ptr, "%s ", channel);
		if (j != 1)
			continue;
		ptr += 37;
		j = sscanf(ptr, "%s %s %s %s %s %s %s", bssid, security, signal, mode, ext_ch, net_type, wps);
		if (j != 7)
			continue;
		ptr = line + 4;
		arr_strcpy(survey[cnt].ssid, ptr);
		ptr = survey[cnt].ssid;
		for (i = strlen(survey[cnt].ssid); i >0; i--) {
			if (ptr[i] != '\0' && ptr[i] != ' ')
				break;
			if (ptr[i] == ' ')
				ptr[i] = '\0';
		}
		survey[cnt].ch = atoi(channel);
		survey[cnt].signal= atoi(signal);
		arr_strcpy(survey[cnt].bssid, bssid);
		arr_strcpy(survey[cnt].mode, mode);
		arr_strcpy(survey[cnt].extch, ext_ch);
		arr_strcpy(survey[cnt].security, security);
		if (!strncmp(wps, "yes", 3))
			survey[cnt].wps = 1;
		cnt++;
		if (cnt >= IW_SUR_MX)
			break;
	}
	return cnt;
}

static void survey_cb(void *data)
{
	int ifindex = *(int *)data;
	surn[ifindex] = 0;
	scannng[ifindex] = 0;
	wifi_site[ifindex] = NULL;
}

static int wifi_get_survey(struct wifi_conf *config, int scan, struct iwsurvey *survey)
{
	int nr = 0;
	int ret = 0;
	int ifindex;
	char cmd[32];
	char *buf = NULL;
	struct timeval tv;
	int len = (IW_SUR_MX * IW_SUR_LINE_LEN) + 100;

	ifindex = config->ifindex;
	/*scan=2 skip cache*/
	if (scan == 2)
		surn[ifindex] = 0;
	if (!scan || surn[ifindex] > 0)
		goto finish;
	/*scan is doing*/
	if (scannng[ifindex])
		return 0;
	/*wifi need set, not support scan*/
	if (wifi_ap[ifindex] || wifi_vap[ifindex] || wifi_acl[ifindex])
		return -1;
	arr_strcpy(cmd, "SiteSurvey=1");
	if (set_iwcmd(cmd, config->apname)) {
		MU_DEBUG("%s:%d get ssid list failed\n", __FUNCTION__, __LINE__);
		return -1;
	}
	scannng[ifindex] = 1;
	if (wifi_site[ifindex]) {
		dschedule(wifi_site[ifindex]);
		wifi_site[ifindex] = NULL;
	}
	ret = 0;
	goto out;
finish:
	scannng[ifindex] = 0;
	snprintf(cmd, sizeof(cmd), "iwpriv %s get_site_survey", config->apname);
	buf = malloc(len);
	if (!buf) {
		MU_DEBUG("%s:%d get ssid list failed\n", __FUNCTION__, __LINE__);
		return -1;
	}
	memset(buf, 0x0, len);
	shell_printf(cmd, buf, len);
	nr = get_iwsurvey(survey, buf);
	free(buf);
	if (!surn[ifindex])
		surn[ifindex] = nr;
	ret = nr;
out:
	if (wifi_site[ifindex])
		return ret;
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	wifi_site[ifindex] = schedule(tv, survey_cb, &config->ifindex);
	return ret;
}

static int wifi_get_channel(struct wifi_conf *config, int *channel)
{
	int i, ret = -1;
	struct iwreq wrq;

	if (wifi_lock[config->ifindex])
		return 1;
	memset(&wrq, 0x0, sizeof(wrq));
	strncpy(wrq.ifr_ifrn.ifrn_name, config->apname, sizeof(wrq.ifr_ifrn.ifrn_name));
	if (ioctl(sockfd, SIOCGIWFREQ, &wrq) < 0) {
		printf("%s:%d get channel failed\n", __FUNCTION__, __LINE__);
		return -1;
	}
	ret = wrq.u.freq.m;
	for(i = 0; i < wrq.u.freq.e; i++)
		ret *= 10;
	*channel = ret;
	return 0;
}

static char *ssid2gust(char *ssid)
{
	char tssid[32];
	static char guest[32];
	unsigned char *p = NULL;
	
	memset(guest, 0x0, sizeof(guest));
	if (strlen(ssid) <= sizeof(guest) - 7) {
		sprintf(guest, "%.*s-Guest", sizeof(guest) - 7, ssid);
		return guest;
	}
	strncpy(tssid, ssid, sizeof(tssid));
	p = (unsigned char *)tssid;
	p[sizeof(tssid) - 7] = '\0';
	while (*p) {
		switch(((*p >> 4) & 0xf)) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:			
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
			/*ascii*/
			p++;
			break;
		case 12:
		case 13:
			/*European*/
			if (*(p+1) == '\0') {
				*p = '\0';
				goto finish;
			}
			p += 2;
			break;
		case 14:
			/*chinese*/
			if ((*(p+1) == '\0') || (*(p+2) == '\0')) {
				*p = '\0';
				goto finish;
			}
			p+=3;
			break;
			/*others*/
		case 15:
			if ((*(p+1) == '\0') || (*(p+2) == '\0') || (*(p+3) == '\0')) {
				*p = '\0';
				goto finish;
			}
			p+=4;
			break;
		}
		continue;
finish:
		break;
	}
	sprintf(guest, "%.*s-Guest", sizeof(guest) - 7, tssid);
	return guest;
}

static int wifi_save_channel(struct wifi_conf *config)
{
	char cmd[128] = {0};
	if (config->channel == 0) {
 		snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].auotch=2",
								WIFI_CONFIG_FILE, config->ifindex);
		uuci_set(cmd);
	} else {
		snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].auotch=0",
								WIFI_CONFIG_FILE, config->ifindex);
		uuci_set(cmd);
	}
	snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].channel=%d",
				WIFI_CONFIG_FILE, config->ifindex, config->channel);
	uuci_set(cmd);
	cfg[config->ifindex].channel = config->channel;
	return 0;
}

static int wifi_save_txpoer(struct wifi_conf *config)
{
	char cmd[128] = {0};
	snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].txpoer=%d",
				WIFI_CONFIG_FILE, config->ifindex, config->txrate);
	uuci_set(cmd);
	cfg[config->ifindex].txrate = config->txrate;
	return 0;
}

static int wifi_save_mode(struct wifi_conf *config)
{
	char rate[8];
	char txmode[8];
	char cmd[128] = {0};
	
	memset(rate, 0x0, sizeof(rate));
	memset(txmode, 0x0, sizeof(txmode));
	switch(config->mode) {
	case MODE_802_11_B:
		arr_strcpy(rate, "3");
		arr_strcpy(txmode, "CCK");
		break;
	case MODE_802_11_G:
		arr_strcpy(rate, "351");
		arr_strcpy(txmode, "OFDM");
	case MODE_802_11_BG:
		arr_strcpy(rate, "15");
		arr_strcpy(txmode, "OFDM");
		break;
	case MODE_802_11_N:
		arr_strcpy(rate, "15");
		arr_strcpy(txmode, "HT");
		break;
	case MODE_802_11_BGN:
		arr_strcpy(rate, "15");
		arr_strcpy(txmode, "HT");
		break;
	case MODE_802_11_AANAC:
	case MODE_802_11_ANAC:
		break;
	case MODE_802_11_A:
		arr_strcpy(rate, "336");
		arr_strcpy(txmode, "OFDM");
		break;
	case MODE_802_11_AN:
	case MODE_802_11_5N:
		arr_strcpy(rate, "336");
		arr_strcpy(txmode, "HT");
		break;
	default:
		return -1;
	}
	snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].basicrate=%s",
				WIFI_CONFIG_FILE, config->ifindex, rate);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].fixedtxmode=%s",
				WIFI_CONFIG_FILE, config->ifindex, txmode);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].wifimode=%d",
				WIFI_CONFIG_FILE, config->ifindex, config->mode);
	uuci_set(cmd);
	cfg[config->ifindex].mode = config->mode;
	return 0;
}

static int wifi_save_acl(struct iwacl *acl)
{
	char cmd[128] = {0};
	struct wifi_conf *apcfg;

	apcfg = &cfg[acl->ifindex];
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].wlist",
						WIFI_CONFIG_FILE, apcfg->apindex);
	uuci_delete(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].blist",
						WIFI_CONFIG_FILE, apcfg->apindex);
	uuci_delete(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].wlisten=%d",
			WIFI_CONFIG_FILE, apcfg->apindex, acl->wlist.enable);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].blisten=%d",
			WIFI_CONFIG_FILE, apcfg->apindex, acl->blist.enable);
	uuci_set(cmd);
	loop_for_each_2(i, 0, acl->wlist.nr) {
		snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].wlist=%02x:%02x:%02x:%02x:%02x:%02x %d",
				WIFI_CONFIG_FILE, apcfg->apindex, MAC_SPLIT(acl->wlist.list[i].mac),
				acl->wlist.list[i].enable);
		uuci_add_list(cmd);
	} loop_for_each_end();
	loop_for_each_2(j, 0, acl->blist.nr) {
		snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].blist=%02x:%02x:%02x:%02x:%02x:%02x %d",
				WIFI_CONFIG_FILE, apcfg->apindex, MAC_SPLIT(acl->blist.list[j].mac),
				acl->blist.list[j].enable);
		uuci_add_list(cmd);
	} loop_for_each_end();
	memcpy(&wacl[acl->ifindex], acl, sizeof(*acl));
	return 0;
}

static int wifi_save_guest(struct wifi_conf *config)
{
	char cmd[128] = {0};

	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].enable=%d",
				WIFI_CONFIG_FILE, config->vapindex, config->vap);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].ssid=%s",
				WIFI_CONFIG_FILE, config->vapindex, config->vssid);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].hidden=%d",
				WIFI_CONFIG_FILE, config->vapindex, config->hidvssid);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].wechat=%d",
				WIFI_CONFIG_FILE, config->vapindex, config->wechat);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].rebind=%d",
				WIFI_CONFIG_FILE, config->vapindex, config->rebind);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].aliset=%d",
				WIFI_CONFIG_FILE, config->vapindex, config->aliset);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].encryption=%s",
				WIFI_CONFIG_FILE, config->vapindex, config->vencryption);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].key=%s",
				WIFI_CONFIG_FILE, config->vapindex, config->vkey);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].noforwardbssid=%d",
				WIFI_CONFIG_FILE, config->ifindex, config->noapforward);
	uuci_set(cmd);
	memcpy(&cfg[config->ifindex], config, sizeof(*config));
	return 0;
}

static int wifi_save_time(struct wifi_conf *config)
{
	char cmd[128] = {0};
	struct time_comm *t = &config->tm;
	snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].enable=%d",
				WIFI_CONFIG_FILE, config->ifindex, config->enable);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-time[0].time%d=%d,%d,%d,%d,%d,%d,%d",
					WIFI_CONFIG_FILE, config->ifindex, config->time_on, t->loop,
					t->day_flags, t->start_hour, t->start_min, t->end_hour,
					t->end_min);
	uuci_set(cmd);
	memcpy(&cfg[config->ifindex], config, sizeof(*config));
	return 0;
}

static int wifi_save_ap(struct wifi_conf *config)
{
	char cmd[128] = {0};
	struct wifi_conf *apcfg;

	apcfg = &cfg[config->ifindex];
	if (!strcmp(apcfg->vssid, ssid2gust(apcfg->ssid))) 
		arr_strcpy(config->vssid, ssid2gust(config->ssid));
	memcpy(apcfg, config, sizeof(*apcfg));
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].ssid=%s",
				WIFI_CONFIG_FILE, apcfg->apindex, apcfg->ssid);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].hidden=%d",
				WIFI_CONFIG_FILE, apcfg->apindex, apcfg->hidssid);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].encryption=%s",
				WIFI_CONFIG_FILE, apcfg->apindex, apcfg->encryption);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].key=%s",
				WIFI_CONFIG_FILE, apcfg->apindex, apcfg->key);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[%d].ssid=%s",
				WIFI_CONFIG_FILE, apcfg->vapindex, apcfg->vssid);
	uuci_set(cmd);
	if (!apcfg->ifindex) {
		snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].bw=%d",
					WIFI_CONFIG_FILE, apcfg->ifindex, apcfg->htbw);
		uuci_set(cmd);
		snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].region=%d",
					WIFI_CONFIG_FILE, apcfg->ifindex, apcfg->country);
		uuci_set(cmd);
	} else {
		snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].vbw=%d",
					WIFI_CONFIG_FILE, apcfg->ifindex, apcfg->htbw);
		uuci_set(cmd);
		snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].aregion=%d",
					WIFI_CONFIG_FILE, apcfg->ifindex, apcfg->country);
		uuci_set(cmd);
	}
	snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].wmm=%d;%d",
				WIFI_CONFIG_FILE, apcfg->ifindex,
				apcfg->wmmcapable, apcfg->wmmcapable);
	uuci_set(cmd);
	snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].noforward=%d;%d",
				WIFI_CONFIG_FILE, apcfg->ifindex,
				apcfg->nohostfoward, apcfg->nohostfoward);
	uuci_set(cmd);
	wifi_save_mode(config);
	wifi_save_channel(config);
	return 0;
}

static int wifi_set_country(struct wifi_conf *config)
{
	char cmd[128] = {0};
	if (!config->ifindex)
		snprintf(cmd, sizeof(cmd), "CountryRegion=%d", config->country);
	else 
		snprintf(cmd, sizeof(cmd), "CountryRegionABand=%d", config->country);
	if (set_iwcmd(cmd, config->apname))
		return -1;
	return 0;
}

static int wifi_set_channel(struct wifi_conf *config)
{
	char cmd[128] = {0};
	if (config->channel > 0)
		snprintf(cmd, sizeof(cmd), "Channel=%d", config->channel);
	else 
		snprintf(cmd, sizeof(cmd), "AutoChannelSel=2");
	if (set_iwcmd(cmd, config->apname))
		return -1;
	return 0;
}

static int wifi_set_txrate(struct wifi_conf *config)
{
	char cmd[128] = {0};
	snprintf(cmd, sizeof(cmd), "TxPower=%d", config->txrate);
	if (set_iwcmd(cmd, config->apname))
		return -1;
	return 0;
}

static int wifi_set_mode(struct wifi_conf *config)
{
	char cmd[128] = {0};
	char rate[IGD_NAME_LEN];
	char txmode[IGD_NAME_LEN];

	memset(rate, 0x0, sizeof(rate));
	memset(txmode, 0x0, sizeof(txmode));
	switch(config->mode) {
	case MODE_802_11_B:
		arr_strcpy(rate, "BasicRate=3");
		arr_strcpy(txmode, "FixedTxMode=CCK");
		break;
	case MODE_802_11_G:
		arr_strcpy(rate, "BasicRate=351");
		arr_strcpy(txmode, "FixedTxMode=OFDM");
	case MODE_802_11_BG:
		arr_strcpy(rate, "BasicRate=15");
		arr_strcpy(txmode, "FixedTxMode=OFDM");
		break;
	case MODE_802_11_N:
		arr_strcpy(rate, "BasicRate=15");
		arr_strcpy(txmode, "FixedTxMode=HT");
		break;
	case MODE_802_11_BGN:
		arr_strcpy(rate, "BasicRate=15");
		arr_strcpy(txmode, "FixedTxMode=HT");
		break;
	case MODE_802_11_AANAC:
	case MODE_802_11_ANAC:
		break;
	case MODE_802_11_A:
		arr_strcpy(rate, "BasicRate=336");
		arr_strcpy(txmode, "FixedTxMode=OFDM");
		break;
	case MODE_802_11_AN:
	case MODE_802_11_5N:
		arr_strcpy(rate, "BasicRate=336");
		arr_strcpy(txmode, "FixedTxMode=HT");
		break;
	default:
		return -1;
	}
	set_iwcmd(rate, config->apname);
	set_iwcmd(txmode, config->apname);
	snprintf(cmd, sizeof(cmd), "WirelessMode=%d", config->mode);
	set_iwcmd(cmd, config->apname);
	return 0;
}

static int wifi_set_2040scan(struct wifi_conf *config)
{
	char cmd[128] = {0};

	return 0;
	if (config->ifindex || !config->htbw)
		return 0;
	snprintf(cmd, sizeof(cmd), "AP2040Rescan=1");
	if (set_iwcmd(cmd, config->apname))
		return -1;
	return 0;
}

static int wifi_set_htbw(struct wifi_conf *config)
{
	char cmd[128] = {0};

	switch(config->ifindex) {
	case 0:
		snprintf(cmd, sizeof(cmd), "HtBw=%d", config->htbw);
		if (set_iwcmd(cmd, config->apname))
			return -1;
		if (!config->htbw || !config->channel)
			break;
		if (config->channel < 6)
			snprintf(cmd, sizeof(cmd), "HT_EXTCHA=1");
		else
			snprintf(cmd, sizeof(cmd), "HT_EXTCHA=0");
		if (set_iwcmd(cmd, config->apname))
			return -1;
		break;
	case 1:
		snprintf(cmd, sizeof(cmd), "VhtBw=%d", config->htbw);
		if (set_iwcmd(cmd, config->apname))
			return -1;
		break;
	default:
		return -1;
	}
	return 0;
}

static int wifi_set_apforward(struct wifi_conf *config)
{
	char cmd[128] = {0};

	snprintf(cmd, 127, "NoForwardingBTNBSSID=%d", config->noapforward);
	if (set_iwcmd(cmd, config->apname))
		return -1;
	return 0;
}

static int wifi_set_hostforward(int enable, char *dev)
{
	char cmd[128] = {0};

	snprintf(cmd, 127, "NoForwarding=%d", enable);
	if (set_iwcmd(cmd, dev))
		return -1;
	return 0;
}

static int wifi_set_wmm(int enable, char *dev)
{
	char cmd[128] = {0};

	snprintf(cmd, 127, "WmmCapable=%d", enable);
	if (set_iwcmd(cmd, dev))
		return -1;
	return 0;
}

static int wifi_set_ssid(char *ssid, char *dev)
{
	char cmd[128] = {0};

	snprintf(cmd, 127, "SSID=\"%s\"", ssid);
	if (set_iwcmd(cmd, dev))
		return -1;
	return 0;
}

static int wifi_set_security(int ifindex, char *encryption,
		char *key, char *dev, char *ssid)
{
	char cmd[128] = {0};
	
	if (!strncmp(encryption, "none", 4)) {
		snprintf(cmd, sizeof(cmd), "AuthMode=OPEN");
		if (set_iwcmd(cmd, dev))
			return -1;
		snprintf(cmd, sizeof(cmd), "EncrypType=NONE");
		if (set_iwcmd(cmd, dev))
			return -1;
		if (wifi_set_ssid(ssid, dev))
			return -1;
		return 0;
	}
	if (strstr(encryption, "psk-mixed+"))
		snprintf(cmd, sizeof(cmd), "AuthMode=WPAPSKWPA2PSK");
	else if (strstr(encryption, "psk+"))
		snprintf(cmd, sizeof(cmd), "AuthMode=WPAPSK");
	else if (strstr(encryption, "psk2+"))
		snprintf(cmd, sizeof(cmd), "AuthMode=WPA2PSK");
	if (set_iwcmd(cmd, dev))
		return -1;
	if (strstr(encryption, "tkip+ccmp"))
		snprintf(cmd, sizeof(cmd), "EncrypType=TKIPAES");
	else if (strstr(encryption, "tkip"))
		snprintf(cmd, sizeof(cmd), "EncrypType=TKIP");
	else if (strstr(encryption, "ccmp"))
		snprintf(cmd, sizeof(cmd), "EncrypType=AES");
	if (set_iwcmd(cmd, dev))
		return -1;
	if (wifi_set_ssid(ssid, dev))
		return -1;
	snprintf(cmd, sizeof(cmd), "WPAPSK=\"%s\"", key);
	if (set_iwcmd(cmd, dev))
		return -1;
	if (!ifindex)
		return wifi_set_ssid(ssid, dev);
	return 0;
}

static int wifi_set_hidden(int hidden, char *dev)
{
	char cmd[128] = {0};
	snprintf(cmd, 127, "HideSSID=%d", hidden);
	if (set_iwcmd(cmd, dev))
		return -1;
	return 0;
}

static int wifi_disconn_all(struct wifi_conf *config)
{
	int i, nr;
	char cmd[32];
	struct host_info *info;

	if (!config->valid)
		return 0;
	info = dump_host_alive(&nr);
	if (!info)
		return 0;
	for (i = 0; i < nr; i++) {
		if (!info[i].is_wifi)
			continue;
		snprintf(cmd, sizeof(cmd), "DisConnectSta=%02x:%02x:%02x:%02x:%02x:%02x", MAC_SPLIT(info[i].mac));
		set_iwcmd(cmd, config->apname);
	}
	free(info);
	return 0;
}

static int wifi_vap_dnsrd_rule_action(struct vap_host *guest, int action)
{
	struct inet_host host;
	struct dns_rule_api arg;
	
	switch (action) {
	case NLK_ACTION_ADD:
		if (guest->dnsrd > 0)
			return 0;
		memset(&host, 0x0, sizeof(host));
		host.type = INET_HOST_MAC;
		memcpy(host.mac, guest->macaddr, ETH_ALEN);
		arg.target = NF_TARGET_REDIRECT;
		arg.type = 0x1;
		arg.ip = (unsigned int)inet_addr("1.127.127.254");
		arg.prio = 1;
		guest->dnsrd = register_dns_rule(&host, &arg);
		if (guest->dnsrd <= 0) {
			MU_DEBUG("add vap dns rdrule error\n");
			return -1;
		}
		break;
	case NLK_ACTION_DEL:
		if (guest->dnsrd > 0) {
			unregister_dns_rule(guest->dnsrd);
			guest->dnsrd = 0;
		}	
		break;
	default:
		return -1;
	}
	return 0;
}

static int wifi_vap_black_rule_action(struct vap_host *guest, int action)
{
	struct inet_host host;
	struct net_rule_api arg;

	switch (action) {
	case NLK_ACTION_ADD:
		if (guest->netid > 0)
			return 0;
		memset(&host, 0, sizeof(host));
		host.type = INET_HOST_MAC;
		memcpy(host.mac, guest->macaddr, ETH_ALEN);
		memset(&arg, 0, sizeof(arg));
		arg.target = NF_TARGET_DENY;
		guest->netid = register_net_rule(&host, &arg);
		if (guest->netid <= 0) {
			MU_DEBUG("add vap black rule error\n");
			return -1;
		}
		break;
	case NLK_ACTION_DEL:
		if (guest->netid > 0) {
			if (unregister_net_rule(guest->netid)) {
				MU_DEBUG("del vap black rule error\n");
				return -1;
			}
			guest->netid = 0;
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static int wifi_vap_aprd_rule_action(struct vap_host *guest, int action)
{
	char *res;
	struct inet_host host;
	struct http_rule_api rule;
	char landomain[IGD_NAME_LEN];

	switch (action) {
	case NLK_ACTION_ADD:
		if (guest->aprdid > 0)
			return 0;
		res = read_firmware("DOMAIN");
		if (!res)
			return -1;
		arr_strcpy(landomain, res);
		memset(&host, 0, sizeof(host));
		memset(&rule, 0, sizeof(rule));
		host.type = INET_HOST_MAC;
		memcpy(host.mac, guest->macaddr, ETH_ALEN);
		rule.l3.type = INET_L3_URL;
		arr_strcpy(rule.l3.dns, "captive.apple.com");
		rule.prio = 1;
		rule.times = 2;
		rule.period = 1;
		rule.mid = URL_MID_REDIRECT;
		igd_set_bit(URL_RULE_GUEST_BIT, &rule.flags);
		igd_set_bit(RD_KEY_MAC, &rule.rd.flags);
		snprintf(rule.rd.url, sizeof(rule.rd.url), "http://%s/wx.html", landomain);
		guest->aprdid = register_http_rule(&host, &rule);
		if (guest->aprdid <= 0) {
			MU_DEBUG("add vap aprd rule error\n");
			return -1;
		}
		break;
	case NLK_ACTION_DEL:
		if (guest->aprdid > 0) {
			if (unregister_http_rule(URL_MID_REDIRECT, guest->aprdid)) {
				MU_DEBUG("del vap aprd rule error\n");
				return -1;
			}
			guest->aprdid = 0;
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static int wifi_vap_http_rule_action(struct vap_host *guest, int action)
{
	char *res;
	struct inet_host host;
	struct http_rule_api rule;
	char landomain[IGD_NAME_LEN];

	switch (action) {
	case NLK_ACTION_ADD:
		if (guest->httpid > 0)
			return 0;
		res = read_firmware("DOMAIN");
		if (!res)
			return -1;
		arr_strcpy(landomain, res);
		memset(&host, 0, sizeof(host));
		memset(&rule, 0, sizeof(rule));
		host.type = INET_HOST_MAC;
		memcpy(host.mac, guest->macaddr, ETH_ALEN);
		rule.l3.type = INET_L3_NONE;
		rule.mid = URL_MID_REDIRECT;
		rule.prio = 2;
		igd_set_bit(RD_KEY_MAC, &rule.rd.flags);
		igd_set_bit(URL_RULE_GUEST_BIT, &rule.flags);
		snprintf(rule.rd.url, sizeof(rule.rd.url), "http://%s/wx.html", landomain);
		guest->httpid = register_http_rule(&host, &rule);
		if (guest->httpid <= 0) {
			MU_DEBUG("add vap http rule error\n");
			return -1;
		}
		break;
	case NLK_ACTION_DEL:
		if (guest->httpid > 0) {
			if (unregister_http_rule(URL_MID_REDIRECT, guest->httpid)) {
				MU_DEBUG("del vap http rule error\n");
				return -1;
			}
			guest->httpid = 0;
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static void vap_wechat_cb(void *data)
{
	struct vap_host *guest = (struct vap_host *)data;

	wifi_vap_black_rule_action(guest, NLK_ACTION_ADD);
	wifi_vap_dnsrd_rule_action(guest, NLK_ACTION_ADD);
	guest->event = NULL;
}

static int wifi_vap_wchat_evnet_action(struct vap_host *guest, int action)
{
	struct timeval tv;

	switch (action) {
	case NLK_ACTION_ADD:
		if (guest->event)
			return 0;
		tv.tv_sec = 60;
		tv.tv_usec = 0;
		guest->allownr++;
		wifi_vap_dnsrd_rule_action(guest, NLK_ACTION_DEL);
		wifi_vap_black_rule_action(guest, NLK_ACTION_DEL);
		guest->event = schedule(tv, vap_wechat_cb, guest);
		break;
	case NLK_ACTION_DEL:
		if (guest->event) {
			dschedule(guest->event);
			guest->event = NULL;
		}
		break;
	default:
		break;
	}
	return 0;
}

static int wifi_vap_add_rule(int wechat, struct vap_host *guest)
{
	wifi_vap_black_rule_action(guest, NLK_ACTION_ADD);
	if (!wechat || guest->authoff)
		return 0;
	wifi_vap_dnsrd_rule_action(guest, NLK_ACTION_ADD);
	wifi_vap_aprd_rule_action(guest, NLK_ACTION_ADD);
	wifi_vap_http_rule_action(guest, NLK_ACTION_ADD);
	return 0;
}

static int wifi_vap_del_rule(struct vap_host *guest)
{
	wifi_vap_aprd_rule_action(guest, NLK_ACTION_DEL);
	wifi_vap_http_rule_action(guest, NLK_ACTION_DEL);
	wifi_vap_dnsrd_rule_action(guest, NLK_ACTION_DEL);
	wifi_vap_black_rule_action(guest, NLK_ACTION_DEL);
	wifi_vap_wchat_evnet_action(guest, NLK_ACTION_DEL);
	return 0;
}

static void wifi_vap_add_notify_event(struct vap_host *guest)
{
	struct timeval tv;
	
	if (time_flag)
		return;
	host_flag = 1;
	vap_host_cb(NULL);
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	if (!schedule(tv, vap_host_cb, NULL)) {
		MU_DEBUG("schedule vap guest event error\n");
		return;
	}
	time_flag = 1;
}

/*notify host module, the vhost is passed auth*/
static int wifi_set_vap_allowed(struct vap_host *guest, int action)
{
	struct host_set_info info;

	memset(&info, 0, sizeof(info));
	memcpy(info.mac, guest->macaddr, ETH_ALEN);
	info.act = action;
	return mu_call(IGD_HOST_VAP_ALLOW, &info, sizeof(info), NULL, 0);
}

static int wifi_vap_host_online(struct nlk_host *host)
{
	struct host_info info;
	struct wifi_conf *config;
	struct vap_host *guest = NULL, *tmp;

	memset(&info, 0x0, sizeof(info));
	if (dump_host_info(host->addr, &info))
		return -1;
	if (!info.is_wifi)
		return 0;
	switch (info.pid) {
	case 2:
		config = &cfg[0];
		break;
	case WIFI_5G_BASIC_ID + 2:
		config = &cfg[1];
		break;
	default:
		return 0;
	}
	if (strcmp(config->vencryption, "none"))
		return 0;
	list_for_each_entry(guest, &vhost, list) {
		if (memcmp(guest->macaddr, host->mac, ETH_ALEN))
			continue;
		guest->online = 1;
		guest->addr = host->addr;
		guest->ifindex = config->ifindex;
		wifi_vap_del_rule(guest);
		if (!guest->authon)
			goto out;
		wifi_set_vap_allowed(guest, NLK_ACTION_ADD);
		return 0;
	}
	if (vhostnum >= IW_VHOST_MX) {
		list_for_each_entry(tmp, &vhost, list) {
			if (tmp->online)
				continue;
			guest = tmp;
			list_del(&tmp->list);
			vhostnum--;
			break;
		}
	} else
		guest = malloc(sizeof(struct vap_host));
	if (!guest)
		return -1;
	vhostnum++;
	memset(guest, 0x0, sizeof(struct vap_host));
	guest->online = 1;
	guest->addr = host->addr;
	guest->ifindex = config->ifindex;
	memcpy(guest->macaddr, host->mac, ETH_ALEN);
	list_add_tail(&guest->list, &vhost);
out:
	if (!config->wechat || guest->authoff) {
		wifi_vap_black_rule_action(guest, NLK_ACTION_ADD);
		return 0;
	}
	wifi_vap_aprd_rule_action(guest, NLK_ACTION_ADD);
	wifi_vap_http_rule_action(guest, NLK_ACTION_ADD);
	wifi_vap_wchat_evnet_action(guest, NLK_ACTION_ADD);
	wifi_vap_add_notify_event(guest);
	return 0;
}

static int wifi_vap_host_offline(struct nlk_host *host)
{
	struct vap_host *guest;

	list_for_each_entry(guest, &vhost, list) {
		if (memcmp(host->mac, guest->macaddr, ETH_ALEN))
			continue;
		guest->online = 0;
		wifi_vap_del_rule(guest);
		if (!guest->authon && !guest->authoff) {
			vhostnum--;
			list_del(&guest->list);
			free(guest);
		}
		return 0;
	}
	return 0;
}

static int wifi_vap_host_action(void *data, int action)
{
	int cnt = 0;
	struct iwguest *host;
	struct vap_host *guest;
	struct host_info *info, tmp;

	switch (action) {
	case NLK_ACTION_ADD:
		host = (struct iwguest *)data;
		list_for_each_entry(guest, &vhost, list) {
			if (memcmp(guest->macaddr, host->mac, ETH_ALEN))
				continue;
			if (host->wechatid[0]) {
				if (guest->authoff)
					break;
				arr_strcpy(guest->passid, host->wechatid);
			}
			guest->allownr = 0;
			wifi_vap_del_rule(guest);
			wifi_set_vap_allowed(guest, NLK_ACTION_ADD);
			guest->authon = 1;
		}
		break;
	case NLK_ACTION_DEL:
		host = (struct iwguest *)data;
		list_for_each_entry(guest, &vhost, list) {
			if (memcmp(guest->macaddr, host->mac, ETH_ALEN))
				continue;
			wifi_vap_black_rule_action(guest, NLK_ACTION_ADD);
			guest->authon = 0;
			guest->authoff = 1;
			wifi_set_vap_allowed(guest, NLK_ACTION_DEL);
		}
		break;
	case NLK_ACTION_DUMP:
		info = (struct host_info *)data;
		list_for_each_entry(guest, &vhost, list) {
			if (!guest->online)
				continue;
			if (dump_host_info(guest->addr, &tmp))
				continue;
			memcpy(&info[cnt], &tmp, sizeof(tmp));
			if (guest->authon)
				info[cnt].is_wifi = 1;
			else
				info[cnt].is_wifi = 0;
			cnt++;
		}
		return cnt;
	default:
		return -1;
	}
	return 0;
}

static int wifi_vap_host_clean(unsigned char ifindex)
{
	struct vap_host *guest, *tmp;

	list_for_each_entry_safe(guest, tmp, &vhost, list) {
		if (guest->ifindex != ifindex)
			continue;
		wifi_vap_del_rule(guest);
		list_del(&guest->list);
		free(guest);
		vhostnum--;
	}
	return 0;
}

static int wifi_wechat_clean(unsigned char ifindex)
{
	struct vap_host *guest;
	
	list_for_each_entry(guest, &vhost, list) {
		if (!guest->passid[0] || guest->ifindex != ifindex)
			continue;
		guest->authon = 0;
		memset(guest->passid, 0x0, sizeof(guest->passid));
		wifi_set_vap_allowed(guest, NLK_ACTION_DEL);
		wifi_vap_wchat_evnet_action(guest, NLK_ACTION_DEL);
		if (!guest->online)
			continue;
		wifi_vap_add_rule(cfg[ifindex].wechat, guest);
	}
	return 0;
}

/*allow guest to use net int one miniute*/
static int wifi_wechat_allow(struct iwguest *host)
{
	struct vap_host *guest;
	struct wifi_conf *config;
	
	list_for_each_entry(guest, &vhost, list) {
		if (memcmp(guest->macaddr, host->mac, ETH_ALEN))
			continue;
		config = &cfg[guest->ifindex];
		if (!config->wechat || guest->authon
			|| guest->authoff || guest->allownr >= 10)
			break;
		return wifi_vap_wchat_evnet_action(guest, NLK_ACTION_ADD);
	}
	return 0;
}

static int wifi_wechat_dump(void *data)
{
	int cnt = 0;
	struct iwguest *info;
	struct vap_host *guest;
	
	info = (struct iwguest *)data;
	list_for_each_entry(guest, &vhost, list) {
		if (!guest->passid[0])
			continue;
		arr_strcpy(info[cnt].wechatid, guest->passid);
		memcpy(info[cnt].mac, guest->macaddr, ETH_ALEN);
		cnt++;
	}
	return cnt;
}

/*vap change to ap and not recv online msg, del the vap rule*/
static void wifi_ap_change_cb(void *data)
{
	int nr = 0;
	struct timeval tv;
	struct host_info *info;
	struct vap_host *guest;

	info = dump_host_alive(&nr);
	if (!info)
		goto next;
	loop_for_each(0, nr) {
		if (!info[i].is_wifi)
			continue;
		if ((info[i].pid != 1) && (info[i].pid != WIFI_5G_BASIC_ID + 1))
			continue;
		list_for_each_entry(guest, &vhost, list) {
			if (memcmp(info[i].mac, guest->macaddr, ETH_ALEN))
				continue;
			guest->online = 0;
			wifi_vap_del_rule(guest);
			if (!guest->authon && !guest->authoff) {
				vhostnum--;
				list_del(&guest->list);
				free(guest);
			}
			goto find;
		}
	} loop_for_each_end();
find:
	free(info);
next:
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if (!schedule(tv, wifi_ap_change_cb, NULL))
		MU_DEBUG("schedule ap change event error\n");
}

static int wifi_set_ap(struct wifi_conf *config)
{
	int ifindex;
	struct timeval tv;
	struct schedule_entry *tmp = NULL;

#ifdef ZHIBOTONG_NEED
	if (strncmp(config->ssid, "CMCC-", 5))
		return -2;
#endif
#ifdef BL360_OER_NEED
	if (strncmp(config->ssid, "ChinaNet-", 9))
		return -2;
#endif

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	ifindex = config->ifindex;
	/*scaning not support wifi set*/
	if (scannng[ifindex])
		return -1;
	tmp = schedule(tv, wifi_ap_cb, &cfg[ifindex]);
	if (!tmp)
		return -1;
	if (wifi_ap[ifindex]) {
		dschedule(wifi_ap[ifindex]);
		wifi_ap[ifindex] = NULL;
	}
	if (wifi_vap[ifindex]) {
		dschedule(wifi_vap[ifindex]);
		wifi_vap[ifindex] = NULL;
	}
	wifi_ap[ifindex] = tmp;
	return 0;
}

static int wifi_set_vap(struct wifi_conf *config)
{
	int ifindex;
	struct timeval tv;
	struct schedule_entry *tmp = NULL;

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	ifindex = config->ifindex;
	/*scaning not support wifi set*/
	if (scannng[ifindex])
		return -1;
	tmp = schedule(tv, wifi_vap_cb, &cfg[ifindex]);
	if (!tmp)
		return -1;
	if (wifi_vap[ifindex]) {
		dschedule(wifi_vap[ifindex]);
		wifi_vap[ifindex] = NULL;
	}
	wifi_vap[ifindex] = tmp;
	return 0;
}

static int wifi_set_time(struct wifi_conf *config)
{
	int ifindex;
	struct timeval tv;
	struct schedule_entry *tmp = NULL;

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	ifindex = config->ifindex;
	/*scaning not support wifi set*/
	if (scannng[ifindex])
		return -1;
	tmp = schedule(tv, wifi_time_cb, &cfg[ifindex]);
	if (!tmp)
		return -1;
	if (wifi_time[ifindex]) {
		dschedule(wifi_time[ifindex]);
		wifi_time[ifindex] = NULL;
	}
	wifi_time[ifindex] = tmp;
	return 0;
}

static int wifi_set_acl(struct iwacl *acl)
{
	struct timeval tv;
	struct schedule_entry *tmp = NULL;

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	/*scaning not support wifi set*/
	if (scannng[acl->ifindex])
		return -1;
	tmp = schedule(tv, wifi_acl_cb, &wacl[acl->ifindex]);
	if (!tmp)
		return -1;
	if (wifi_acl[acl->ifindex]) {
		dschedule(wifi_acl[acl->ifindex]);
		wifi_acl[acl->ifindex] = NULL;
	}
	wifi_acl[acl->ifindex] = tmp;
	return 0;
}

static void wifi_restart(void)
{
	char datpath[IGD_NAME_LEN];

	if (!access("/etc/wireless", F_OK))
		arr_strcpy(datpath, "/etc/wireless");
	else if (!access("/etc/Wireless", F_OK))
		arr_strcpy(datpath, "/etc/Wireless");
	else
		return;
	loop_for_each(0, IW_IF_MX) {
		if (!cfg[i].valid)
			continue;
		exec_cmd("uci2dat -d %s -f %s/%s/%s.dat", cfg[i].devname,
						datpath, cfg[i].devname, cfg[i].devname);
		exec_cmd("ifconfig %s up", cfg[i].apname);
		if (cfg[i].vap)
			exec_cmd("ifconfig %s up", cfg[i].vapname);
		exec_cmd("ifconfig %s up", cfg[i].apcname);
		exec_cmd("brctl addif %s %s", LAN_DEVNAME, cfg[i].apname);
		exec_cmd("brctl addif %s %s", LAN_DEVNAME, cfg[i].vapname);
		wifi_time_cb(&cfg[i]);
		wifi_acl_cb(&wacl[i]);
	} loop_for_each_end();
	wifi_set_led();
	wifi_ap_change_cb(NULL);
}

static int wifi_cfg_check(void)
{
	char *retstr;
	char cmd[128];
	struct wifi_conf *apcfg;
	char *country = NULL;
	
	loop_for_each(0, IW_IF_MX) {
		apcfg = &cfg[i];
		if (!apcfg->valid)
			continue;
		if (apcfg->channel < 0) {
			apcfg->channel = 0;
			snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].channel=0",
										WIFI_CONFIG_FILE, apcfg->ifindex);
			uuci_set(cmd);
		} else if (apcfg->channel > 0) {
			snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].auotch",
										WIFI_CONFIG_FILE, apcfg->ifindex);
			retstr = uci_getval_cmd(cmd);
			if (atoi(retstr) != 0) {
				snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].auotch=0",
										WIFI_CONFIG_FILE, apcfg->ifindex);
				uuci_set(cmd);
			}
			free(retstr);
		}
		if (apcfg->time_on < 0) {
			apcfg->time_on = 0;
			if (apcfg->ifindex == 0)
				uuci_add(WIFI_CONFIG_FILE, "wifi-time");
			snprintf(cmd, 127, "%s.@wifi-time[0].time%d=0,0,0,0,0,0,0",
										WIFI_CONFIG_FILE, apcfg->ifindex);
			uuci_set(cmd);
		}
		snprintf(cmd, 127, "%s.@wifi-iface[%d].rebind=%d",
						WIFI_CONFIG_FILE, apcfg->vapindex,
						apcfg->rebind);
		uuci_set(cmd);
		if (!apcfg->ifindex) {
			snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].region",
									WIFI_CONFIG_FILE, apcfg->ifindex);
			retstr = uci_getval_cmd(cmd);
			if (retstr) {
				free(retstr);
				continue;
			}
			country = read_firmware("REGION");
			if (!country[0])
				continue;
			snprintf(cmd, 127, "%s.@wifi-device[%d].region=%s",
					WIFI_CONFIG_FILE, apcfg->ifindex, country);
			apcfg->country = atoi(country);
			uuci_set(cmd);
		} else {
			snprintf(cmd, sizeof(cmd), "%s.@wifi-device[%d].aregion",
									WIFI_CONFIG_FILE, apcfg->ifindex);
			retstr = uci_getval_cmd(cmd);
			if (retstr) {
				free(retstr);
				continue;
			}
			country = read_firmware("AREGION");
			if (!country[0])
				continue;
			snprintf(cmd, 127, "%s.@wifi-device[%d].aregion=%s",
					WIFI_CONFIG_FILE, apcfg->ifindex, country);
			apcfg->country = atoi(country);
			uuci_set(cmd);
		}
	} loop_for_each_end();
	
	/*del old config for midea*/
	snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[2].ssid", WIFI_CONFIG_FILE);
	retstr = uci_getval_cmd(cmd);
	if (retstr && !strcmp(retstr, "md12345678")) {
		snprintf(cmd, sizeof(cmd), "%s.@wifi-iface[2]", WIFI_CONFIG_FILE);
		uuci_delete(cmd);
	}
	if (retstr)
		free(retstr);
	return 0;
}

static int wifi_init_device(struct uci_section *section)
{
	struct uci_element *oe;
	struct uci_option *option;
	struct wifi_conf *apcfg = NULL;
	
	loop_for_each(0, IW_IF_MX) {
		if (!cfg[i].valid) {
			apcfg = &cfg[i];
			apcfg->valid = 1;
			apcfg->ifindex = i;
			break;
		}
	} loop_for_each_end();
	if (!apcfg)
		return 0;
	uci_foreach_element(&section->options, oe) {
		option = uci_to_option(oe);
		if (option == NULL || option->type != UCI_TYPE_STRING)
			continue;
		if (!strcmp(oe->name, "type"))
			arr_strcpy(apcfg->devname, option->v.string);
		else if (!strcmp(oe->name, "enable")) 
			apcfg->enable = atoi(option->v.string);
		else if (!strcmp(oe->name, "txpoer"))
			apcfg->txrate = atoi(option->v.string);
		else if (!strcmp(oe->name, "channel")
			&& strcmp(option->v.string, "auto"))
			apcfg->channel = atoi(option->v.string);
		else if (!strcmp(oe->name, "bw"))
			apcfg->htbw = atoi(option->v.string);
		else if (!strcmp(oe->name, "vbw"))
			apcfg->htbw = atoi(option->v.string);
		else if (!strcmp(oe->name, "wifimode"))
			apcfg->mode = atoi(option->v.string);
		else if (!strcmp(oe->name, "region"))
			apcfg->country = atoi(option->v.string);
		else if (!strcmp(oe->name, "aregion"))
			apcfg->country = atoi(option->v.string);
		else if (!strcmp(oe->name, "wmm"))
			apcfg->wmmcapable= atoi(option->v.string);
		else if (!strcmp(oe->name, "noforward"))
			apcfg->nohostfoward= atoi(option->v.string);
		else if (!strcmp(oe->name, "noforwardbssid"))
			apcfg->noapforward = atoi(option->v.string);
	}
	return 0;
}

static int wifi_init_iface(struct uci_section *section)
{
	char *p;
	int iflen;
	int i = 0, j = 0;
	struct uci_element *e;
	struct uci_element *oe;
	struct uci_option *option;
	struct wifi_conf *apcfg = NULL;
	struct iwacl *apacl = NULL;
	static int ifapindex = 0;
	
	uci_foreach_element(&section->options, oe) {
		option = uci_to_option(oe);
		if (option == NULL || option->type != UCI_TYPE_STRING)
			continue;
		if (!strcmp(oe->name, "device")) {
			apcfg = wifi_get_dev(option->v.string);
			if (!apcfg)
				return -1;
			apacl = &wacl[apcfg->ifindex];
			apacl->ifindex = apcfg->ifindex;
		} else if (!strcmp(oe->name, "ifname")) {
			iflen = strlen(option->v.string);
			if (option->v.string[iflen - 1] != '0') {
				ifapindex++;
				goto guest;
			}
			break;
		}
	}
	if (!apcfg || !apacl)
		return -1;
	uci_foreach_element(&section->options, oe) {
		option = uci_to_option(oe);
		if (option == NULL)
			continue;
		if (!strcmp(oe->name, "ifname") && (option->type == UCI_TYPE_STRING)) 
			arr_strcpy(apcfg->apname, option->v.string);
		else if (!strcmp(oe->name, "ssid") && (option->type == UCI_TYPE_STRING)) 
			arr_strcpy(apcfg->ssid, option->v.string);
		else if (!strcmp(oe->name, "encryption") && (option->type == UCI_TYPE_STRING))
			arr_strcpy(apcfg->encryption, option->v.string);
		else if (!strcmp(oe->name, "key") && (option->type == UCI_TYPE_STRING))
			arr_strcpy(apcfg->key, option->v.string);
		else if (!strcmp(oe->name, "hidden") && (option->type == UCI_TYPE_STRING))
			apcfg->hidssid = atoi(option->v.string);
		else if (!strcmp(oe->name, "wlisten") && (option->type == UCI_TYPE_STRING))
			apacl->wlist.enable = atoi(option->v.string);
		else if (!strcmp(oe->name, "blisten") && (option->type == UCI_TYPE_STRING))
			apacl->blist.enable = atoi(option->v.string);
		else if (!strcmp(oe->name, "wlist") && (option->type == UCI_TYPE_LIST)) {
			uci_foreach_element(&option->v.list, e) {
				parse_mac(e->name, apacl->wlist.list[i].mac);
				p = strchr(e->name, ' ');
				if (!p)
					continue;
				p++;
				apacl->wlist.list[i].enable = atoi(p);
				i++;
			}
			apacl->wlist.nr = i;
		} else if (!strcmp(oe->name, "blist") && (option->type == UCI_TYPE_LIST)) {
			uci_foreach_element(&option->v.list, e) {
				parse_mac(e->name, apacl->blist.list[j].mac);
				p = strchr(e->name, ' ');
				if (!p)
					continue;
				p++;
				apacl->blist.list[j].enable = atoi(p);
				j++;
			}
			apacl->blist.nr = j;
		}
	}
	apcfg->apindex = ifapindex;
	set_dev_vlan(apcfg->apname, 1);
	ifapindex++;
	return 0;
guest:
	if (!apcfg || apcfg->vapindex)
		return 0;
	uci_foreach_element(&section->options, oe) {
		option = uci_to_option(oe);
		if (option == NULL || option->type != UCI_TYPE_STRING)
			continue;
		if (!strcmp(oe->name, "ifname"))
			arr_strcpy(apcfg->vapname, option->v.string);
		else if (!strcmp(oe->name, "enable"))
			apcfg->vap = atoi(option->v.string);
		else if (!strcmp(oe->name, "ssid"))
			arr_strcpy(apcfg->vssid, option->v.string);
		else if (!strcmp(oe->name, "hidden"))
			apcfg->hidvssid = atoi(option->v.string);
		else if (!strcmp(oe->name, "wechat"))
			apcfg->wechat = atoi(option->v.string);
		else if (!strcmp(oe->name, "rebind"))
			apcfg->rebind = atoi(option->v.string);
		else if (!strcmp(oe->name, "aliset"))
			apcfg->aliset = atoi(option->v.string);
		else if (!strcmp(oe->name, "encryption"))
			arr_strcpy(apcfg->vencryption, option->v.string);
		else if (!strcmp(oe->name, "key"))
			arr_strcpy(apcfg->vkey, option->v.string);
	}
	apcfg->vapindex = apcfg->apindex + 1;
	set_dev_vlan(apcfg->vapname, 2);
	return 0;
}

static int wifi_init_time(struct uci_section *section)
{
	int len, ifindex;
	struct uci_element *oe;
	struct uci_option *option;
	struct wifi_conf *apcfg;
	struct time_comm *tm;
	
	uci_foreach_element(&section->options, oe) {
		option = uci_to_option(oe);
		if (option == NULL || option->type != UCI_TYPE_STRING)
			continue;
		if (strncmp(oe->name, "time", 4))
			continue;
		len = strlen(oe->name);
		if (len == 4)
			ifindex = 0;
		else
			ifindex = atoi(&oe->name[4]);
		if (ifindex >= IW_IF_MX)
			continue;
		apcfg = &cfg[ifindex];
		tm = &apcfg->tm;
		sscanf(option->v.string, "%d,%hhd,%hhd,%hhd,%hhd,%hhd,%hhd",
				&apcfg->time_on, &tm->loop, &tm->day_flags, &tm->start_hour,
				&tm->start_min,&tm->end_hour, &tm->end_min);
	}
	return 0;
}

static void wifi_cfg_default(void)
{
	int flag = 0;
	
	flag = get_sysflag(SYSFLAG_RESET);
	if (flag != 1)
		flag = get_sysflag(SYSFLAG_RECOVER);
	if (flag != 1)
		flag = 0;
	memset(&cfg, 0x0, sizeof(cfg));
	memset(&wacl, 0x0, sizeof(wacl));
	loop_for_each(0, IW_IF_MX) {
		cfg[i].htbw = 1;
		cfg[i].time_on = -1;
		cfg[i].enable = 1;
		cfg[i].txrate = 100;
		cfg[i].channel = -1;
		cfg[i].rebind = flag;
		cfg[i].wmmcapable = 1;
		cfg[i].nohostfoward = 0;
		cfg[i].noapforward = 0;
		wacl[i].blist.enable = 1;
	} loop_for_each_end();
	INIT_LIST_HEAD(&vhost);
	cfg[0].country = 1;
	cfg[1].country = 5;
	cfg[0].mode = MODE_802_11_BGN;
	cfg[1].mode = MODE_802_11_AANAC;
	arr_strcpy(cfg[0].apcname, APCLI_DEVNAME);
	arr_strcpy(cfg[1].apcname, APCLII_DEVNAME);
}

static int wifi_init(void)
{
	struct uci_element *se;
	struct uci_section *section;
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd <= 0) {
		MU_DEBUG("%s:%d create sock failed\n", __FUNCTION__, __LINE__);
		return -1;
	}
	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, WIFI_CONFIG_FILE, &pkg)) {
		uci_free_context(ctx);
		close(sockfd);
		return -1;
	}
	wifi_cfg_default();
	uci_foreach_element(&pkg->sections, se) {
		section = uci_to_section(se);
		if (section == NULL)
			continue;
		if (!strcmp(section->type, "wifi-device"))
			wifi_init_device(section);
		else if (!strcmp(section->type, "wifi-iface"))
			wifi_init_iface(section);
		else if (!strcmp(section->type, "wifi-time"))
			wifi_init_time(section);
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx);
	wifi_cfg_check();
	wifi_restart();
	set_dev_vlan(WIRE_DEVNAME, 1);
	return 0;
}

int wifi_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;
	struct wifi_conf wcfg;
	
	switch (msgid) {
	case WIFI_MOD_INIT:
		ret = wifi_init();
		break;
	case WIFI_MOD_SET_AP:
		if (!data || len != sizeof(struct wifi_conf))
			return -1;
		ret = wifi_set_ap((struct wifi_conf *)data);
		if (!ret)
			wifi_save_ap((struct wifi_conf *)data);
		break;
	case WIFI_MOD_SET_VAP:
		if (!data || len != sizeof(struct wifi_conf))
			return -1;
		ret = wifi_set_vap((struct wifi_conf *)data);
		if (!ret)
			wifi_save_guest((struct wifi_conf *)data);
		break;
	case WIFI_MOD_SET_TIME:
		if (!data || len != sizeof(struct wifi_conf))
			return -1;
		ret = wifi_set_time((struct wifi_conf *)data);
		if (!ret)
			wifi_save_time((struct wifi_conf *)data);
		break;
	case WIFI_MOD_GET_CONF:
		if (!rbuf || rlen != sizeof(struct wifi_conf))
			return -1;
		ret = wifi_get_conf(0, (struct wifi_conf *)rbuf);
		break;
	case WIFI_MOD_GET_CONF_5G:
		if (!rbuf || rlen != sizeof(struct wifi_conf))
			return -1;
		ret = wifi_get_conf(1, (struct wifi_conf *)rbuf);
		break;
	case WIFI_MOD_SET_CHANNEL:
		if (!data || len != sizeof(int))
			return -1;
		memcpy(&wcfg, &cfg[0], sizeof(wcfg));
		wcfg.channel = *(int *)data;
		ret = wifi_set_channel(&wcfg);
		if (!ret)
			wifi_save_channel(&wcfg);
		break;
	case WIFI_MOD_SET_CHANNEL_5G:
		if (!data || len != sizeof(int))
			return -1;
		if (!cfg[1].valid)
			return -1;
		memcpy(&wcfg, &cfg[1], sizeof(wcfg));
		wcfg.channel = *(int *)data;
		ret = wifi_set_channel(&wcfg);
		if (!ret)
			wifi_save_channel(&wcfg);
		break;
	case WIFI_MOD_SET_TXRATE:
		if (!data || len != sizeof(int))
			return -1;
		memcpy(&wcfg, &cfg[0], sizeof(wcfg));
		wcfg.txrate = *(int *)data;
		ret = wifi_set_txrate(&wcfg);
		if (!ret)
			wifi_save_txpoer(&wcfg);
		break;
	case WIFI_MOD_SET_TXRATE_5G:
		if (!data || len != sizeof(int))
			return -1;
		if (!cfg[1].valid)
			return -1;
		memcpy(&wcfg, &cfg[1], sizeof(wcfg));
		wcfg.txrate = *(int *)data;
		ret = wifi_set_txrate(&wcfg);
		if (!ret)
			wifi_save_txpoer(&wcfg);
		break;
	case WIFI_MOD_GET_CHANNEL:
		if (!rbuf || rlen != sizeof(int))
			return -1;
		ret = wifi_get_channel(&cfg[0], (int *)rbuf);
		break;
	case WIFI_MOD_GET_CHANNEL_5G:
		if (!rbuf || rlen != sizeof(int))
			return -1;
		if (!cfg[1].valid)
			return -1;
		ret = wifi_get_channel(&cfg[1], (int *)rbuf);
		break;
	case WIFI_MOD_GET_SURVEY:
		if (!data || len!= sizeof(int) || !rbuf || rlen != sizeof(struct iwsurvey) * IW_SUR_MX)
			return -1;
		ret = wifi_get_survey(&cfg[0], *(int *)data, (struct iwsurvey *)rbuf);
		break;
	case WIFI_MOD_GET_SURVEY_5G:
		if (!data || len!= sizeof(int) || !rbuf || rlen != sizeof(struct iwsurvey) * IW_SUR_MX)
			return -1;
		if (!cfg[1].valid)
			return -1;
		ret = wifi_get_survey(&cfg[1], *(int *)data, (struct iwsurvey *)rbuf);
		break;
	case WIFI_MOD_VAP_HOST_ONLINE:
		if (!data || len != sizeof(struct nlk_host))
			return -1;
		ret = wifi_vap_host_online((struct nlk_host *)data);
		break;
	case WIFI_MOD_VAP_HOST_ADD:
		if (!data || len != sizeof(struct iwguest))
			return -1;
		ret = wifi_vap_host_action(data, NLK_ACTION_ADD);
		break;
	case WIFI_MOD_VAP_HOST_DEL:
		if (!data || len != sizeof(struct iwguest))
			return -1;
		ret = wifi_vap_host_action(data, NLK_ACTION_DEL);
		break;
	case WIFI_MOD_VAP_HOST_DUMP:
		if (!rbuf || rlen != sizeof(struct host_info) * HOST_MX)
			return -1;
		ret = wifi_vap_host_action(rbuf, NLK_ACTION_DUMP);
		break;
	case WIFI_MOD_DISCONNCT_ALL:
		loop_for_each(0, IW_IF_MX) {
			if (!cfg[i].valid)
				continue;
			wifi_disconn_all(&cfg[i]);
		} loop_for_each_end();
		ret = 0;
		break;
	case WIFI_MOD_DUMP_STATUS:
		if (!rbuf || rlen != sizeof(int))
			return -1;
		*(int *)rbuf = 1;
		loop_for_each(0, IW_IF_MX) {
			if (!cfg[i].valid)
				continue;
			if (!wifi_lock[i])
				*(int *)rbuf = 0;
		} loop_for_each_end();
		ret = 0;
		break;
	case WIFI_MOD_SET_MODE:
		if (!data || len != sizeof(int))
			return -1;
		memcpy(&wcfg, &cfg[0], sizeof(wcfg));
		wcfg.mode = *(int *)data;
		ret = wifi_set_mode(&wcfg);
		if (!ret)
			wifi_save_mode(&wcfg);
		break;
	case WIFI_MOD_SET_MODE_5G:
		if (!data || len != sizeof(int))
			return -1;
		if (!cfg[1].valid)
			return -1;
		memcpy(&wcfg, &cfg[1], sizeof(wcfg));
		wcfg.mode = *(int *)data;
		ret = wifi_set_mode(&wcfg);
		if (!ret)
			wifi_save_mode(&wcfg);
		break;
	case WIFI_MOD_SET_ACL:
		if (!data || len != sizeof(struct iwacl))
			return -1;
		ret = wifi_set_acl((struct iwacl *)data);
		if (!ret)
			wifi_save_acl((struct iwacl *)data);
		break;
	case WIFI_MOD_SET_ACL_5G:
		if (!data || len != sizeof(struct iwacl))
			return -1;
		if (!cfg[1].valid)
			return -1;
		ret = wifi_set_acl((struct iwacl *)data);
		if (!ret)
			wifi_save_acl((struct iwacl *)data);
		break;
	case WIFI_MOD_GET_ACL:
		if (!rbuf || rlen != sizeof(struct iwacl))
			return -1;
		memcpy(rbuf, &wacl[0], rlen);
		ret = 0;
		break;
	case WIFI_MOD_GET_ACL_5G:
		if (!rbuf || rlen != sizeof(struct iwacl))
			return -1;
		if (!cfg[1].valid)
			return -1;
		memcpy(rbuf, &wacl[1], rlen);
		ret = 0;
		break;
	case WIFI_MOD_VAP_WECHAT_ALLOW:
		if (!data || len != sizeof(struct iwguest))
			return -1;
		ret = wifi_wechat_allow((struct iwguest *)data);
		break;
	case WIFI_MOD_VAP_HOST_WECHAT_CLEAN:
		if (!data || len != sizeof(unsigned char))
			return -1;
		ret = wifi_wechat_clean(*(unsigned char *)data);
		break;
	case WIFI_MOD_VAP_HOST_WECHAT_DUMP:
		if (!rbuf || rlen != sizeof(struct iwguest) * HOST_MX)
			return -1;
		ret = wifi_wechat_dump(rbuf);
		break;
	case WIFI_MOD_VAP_HOST_OFFLINE:
		if (!data || len != sizeof(struct nlk_host))
			return -1;
		ret = wifi_vap_host_offline((struct nlk_host *)data);
		break;
	case WIFI_MOD_GET_STA_INFO:
		if (!data || len != sizeof(int) || !rbuf
			|| rlen != sizeof(struct sta_status))
			return -1;
		ret = wifi_get_sta(*(int *)data, (struct sta_status *)rbuf);
		break;
	default:
		break;
	}
	return ret;
}
