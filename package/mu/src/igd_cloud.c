#include <string.h>
#include <stdlib.h>
#include "linux_list.h"
#include "nlk_ipc.h"
#include "igd_cloud.h"
#include "igd_lib.h"
#include "igd_plug.h"

static unsigned char igd_cloud_cache_num = 0;
static struct list_head igd_cloud_cache_list = LIST_HEAD_INIT(igd_cloud_cache_list);
static unsigned long igd_cloud_flag[BITS_TO_LONGS(ICFT_MAX)];
static unsigned long igd_cloud_alone_check_up_flag[BITS_TO_LONGS(CCA_MAX)];
static unsigned long igd_cloud_alone_lock_flag[BITS_TO_LONGS(CCA_MAX)];
static unsigned long igd_cloud_alone_ver_flag[BITS_TO_LONGS(CCA_MAX)];
#define MAX_DNS_MSG_CNT 50
int dns_total_cnt = 0;
struct dns_msg {
	uint32_t addr[DNS_ADDR_CAP_MX];
	unsigned char dns[IGD_DNS_LEN];
	unsigned char mac[ETH_ALEN];
	time_t now;
};
struct dns_msg dm[MAX_DNS_MSG_CNT];

#define DNS_UPLOAD_FILE "/tmp/dns_upload_file"
#define DNS_UPLOAD_GZ_FILE DNS_UPLOAD_FILE".gz"
#define DNS_FILE_MX_SIZE 10 * 1024

