#include "igd_lib.h"
#include "igd_interface.h"
#include "igd_url_safe.h"
#include "igd_cloud.h"

static struct urlsafe_conf config;
static char sdomain[IGD_DNS_LEN];

#ifdef FLASH_4_RAM_32
#define URL_SAFE_DEBUG(fmt,args...) do {}while(0)
#else
#define URL_SAFE_DEBUG(fmt,args...) console_printf("%s :[%d]\n" ,__FUNCTION__, __LINE__)
#endif

static void url_safe_cb(void *data)
{
	struct timeval tv;
	
	tv.tv_sec = 120;
	tv.tv_usec = 0;
	if (!schedule(tv, url_safe_cb, NULL)) {
		URL_SAFE_DEBUG("schedule url safe event error\n");
		return;
	}
	if (!config.devid)
		config.devid = get_devid();
	exec_cmd("igd_dns %s %s %u &", sdomain, IPC_PATH_MU, URL_SAFE_MOD_GET_DNS);
}

static inline int check_url_char(char data)
{
	if (data > ' ' && data <= '~')
		return 1;
	else
		return 0;
}

static int wlist_clean(void)
{
	struct nlk_msg_comm nlk;
	memset(&nlk, 0x0, sizeof(nlk));
	nlk.action = NLK_ACTION_DEL;
	nlk.mid = URL_SEC_WLIST;
	return nlk_head_send(NLK_MSG_URL_SAFE, &nlk, sizeof(nlk));
}

static int set_wlist2kernel(char (*url)[IGD_DNS_LEN], int nr)
{
	struct nlk_msg_comm nlk;
	memset(&nlk, 0x0, sizeof(nlk));
	nlk.action = NLK_ACTION_ADD;
	nlk.mid = URL_SEC_WLIST;
	nlk.obj_nr = nr;
	nlk.obj_len = IGD_DNS_LEN;
	return nlk_data_send(NLK_MSG_URL_SAFE, &nlk, sizeof(nlk), url, nr * IGD_DNS_LEN);
}

static int set_wlist(void)
{
	int i = 0, j;
	char *host = NULL;
	char *tmp = NULL;
	char *data = NULL;
	char buf[IGD_DNS_LEN];
	char url[URL_WLIST_PER_MX][IGD_DNS_LEN] = {{0},};
	unsigned char pwd[16] = {0xcc, 0xfc, 0x78, 0x66, 0x35, 0x32, 0x97, 0xfc, 0x78, 0x99};
	
	if (access(URL_SAFE_WHITE_LIST, 0))
		return -1;
	data = malloc(URL_SAFE_DATA_MX);
	if (!data) {
		URL_SAFE_DEBUG("malloc url safe wlist mem error\n");
		return -1;
	}
	if (igd_aes_dencrypt(URL_SAFE_WHITE_LIST, (unsigned char *)data, URL_SAFE_DATA_MX, pwd) <= 0) {
		URL_SAFE_DEBUG("dencrypt url safe wlist  error\n");
		return -1;
	}

	host = data;
	wlist_clean();
	while ((tmp = strchr(host, '\n')) != NULL) {
		if (tmp - host > sizeof(buf) - 1)
			goto next;
		__arr_strcpy_end((unsigned char *)buf, (unsigned char *)host, tmp - host, '\n');
		if (!strlen(buf))
			goto next;
		for (j = 0; j < strlen(buf); j++) {
			if (check_url_char(buf[j])) {
				url[i][j] = buf[j];
			} else
				break;
		}
		i++;
		if (i >= URL_WLIST_PER_MX) {
			set_wlist2kernel(url, i);
			i = 0;
			memset(url, 0x0, sizeof(url));
		}
		memset(buf, 0x0, sizeof(buf));
	next:
		host = tmp + 1;
		continue;
	}
	if (i != 0) 
		set_wlist2kernel(url, i);
	free(data);
	remove(URL_SAFE_WHITE_LIST);
	return 0;
}

static int set_trust_ip(struct urlsafe_trust *info)
{
	struct nlk_url_ip nlk;
	
	memset(&nlk, 0x0, sizeof(nlk));
	nlk.addr = info->ip.s_addr;
	__arr_strcpy_end(nlk.url, info->url, sizeof(nlk.url), '\0');
	nlk.comm.mid = URL_SET_SAFE_IP;
	return nlk_head_send(NLK_MSG_URL_SAFE, &nlk, sizeof(nlk));
}

