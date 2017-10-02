#include "igd_lib.h"
#include "igd_system.h"
#include "uci.h"
#include "uci_fn.h"
#include "igd_cloud.h"
#include "igd_wifi.h"
#include "time.h"
#include "nlk_ipc.h"
#include "igd_interface.h"
#include <sys/sysinfo.h>


#if 1 //def FLASH_4_RAM_32
#define IGD_SYS_DBG(fmt,args...) do {}while(0)
#else
#define IGD_SYS_DBG(fmt,args...) \
	igd_log("/tmp/igd_system_dbg", "%s[%d]:"fmt, __FUNCTION__, __LINE__, ##args);
#endif

#define IGD_SYS_ERR(fmt,args...) \
	igd_log("/tmp/igd_system_err", "%s[%d]:"fmt, __FUNCTION__, __LINE__, ##args);

#define IGD_PPPD_DBG(fmt,args...) \
	igd_log("/tmp/ppp_log", "[t=%d]%s[%d]:"fmt, time(NULL), __FUNCTION__, __LINE__, ##args);

//#define EEPROM_AMEND_5G
#define POWER_ON_CHECK_FIRMWARE

static int event_flag;
static struct fw_info fwinfo;
static char fw_file[IGD_NAME_LEN];
static unsigned char led_section_flag = 0;
static unsigned char account_section_flag = 0;
static struct sys_account login_account;
static struct sys_time retime;
static struct schedule_entry *rboot_event;
static char wait_pppoe_succ = 0;
static struct sys_guest guest[SYS_GUEST_MX];
static char tz[16];
static int auto_check_firmware = 0;

static int json_getval(char*value, const char*key, const char*msg)
{
	char *p;
	char *pend;
	
	if ((p = strstr(msg, key)) == NULL)
		return -1;
	p += strlen(key);
	if ( *p != '"')
		return -1;
	p++;
	if ((pend = strchr(p, '"')) == NULL)
		return -1;
	snprintf(value, pend - p + 1, "%s", p);
	if (!strlen(value))
		return -1;
	return 0;
}

static int tcp_connect(char *dns)
{
	int sockfd = -1;
	struct addrinfo hints, *iplist = NULL, *ip = NULL;

	bzero(&hints,sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(dns, FW_SERVER_PORT, &hints, &iplist) != 0)
		return -1;
	for (ip = iplist; ip != NULL; ip = ip->ai_next) {
		sockfd = socket(ip->ai_family, ip->ai_socktype, ip->ai_protocol);
		if (sockfd < 0)
			continue;
		if (!connect(sockfd, ip->ai_addr, ip->ai_addrlen))
			break;
		close(sockfd);
	}
	freeaddrinfo(iplist);
	return sockfd;
}

static int parse_ver_info(char *buf, struct fw_info *info)
{
	char *p = NULL;
	char *js_start = NULL;
	char *js_end = NULL;
	char alow_auto[4] = {0};

	p = strstr(buf, "\r\n\r\n");
	if (p == NULL)
		return -1;
	js_start = strchr(p, '{');
	if (js_start == NULL)
		return -1;
	js_end = strchr(js_start, '}');
	if (js_end == NULL)
		return -1;
	*js_end = '\0';

	if (json_getval(info->ver, "\"ver\":", js_start))
		return -1;
	if (json_getval(info->md5, "\"md5\":", js_start))
		return -1;
	if (json_getval(info->url, "\"url\":", js_start))
		return -1;
	if (json_getval(info->desc, "\"desc\":", js_start))
		return -1;
	if (json_getval(info->size, "\"size\":", js_start))
		return -1;
	if (json_getval(info->name, "\"name\":", js_start))
		return -1;
	if (json_getval(info->time, "\"time\":", js_start))
		return -1;
	if (auto_check_firmware) {
		if (json_getval(alow_auto, "\"auto\":", js_start))
			return -1;
		if (atoi(alow_auto)) {
			console_printf("allow auto upgrade[alow_auto=%s]!\n", alow_auto);
			return 0;
		} else {
			console_printf("not allow auto upgrade[alow_auto=%s]!\n", alow_auto);
			return -1;
		}
	}
	return 0;
}

static int curl_get(void)
{	
	int plen;
	int len = 0, nr;
	int sock_fd;
	char *pend = NULL;
	char *clenp = NULL;
	char *retstr;
	char buf[1024];
	char rbuf[2048];
	char vendor[64];
	char product[64];
	char dns[64];
	struct fw_info info;

	memset(&vendor, 0x0, sizeof(vendor));
	retstr = read_firmware(FW_VENDOR_KEY);
	if (!retstr)
		return -1;
	arr_strcpy(vendor, retstr);
	memset(&product, 0x0, sizeof(product));
	retstr = read_firmware(FW_PRODUCT_KEY);
	if (!retstr)
		return -1;
	arr_strcpy(product, retstr);

	if (CC_CALL_RCONF_INFO(RCONF_FLAG_CHECKUP, dns, sizeof(dns)) < 0)
		return -1;

	memset(&info, 0x0, sizeof(info));
	sock_fd = tcp_connect(dns); // FW_SERVER_DOMAIN
	if (sock_fd < 0) {
		info.flag = FW_FLAG_FINISH;
		goto out;
	}
	memset(buf, 0x0, sizeof(buf));
	len += sprintf(buf, "GET /RTRVer?s=%s&t=%s_%s HTTP/1.1\r\n", fwinfo.cur_ver, vendor, product);
	len += sprintf(buf + len, "User-Agent:  Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; Trident/5.0)\r\n");
	len += sprintf(buf + len, "Depth: 1\r\n");
	len += sprintf(buf + len, "Content-Length: 0\r\n");
	len += sprintf(buf + len, "Host: %s\r\n\r\n", dns);
	len = write(sock_fd, buf, len);
	if (len < 0) {
		close(sock_fd);
		info.flag = FW_FLAG_FINISH;
		goto out;
	}
	len = 0;
	memset(rbuf, 0x0, sizeof(rbuf));
	while (1) {
		nr = read(sock_fd, &rbuf[len], sizeof(rbuf) - len);
		if (nr <= 0) {
			close(sock_fd);
			info.flag = FW_FLAG_FINISH;
			goto out;
		}
		
		len += nr;
		if (len > 4 && (pend = strstr(rbuf, "\r\n\r\n")) != NULL) {
			clenp = strstr(rbuf, "Content-Length:");
			if (clenp != NULL) {
				clenp = clenp + strlen("Content-Length:");
				plen = atoi(clenp);
				if (strlen(pend) < plen + 4)
					continue;
			}
		}
		break;
	}
	close(sock_fd);
	if (parse_ver_info(rbuf, &info))
		info.flag = FW_FLAG_FINISH;
	else
		info.flag = FW_FLAG_HAVEFW;
out:
	retstr = read_firmware(FW_VER_KEY);
	if (NULL == retstr)
		return -1;
	arr_strcpy(info.cur_ver, retstr);
	return mu_msg(SYSTEM_MOD_VERSION_INFO, &info, sizeof(info), NULL, 0);
}

static int fw_version_check(struct fw_info *info)
{
	int i;
	pid_t pid;
	struct stat st;
	struct stat stend;
	char *retstr = NULL;

	if (fwinfo.flag == FW_FLAG_HAVEFW && fwinfo.status == FW_STATUS_CHCFINISH) {
		fwinfo.flag = FW_FLAG_NOFW;
		fwinfo.status = FW_STATUS_INIT;
	}

	switch (fwinfo.flag) {
	case FW_FLAG_NOFW:
	case FW_FLAG_FAILE:
	case FW_FLAG_SUCCESS:
		if (fwinfo.status == FW_STATUS_CHECKING)
			break;
		retstr = read_firmware(FW_VER_KEY);
		if (NULL == retstr)
			return -1;
		memset(&fwinfo, 0x0, sizeof(fwinfo));
		arr_strcpy(fwinfo.cur_ver, retstr);
		fwinfo.status = FW_STATUS_CHECKING;
		pid = fork();
		if (pid < 0) {
			fwinfo.status = FW_STATUS_INIT;
			return -1;
		} else if (pid == 0) {
			for (i = 3; i < 256; i++)
				close(i);
			curl_get();
			exit(0);
		}
		break;
	case FW_FLAG_FINISH:
		break;
	case FW_FLAG_HAVEFW:
		if (strlen(fw_file) > 0 && strncmp(fw_file, strrchr(fwinfo.url, '/'), strlen(fw_file))) {
			remove(FW_FILE);
			remove(FW_LOCAL_FILE);
			remove(FW_CLOUD_FILE);
			remove(FW_STATUS_FILE);
		}
	case FW_FLAG_DOWNING:	
		/*wget is not finish*/
		if (stat(FW_STATUS_FILE, &stend)) {
			if (stat(FW_FILE, &st)) {
				if (fwinfo.status == FW_STATUS_DOWNLOAD)
					fwinfo.flag = FW_FLAG_DOWNING;
				else {
					fwinfo.status = FW_STATUS_CHCFINISH;
					fwinfo.flag = FW_FLAG_HAVEFW;
				}
				fwinfo.cur = 0;
				fwinfo.speed = 0;
			} else {
				fwinfo.cur = st.st_size;
				fwinfo.speed = (st.st_size * 100 / atoi(fwinfo.size));
				fwinfo.flag = FW_FLAG_DOWNING;
			}
			break;
		}
		/*wget has finished, but fwfile is not download*/
		if (stat(FW_FILE, &st)) {
			fwinfo.cur = 0;
			fwinfo.speed = 0;
			fwinfo.flag = FW_FLAG_FAILE;
			break;
		}
		/*wget has finished, fwfile has download*/
		if (st.st_size != atoi(fwinfo.size)) {
			fwinfo.cur = 0;
			fwinfo.speed = 0;
			fwinfo.flag = FW_FLAG_FAILE;
		} else {
			fwinfo.cur = st.st_size;
			fwinfo.speed = 100;
			fwinfo.flag = FW_FLAG_SUCCESS;
		}
		break;
	default:
		return -1;
	}
	memcpy(info, &fwinfo, sizeof(*info));
	if (fwinfo.flag == FW_FLAG_FINISH)
		fwinfo.flag = FW_FLAG_NOFW;
	return 0;
}

static int fw_download(void)
{
	if (!strlen(fwinfo.url))
		return -1;
	remove(FW_FILE);
	remove(FW_LOCAL_FILE);
	remove(FW_CLOUD_FILE);
	remove(FW_STATUS_FILE);
	arr_strcpy(fw_file, strrchr(fwinfo.url, '/'));
	fwinfo.status = FW_STATUS_DOWNLOAD;
#ifdef FLASH_4_RAM_32
	system("up_ready.sh");
#endif
	return exec_cmd("getfw.sh %s %s &", fwinfo.url, FW_FILE);
}

#ifdef FLASH_4_RAM_32
static int get_file_size(char *file)
{
	int fd = -1;
	struct stat st;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		goto err;
	if (fstat(fd, &st) < 0)
		goto err;
	close(fd);
	return (int)st.st_size;
err:
	if (fd > 0)
		close(fd);
	return -1;
}
#endif

static void fw_update_cb(void *data)
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		event_flag = 0;
		return;
	} else if (pid > 0) {
		return;
	}
	exec_cmd("killall pppd");
	sleep(2);
	#ifdef FLASH_4_RAM_32
	int size = 0;
	size = get_file_size(data);
	if (size < 0 || size >= 0x3a0000)
		exec_cmd("reboot");
	else
	#endif
	exec_cmd("sysupgrade %s", (char *)data);
	sleep(5);
	exec_cmd("reboot");
	exit(0);
}

static void sys_reboot_cb(void *data)
{
	exec_cmd("killall pppd");
	usleep(2000000);
	exec_cmd("reboot &");
}

static void sys_default_cb(void *data)
{
	exec_cmd("killall pppd");
	usleep(2000000);
	exec_cmd("mtd -r erase rootfs_data &");
}

static void sys_config_cb(void *data)
{
	char cmd[128];
	
	system("killall pppd");
	system("rm -rf /etc/config/*");
	system("sync");
	snprintf(cmd, sizeof(cmd) - 1, "tar -zxvf %s -C /etc/ config", CONFIG_LOCAL_FILE);
	system(cmd);
	system("sync");
	usleep(2000000);
	system("reboot &");
}

static int fw_update(int flag)
{
	struct timeval tv;
	struct schedule_entry *event = NULL;
	
	if (event_flag)
		return 0;
	switch (flag) {
	case FLAG_LOCAL :
		if (access(FW_LOCAL_FILE, F_OK))
			return -1;
		arr_strcpy(fwinfo.fw_file, FW_LOCAL_FILE);
		break;
	case FLAG_SERVER:
		if (fwinfo.flag != FW_FLAG_SUCCESS)
			return -1;
		arr_strcpy(fwinfo.fw_file, FW_FILE);
		break;
	case FLAG_CLOUD:
		if (access(FW_CLOUD_FILE, F_OK))
			return -1;
		arr_strcpy(fwinfo.fw_file, FW_CLOUD_FILE);
		break;
	default:
		return -1;
	}
	
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	event = schedule(tv, fw_update_cb, fwinfo.fw_file);
	if (!event)
		return -1;
	event_flag = 1;
	return 0;
}

static int sys_reboot(void)
{
	struct timeval tv;
	
	if (event_flag)
		return 0;
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	if (!schedule(tv, sys_reboot_cb, NULL))
		return -1;
	event_flag = 1;
	return 0;
}

static int sys_default(int flag)
{
	struct timeval tv;

	if (event_flag)
		return 0;
	if ((flag != SYSFLAG_RECOVER) && (flag != SYSFLAG_RESET))
		return -1;
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	if (!schedule(tv, sys_default_cb, NULL))
		return -1;
	set_sysflag(flag, 1);
	set_sysflag(SYSFLAG_FIRST, 1);
	if (!access("/etc/init.d/ali_cloud.init", F_OK))
		set_sysflag(SYSFLAG_ALIRESET, 1);
	event_flag = 1;
	return 0;
}

static int sys_config(void)
{
	struct timeval tv;

	if (event_flag)
		return 0;
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	if (!schedule(tv, sys_config_cb, NULL))
		return -1;
	event_flag = 1;
	return 0;
}

static int sys_config_get(void)
{
	remove(CONFIG_LOCAL_FILE);
	return exec_cmd("tar -zcvf %s -C /etc/ config", CONFIG_LOCAL_FILE);
}

static int sys_dev_check(int *status)
{
	char *sta_s = NULL;
	
	sta_s = uci_getval(SYS_CONFIG_FILE, "status", "status");
	if (!sta_s)
		return -1;
	*status = atoi(sta_s);
	free(sta_s);
	return 0;
}

static int sys_led_state = 1;
static int sys_set_led(int *act, int len, int *led, int rlen)
{
	int i = 0;
	
	if (!act || sizeof(*act) != len)
		return -1;
	if (*act == NLK_ACTION_DUMP) {
		if (!led || sizeof(*led) != rlen)
			return -2;
		*led = sys_led_state;
		return 0;
	} else if (*act == NLK_ACTION_ADD) {
		if (sys_led_state == 1)
			return 0;
		sys_led_state = 1;
		for (i = (LED_MIN + 1); i < LED_MAX; i++)
			led_act(i, LED_ACT_ENABLE);
		uuci_set("system.led.enable=1");
	} else {
		if (sys_led_state == 0)
			return 0;
		sys_led_state = 0;
		for (i = (LED_MIN + 1); i < LED_MAX; i++)
			led_act(i, LED_ACT_DISABLE);
		uuci_set("system.led.enable=0");
	}
	return 0;
}

static int get_login_account(struct sys_account *info, int len)
{
	if (sizeof(*info) != len)
		return -1;
	memcpy(info, &login_account, len);
	return 0;
}

static int set_login_account(struct sys_account *info, int len)
{
	char str[512];

	if (sizeof(*info) != len)
		return -1;
	memcpy(&login_account, info, len);

	snprintf(str, sizeof(str) - 1, 
		"system.account.user=%s", login_account.user);
	uuci_set(str);
	snprintf(str, sizeof(str) - 1, 
		"system.account.password=%s", login_account.password);
	uuci_set(str);
	return 0;
}

static int show_login_guest(int index, struct sys_guest *cfg)
{
	int i = index;

	if (i > (SYS_GUEST_MX - 1) || !guest[i].invalid)
		return -1;
	*cfg = guest[i];
	return 0;
}

static int set_login_guest(struct sys_guest *cfg)
{
	int i;
	int index = cfg->index;
	int wguestchange = 0; 
	char cmd[128];
	
	if (index >= 0) {
		if ((index > (SYS_GUEST_MX - 1)) || guest[index].readonly
			|| !guest[index].invalid)
			return -1;
		if (guest[index].system)
			wguestchange = 1;
		if (!cfg->invalid)
			memset(cfg, 0x0, sizeof(struct sys_guest));
	} else {
		for (i = 0; i < SYS_GUEST_MX; i++) {
			if (guest[i].invalid)
				continue;
			index = i;
			break;
		}
	}
	if (index < 0)
		return -1;
	arr_strcpy(guest[index].user, cfg->user);
	arr_strcpy(guest[index].password, cfg->password);
	guest[index].limits = cfg->limits;
	guest[index].invalid = cfg->invalid;

	if (wguestchange) {
		snprintf(cmd, sizeof(cmd), "system.account.wguestchange=%d", wguestchange);
		uuci_set(cmd);
	}
	snprintf(cmd, sizeof(cmd), "system.account.guests");
	uuci_delete(cmd);
	for (i = 0; i < SYS_GUEST_MX; i++) {
		if (!guest[i].invalid)
				continue;
		if (guest[i].readonly)
			continue;
		if (!wguestchange && guest[i].system)
			continue;
		snprintf(cmd, sizeof(cmd), "system.account.guests=%d;%s;%d;%s;%lu",
				strlen(guest[i].user), guest[i].user, strlen(guest[i].password),
				guest[i].password, guest[i].limits);
		uuci_add_list(cmd);
	}
	return 0;
}

static int system_cmdline(char *cmd, int len)
{
	if (!cmd)
		return -1;
	return system(cmd);
}

static int system_get_mtd_param(char *valname, char *value, int len)
{
	return mtd_get_val(valname, value, len);
}

static void system_save_retime(void)
{
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "system.@system[0].retime=%d,%hhd,%hhd,%hhd,%hhd",
				retime.enable, retime.loop, retime.day, retime.hour, retime.min);
	uuci_set(cmd);
}

