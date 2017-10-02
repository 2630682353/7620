#include "igd_lib.h"
#include "igd_wifi.h"
#include "igd_interface.h"
#include <linux/wireless.h>

enum {
	WISP_INIT = 0,
	WISP_UP,
	WISP_DOWN,
};

struct wisp_config {
	char ssid[32];
	char bssid[32];
	char encrypt[32];
	char key[32];
	int channel;
};

static int sockfd;
static int retry;
static int status;
static int timeout;
static struct wisp_config wconfig;

#if 0
#define WISP_DEBUG(fmt,args...) do{}while(0)
#else
//#define WISP_DEBUG(fmt,args...) console_printf("%s :[%d]:"fmt, __FUNCTION__, __LINE__, ##args)
#define WISP_DEBUG(fmt, args...) igd_log("/tmp/wisp.log", fmt, ##args)
#endif

#define OID_802_11_RTPRIV_IOCTL_APCLI_STATUS	0x0950
#define RT_PRIV_IOCTL 0x8BE1

static int wisp_ipup(void)
{
	int uid = 1;
	int wispstat = ISP_STAT_WISP_FINISH;
	struct iface_conf ifcfg;

	memset(&ifcfg, 0x0, sizeof(ifcfg));
	if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &ifcfg, sizeof(ifcfg)))
		return -1;
	WISP_DEBUG("wisp ip up, bridge is %d\n", ifcfg.isbridge);
	mu_msg(IF_MOD_ISP_STATUS, &wispstat, sizeof(int), NULL, 0);
	if (ifcfg.isbridge)
		return exec_cmd("udhcpc -p /var/run/udhcpc.pid -f -t 0 -i %s &", LAN_DEVNAME);
	else
		return exec_cmd("udhcpc -p /var/run/udhcpc.pid -f -t 0 -i %s &", APCLI_DEVNAME);
}

static int wisp_ipdown(void)
{
	int uid = 1;
	struct iface_conf ifcfg;
	struct sys_msg_ipcp wisp;

	WISP_DEBUG("wisp ip down\n");
	exec_cmd("killall -kill udhcpc");
	memset(&wisp, 0, sizeof(wisp));
	memset(&ifcfg, 0x0, sizeof(ifcfg));
	if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &ifcfg, sizeof(ifcfg)))
		return -1;
	if (ifcfg.isbridge)
		arr_strcpy(wisp.devname, LAN_DEVNAME);
	else
		arr_strcpy(wisp.devname, APCLI_DEVNAME);
	wisp.uid = 1;
	wisp.type = IF_TYPE_WAN;
	return mu_msg(IF_MOD_IFACE_IP_DOWN, &wisp, sizeof(wisp), NULL, 0);
}

static int set_iwcmd(char *cmd)
{
	char cmdline[1024];
	snprintf(cmdline, sizeof(cmdline), "iwpriv %s set %s", APCLI_DEVNAME, cmd);
	return system(cmdline);
}

static int wisp_get_status(char *ssid)
{
	char *p;
	char buf[256];
	struct iwreq wrq;

	memset(&wrq, 0x0, sizeof(wrq));
	memset(buf, 0x0, sizeof(buf));
	strncpy(wrq.ifr_ifrn.ifrn_name, APCLI_DEVNAME, sizeof(wrq.ifr_ifrn.ifrn_name));
	wrq.u.data.pointer = (caddr_t)buf;
	wrq.u.data.length = sizeof(buf);
	wrq.u.data.flags = OID_802_11_RTPRIV_IOCTL_APCLI_STATUS;
	
	if (ioctl(sockfd, RT_PRIV_IOCTL, &wrq) < 0) {
		WISP_DEBUG("get connstatus failed, %s\n", buf);
		return -1;
	}
	if (strstr(buf, "Disconnect")) {
		//WISP_DEBUG("status:%s\n", buf);
		return WISP_DOWN;
	}
	if (!(p = strstr(buf, "SSID:"))) {
		//WISP_DEBUG("status:%s\n", buf);
		return WISP_DOWN;
	}
	p += 5;
	if (strcmp(p, ssid)) {
		//WISP_DEBUG("status:%s\n", buf);
		return WISP_DOWN;
	}
	//WISP_DEBUG("status:%s\n", buf);
	if (wconfig.bssid[0])
		return WISP_UP;
	strncpy(wconfig.bssid, buf, 17);
	WISP_DEBUG("bssid %s\n", wconfig.bssid);
	return WISP_UP;
}

