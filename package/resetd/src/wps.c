#include <stdio.h>
#include "wd_com.h"
#include "igd_share.h"
#include "igd_lib.h"
#include "igd_wifi.h"
#include "igd_interface.h"
#include <linux/wireless.h>

static long led_timer = 0;
int wps_init(void)
{
	WD_DBG("wps init\n");

	system("iwpriv ra0 set WscConfMode=0");
	system("iwpriv ra0 set WscConfMode=4");
	system("iwpriv ra0 set WscConfStatus=1");
	system("iwpriv ra0 set WscConfStatus=2");
	return 0;
}

int wps_run(void)
{
	WD_DBG("wps start\n");

	system("iwpriv ra0 set WscMode=2");
	system("iwpriv ra0 set WscGetConf=1");

	led_timer = sys_uptime();
	led_act(LED_WPS, LED_ACT_ON);
	return 0;
}
int wps_loop(void)
{
	if (!led_timer)
		return 0;
	if ((led_timer + 2*60) > sys_uptime())
		return 0;

	WD_DBG("wps led off\n");
	led_act(LED_WPS, LED_ACT_OFF);
	led_timer = 0;
	return 0;
}
#ifdef WPS_REPEATER
//#if 1
int get_value_from_buf(char *data, char *key, char *value, int vlen)
{
	if (!data || !key || key[0] == '\0' || !value) {
		goto error;
	}
	char *begin_ptr = data, *end_ptr = NULL, ptr = NULL;
	int len = 0;

	while (begin_ptr = strstr(begin_ptr, key)) {
		if (begin_ptr > data && *(begin_ptr - 1) != '\n') {
			begin_ptr += strlen(key);
			continue;
		}
		if (end_ptr = strchr(begin_ptr, '\n')) {
			begin_ptr += strlen(key);
			//if key=Key or key=KeyIndex,next char of key must be space
			if (*begin_ptr != ' ') {
				continue;
			}
			while(*begin_ptr == ' ' || *begin_ptr == '=') {
				begin_ptr ++;
			}
			len = end_ptr - begin_ptr;
			if (len > 0) {
				len = len > (vlen - 1)?(vlen - 1):len;
				strncpy(value, begin_ptr, len);
				value[len] = '\0';
				//printf("%s[%s].\n", key, value);
				return 0;
			} else {
				goto error;
			}
		}else {
			goto error;
		}
	}
error:
	return -1;
}

#define OID_802_11_RTPRIV_IOCTL_APCLI_STATUS	0x0950
#define RT_PRIV_IOCTL 0x8BE1

static int wifi_is_ok()
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
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd <= 0) {
		WD_DBG("create channel sock failed.\n");
		goto not_ok;
	}
	if (ioctl(sockfd, RT_PRIV_IOCTL, &wrq) < 0) {
		WD_DBG("get connstatus failed, %s\n", buf);
		goto not_ok;
	}
	close(sockfd);
	if (strstr(buf, "Disconnect")) {
		WD_DBG("status:%s\n", buf);
		goto not_ok;
	}
	if (!(strstr(buf, "SSID:"))) {
		WD_DBG("status:%s\n", buf);
		goto not_ok;
	}
	WD_DBG("wifi hard link ok[%s]\n", buf);
	return 1;
not_ok:
	WD_DBG("wifi not ok!\n");
	return 0;
}

#define RTPRIV_IOCTL_STATISTICS 0x8be9
int wps_repeater_run()
{
	char buf[4096] = {0};
	char security[64] = "NONE";
	char tmp[32] = {0};
	char auth[32] = {0};
	char enc[32] = {0};
	char key[32] = "NONE";
	int uid = 1, i;
	struct iface_conf config;
	struct iwreq wrq;
	memset(&wrq, 0x0, sizeof(wrq));
	memset(&config, 0x0, sizeof(config));
	strncpy(wrq.ifr_ifrn.ifrn_name, APCLI_DEVNAME, sizeof(wrq.ifr_ifrn.ifrn_name));
	wrq.u.data.pointer = (caddr_t)buf;
	wrq.u.data.length = sizeof(buf);
	wrq.u.data.flags = 0;
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd <= 0) {
		WD_DBG("create channel sock failed.\n");
		exit(0);
	}
	WD_DBG("wps fork repeater start\n");
	//system("iwpriv apcli0 set ResetCounter=1");
	system("iwpriv apcli0 set WscConfMode=0");
	system("iwpriv apcli0 set ApCliEnable=0");
	system("iwpriv apcli0 set WscConfMode=1");
	system("iwpriv apcli0 set WscMode=2");
	system("iwpriv apcli0 set WscGetConf=1");
	system("iwpriv apcli0 set ApCliEnable=1");
	led_timer = sys_uptime();
	set_wps_led(NLK_ACTION_ADD);
	sleep(2);
	i = 20;
	while (i-- > 0) {
		memset(buf, 0x0, sizeof(buf));
		if (ioctl(sockfd, RTPRIV_IOCTL_STATISTICS, &wrq) < 0) {
			WD_DBG("wps RTPRIV_IOCTL_STATISTICS failed, retry..[%s]\n", buf);
			goto retry;
		}
		//WD_DBG("RTPRIV_IOCTL_STATISTICS:%s\n", buf);
		if (wifi_is_ok() && 
			get_value_from_buf(buf, "SSID", tmp, sizeof(tmp)) == 0) {	
			if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &config, sizeof(config))) {
				goto retry;
			} else {
				config.mode = MODE_WISP;
				// find no key means no auth or encrytype,set security as NONE;
				if (!get_value_from_buf(buf, "Key", key, sizeof(key))) {
					get_value_from_buf(buf, "AuthType", auth, sizeof(auth));
					get_value_from_buf(buf, "EncrypType", enc, sizeof(enc));
					snprintf(security, sizeof(security), "%s/%s", auth, enc);
				}
				arr_strcpy(config.wisp.key, key);
				arr_strcpy(config.wisp.security, security);
				get_value_from_buf(buf, "SSID", config.wisp.ssid, sizeof(config.wisp.ssid));
				mu_msg(WIFI_MOD_GET_CHANNEL, NULL, 0, &config.wisp.channel, sizeof(config.wisp.channel));
				WD_DBG("wps repeater get:ssid=%s, channel=%d, key=%s, security=%s.\n", config.wisp.ssid,
						config.wisp.channel, config.wisp.key, config.wisp.security);
				if (mu_msg(IF_MOD_PARAM_SET, &config, sizeof(config), NULL, 0)) {
					goto retry;
				} else {
					goto finish;
				}
			}
		} else {
			goto retry;
		}
	retry:
		sleep(5);
	}
finish:
	set_wps_led(NLK_ACTION_DEL);
	close(sockfd);
	exit(0);
}
#endif