static int system_time_cmp(struct tm *t)
{
	int i = 0;
	int now, systime, week;

	week = t->tm_wday;
	now = t->tm_hour * 60 + t->tm_min;
	systime = retime.hour * 60 + retime.min;
	if ((week < 0) || (week > 6))
		return 0;
	if (!retime.loop) {
		for (i = 0; i < 7; i++) {
			if (retime.day & (1<<i))
				break;
		}
		if ((week == i && now >= systime)) {
			memset(&retime, 0x0, sizeof(retime));
			return 1;
		} else
			return 0;
	}
	for (i = 0; i < 7; i++) {
		if (!(retime.day & (1<<i)) || (i != week) || (now != systime))
			continue;
		return 1;
	}
	return 0;
}

static int system_confirm_timezone(void)
{
	char *ptr;
	char str[64];
	int num;
	char **val;
	
	ptr = read_firmware("TIMEZONE");
	if (NULL == ptr)
		return -1;	
	if (uuci_get("system.@system[0].timezone", &val, &num) || !val)
		return -1;
	if (strcmp(ptr, val[0])) {
		char temptz[16] = {0};
		arr_strcpy(temptz,val[0]);
		uuci_get_free(val, num);
		if (uuci_get("system.@system[0].manual_timezone", &val, &num)
			 || !val || strcmp(val[0], "1")) {
			snprintf(str, sizeof(str), "system.@system[0].timezone=%s", ptr);
			uuci_set(str);
			char cmd[32] = {0};
			snprintf(cmd, sizeof(cmd), "echo %s > /tmp/TZ;date -k", ptr);
			system(cmd);
			arr_strcpy(tz,ptr);
		} else {
			arr_strcpy(tz,temptz);
		}
		uuci_get_free(val, num);
	} else {
		arr_strcpy(tz,ptr);
		uuci_get_free(val, num);
	}
	return 0;
}