static int wisp_set_channel(int channel)
{
	int i, ret = -1;
	struct iwreq wrq;

	memset(&wrq, 0x0, sizeof(wrq));
	strncpy(wrq.ifr_ifrn.ifrn_name, APCLI_DEVNAME, sizeof(wrq.ifr_ifrn.ifrn_name));
	if (ioctl(sockfd, SIOCGIWFREQ, &wrq) < 0) {
		WISP_DEBUG("get channel failed\n");
		return -1;
	}
	ret = wrq.u.freq.m;
	for(i = 0; i < wrq.u.freq.e; i++)
		ret *= 10;
	if (ret == channel)
		return 0;
	if (mu_msg(WIFI_MOD_SET_CHANNEL, &channel, sizeof(channel), NULL, 0))
		return -1;
	return 0;
}

static int wisp_set_security(char *encrypt, char *key, char *ssid)
{
	char cmd[128];
	char auth[32];
	char encryp[32];
	
	if (!strncmp(encrypt, "NONE", 4)) {
		snprintf(cmd, sizeof(cmd), "ApCliAuthMode=OPEN");
		set_iwcmd(cmd);
		snprintf(cmd, sizeof(cmd), "ApCliEncrypType=NONE");
		set_iwcmd(cmd);
		return 0;
	}
	if (strlen(key) < 8)
		return 0;
	if (strstr(encrypt, "WPA1PSKWPA2PSK"))
		snprintf(auth, sizeof(auth), "WPAPSKWPA2PSK");
	else if (strstr(encrypt, "WPA2PSK"))
		snprintf(auth, sizeof(auth), "WPA2PSK");
	else if (strstr(encrypt, "WPAPSK"))
		snprintf(auth, sizeof(auth), "WPAPSK");
	else 
		return -1;
	if (strstr(encrypt, "TKIPAES"))
		snprintf(encryp, sizeof(encryp), "TKIPAES");
	else if (strstr(encrypt, "TKIP"))
		snprintf(encryp, sizeof(encryp), "TKIP");
	else if (strstr(encrypt, "AES"))
		snprintf(encryp, sizeof(encryp), "AES");
	else
		return -1;
	WISP_DEBUG("auth is %s, encrypt is %s\n", auth, encryp);
	snprintf(cmd, sizeof(cmd) -1, "ApCliAuthMode=%s", auth);
	set_iwcmd(cmd);
	snprintf(cmd, sizeof(cmd) -1, "ApCliEncrypType=%s", encryp);
	set_iwcmd(cmd);
	snprintf(cmd, sizeof(cmd) -1, "ApCliSsid=\"%s\"", ssid);
	set_iwcmd(cmd);
	snprintf(cmd, sizeof(cmd) -1, "ApCliWPAPSK=\"%s\"", key);
	set_iwcmd(cmd);
	return 0;
}

static int wisp_up(struct wisp_config *cfg)
{
	char cmd[128];
	struct wifi_conf wcfg;

	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &wcfg, sizeof(wcfg)))
		return -1;
	if (wcfg.htbw) {
		snprintf(cmd, sizeof(cmd), "iwpriv %s set HtBw=0", wcfg.apname);
		system(cmd);
		snprintf(cmd, sizeof(cmd), "iwpriv %s set SSID=\"%s\"", wcfg.apname, wcfg.ssid);
		system(cmd);
	}
	if (wisp_set_channel(cfg->channel)) {
		WISP_DEBUG("wisp_set_channel faild, channel %d\n", cfg->channel);
		return -1;	
	}
	set_iwcmd("ApCliEnable=0");
	if (wisp_set_security(cfg->encrypt, cfg->key, cfg->ssid)) {
		WISP_DEBUG("wisp_set_security faild, encrypt is %s, key is %s\n", cfg->encrypt, cfg->key);
		return -1;
	}
	snprintf(cmd, sizeof(cmd) -1, "ApCliSsid=\"%s\"", cfg->ssid);
	set_iwcmd(cmd);
	set_iwcmd("ApCliEnable=1");
	return 0;
}

static int wisp_down(void)
{
	char cmd[128];
	struct wifi_conf wcfg;
	
	set_iwcmd("ApCliEnable=0");
	if (mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &wcfg, sizeof(wcfg)))
		return -1;
	if (wcfg.htbw) {
		snprintf(cmd, sizeof(cmd), "iwpriv %s set HtBw=1", wcfg.apname);
		system(cmd);
		if (wcfg.channel > 0) {
			if (wcfg.channel < 6)
				snprintf(cmd, sizeof(cmd), "iwpriv %s set HT_EXTCHA=1", wcfg.apname);
			else
				snprintf(cmd, sizeof(cmd), "iwpriv %s set HT_EXTCHA=0", wcfg.apname);
			system(cmd);
		}
	}
	if (!wcfg.channel)
		return 0;
	snprintf(cmd, sizeof(cmd), "iwpriv %s set Channel=%d", wcfg.apname, wcfg.channel);
	system(cmd);
	return 0;
}