#ifdef FLASH_4_RAM_32
#define IGD_CLOUD_DBG(fmt,args...) do{}while(0)
#else
#define IGD_CLOUD_DBG(fmt,args...) \
	igd_log("/tmp/igd_cloud_dbg", "DBG:%s[%d]:"fmt, __FUNCTION__, __LINE__, ##args);
#endif

#ifdef FLASH_4_RAM_32
#define IGD_CLOUD_ERR(fmt,args...) do{}while(0)
#else
#define IGD_CLOUD_ERR(fmt,args...) \
	igd_log("/tmp/igd_cloud_err", "ERR:%s[%d]:"fmt, __FUNCTION__, __LINE__, ##args);
#endif

struct igd_cloud_cache_info {
	struct list_head list;
	int len;
	void *cache;
};

static struct igd_cloud_cache_info *
	igd_cloud_find_cache(void *cache, int len)
{
	struct igd_cloud_cache_info *icci;

	list_for_each_entry(icci, &igd_cloud_cache_list, list) {
		if (icci->len != len)
			continue;
		if (memcmp(cache, icci->cache, len))
			continue;
		return icci;
	}
	return NULL;
}

static struct igd_cloud_cache_info *
	igd_cloud_add_cache(void *cache, int len)
{
	struct igd_cloud_cache_info *icci;

	icci = malloc(sizeof(*icci));
	if (!icci) {
		IGD_CLOUD_ERR("malloc fail, %d\n", sizeof(*icci));
		return NULL;
	}
	icci->cache = malloc(len);
	if (!icci->cache) {
		IGD_CLOUD_ERR("malloc fail, %d\n", len);
		return NULL;
	}
	icci->len = len;
	memcpy(icci->cache, cache, len);
	list_add(&icci->list, &igd_cloud_cache_list);
	igd_cloud_cache_num++;
	IGD_CLOUD_DBG("cache num: %d\n", igd_cloud_cache_num);
	return icci;
}

static void igd_cloud_del_cache(
	struct igd_cloud_cache_info *input)
{
	struct igd_cloud_cache_info *icci;

	if (input) {
		list_del(&input->list);
		free(input->cache);
		free(input);
		igd_cloud_cache_num--;
	} else {
		list_for_each_entry_reverse(icci, &igd_cloud_cache_list, list) {
			igd_cloud_del_cache(icci);
			break;
		}
	}
}

static int igd_cloud_cache_add(void *cache, int len)
{
	struct igd_cloud_cache_info *icci;

	if (len <= 0)
		return -1;

	if (igd_cloud_cache_num > IGD_CLOUD_CACHE_MX)
		igd_cloud_del_cache(NULL);

	icci = igd_cloud_find_cache(cache, len);
	if (icci) {
		list_move(&icci->list, &igd_cloud_cache_list);
		return 0;
	}
	icci = igd_cloud_add_cache(cache, len);
	if (icci)
		return 0;
	return -1;
}

static int igd_cloud_cache_dump(void *dump, int len)
{
	struct igd_cloud_cache_info *icci;

	list_for_each_entry(icci, &igd_cloud_cache_list, list) {
		if (len < icci->len) {
			IGD_CLOUD_ERR("big, %d,%d\n", len, icci->len);
			igd_cloud_del_cache(icci);
			return -1;
		}
		memcpy(dump, icci->cache, icci->len);
		len = icci->len;
		igd_cloud_del_cache(icci);
		return len;
	}
	return 0;
}

static int igd_cloud_read(char *file, char *key, char *val, int val_len)
{
	FILE *fp;
	int len, key_len = strlen(key);
	char buf[512];

	fp = fopen(file, "rb");
	if (!fp) {
		IGD_CLOUD_ERR("fopen fail, %s\n", RCONF_CHECK);
		return -1;
	}

	while (1) {
		memset(buf, 0, sizeof(buf));
		if (!fgets(buf, sizeof(buf) - 1, fp))
			break;
		if (memcmp(buf, key, key_len))
			continue;
		len = strlen(buf);
		while ((len > 0) && 
			((buf[len - 1] == '\r')
			|| (buf[len - 1] == '\n'))) {
			buf[len - 1] = '\0';
			len--;
		}
		len = strlen(buf + 4);
		if (len < 0) {
			IGD_CLOUD_ERR("len err, %d\n", len);
			break;
		}
		strncpy(val, buf + 4, val_len - 1);
		val[val_len - 1] = '\0';
		fclose(fp);
		return len;
	}
	fclose(fp);
	return -1;
}

static int igd_cloud_read_rconf(char *key, char *val, int val_len)
{
	return igd_cloud_read(RCONF_CHECK, key, val, val_len);
}

static int igd_cloud_read_alone(int flag, char *val, int val_len)
{
	int fd, ret;
	char file[64];

	snprintf(file, sizeof(file), "%s/%d", ALONE_DIR, flag);
	fd = open(file, O_RDONLY);
	if (fd < 0)
		return -1;
	ret = read(fd, val, val_len - 1);
	if (ret < 0)
		goto err;
	val[ret] = 0;
err:
	close(fd);
	return ret;
}

static int igd_cloud_rconf_check(int order, int flag)
{
	int i, len;
	char *ptr, tmp[256];
	unsigned char buf[512];

	IGD_CLOUD_DBG("%d, %d\n", order, flag);

	memset(buf, 0, sizeof(buf));
	CC_PUSH2(buf, 0, 8);
	CC_PUSH2(buf, 2, order);
	i = 8;

	if (flag != -1) {
		CC_PUSH2(buf, i, flag);
		i += 2;
	}

	ptr = read_firmware("CURVER");
	if (!ptr) {
		IGD_CLOUD_ERR("read curver fail\n");
		return -1;
	}
	IGD_CLOUD_DBG("CURVER:[%s]\n", ptr);

	len = strlen(ptr);
	CC_PUSH1(buf, i, len);
	i += 1;
	CC_PUSH_LEN(buf, i, ptr, len);
	i += len;

	len = 0;
	ptr = read_firmware("VENDOR");
	if (!ptr) {
		IGD_CLOUD_ERR("read VENDOR fail\n");
		return -1;
	}
	len += snprintf(tmp + len, sizeof(tmp) - len, "%s_", ptr);

	ptr = read_firmware("PRODUCT");
	if (!ptr) {
		IGD_CLOUD_ERR("read PRODUCT fail\n");
		return -1;
	}
	len += snprintf(tmp + len, sizeof(tmp) - len, "%s", ptr);
	IGD_CLOUD_DBG("MODEL:[%s]\n", tmp);

	CC_PUSH1(buf, i, len);
	i += 1;
	CC_PUSH_LEN(buf, i, tmp, len);
	i += len;

	memset(tmp, 0, sizeof(tmp));
	if (flag == -1) {
		len = igd_cloud_read_rconf(
			RCONF_FLAG_VER, tmp, sizeof(tmp) - 1);
		if (len < 0) {
			IGD_CLOUD_ERR("rconf ver fail\n");
			return -1;
		}
		IGD_CLOUD_DBG("RCONF_VER:[%s]\n", tmp);
	} else {
		len = igd_cloud_read_alone(
			flag, tmp, sizeof(tmp) - 1);
		if (len < 0)
			len = snprintf(tmp, sizeof(tmp), "0");
		IGD_CLOUD_DBG("ALONE_VER:%d,[%s]\n", flag, tmp);
	}

	CC_PUSH1(buf, i, len);
	i += 1;
	CC_PUSH_LEN(buf, i, tmp, len);
	i += len;

	CC_PUSH2(buf, 0, i);
	CC_CALL_ADD(buf, i);
	return 0;
}

static pid_t rconf_pid = 0;
static long rconf_update_timer = 0; 

static int igd_cloud_rconf_pid(pid_t *pid, int len)
{
	if (sizeof(*pid) != len)
		return -1;
	if (rconf_pid && *pid)
		return 1;
	if (!rconf_pid && !(*pid))
		return 0;

	rconf_pid = *pid;
	if (rconf_pid > 0) {
		rconf_update_timer = sys_uptime() + 60*6;
	} else if (rconf_update_timer) {
		rconf_update_timer = 0;
	}
	IGD_CLOUD_DBG("pid:%d, %d, %d\n",
		*pid, rconf_pid, rconf_update_timer);
	return 0;
}

static int igd_cloud_rconf_ver_upload(int order, int flag)
{
	int i, len;
	char tmp[256];
	unsigned char buf[128];

	IGD_CLOUD_DBG("start\n");

	memset(buf, 0, sizeof(buf));
	CC_PUSH2(buf, 0, 8);
	CC_PUSH2(buf, 2, order);
	i = 8;

	memset(tmp, 0, sizeof(tmp));
	if (flag == -1) {
		len = igd_cloud_read_rconf(
			RCONF_FLAG_VER, tmp, sizeof(tmp) - 1);
		if (len < 0) {
			IGD_CLOUD_ERR("rconf ver fail\n");
			return -1;
		}
	} else {
		len = igd_cloud_read_alone(flag, tmp, sizeof(tmp) - 1);
		if (len < 0) {
			IGD_CLOUD_ERR("alone ver fail, %d\n", flag);
			return -1;
		}
	}
	IGD_CLOUD_DBG("VER:[%s]\n", tmp);

	CC_PUSH1(buf, i, len);
	i += 1;
	CC_PUSH_LEN(buf, i, tmp, len);
	i += len;
	if (flag != -1) {
		CC_PUSH2(buf, i, flag);
		i += 2;
	}

	CC_PUSH2(buf, 0, i);
	CC_CALL_ADD(buf, i);
	return 0;
}

static int igd_cloud_rconf_get(char *key, int key_len, char *info, int info_len)
{
	if (!key || !key_len || !info || !info_len) {
		IGD_CLOUD_ERR("input err, %p,%d,%p,%d\n",
			key, key_len, info, info_len);
		return -1;
	}
	return igd_cloud_read_rconf(key, info, info_len);
}

static int igd_cloud_rconf_get_info(char *key, int key_len, char *info, int info_len)
{
	char buf[512], *ptr;

	if (!key || !key_len || !info || !info_len) {
		IGD_CLOUD_ERR("input err, %p,%d,%p,%d\n",
			key, key_len, info, info_len);
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	if (igd_cloud_read_rconf(key, buf, sizeof(buf)) <= 0)
		return -1;
	ptr = strchr(buf, ',');
	if (!ptr)
		return -1;
	strncpy(info, ptr + 1, info_len - 1);
	info[info_len - 1] = '\0';
	return 0;
}

static int igd_cloud_flag_act(int *bit, int len)
{
	if (len != sizeof(*bit)*2) {
		IGD_CLOUD_ERR("input err\n");
		return -1;
	}
	if (bit[0] < 0 || bit[0] >= ICFT_MAX) {
		IGD_CLOUD_ERR("bit err, %d\n", *bit);
		return -1;
	}

	IGD_CLOUD_DBG("%d,%d\n", bit[0], bit[1]);

	if (bit[1] == NLK_ACTION_DUMP)
		return igd_test_bit(bit[0], igd_cloud_flag) ? 1 : 0;
	else if (bit[1] == NLK_ACTION_ADD)
		igd_set_bit(bit[0], igd_cloud_flag);
	else if (bit[1] == NLK_ACTION_DEL)
		igd_clear_bit(bit[0], igd_cloud_flag);
	else
		return -1;
	return 0;
}

static int igd_cloud_alone_flag(int type, int *bit, int len)
{
	unsigned long *bit_flag;

	if (len != sizeof(*bit)*2) {
		IGD_CLOUD_ERR("input err\n");
		return -1;
	}
	if (bit[0] < 0 || bit[0] >= CCA_MAX) {
		IGD_CLOUD_ERR("bit err, %d\n", *bit);
		return -1;
	}

	IGD_CLOUD_DBG("%d,%d,%d\n", type, bit[0], bit[1]);

	if (type == 1)
		bit_flag = igd_cloud_alone_check_up_flag;
	else if (type == 2)
		bit_flag = igd_cloud_alone_lock_flag;
	else if (type == 3)
		bit_flag = igd_cloud_alone_ver_flag;
	else
		return -1;

	if (bit[1] == NLK_ACTION_DUMP)
		return igd_test_bit(bit[0], bit_flag) ? 1 : 0;
	else if (bit[1] == NLK_ACTION_ADD)
		igd_set_bit(bit[0], bit_flag);
	else if (bit[1] == NLK_ACTION_DEL)
		igd_clear_bit(bit[0], bit_flag);
	else
		return -1;
	return 0;
}

static int igd_cloud_ip(
	struct in_addr *iip, int ilen,
	struct in_addr *oip, int olen)
{
	static struct in_addr ip;

	if (iip) {
		if (ilen != sizeof(*iip))
			return -1;
		ip = *iip;
	} else if (oip) {
		if (olen != sizeof(*oip))
			return -1;
		*oip = ip;
	} else {
		return -1;
	}
	return 0;
}

static void igd_cloud_flag_check(void)
{
	int i;

	if (!igd_test_bit(ICFT_ONLINE, igd_cloud_flag))
		return;

	if (igd_test_bit(ICFT_CHECK_RCONF, igd_cloud_flag))
		igd_cloud_rconf_check(CSO_REQ_ROUTER_CONFIG, -1);

	if (igd_test_bit(ICFT_UP_RCONF_VER, igd_cloud_flag))
		igd_cloud_rconf_ver_upload(CSO_REQ_CONFIG_VER, -1);

	for (i = CCA_JS; i < CCA_MAX; i++) {
		if (igd_test_bit(i, igd_cloud_alone_check_up_flag))
			igd_cloud_rconf_check(CSO_REQ_AREA_CONFIG, i);

		if (igd_test_bit(i, igd_cloud_alone_ver_flag))
			igd_cloud_rconf_ver_upload(CSO_REQ_AREA_VER, i);
	}
}

static void igd_cloud_timer(void *data)
{
	struct timeval tv;
	long systime = sys_uptime();
	static long errlog_timer = 0;
	static long l7_gather_timer = 0; 
	static long url_host_timer = 0; 
	static long dbg_log_timer =0;
	int dbglog_data = CSS_DBGLOG;

	if (systime > 120)
		igd_cloud_flag_check();

	if (systime > (errlog_timer + 5*60)) {
		system("cloud_exchange l&");
		errlog_timer = systime;
	}

	if (systime > (l7_gather_timer + 1*60*60)) {
		/*system("cloud_exchange 7&");*/
		l7_gather_timer = systime;
	}

	if (systime > (url_host_timer + 2*60)) {
		system("cloud_exchange u&");
		url_host_timer = systime;
	}
	
	if (systime > (dbg_log_timer + 20*60)) {
		if (mu_call(IGD_PLUG_SWITCH_GET, &dbglog_data, sizeof(dbglog_data), NULL, 0) == 1)
			system("command_log.sh /tmp/dbg_log &");
		dbg_log_timer = systime;
	}

	if (rconf_update_timer && 
		(rconf_update_timer < systime) &&
		(rconf_pid > 0)) {
		IGD_CLOUD_DBG("timeout kill rconf update %d\n", rconf_pid);
		rconf_update_timer = 0;
		kill(rconf_pid, SIGTERM);
		rconf_pid = 0;
	}

	tv.tv_sec = 60;
	tv.tv_usec = 0;
	if (!schedule(tv, igd_cloud_timer, NULL))
		IGD_CLOUD_ERR("schedule err\n");
}

static int igd_cloud_init(void)
{
	struct timeval tv;

	tv.tv_sec = 60;
	tv.tv_usec = 0;
	if (!schedule(tv, igd_cloud_timer, NULL))
		IGD_CLOUD_ERR("schedule err\n");

	memset(igd_cloud_flag, 0, sizeof(igd_cloud_flag));
	igd_set_bit(ICFT_CHECK_RCONF, igd_cloud_flag);

	memset(igd_cloud_alone_check_up_flag, 0xFF,
		sizeof(igd_cloud_alone_check_up_flag));
	igd_clear_bit(CCA_SYSTEM, igd_cloud_alone_check_up_flag);
	memset(igd_cloud_alone_lock_flag, 0,
		sizeof(igd_cloud_alone_lock_flag));
	memset(igd_cloud_alone_ver_flag, 0,
		sizeof(igd_cloud_alone_ver_flag));
	return 0;
}

void format_mac(unsigned char *mac, char *macptr)
{
	char buf[18] = {0};

	sprintf(buf, "%02X%02X%02X%02X%02X%02X",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	strcpy(macptr, buf);
}

int format_ip(uint32_t *ip, char *ipstr)
{
	int i = 0;
	int count = 0;
	int offset = 0;
	struct in_addr addr;

	for (i=0; i<DNS_ADDR_CAP_MX; i++)
	{
		if ((*ip) <= 0)
			break;
		memset(&addr, 0x00, sizeof(addr));
		addr.s_addr = htonl(*ip);
		sprintf(ipstr + offset, "%s,", inet_ntoa(addr));
		ip++;
		count++;
		offset = strlen(ipstr);
	}
	ipstr[offset - 1] = '\0';
	return count;
}

int cp_dns_msg(struct nlk_dns_msg *ndm)
{
	memcpy(dm[dns_total_cnt].addr, ndm->addr, sizeof(dm->addr));
	memcpy(dm[dns_total_cnt].dns, ndm->dns, sizeof(dm->dns));
	memcpy(dm[dns_total_cnt].mac, ndm->mac, sizeof(dm->mac));
	dm[dns_total_cnt].now = time(NULL);
	dns_total_cnt++;
	return 0;
}

void release_dns_msg(void)
{
	memset(dm, 0x00, sizeof(dm));
	dns_total_cnt = 0;
}

int write_dns_upload_file(void)
{
	FILE *fp = NULL;
	char buf[18] = {0};
	int count = 0;
	char ipstr[80] = {0};
	int i = 0;
	unsigned int devid = get_devid();

	fp = fopen(DNS_UPLOAD_FILE, "wb");
	if (fp == NULL) {
		release_dns_msg();
		return -1;
	}
	for (i=0; i<dns_total_cnt; i++) {
		fprintf(fp, "%u\1", devid);
		format_mac(dm[i].mac, buf);
		fprintf(fp, "%s\1", buf);
		fprintf(fp, "%.*s\1", IGD_DNS_LEN, dm[i].dns);
		count = format_ip(dm[i].addr, ipstr);
		fprintf(fp, "%d:", count);
		fprintf(fp, "%s\1",  ipstr);
		fprintf(fp, "%ld\n", dm[i].now);
	}
	fclose(fp);
	release_dns_msg();
	return 0;
}

int dns_upload(void)
{
	if (system("cloud_exchange n &") < 0) {
		remove(DNS_UPLOAD_FILE);
		return -1;
	}
	return 0;
}

int igd_cloud_dns_upload(struct nlk_dns_msg *ndm, int len)
{
	if (!len || len != sizeof(*ndm))
		return -1;
	if (cp_dns_msg(ndm))
		return -1;
	if (dns_total_cnt < MAX_DNS_MSG_CNT)
		return 0;
	if (write_dns_upload_file())
		return -1;
	return dns_upload();
}

int igd_cloud_call(MSG_ID msgid, void *data, int d_len, void *back, int b_len)
{
	switch (msgid) {
	case IGD_CLOUD_INIT:
		return igd_cloud_init();
	case IGD_CLOUD_CACHE_ADD:
		return igd_cloud_cache_add(data, d_len);
	case IGD_CLOUD_CACHE_DUMP:
		return igd_cloud_cache_dump(back, b_len);
	case IGD_CLOUD_RCONF_FLAG:
		return igd_cloud_rconf_pid(data, d_len);
	case IGD_CLOUD_RCONF_GET:
		return igd_cloud_rconf_get(data, d_len, back, b_len);
	case IGD_CLOUD_RCONF_GET_INFO:
		return igd_cloud_rconf_get_info(data, d_len, back, b_len);
	case IGD_CLOUD_FLAG:
		return igd_cloud_flag_act(data, d_len);
	case IGD_CLOUD_ALONE_CHECK_UP:
		return igd_cloud_alone_flag(1, data, d_len);
	case IGD_CLOUD_ALONE_LOCK:
		return igd_cloud_alone_flag(2, data, d_len);
	case IGD_CLOUD_ALONE_VER:
		return igd_cloud_alone_flag(3, data, d_len);
	case IGD_CLOUD_IP:
		return igd_cloud_ip(data, d_len, back, b_len);
	case IGD_CLOUD_DNS_UPLOAD:
		return igd_cloud_dns_upload(data, d_len);
	default:
		IGD_CLOUD_ERR("msgid:%d nonsupport\n", msgid);
		return -1;
	}
	return -1;
}