static void system_retime_cb(void *data)
{
	int ret = 0;
	struct timeval tv;
	struct tm *t = NULL;

	rboot_event = NULL;
	if (!retime.enable)
		return;
	t = get_tm();
	if (!t)
		goto next;
	ret = system_time_cmp(t);
	if (ret > 0 && !event_flag) {
		event_flag = 1;
		system_save_retime();
		exec_cmd("killall pppd");
		usleep(2000000);
		exec_cmd("reboot &");
	}
next:
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	rboot_event = schedule(tv, system_retime_cb, data);
	return;
}

static int igd_system_retime(struct sys_time *tm)
{
	struct timeval tv;
	
	if (rboot_event) {
		dschedule(rboot_event);
		rboot_event = NULL;
	}
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	memcpy(&retime, tm, sizeof(retime));
	rboot_event = schedule(tv, system_retime_cb, NULL);
	system_save_retime();
	return 0;
}

#define SYS_DAEMON_NAME_MX  128
struct sys_daemon_info {
	struct list_head list;
	pid_t pid;
	int oom;
	char name[SYS_DAEMON_NAME_MX];
	char path[SYS_DAEMON_NAME_MX];
};
struct list_head sys_daemon_list = LIST_HEAD_INIT(sys_daemon_list);

static struct sys_daemon_info *igd_sys_daemon_find(char *name, pid_t pid)
{
	struct sys_daemon_info *sdi;

	list_for_each_entry(sdi, &sys_daemon_list, list) {
		if (name && !strncmp(sdi->name, name, SYS_DAEMON_NAME_MX))
			return sdi;
		if (pid && (pid == sdi->pid))
			return sdi;
	}
	return NULL;

}

static int igd_sys_daemon_add(char *name)
{
	struct sys_daemon_info *sdi;

	if (!name || (strlen(name) >= SYS_DAEMON_NAME_MX)) {
		IGD_SYS_ERR("proc:%s\n", name);
		return -1;
	}

	IGD_SYS_DBG("proc:%s\n", name);
	sdi = igd_sys_daemon_find(name, 0);
	if (sdi)
		return -2;

	sdi = malloc(sizeof(*sdi));
	if (!sdi)
		return -1;
	memset(sdi, 0, sizeof(*sdi));
	arr_strcpy(sdi->name, name);
	sdi->pid = fork();
	if (sdi->pid < 0) {
		goto err;
	} else if (sdi->pid > 0) {
		list_add(&sdi->list, &sys_daemon_list);
	} else {
		exe_default(sdi->name);
		exit(0);
	}
	return 0;
err:
	if (sdi)
		free(sdi);
	return -1;
}

static int igd_plug_daemon_add(
	struct plug_daemon_info *info, int len)
{
	char str[512], *ptr;
	struct sys_daemon_info *sdi;

	if (!info || sizeof(*info) != len) {
		IGD_SYS_ERR("input err\n");
		return -1;
	}

	IGD_SYS_DBG("plug daemon:%s\n", info->name);
	sdi = igd_sys_daemon_find(info->name, 0);
	if (sdi)
		return -2;

