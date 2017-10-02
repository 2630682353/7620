#include "igd_lib.h"
#include "igd_url_log.h"
#include "igd_cloud.h"

static struct in_addr ip;
static unsigned int devid;
static char sdomain[IGD_DNS_LEN];

#ifdef FLASH_4_RAM_32
#define URL_LOG_DEBUG(fmt,args...) do {}while(0)
#else
#define URL_LOG_DEBUG(fmt,args...) console_printf("%s :[%d]\n" ,__FUNCTION__, __LINE__)
#endif

static inline int check_url_char(char data)
{
	if (data > ' ' && data <= '~')
		return 1;
	else
		return 0;
}

#if 0
static int send_log_file(void)
{
	int sock;
	int ov = 65535;
	char *data;
	struct stat st;
	struct sockaddr_in remote;
	unsigned int len, cnt, nlen;
	unsigned char pwd[16] = {0xcd, 0xfc, 0x78, 0x6b, 0x35, 0x3e, 0x97, 0xfc, 0x78, 0x99};

	if (stat(URL_LOG_ZIP, &st)) {
		URL_LOG_DEBUG("stat error\n");
		goto out;
	}
	cnt = st.st_size + 256;
	data = malloc(cnt);
	if (NULL == data) {
		URL_LOG_DEBUG("no mem\n");
		goto out;
	}
	memset(data, 0x0, cnt);
	nlen = st.st_size + (16 - (unsigned int)st.st_size % 16);
	len = sprintf(data, "POST /URLLog1 HTTP/1.1\r\n");
	len += sprintf(data + len, "User-Agent: wiair\r\n");
	len += sprintf(data + len, "Accept: */*\r\n");
	len += sprintf(data + len, "Host: %s\r\n", sdomain);
	len += sprintf(data + len, "Connection: Keep-Alive\r\n");
	len += sprintf(data + len, "Content-Type: application/x-www-form-urlencoded\r\n");
	len += sprintf(data + len, "Content-Length: %u\r\n\r\n", nlen);
	cnt = igd_aes_encrypt(URL_LOG_ZIP, (unsigned char *)data + len, cnt - len, pwd);
	if (cnt <= 0) {
		URL_LOG_DEBUG("enc error\n");
		goto free;
	}
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		URL_LOG_DEBUG("socket error\n");
		goto free;
	}
	memset(&remote, 0x0, sizeof(remote));
	remote.sin_family = AF_INET;
	remote.sin_port = htons(80);
	remote.sin_addr.s_addr = ip.s_addr;
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void *)&ov, sizeof(ov))) {
		URL_LOG_DEBUG("set send buf error\n");
		goto sock_fd;
	}
	if (connect(sock, (struct sockaddr *)&remote, sizeof(remote)) < 0) {
		URL_LOG_DEBUG("connect sock error\n");
		goto sock_fd;
	}
	if (write(sock, data, len + cnt) < 0)
		URL_LOG_DEBUG("send log file error\n");
sock_fd:
	close(sock);
free:
	free(data);
out:
	remove(URL_LOG_ZIP);
	return 0;
}
#endif

static int dump_url(void)
{
#if 0
	int nr = 0;
	pid_t pid = 0;
	FILE *fp = NULL;
	struct nlk_http_host nlk;
	struct nlk_url_log *tmp = NULL;
	struct nlk_url_log log[URL_LOG_MX_NR];

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.comm.key = NLK_HTTP_HOST_SHOP,
	nlk.comm.mid = NLK_URL_LOG_DUMP;
	nlk.comm.obj_nr = 40;
	nlk.comm.obj_len = sizeof(struct nlk_url_log);
	nr = nlk_dump_from_kernel(NLK_MSG_URL_LOG, &nlk, sizeof(nlk), log, URL_LOG_MX_NR);
	if (nr <= 0)
		return -1;
	if (!ip.s_addr || !devid)
		return -1;
	fp = fopen(URL_LOG_FILE, "w");
	if (NULL == fp) {
		URL_LOG_DEBUG("open url log file error\n");
		return -1;
	}
	
	loop_for_each(0, nr) {
		tmp = &log[i];
		fprintf(fp, "%u,%d,%d,%02x%02x%02x%02x%02x%02x,%u,%s\n", devid, tmp->id, tmp->type, MAC_SPLIT(tmp->mac), (unsigned int)tmp->time, tmp->url);
	} loop_for_each_end();
	
	fclose(fp);
	pid = fork();
	if (pid < 0)
		return -1;
	else if (pid == 0) {
		exec_cmd("gzip %s", URL_LOG_FILE);
		send_log_file();
		exit(0);
	}
#endif
	return 0;
}

static int set_server_dns_info(struct igd_dns *info)
{
	if (info->nr <= 0)
		return -1;
	ip.s_addr = info->addr[0].s_addr;
	return 0;
}