static int set_enable(int enable)
{
	struct nlk_url_set info;
	if (enable < 0)
		return IGD_ERR_DATA_ERR;
	memset(&info, 0x0, sizeof(info));
	info.comm.mid = URL_SEC_ENABLE;
	info.enable = enable;
	return nlk_head_send(NLK_MSG_URL_SAFE, &info, sizeof(info));
}

static int set_param(void)
{
	struct nlk_url_sec info;

	if (!config.devid || !config.local.s_addr || !config.remote[0].s_addr)
		return -1;
	memset(&info, 0x0, sizeof(info));
	loop_for_each(0, DNS_IP_MX) {
		info.sip[i] = config.remote[i].s_addr;
	} loop_for_each_end();
	
	info.sport = URL_SERVER_PORT;
	info.rip = config.local.s_addr;
	info.rport = 80;
	info.devid = config.devid;
	arr_strcpy(info.rdhost, config.rd_url);
	info.comm.mid = URL_SET_PARAM;
	return nlk_head_send(NLK_MSG_URL_SAFE, &info, sizeof(info));
}

static int url_safe_save_param(void)
{
	char cmd[128] = {0};
	
	snprintf(cmd, 128, "%s.security.status=%d", URLSAFE_CONFIG_FILE, config.enable);
	uuci_set(cmd);
	return 0;
}

static int set_safe_server(void)
{
	if (CC_CALL_RCONF_INFO(RCONF_FLAG_URLSAFE, sdomain, sizeof(sdomain))) {
		URL_SAFE_DEBUG("can not get url safe server\n");
		return -1;
	}
	if (CC_CALL_RCONF_INFO(RCONF_FLAG_URLREDIRECT, config.rd_url, sizeof(config.rd_url))) {
		URL_SAFE_DEBUG("can not get url safe rd host\n");
		return -1;
	}
	return 0;
}

static int url_safe_init(void)
{
	char *lan_s = NULL;
	char *enable_s = NULL;
	
	memset(&config, 0x0, sizeof(config));
	enable_s = uci_getval(URLSAFE_CONFIG_FILE, "security", "status");
	lan_s = uci_getval(IF_CONFIG_FILE, LAN_UINAME, "ipaddr");
	if (enable_s) {
		config.enable = atoi(enable_s);
		free(enable_s);
	} else {
		config.enable = 1;
	}
	if (lan_s) {
		inet_aton(lan_s, &config.local);
		free(lan_s);
	}	
	if (set_enable(config.enable)) {
		URL_SAFE_DEBUG("set url safe enable error\n");
		return -1;
	}
	if (set_wlist()) {
		URL_SAFE_DEBUG("set url safe wlist error\n");
		return -1;
	}
	if (set_safe_server()) {
		URL_SAFE_DEBUG("set url safe server error\n");
		return -1;
	}
	url_safe_cb(NULL);
	return 0;
}

int urlsafe_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;
	struct igd_dns *dns = NULL;
	struct sys_msg_ipcp *ipcp = NULL;
	
	switch (msgid) {
	case URL_SAFE_MOD_INIT:
		ret = url_safe_init();
		break;
	case URL_SAFE_MOD_SET_ENABLE:
		if (!data || len != sizeof(int))
			return -1;
		ret = set_enable(*(int *)data);
		if (!ret) {
			config.enable = *(int *)data;
			url_safe_save_param();
		}
		break;
	case URL_SAFE_MOD_GET_DNS:
		if (!data || len != sizeof(struct igd_dns))
			return -1;
		dns = (struct igd_dns*)data;
		if (dns->nr <= 0 || dns->nr > DNS_IP_MX)
			return -1;
		memcpy(config.remote, dns->addr, sizeof(config.remote));
		ret = set_param();
		break;
	case URL_SAFE_MOD_SET_IP:
		if (!data || len != sizeof(struct urlsafe_trust))
			return -1;
		ret = set_trust_ip((struct urlsafe_trust *)data);
		break;
	case URL_SAFE_MOD_SET_WLIST:
		ret = set_wlist();
		break;
	case URL_SAFE_MOD_SET_SERVER:
		ret = set_safe_server();
		break;
	case URL_SAFE_MOD_SET_LANIP:
		if (!data || len != sizeof(struct sys_msg_ipcp))
			return -1;
		ipcp = (struct sys_msg_ipcp *)data;
		config.local.s_addr = ipcp->ip.s_addr;
		ret = set_param();
		break;
	default:
		break;
	}

	return ret;
}