	sdi = malloc(sizeof(*sdi));
	if (!sdi)
		return -1;
	memset(sdi, 0, sizeof(*sdi));
	sdi->oom = info->oom;
	arr_strcpy(sdi->name, info->name);
	arr_strcpy(sdi->path, info->path);
	sdi->pid = fork();
	if (sdi->pid < 0) {
		goto err;
	} else if (sdi->pid > 0) {
		list_add(&sdi->list, &sys_daemon_list);
	} else {
		if (info->path[0]) {
			ptr = getenv("LD_LIBRARY_PATH");
			snprintf(str, sizeof(str), "%s/lib%s%s",
					info->path, ptr ? ":" : "", ptr ? ptr : "");
			setenv("LD_LIBRARY_PATH", str, 1);
			IGD_SYS_DBG("LD_LIBRARY_PATH:%s\n", str);

			ptr = getenv("PATH");
			snprintf(str, sizeof(str), "%s/bin%s%s",
					info->path, ptr ? ":" : "", ptr ? ptr : "");
			setenv("PATH", str, 1);
			IGD_SYS_DBG("PATH:%s\n", str);
		}
		if (sdi->oom) {
			snprintf(str, sizeof(str), \
				"echo %d > /proc/%d/oom_score_adj", sdi->oom, getpid());
			system(str);
		}
		exe_default(sdi->name);
		exit(0);
	}
	return 0;
err:
	if (sdi)
		free(sdi);
	return -1;
}

static int igd_sys_daemon_del(char *name, pid_t pid)
{
	struct sys_daemon_info *sdi;

	if (!name && !pid) {
		IGD_SYS_ERR("\n");
		return -1;
	}

	IGD_SYS_DBG("proc:%s, pid:%d\n", name ? : "", pid);
	sdi = igd_sys_daemon_find(name, pid);
	if (!sdi)
		return -2;
	kill(sdi->pid, SIGKILL);
	list_del(&sdi->list);
	free(sdi);
	return 0;
}

struct list_head sys_oomkill_list = LIST_HEAD_INIT(sys_oomkill_list);
static long sys_oomkill_time = 0;

static int igd_sys_daemon_oom(struct sys_msg_oom *oom, int len)
{
	struct sysinfo info;
	struct sys_daemon_info *sdi;

	if (!oom || (sizeof(*oom) != len)) {
		IGD_SYS_ERR("\n");
		return -1;
	}

	sysinfo(&info);
	igd_log("/tmp/kill_log", "name:%s, pid:%d, mem:%d, free:%lu\n",
		oom->name, oom->pid, oom->mem, info.freeram/1024);

	sdi = igd_sys_daemon_find(NULL, oom->pid);
	if (!sdi)
		return -2;
	kill(sdi->pid, SIGKILL);
	list_del(&sdi->list);

	sdi->pid = -1;
	list_add_tail(&sdi->list, &sys_oomkill_list);
	sys_oomkill_time = sys_uptime();
	return 0;
}

static int check_file_exist(char *name)
{
	char str[512], file[512] = {0,};

	if (!name || !name[0])
		return 0;

	__arr_strcpy_end((unsigned char *)file,
		(unsigned char *)name, sizeof(file) - 1, ' ');

	if (file[0] == '/') {
		if (!access(file, F_OK))
			return 1;
	} else {
		snprintf(str, sizeof(str), "/bin/%s", file);
		if (!access(str, F_OK))
			return 1;
		snprintf(str, sizeof(str), "/sbin/%s", file);
		if (!access(str, F_OK))
			return 1;
		snprintf(str, sizeof(str), "/usr/bin/%s", file);
		if (!access(str, F_OK))
			return 1;
		snprintf(str, sizeof(str), "/usr/sbin/%s", file);
		if (!access(str, F_OK))
			return 1;
	}
	return 0;
}

static int igd_sys_oom_check(void)
{
	struct sysinfo info;
	struct sys_daemon_info *sdi = NULL;
	struct plug_daemon_info plug;

	if ((sys_oomkill_time + 10*60) > sys_uptime())
		return -1;

	sysinfo(&info);
	if (info.freeram < 3145728)
		return 0;

	list_for_each_entry(sdi, &sys_oomkill_list, list) {
		if (!check_file_exist(sdi->name)) {
			IGD_SYS_DBG("[%s] is nonexist\n", sdi->name);
			list_del(&sdi->list);
			free(sdi);
			return -1;
		}
		memset(&plug, 0, sizeof(plug));
		plug.oom = sdi->oom;
		arr_strcpy(plug.name, sdi->name);
		arr_strcpy(plug.path, sdi->path);

		IGD_SYS_DBG("[%s] restart\n", sdi->name);
		igd_plug_daemon_add(&plug, sizeof(plug));

		list_del(&sdi->list);
		free(sdi);
		break;
	}
	sys_oomkill_time = sys_uptime();
	return 0;
}

static int igd_sys_daemon_pid(char *name)
{
	struct sys_daemon_info *sdi;

	if (!name || (strlen(name) >= SYS_DAEMON_NAME_MX)) {
		IGD_SYS_ERR("\n");
		return -1;
	}

	IGD_SYS_DBG("proc:%s\n", name);
	sdi = igd_sys_daemon_find(name, 0);
	if (sdi)
		return sdi->pid;
	return 0;
}

static int igd_sys_daemon_check(struct sys_waitpid *daemon)
{
	char str[512], *ptr;
	struct sys_daemon_info *sdi;

	sdi = igd_sys_daemon_find(NULL, daemon->pid);
	if (!sdi)
		return -1;

	kill(sdi->pid, SIGKILL);
	sdi->pid = fork();
	if (sdi->pid < 0) {
		console_printf("fork fail\n");
		list_del(&sdi->list);
		free(sdi);
	} else if (sdi->pid == 0) {
		if (sdi->path[0]) {
			ptr = getenv("LD_LIBRARY_PATH");
			snprintf(str, sizeof(str), "%s/lib%s%s",
					sdi->path, ptr ? ":" : "", ptr ? ptr : "");
			setenv("LD_LIBRARY_PATH", str, 1);
			IGD_SYS_DBG("LD_LIBRARY_PATH:%s\n", str);

			ptr = getenv("PATH");
			snprintf(str, sizeof(str), "%s/bin%s%s",
					sdi->path, ptr ? ":" : "", ptr ? ptr : "");
			setenv("PATH", str, 1);
			IGD_SYS_DBG("PATH:%s\n", str);
		}
		if (sdi->oom) {
			snprintf(str, sizeof(str), \
				"echo %d > /proc/%d/oom_score_adj", sdi->oom, getpid());
			system(str);
		}
		exe_default(sdi->name);
		exit(0);
	}
	return 0;
}

struct igd_sys_task_info {
	struct list_head list;
	char cmd[1024];
	pid_t pid;
	int ret;
	long timeout; // second
	long runtime; //start run time;
};

struct igd_sys_task_list {
	struct list_head list;
	struct list_head sub;
	int nr;
	int err_nr;
	int finish_nr;
	int timeout;
	char name[16];
};

struct list_head igd_sys_task_head = LIST_HEAD_INIT(igd_sys_task_head);

static struct igd_sys_task_list *igd_sys_task_find(char *name)
{
	struct igd_sys_task_list *istl;

	if (!name || !name[0])
		return NULL;
	list_for_each_entry(istl, &igd_sys_task_head, list) {
		if (!strcmp(name, istl->name))
			return istl;
	}
	return NULL;
}