static int set_log_param(void)
{
	int i = 0, j = 0; 
	int id, type, nr = 0;
	struct nlk_http_host nlk;
	char buf[128] = {0};
	char *url = NULL;
	char *tmp = NULL;
	char *data = NULL;
	char suffix[IGD_SUFFIX_LEN] = {0};
	char domain[IGD_DNS_LEN] = {0};
	char uri[IGD_URI_LEN]= {0};
	struct nlk_url_data log[URL_LOG_LIST_MX];
	unsigned char pwd[16] = {0xcc, 0xfc, 0x78, 0x66, 0x35, 0x32, 0x97, 0xfc, 0x78, 0x99};
	
	if (access(URL_LOG_DOMAIN_FILE, 0))
		return 0;
	data = malloc(URL_LOG_LEN_MX);
	if (!data) {
		URL_LOG_DEBUG("malloc url log data error\n");
		return -1;
	}
	memset(data, 0x0, URL_LOG_LEN_MX);
	if (igd_aes_dencrypt(URL_LOG_DOMAIN_FILE, (unsigned char *)data, URL_LOG_LEN_MX, pwd) <= 0) {
		URL_LOG_DEBUG("dencrypt url domain error\n");
		return -1;
	}
	
	url = data;
	while ((tmp = strchr(url, '\n')) != NULL) {
		if (tmp - url > sizeof(buf) - 1)
			goto next;
		strncpy(buf, url, tmp - url + 1);
		nr = sscanf(buf, "%d,%d,%[^,],%[^,],%[^,]", &type, &id, domain, uri, suffix);
		if ((nr != 5) || !strlen(domain)) {
			URL_LOG_DEBUG("%s:%d\n", __FUNCTION__, __LINE__);
			goto next;
		}
		for (j = 0; j < strlen(domain); j++) {
			if (check_url_char(domain[j])) {
				log[i].url[j] = domain[j];
			} else
				break;
		}
		__arr_strcpy_end((unsigned char *)log[i].uri, (unsigned char *)uri, sizeof(log[i].uri) - 1, ' ');
		__arr_strcpy_end((unsigned char *)log[i].suffix, (unsigned char *)suffix, sizeof(log[i].suffix) - 1, '\n');
		log[i].id = id;
		log[i].type = type;
		//URL_LOG_DEBUG("%d, %d, +%s+, +%s+, +%s+\n", log[i].type, log[i].id, log[i].url, log[i].uri, log[i].suffix);
		memset(buf, 0x0, sizeof(buf));
		memset(suffix, 0x0, sizeof(suffix));
		memset(uri, 0x0, sizeof(uri));
		memset(domain, 0x0, sizeof(domain));
		i++;
next:
		url = tmp + 1;
		continue;
	}
	
	if (i > 0) {
		memset(&nlk, 0x0, sizeof(nlk));
		nlk.comm.key = NLK_HTTP_HOST_SHOP;
		nlk.comm.obj_nr = i;
		nlk.comm.obj_len = sizeof(struct nlk_url_data);
		nlk.comm.mid = NLK_URL_LOG_PARAM;
		nlk_data_send(NLK_MSG_URL_LOG, &nlk, sizeof(nlk), log, i * sizeof(struct nlk_url_data));
	}
	free(data);
	remove(URL_LOG_DOMAIN_FILE);
	return 0;
}

static void url_log_dns_cb(void *data)
{
	struct timeval tv;
	
	tv.tv_sec = 120;
	tv.tv_usec = 0;
	if (!schedule(tv, url_log_dns_cb, NULL)) {
		URL_LOG_DEBUG("schedule url log dns event error\n");
		return;
	}
	if (!devid)
		devid = get_devid();
	exec_cmd("igd_dns %s %s %u &", sdomain, IPC_PATH_MU, URL_LOG_MOD_GET_DNS);
}

static void url_log_dump_cb(void *data)
{
	struct timeval tv;

	tv.tv_sec = 60 *60;
	tv.tv_usec = 0;
	if (!schedule(tv, url_log_dump_cb, NULL)) {
		URL_LOG_DEBUG("schedule url log dump event error\n");
		return;
	}
	dump_url();
	return;
}

static int set_log_server(void)
{
	if (CC_CALL_RCONF_INFO(RCONF_FLAG_URLLOG, sdomain, sizeof(sdomain))) {
		URL_LOG_DEBUG("can not get url log server\n");
		return -1;
	}
	return 0;
}

static int url_log_init(void)
{
	set_log_param();
	set_log_server();
	url_log_dump_cb(NULL);
	url_log_dns_cb(NULL);
	return 0;
}

int urllog_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;

	switch (msgid) {
	case URL_LOG_MOD_INIT:
		ret = url_log_init();
		break;
	case URL_LOG_MOD_DUMP_URL:
		ret = dump_url();
		break;
	case URL_LOG_MOD_SET_URL:
		ret = set_log_param();
		break;
	case URL_LOG_MOD_GET_DNS:
		if (!data || len != sizeof(struct igd_dns))
			return -1;
		ret = set_server_dns_info((struct igd_dns *)data);
		break;
	case URL_LOG_MOD_SET_SERVER:
		ret = set_log_server();
		break;
	default:
		break;
	}
	
	return ret;
}