static int wisp_reconnect(struct wisp_config *cfg)
{
	char cmd[128] = {0};
	char nencrypt[32] = {0};
	int nr, scan = 1, nch;
	struct iwsurvey survey[IW_SUR_MX];
	
	WISP_DEBUG("rescan the ssid %s info\n", cfg->ssid);
	nr = mu_msg(WIFI_MOD_GET_SURVEY, &scan, sizeof(int), survey, sizeof(struct iwsurvey) * IW_SUR_MX);
	if (nr > 0)
		goto finish;
	usleep(8000000);
	scan = 0;
	nr = mu_msg(WIFI_MOD_GET_SURVEY, &scan, sizeof(int), survey, sizeof(struct iwsurvey) * IW_SUR_MX);
	if (nr <= 0)
		goto error;
finish:
	loop_for_each(0, nr) {
		if (!strcmp(survey[i].ssid, cfg->ssid) || !strcmp(survey[i].bssid, cfg->bssid)) {
			arr_strcpy(nencrypt, survey[i].security);
			nch = survey[i].ch;
			goto find;
		}
	}loop_for_each_end();
	WISP_DEBUG("rescan the ssid %s info falid\n",  cfg->ssid);
	goto error;
find:
	if (strcmp( cfg->encrypt, nencrypt)) {
		snprintf(cmd, sizeof(cmd) - 1, "%s.wan.security=%s", IF_CONFIG_FILE, nencrypt);
		uuci_set(cmd);
		arr_strcpy(cfg->encrypt, nencrypt);
		WISP_DEBUG("encrypt changed to %s need reconnect\n", nencrypt);
	}
	if (cfg->channel!= nch) {
		snprintf(cmd, sizeof(cmd) - 1, "%s.wan.channel=%d", IF_CONFIG_FILE, nch);
		uuci_set(cmd);
		cfg->channel = nch;
		WISP_DEBUG("channel changed to %d need reconnect\n", nch);
	}
	WISP_DEBUG("reconnect the ssid %s\n", cfg->ssid);
	wisp_up(cfg);
	timeout = 30;
	return 0;
error:
	if (retry >= 6)
		timeout = 30;
	else
		timeout = 5;
	retry++;
	return -1;
}

int main(int argc, char *argv[])
{
	int ret;
	int extst;
	int count = 0;
	int opt = 0;

	if (!strncmp(argv[1], "down", 4)) {
		wisp_down();
		return 0;
	}
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd <= 0) {
		WISP_DEBUG("%s:%d create channel sock failed\n", __FUNCTION__, __LINE__);
		return -1;
	}
	memset(&wconfig, 0x0, sizeof(wconfig));
	while ((opt = getopt (argc, argv, "s:c:e:k:")) != -1) {
		switch (opt) {
		case 's':
			arr_strcpy(wconfig.ssid, optarg);
			if (!strlen(wconfig.ssid))
				WISP_DEBUG("ApCliConnect ssid error\n");
			WISP_DEBUG("---- ssid=%s\n", wconfig.ssid);
			break;
		case 'c':
			wconfig.channel = atoi(optarg);
			WISP_DEBUG("---- channel=%d\n", wconfig.channel);
			break;
		case 'e':
			arr_strcpy(wconfig.encrypt, optarg);
			if (!strlen(wconfig.encrypt))
				WISP_DEBUG("ApCliConnect encrypt error\n");
			WISP_DEBUG("---- encrypt=%s\n", wconfig.encrypt);
			break;
		case 'k':
			arr_strcpy(wconfig.key, optarg);
			if (!strlen(wconfig.key))
				WISP_DEBUG("ApCliConnect key error\n");
			WISP_DEBUG("---- key=%s\n", wconfig.key);
			break;
		default:
			return -1;
		}
	}
	timeout = 60;
	status = WISP_INIT;
	wisp_up(&wconfig);
	usleep(3000000);
	for (;;) {
		usleep(2000000);
		ret = wisp_get_status(wconfig.ssid);
		switch (ret) {
		case WISP_UP:
			if (status != WISP_UP) {
				wisp_ipup();
				status = WISP_UP;
			}
			continue;
		case WISP_DOWN:
			if (status == WISP_UP) {
				count = 0;
				retry = 0;
				timeout = 5;
				wisp_ipdown();
			}
			if (count > timeout) {
				count = 0;
				wisp_reconnect(&wconfig);
			}
			if (status == WISP_UP)
				status = WISP_DOWN;
			break;
		default:
			break;
		}
		count++;
		waitpid(-1, &extst, WNOHANG);
	}
	close(sockfd);
	return 0;
}