static int igd_sys_task_add(struct igd_sys_task_set *ists, int len)
{
	struct igd_sys_task_list *istl = NULL;
	struct igd_sys_task_info *isti = NULL;

	if (sizeof(*ists) != len)
		return -1;
	istl = igd_sys_task_find(ists->name);
	if (!istl) {
		istl = malloc(sizeof(*istl));
		if (!istl) {
			IGD_SYS_ERR("malloc fail\n");
			return -1;
		}
		memset(istl, 0, sizeof(*istl));
		INIT_LIST_HEAD(&istl->sub);
		arr_strcpy(istl->name, ists->name);
		list_add(&istl->list, &igd_sys_task_head);
	}

	list_for_each_entry_reverse(isti, &istl->sub, list) {
		if (isti->pid < 0)
			continue;
		if (!strcmp(isti->cmd, ists->cmd))
			return 0;
	}

	isti = malloc(sizeof(*isti));
	if (!isti) {
		IGD_SYS_ERR("malloc fail\n");
		goto err;
	}
	istl->timeout = 0;

	memset(isti, 0, sizeof(*isti));
	arr_strcpy(isti->cmd, ists->cmd);
	isti->timeout = ists->timeout;
	isti->runtime = sys_uptime();
	istl->nr++;
	list_add(&isti->list, &istl->sub);
	IGD_SYS_DBG("[%s] [%s]: [%d]\n", ists->name, ists->cmd, ists->timeout);
	return 0;

err:
	if (isti)
		free(isti);
	if (!istl->nr) {
		list_del(&istl->list);
		free(istl);
	}
	return -1;
}

static int igd_sys_task_dump(
	char *name, int len, struct igd_sys_task_get *istg, int rlen)
{
	struct igd_sys_task_list *istl = NULL;
	struct igd_sys_task_info *isti, *_isti;

	if (sizeof(*istg) != rlen) {
		IGD_SYS_ERR("input err, %d\n", rlen);
		return -1;
	}

	istl = igd_sys_task_find(name);
	if (!istl)
		return 1;
	if (!istl->nr) {
		IGD_SYS_ERR("[%s], nr 0\n", istl->name);
		return 1;
	}
	istg->nr = istl->nr;
	istg->err_nr = istl->err_nr;
	istg->finish_nr = istl->finish_nr;

	if (istl->finish_nr != istl->nr)
		return 0;

	IGD_SYS_DBG("free [%s], %d,%d,%d\n",
		istl->name, istl->nr, istl->err_nr, istl->finish_nr);
	list_for_each_entry_safe(isti, _isti, &istl->sub, list) {
		list_del(&isti->list);
		free(isti);
	}
	list_del(&istl->list);
	free(istl);
	return 0;
}

static int igd_sys_task_check(struct sys_waitpid *sw)
{
	struct sys_waitpid *info = sw;
	struct igd_sys_task_list *istl, *_istl;
	struct igd_sys_task_info *isti, *_isti;

	list_for_each_entry_safe(istl, _istl, &igd_sys_task_head, list) {
		if (istl->nr == istl->finish_nr) {
			if (!istl->timeout) {
				istl->timeout = sys_uptime();
			} else if ((istl->timeout + 60) < sys_uptime()) {
				IGD_SYS_DBG("timeout free [%s], %d,%d,%d\n",
					istl->name, istl->nr, istl->err_nr, istl->finish_nr);
				list_for_each_entry_safe(isti, _isti, &istl->sub, list) {
					list_del(&isti->list);
					free(isti);
				}
				list_del(&istl->list);
				free(istl);
			}
			continue;
		}
			
		list_for_each_entry_reverse(isti, &istl->sub, list) {
			if (isti->pid < 0) {
				continue;
			} else if (!info) {
				if (isti->pid)
					break;
				isti->pid = fork();
				if (isti->pid < 0) {
					IGD_SYS_ERR("fork err\n");
					istl->err_nr++;
					istl->finish_nr++;
				} else if (isti->pid == 0) {
					system(isti->cmd);
					exit(0);
				}
				IGD_SYS_DBG("[%s] [%s]: [%d]\n", istl->name, isti->cmd, isti->timeout);
				break;
			} else if (isti->pid == info->pid) {
				isti->pid = -1;
				if (WIFEXITED(info->status))
					isti->ret = (int)((char)WEXITSTATUS(info->status));
				else
					isti->ret = -1;
				if (isti->ret)
					istl->err_nr++;
				istl->finish_nr++;
				info = NULL;
				continue;
			}
		}
	}
	return -1;
}

static void igd_sys_task_timer(void *data)
{
	struct timeval tv;

	igd_sys_oom_check();
	igd_sys_task_check(NULL);

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	schedule(tv, igd_sys_task_timer, NULL);
}

static int igd_system_load_section(struct uci_section *s)
{
	struct uci_element *oe = NULL;
	struct uci_option *o = NULL;

	led_section_flag = 1;

	uci_foreach_element(&s->options, oe) {
		o = uci_to_option(oe);

		if (!strcmp(oe->name, "enable")) {
			if (o->type != UCI_TYPE_STRING)
				continue;
			sys_led_state = atoi(o->v.string);
		}
	}
	return 0;
}

static int igd_system_load_account(struct uci_section *s)
{
	char *p = NULL;
	int len = 0, i = 0;
	struct uci_element *oe = NULL;
	struct uci_element *og = NULL;
	struct uci_option *o = NULL;

	account_section_flag = 1;

	uci_foreach_element(&s->options, oe) {
		o = uci_to_option(oe);
		if (!strcmp(oe->name, "user")) {
			if (o->type != UCI_TYPE_STRING)
				continue;
			arr_strcpy(login_account.user, o->v.string);
		} else if (!strcmp(oe->name, "password")) {
			if (o->type != UCI_TYPE_STRING)
				continue;
			arr_strcpy(login_account.password, o->v.string);
		} else if (!strcmp(oe->name, "guests")) {
			if (o->type != UCI_TYPE_LIST)
				continue;
			/*format:4;user;3;pwd;limit*/
			uci_foreach_element(&o->v.list, og) {
				p = og->name;
				for (i = 0; i < SYS_GUEST_MX; i++) {
					if (guest[i].invalid)
						continue;
					len = atoi(p);
					if (len <= 0 || len > 31)
						continue;
					p = strchr(p, ';');
					if (!p)
						continue;
					strncpy(guest[i].user, p + 1, len);
					p = p + len + 1;

					if (*p != ';') {
						memset(&guest[i], 0x0, sizeof(struct sys_guest));
						continue;
					}
					p++;
					len = atoi(p);
					if (len <= 0 || len > 31) {
						memset(&guest[i], 0x0, sizeof(struct sys_guest));
						continue;
					}
					p = strchr(p, ';');
					if (!p) {
						memset(&guest[i], 0x0, sizeof(struct sys_guest));
						continue;
					}
					strncpy(guest[i].password, p + 1, len);
					p = p + len + 1;

					if (*p != ';') {
						memset(&guest[i], 0x0, sizeof(struct sys_guest));
						continue;
					}
					p++;
					guest[i].limits = atoll(p);
					guest[i].invalid = 1;
					guest[i].index = i;
				}
			}
		}
	}
	return 0;
}

static int igd_system_load_time(struct uci_section *s)
{
	struct uci_element *oe = NULL;
	struct uci_option *o = NULL;
	char *ptr = NULL;
	int mark = 0;
	uci_foreach_element(&s->options, oe) {
		o = uci_to_option(oe);
		if (!strcmp(oe->name, "retime")) {
			if (o->type != UCI_TYPE_STRING)
				continue;
			sscanf(o->v.string, "%d,%hhd,%hhd,%hhd,%hhd",
				&retime.enable, &retime.loop,&retime.day,
				&retime.hour, &retime.min);
			mark = 1;
		} 
	}
	if (mark == 0) {
		ptr = read_firmware("RETIME");
		if (!ptr)
			return -1;
		sscanf(ptr, "%d,%hhd,%hhd,%hhd,%hhd",
			&retime.enable, &retime.loop,&retime.day,
			&retime.hour, &retime.min);	
	}
	return 0;
}

static int igd_system_load(void)
{
	struct uci_package *pkg = NULL;
	struct uci_context *ctx = NULL;
	struct uci_element *se = NULL;
	struct uci_section *s = NULL;

	ctx = uci_alloc_context();
	if (0 != uci_load(ctx, "system", &pkg)) {
		uci_free_context(ctx);
		return -1;
	}

	uci_foreach_element(&pkg->sections, se) {
		s = uci_to_section(se);
		if (!strcmp(se->name, "led")) {
			igd_system_load_section(s);
		} else if (!strcmp(se->name, "account")) {
			igd_system_load_account(s);
		} else if (!strcmp(s->type, "system")) {
			igd_system_load_time(s);
		}
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx);
	return 0;
}

#ifdef EEPROM_AMEND_5G

#define MTD_LOG_FILE  "/tmp/eeprom_log"
#define MTD_EEPROM_NAME  "/dev/mtdblock3"
#define MTD_EEPROM_OFFSET(of) (0x8000 + of)

#define EEPROM_DBG(fmt,args...) \
	igd_log(MTD_LOG_FILE, fmt, ##args)

static int eeprom_write(int fd, int of, unsigned char *val)
{
	lseek(fd, MTD_EEPROM_OFFSET(of), SEEK_SET);
	if (write(fd, val, sizeof(*val)) != 1) {
		EEPROM_DBG("write fail, %d\n", of);
		return -1;
	}
	EEPROM_DBG("write %02X:%02X\n", of, *val);
	return 0;
}

int eeprom_amend_5G(void)
{
	int ret, fd;
	char eeprom[32];
	unsigned char bit;

	ret = mtd_get_val("eeprom", eeprom, sizeof(eeprom));
	if (ret == -2)
		return -1;
	if (!ret && (eeprom[0] == 'y'))
		return 0;

	fd = open(MTD_EEPROM_NAME, O_RDWR | O_EXCL, 0444);
	if (fd < 0) {
		EEPROM_DBG("open fail\n");
		return -1;
	}
	ret = 0;

	bit = 0x14;
	ret += eeprom_write(fd, 0X69, &bit);
	bit = 0x16;
	ret += eeprom_write(fd, 0X87, &bit);
	bit = 0x13;
	ret += eeprom_write(fd, 0X6E, &bit);
	bit = 0x16;
	ret += eeprom_write(fd, 0X8C, &bit);
	bit = 0x12;
	ret += eeprom_write(fd, 0X73, &bit);
	bit = 0x15;
	ret += eeprom_write(fd, 0X91, &bit);
	bit = 0x14;
	ret += eeprom_write(fd, 0X78, &bit);
	bit = 0x16;
	ret += eeprom_write(fd, 0X96, &bit);
	bit = 0x15;
	ret += eeprom_write(fd, 0X7D, &bit);
	bit = 0x15;
	ret += eeprom_write(fd, 0X9B, &bit);
	bit = 0x05;
	ret += eeprom_write(fd, 0X38, &bit);

	close(fd);
	if (ret)
		return -1;

	strcpy(eeprom, "eeprom=y");
	ret = mtd_set_val(eeprom);
	if (ret < 0) {
		EEPROM_DBG("set val fail\n");
		return -1;
	}

	pid_t pid;
	pid = fork();
	if (pid == 0) {
		while (sys_uptime() < 2*60)
			sleep(6);
		console_printf("eeprom update, reboot\n");
		sleep(3);
		system("reboot &");
		exit(0);
	}
	return 0;
}
#endif

static void led_net_timer(void *data)
{
	int uid = 1;
	struct timeval tv;
	struct iface_info ifinfo;
	static unsigned char state = 0;

	memset(&ifinfo, 0, sizeof(ifinfo));
	mu_call(IF_MOD_IFACE_INFO, &uid, sizeof(int), &ifinfo, sizeof(ifinfo));
	if (ifinfo.net) {
		if (!state) {
			led_act(LED_INTERNET, LED_ACT_ON);
			state = 1;
		}
	} else {
		led_act(LED_INTERNET, state ? LED_ACT_OFF : LED_ACT_ON);
		state = state ? 0 : 1;
	}

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (!schedule(tv, led_net_timer, NULL))
		console_printf("-----led timer fail\n");
}

static int igd_sys_waitpid(struct sys_waitpid *daemon)
{
	/*
	IGD_SYS_ERR("proc exit:pid:%d, name:%s, %d, %d\n", daemon->pid, sdi->name,
		WIFEXITED(daemon->status), WEXITSTATUS(daemon->status));
	*/

	if (!igd_sys_daemon_check(daemon))
		return 0;
	if (!igd_sys_task_check(daemon))
		return 0;
	return 0;
}
#ifdef POWER_ON_CHECK_FIRMWARE
static void self_check_firmware_cb(void *data)
{
	struct fw_info info;
	struct timeval tv;
	static int times = 3;

	if (!get_tm()){
		IGD_SYS_DBG("get utc falied!\n");
		goto timer;	
	}
	
	if (times <= 0){
		auto_check_firmware = 0;
		return;
	}
	
	auto_check_firmware = 1;
	memset(&info, 0x0, sizeof(info));
	mu_call(SYSTEM_MOD_VERSION_CHECK, NULL, 0, &info, sizeof(struct fw_info));
	
	switch (info.flag){
		case FW_FLAG_NOFW:
		case FW_FLAG_FINISH:
			times--;
			break;
		case FW_FLAG_HAVEFW:
			IGD_SYS_DBG("begin download firmware.\n");
			mu_call(SYSTEM_MOD_FW_DOWNLOAD, NULL, 0, NULL, 0);
			break;
		case FW_FLAG_DOWNING:
			IGD_SYS_DBG("downloading firmware...\n");
			break;
		case FW_FLAG_SUCCESS:
			IGD_SYS_DBG("download firmware sucess, begin update\n");
			mu_call(SYSTEM_MOD_FW_UPDATE, NULL, 0, NULL, 0);
			auto_check_firmware = 0;
			return;
		default:
			break;
	}
	
timer:
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	schedule(tv, self_check_firmware_cb, NULL);
}
#endif
static int has_alive_host()
{
	int n = 0;
	struct host_info *hi;

	hi = dump_host_alive(&n);
	
	if (hi){
		free(hi);
		return n;
	}else{
		return 0;
	}
}
#define SECS_IN_ONE_MIN			60
#define MINS_IN_ONE_HOUR		60
#define RESTART_PPPD_MIN_HOUR	2
#define RESTART_PPPD_MAX_HOUR	6
int get_secs_to_next_hour()
{
	struct tm *now_tm = get_tm();
	return (MINS_IN_ONE_HOUR - now_tm->tm_min - 1) * 
					SECS_IN_ONE_MIN + SECS_IN_ONE_MIN - now_tm->tm_sec;
}
int get_rand_with_devid_pid(int min, int max)
{
	static unsigned int i = 1;
	srand(get_devid() * getpid() * time(NULL) * i++);
	return min + rand() % (max - min + 1);
}

time_t get_local_time()
{
	time_t tt = 0;
	time(&tt);
	struct tm *now_tm = localtime(&tt);
	return mktime(now_tm);
}

static int sys_set_timezone(struct sys_timezone *timezone)
{
	if (!strlen(timezone->tz))
		return -1;
	if (strcmp(timezone->tz, tz)) {
		char str[64];
		snprintf(str, sizeof(str), "system.@system[0].timezone=%s", timezone->tz);
		uuci_set(str);
		snprintf(str, sizeof(str), "system.@system[0].manual_timezone=%d", 1);
		uuci_set(str);
		char cmd[32] = {0};
		snprintf(cmd, sizeof(cmd), "echo %s > /tmp/TZ;date -k", timezone->tz);
		if (-1 == system(cmd))
			return -1;
		arr_strcpy(tz,timezone->tz);
	}
	if (strlen(timezone->tz_time)) {
		char cmd[32] = {0};
		snprintf(cmd, sizeof(cmd), "date %s", timezone->tz_time);
		if (-1 == system(cmd))
			return -1;
		if (-1 == system("date -k")) 
			return -1;
	}
	return 0;
}

static int sys_get_timezone(struct sys_timezone *timezone)
{
	time_t rawtime;
	struct tm *timeinfo = NULL;
	
	arr_strcpy(timezone->tz, tz);
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	if (!timeinfo) {
		return -1;
	}
	char date[32] = {0};
	snprintf(date, 32, "%d.%02d.%02d-%02d:%02d:%02d", timeinfo->tm_year + 1900, 
		timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour,
		timeinfo->tm_min, timeinfo->tm_sec);
	arr_strcpy(timezone->tz_time, date);
	return 0;
}

extern struct iface *interface_get(int uid);
static void pppd_restart_cb(void *data)
{
	char cmd[1024];
	char pppoetag[64];
	struct sys_msg_ipcp pppd;
	struct iface *ifc = interface_get(1);
	struct iface_conf *config = (struct iface_conf *)data;

	memset(&pppd, 0, sizeof(pppd));
	pppd.pppd_type = PPPD_TYPE_PPPOE_CLIENT;
	pppd.uid = 1;
    pppd.type = IF_TYPE_WAN;
	mu_call(IF_MOD_IFACE_IP_DOWN, &pppd, sizeof(pppd), NULL, 0);
	
	memset(cmd, 0x0, sizeof(cmd));
	memset(pppoetag, 0x0, sizeof(pppoetag));
	
	if (strlen(config->pppoe.pppoe_server) > 0) {
		snprintf(pppoetag, sizeof(pppoetag), "rp_pppoe_service %s ", 
			config->pppoe.pppoe_server);
	}
	
	sprintf(cmd,
		"pppd file "
		"/etc/ppp/options.pppoe "
		"nodetach "
		"ipparam wan "
		"ifname pppoe-wan "
		"nodefaultroute "
		"usepeerdns persist "
		"user %s "
		"password %s "
		"mtu %d "
		"mru %d "
		"plugin rp-pppoe.so "
		"%s"
		"nic-%s "
		"pppd_type %d "
		"holdoff 20 &",
		config->pppoe.user,
		config->pppoe.passwd,
		config->pppoe.comm.mtu,
		config->pppoe.comm.mtu,
		pppoetag,
		ifc->devname,
		PPPD_TYPE_PPPOE_CLIENT);
	IGD_PPPD_DBG("now restart pppd, time=%d\n", get_local_time());

	exec_cmd(cmd); 
	wait_pppoe_succ = 1;
}
static void pppoe_check_cb(void *data);
static void pppoe_do_cb(void *data)
{
	struct timeval tv;
	tv.tv_usec = 0;
	static struct iface_conf config;
	//wan
	int uid = 1;
	int nr = 0;
	struct tm *now_tm = get_tm();
	if (now_tm->tm_hour >= RESTART_PPPD_MIN_HOUR && 
		now_tm->tm_hour < RESTART_PPPD_MAX_HOUR) {
		nr = has_alive_host();
		if (nr) {
			tv.tv_sec = 10 * 60;
			schedule(tv, pppoe_do_cb, NULL);
			return;
		}
		mu_call(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &config, sizeof(config));
		
		if (config.mode == MODE_PPPOE) {
			tv.tv_sec = 6;
			if (schedule(tv, pppd_restart_cb, &config)) {
				system("killall pppd");
			}
		}

		tv.tv_sec = (RESTART_PPPD_MAX_HOUR - now_tm->tm_hour) * 3600 +  
					get_secs_to_next_hour() ;
	}else {
		tv.tv_sec = get_secs_to_next_hour();
	}
	schedule(tv, pppoe_check_cb, NULL);
}
static void pppoe_check_cb(void *data)
{
	struct timeval tv;
	int secs_to_next_hour;
	tv.tv_usec = 0;
	
	struct tm *now_tm = get_tm();
	
	if (NULL == now_tm) {
		tv.tv_sec = 10;
		goto end;
	}
	secs_to_next_hour = get_secs_to_next_hour();

	if (now_tm->tm_hour >= RESTART_PPPD_MIN_HOUR && 
		now_tm->tm_hour < RESTART_PPPD_MAX_HOUR) {
		int max_sec = (RESTART_PPPD_MAX_HOUR - now_tm->tm_hour - 1) * 
						3600 + secs_to_next_hour ;	
		
		tv.tv_sec = get_rand_with_devid_pid(0, max_sec);
		schedule(tv, pppoe_do_cb, NULL);
		return;
	}else {
		tv.tv_sec = secs_to_next_hour;
	}
end:
	schedule(tv, pppoe_check_cb, NULL);
		
}
static int igd_sys_pppoe_succ()
{
	if (wait_pppoe_succ) {
		IGD_PPPD_DBG("pppd restart success,upload log,time=%d!\n", get_local_time());
		system("cloud_exchange l&");
		wait_pppoe_succ = 0;
	}
	return 0;
}
int auto_switch_to_pppoe(struct iface_conf *config)
{
	config->mode = MODE_PPPOE;
	config->pppoe.comm.mtu = 1480;
	if (mtd_get_val("pppoe_username", config->pppoe.user, sizeof(config->pppoe.user))) {
		IGD_PPPD_DBG("not set pppoe_username yet.\n");
		return -1;
	}
	if (mtd_get_val("pppoe_passwd", config->pppoe.passwd, sizeof(config->pppoe.passwd))) {
		IGD_PPPD_DBG("not set pppoe_passwd yet.\n");
		return -1;
	}
	IGD_PPPD_DBG("username=%s,passwd=%s\n", config->pppoe.user, config->pppoe.passwd);
	if (mu_call(IF_MOD_PARAM_SET, config, sizeof(struct iface_conf), NULL, 0)) {
		IGD_PPPD_DBG("IF_MOD_PARAM_SET failed[to pppoe].\n");
		return -1;
	}
	return 0;
}
void auto_switch_to_dhcp(struct iface_conf *config)
{
	config->mode = MODE_DHCP;
	config->dhcp.mtu = 1500;
	if (mu_call(IF_MOD_PARAM_SET, config, sizeof(struct iface_conf), NULL, 0)) {
		IGD_PPPD_DBG("IF_MOD_PARAM_SET failed[to dhcp].\n");
	}
}
extern int get_sysflag(unsigned char bit);
static void auto_swtich_to_pppoe_cb(void *data)
{
	struct timeval tv;
	int uid = 1;
	long uptime = sys_uptime();
	static long last_uptime = 0;
	static int has_switch = 0;
	struct iface_info ifinfo;
	struct iface_conf config;

	if (!get_sysflag(SYSFLAG_RESET)) {
		goto stop;
	}

	memset(&ifinfo, 0, sizeof(ifinfo));
	if (mu_call(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &ifinfo, sizeof(ifinfo))) {
		IGD_PPPD_DBG("IF_MOD_IFACE_INFO failed.\n");
		goto stop;
	}

	if (ifinfo.net) {
		goto stop;
	}

	if (!ifinfo.link) {
		last_uptime = uptime;
		goto check;
	}
	
	if (!last_uptime) {
		last_uptime = uptime;
	}
	
	if (uptime >= last_uptime && uptime - last_uptime < 25) {
		goto check;
	} else {
		last_uptime = uptime;
	}
	
	memset(&config, 0, sizeof(config));
	if (mu_call(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &config, sizeof(config))) {
		IGD_PPPD_DBG("IF_MOD_PARAM_SHOW failed,\n");
		goto stop;
	}
	if (!has_switch && config.mode == MODE_DHCP) {
		IGD_PPPD_DBG("begin auto_switch_to_pppoe..\n");
		if (auto_switch_to_pppoe(&config)) {
			IGD_PPPD_DBG("auto_switch_to_pppoe failed.\n");
			goto stop;
		}
		has_switch = 1;
		goto check;
	}else if (has_switch && config.mode == MODE_PPPOE) {
		IGD_PPPD_DBG("auto pppoe failed, switch to dhcp..\n");
		auto_switch_to_dhcp(&config);
		goto stop;
	} else {
		goto stop;
	}
check:
	tv.tv_usec = 0;
	tv.tv_sec = 5;
	schedule(tv, auto_swtich_to_pppoe_cb, NULL);
	return;
stop:
	//IGD_PPPD_DBG("auto switch to pppoe stop.\n");
	return;
}

static int igd_system_init(void)
{
	char *ptr;
	struct timeval tv;
	int wguestchange = 0;
	int i = 0, j = 0;

	system_confirm_timezone();
		
#ifdef EEPROM_AMEND_5G
	eeprom_amend_5G();
#endif

	led_act(LED_POWER, LED_ACT_ON);
	led_act(LED_INTERNET, LED_ACT_OFF);
	led_net_timer(NULL);

	led_section_flag = 0;
	account_section_flag = 0;
	ptr = read_firmware("LOGIN_USER");
	arr_strcpy(login_account.user, ptr ? ptr : "admin");
	ptr = read_firmware("LOGIN_PWD");
	arr_strcpy(login_account.password, ptr ? ptr : "admin");

	ptr = read_firmware("RUSER");
	arr_strcpy(guest[i].user, ptr ? ptr : "");
	ptr = read_firmware("RPWD");
	arr_strcpy(guest[i].password, ptr ? ptr : "");
	if (!strlen(guest[i].user) || !strlen(guest[i].password)) {
		memset(&guest[i], 0x0, sizeof(struct sys_guest));
	} else {
		guest[i].invalid = 1;
		guest[i].readonly = 1;
		guest[i].index = i;
		i++;
	}
	
	ptr = read_firmware("WUSER");
	arr_strcpy(guest[i].user, ptr ? ptr : "");
	ptr = read_firmware("WPWD");
	arr_strcpy(guest[i].password, ptr ? ptr : "");
	ptr = uci_getval("system", "account", "wguestchange");
	wguestchange = ptr?atoi(ptr):0;
	if (ptr) {
		free(ptr);
		ptr = NULL;
	}
	if (wguestchange || !strlen(guest[i].user)
		|| !strlen(guest[i].password)) {
		memset(&guest[i], 0x0, sizeof(struct sys_guest));
	} else {
		guest[i].invalid = 1;
		guest[i].system = 1;
		guest[i].index = i;
	}
	igd_system_load();
	if (!led_section_flag) {
		uuci_set("system.led=state");
	} else {
		for (j = (LED_MIN + 1); j < LED_MAX; j++)
			led_act(j, sys_led_state ? LED_ACT_ENABLE : LED_ACT_DISABLE);
	}

	if (!account_section_flag)
		uuci_set("system.account=login");
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	rboot_event = schedule(tv, system_retime_cb, NULL);


	tv.tv_sec = 1;
	tv.tv_usec = 0;
	schedule(tv, igd_sys_task_timer, NULL);
#ifdef POWER_ON_CHECK_FIRMWARE
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	schedule(tv, self_check_firmware_cb, NULL);
#endif

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	schedule(tv, pppoe_check_cb, NULL);

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	schedule(tv, auto_swtich_to_pppoe_cb, NULL);
	
	return 0;
}

int system_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;

	switch (msgid) {
	case SYSTEM_MOD_INIT:
		ret = igd_system_init();
		break;
	case SYSTEM_MOD_DEV_CHECK:
		if (!rbuf || rlen != sizeof(int))
			return -1;
		ret = sys_dev_check((int *)rbuf);
		break;
	case SYSTEM_MOD_FW_UPDATE:
		ret = fw_update(FLAG_SERVER);
		break;
	case SYSTME_MOD_LOCAL_UPDATE:
		ret = fw_update(FLAG_LOCAL);
		break;
	case SYSTME_MOD_CLOUD_UPDATE:
		ret = fw_update(FLAG_CLOUD);
		break;
	case SYSTEM_MOD_VERSION_CHECK:
		if (!rbuf || rlen != sizeof(struct fw_info))
			return -1;
		ret = fw_version_check((struct fw_info *)rbuf);
		break;
	case SYSTEM_MOD_PORT_STATUS:
		ret = get_link_status((unsigned long *)rbuf);
		break;
	case SYSTEM_MOD_FW_DOWNLOAD:
		ret = fw_download();
		break;
	case SYSTEM_MOD_SYS_REBOOT:
		ret = sys_reboot();
		break;
	case SYSTEM_MOD_SYS_DEFAULT:
		if (!data || len != sizeof(int))
			return -1;
		ret = sys_default(*(int *)data);
		break;
	case SYSTEM_MOD_VERSION_INFO:
		if (!data || len != sizeof(struct fw_info))
			return -1;
		memcpy(&fwinfo, (struct fw_info *)data, sizeof(fwinfo));
		break;
	case SYSTME_MOD_SET_LED:
		ret = sys_set_led(data, len, rbuf, rlen);
		break;
	case SYSTME_MOD_GET_ACCOUNT:
		ret = get_login_account(rbuf, rlen);
		break;
	case SYSTME_MOD_SET_ACCOUNT:
		ret = set_login_account(data, len);
		break;
	case SYSTME_MOD_SYS_CMD:
		ret = system_cmdline(data, len);
		break;
	case SYSTME_MOD_MTD_PARAM:
		if (!data || !len || !rbuf || !rlen)
			return -1; 
		ret = system_get_mtd_param((char *)data, (char *)rbuf, len);
		break;
	case SYSTME_MOD_TIME_REBOOT:
		if (rbuf && rlen == sizeof(struct sys_time)) {
			memcpy(rbuf, &retime, sizeof(retime));
			return 0;
		}
		if (!data || (len != sizeof(struct sys_time)))
			return -1; 
		ret = igd_system_retime((struct sys_time *)data);
		break;
	case SYSTEM_MOD_DAEMON_ADD:
		ret = igd_sys_daemon_add(data);
		break;
	case SYSTEM_MOD_DAEMON_PLUG_ADD:
		ret = igd_plug_daemon_add(data, len);
		break;
	case SYSTEM_MOD_DAEMON_DEL:
		ret = igd_sys_daemon_del(data, 0);
		break;
	case SYSTEM_MOD_DAEMON_OOM:
		ret = igd_sys_daemon_oom(data, len);
		break;
	case SYSTEM_MOD_DAEMON_PID:
		ret = igd_sys_daemon_pid(data);
		break;
	case SYSTEM_MOD_WAITPID:
		ret = igd_sys_waitpid(data);
		break;
	case SYSTEM_MOD_TASK_ADD:
		ret = igd_sys_task_add(data, len);
		break;
	case SYSTEM_MOD_TASK_DUMP:
		ret = igd_sys_task_dump(data, len, rbuf, rlen);
		break;
	case SYSTEM_MOD_PPPOE_SUCC:
		ret = igd_sys_pppoe_succ();
		break;
	case SYSTEM_MOD_CONFIG_SET:
		ret = sys_config();
		break;
	case SYSTEM_MOD_CONFIG_GET:
		ret = sys_config_get();
		break;
	case SYSTEM_MOD_GUEST_SET:
		if (!data || len != sizeof(struct sys_guest))
			return -1;
		ret = set_login_guest((struct sys_guest *)data);
		break;
	case SYSTEM_MOD_GUEST_GET:
		if (!data || (len != sizeof(int)) || !rbuf || (rlen != sizeof(struct sys_guest)))
			return -1;
		ret = show_login_guest(*(int *)data, (struct sys_guest *)rbuf);
		break;
	case SYSTEM_MOD_TIMEZONE_SET:
		if (!data || len != sizeof(struct sys_timezone))
			return -1;
		ret = sys_set_timezone((struct sys_timezone *)data);
		break;
	case SYSTEM_MOD_TIMEZONE_GET:
		if (!rbuf || rlen != sizeof(struct sys_timezone))
			return -1;
		ret = sys_get_timezone((struct sys_timezone *)rbuf);
		break;
		
	default:
		break;
	}
	return ret;
}
