#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <signal.h>
#include <pthread.h>

#include "module.h"
#include "nlk_ipc.h"
#include "ccapp.h"
#include "nlk_msg_c.h"
#include "igd_interface.h"
#include "igd_share.h"
#include "linux_list.h"
#include "ver.h"

#define LX_MAX_TIMER 					16
#define LX_MAX_RULE						64
#define LX_DNS_IP_MX					1

#define LX_HEAT_BEAT_SECS				(get_rand_with_pid(15 * 60, 25 * 60))


#define LX_DNS_PROBE_SECS				(get_rand_with_pid(10 * 60, 20 * 60))

#define LX_CHECK_LOG_SECS				(get_rand_with_pid(30, 90))

//防止dnsmasq配置恢复,定时检测配置
#define LX_CHECK_DNS_CONFIG_SECS		(30 * 60)
#define LX_CNAME_QUERY_SECS				(get_rand_with_pid(10 * 60, 20 * 60))

#define LX_RIGHT_NOW_SEC				0
#define LX_MAIN_SLEEP_TIME				1

#define LOG_REDIRECT_FILE	"/etc/app_conf/ccapp/log/ccapp_access.log"
#define LOG_DNS_PROBE_FILE	"/etc/app_conf/ccapp/log/ccapp_probe.log"
#define LOG_CDN_PROBE_FILE	"/etc/app_conf/ccapp/log/ccapp_ccprobe.log"
#define LOG_FORMAT_CONF_FILE	"/etc/app_conf/ccapp/nginx/logformat.ccapp_nginx.conf"
enum{
	LOG_TYPE_REDIRECT = 1,
	LOG_TYPE_DNS_PROBE = 2,
	LOG_TYPE_CDN_PROBE = 3,
};
enum{
	CDN_PROBE_TYPE_IP = 1,
	CDN_PROBE_TYPE_HTTP = 2,	
};

enum{
	DP_OK = 0,
	DP_OK_UP = 1,
	DP_FAILED = 2,	
};

#define MAX_LOG_FILE_SIZE	(32 * 1024)	
#define MAX_ERR_LOG_FILE_SIZE	(128 * 1024)

#define LANXUN_MIN(a,b)  ((a) > (b)?(b):(a))


#define LANXUN_DBG(fmt,args...) \
	printf("[%d]:"fmt, get_local_time(), ##args); 

#define LANXUN_ERR(fmt,args...) \
	printf("[%d]:"fmt, get_local_time(), ##args);  \
	igd_log("/tmp/tmp_log", "[%d]:"fmt, get_local_time(), ##args);


typedef struct{
	char *http_req;
	int con_id;
	int failed_times;
	uint64_t rep_len;
	uint16_t rep_code;
	struct in_addr dst_ip;
	struct timeval req_tv;
	long last_sysup_time;
	struct timeval last_utc;
	unsigned char mac[ETH_ALEN];
	struct list_head list;
}http_info_t;

struct list_head http_info_list;

typedef int (*func)(void *data, int len);
typedef struct {
	func cb; 
	int dst_utc;
	void *data;
	int len;
	char is_used;
}stimer_t;

static stimer_t timers[LX_MAX_TIMER];

char log_flag[32] = "cc_speedup";
char conf_ver[8] = "496";
char soft_ver[16] = "3.0.4.0";
static pthread_mutex_t log_file_mutex = PTHREAD_MUTEX_INITIALIZER;

static time_t get_local_time()
{
	time_t tt = 0;
	time(&tt);
	struct tm *now_tm = localtime(&tt);
	return mktime(now_tm);
}
int get_rand_with_pid(int min, int max)
{
	static unsigned int i = 1;
	srand(getpid() * time(NULL) * i++);
	return min + rand() % (max - min + 1);
}

static int get_free_index()
{
	int i;
	for (i = 0; i < LX_MAX_TIMER; i++) {
		if (!timers[i].is_used) {
			return i;
		}
	}
	return -1;
}

int simple_timer_add(int timeout_sec, func _cb, void *d, int dlen)
{
	LANXUN_DBG("timeout_sec=%d\n", timeout_sec);
	int i;
	if (!_cb) {
		return -1;
	}

	for (i = 0; i < LX_MAX_TIMER; i++) {
		if (timers[i].cb == _cb) {
			break;
		}
	}
	
	if (i == LX_MAX_TIMER) {
		i = get_free_index();
	}
	
	if (i >= 0) {
		timers[i].cb = _cb;
		timers[i].data = d;
		timers[i].len = dlen;
		timers[i].is_used = 1; 
		timers[i].dst_utc = time(NULL) + timeout_sec;
		return 0;
	}else {
		return -1;
	}
}
void simple_timer_sched()
{
	int i;
	time_t now = time(NULL);
	for (i = 0; i < LX_MAX_TIMER; i++) {
		if (timers[i].is_used && now >= timers[i].dst_utc) {
			timers[i].is_used = 0;
			if (timers[i].cb) {
				timers[i].cb(timers[i].data, timers[i].len);
			}
		}
	}
}

http_info_t *http_info_list_add_req(char *head, int con_id, 
		unsigned char mac[ETH_ALEN], int daddr)
{
	http_info_t *hi = malloc(sizeof(http_info_t));

	if (hi) {
		int len = strlen(head) + 1;
		memset(hi, 0, sizeof(http_info_t));
		hi->http_req = malloc(len);
		if (hi->http_req) {
			strncpy(hi->http_req, head, len);
			memcpy(hi->mac, mac, sizeof(hi->mac));
			hi->con_id = con_id;
			hi->dst_ip.s_addr = daddr;
			gettimeofday(&hi->req_tv, NULL);
			gettimeofday(&hi->last_utc, NULL);
			list_add_tail(&hi->list, &http_info_list);
			return hi;
		} else {
			free(hi);
			return NULL;
		}
	} else {
		return NULL;
	}
	
}
int http_info_list_del(http_info_t *hi)
{
	if (hi) {
		list_del(&hi->list);
		free(hi->http_req);
		free(hi);
		return 0;
	} else {
		return -1;
	}
}
http_info_t *find_http_info_by_id(int id)
{
	http_info_t *hi = NULL;
	list_for_each_entry(hi, &http_info_list, list) {
		if (hi->con_id == id) {
			return hi;
		}
	}
	return NULL;
}

int http_info_list_del_by_id(int id)
{
	return http_info_list_del(find_http_info_by_id(id));
}

static int ntp_ok()
{
	if (time(NULL) > 1466352000) {
		return 1;
	}else {
		return 0;
	}
}
static void reload_dnsmasq_config()
{
	system("killall dnsmasq");
}
static int lanxun_log_check(void *data, int len);

static int lanxun_dns_probe(void *data, int len)
{
	int ret = ccapp_dns_probe(data);
	
	switch (ret) {
		case DP_OK:
			break;
		case DP_OK_UP:
			reload_dnsmasq_config();
			simple_timer_add(LX_RIGHT_NOW_SEC, lanxun_log_check, NULL, 0);
			break;
		case DP_FAILED:
		//fall through	
		default:
			LANXUN_DBG("lanxun_dns_probe error, ret = %d\n", ret);
			break;
	}
	simple_timer_add(LX_DNS_PROBE_SECS, lanxun_dns_probe, NULL, 0);
	return 0;
}

static int reload_log_format_file()
{
	init_log_head();
	return 0;
}

char *read_file_to_malloc_buf(char *filename)
{
    FILE *fp;
    fp = fopen(filename, "r");
	if (fp) {
	    fseek( fp , 0 , SEEK_END );
	    int file_size = ftell( fp );
	    fseek( fp , 0 , SEEK_SET);
	    char *tmp =  (char *)calloc(1, file_size);
		if (tmp) {
	    	fread( tmp, file_size, 1, fp);
			fclose(fp);
			return tmp;
		}
		fclose(fp);
	} 
	return NULL;
}
void init_log_head()
{
	char *tmp = NULL;
	char *chs = "'\" ";
	int index = 0; 
	char *conf = read_file_to_malloc_buf(LOG_FORMAT_CONF_FILE);
	if (conf) {
		tmp = strtok(conf, chs);
		tmp = strtok(NULL, chs);
		tmp = strtok(NULL, chs);
		strncpy(log_flag, tmp, sizeof(log_flag));
		tmp = strtok(NULL, chs);
		strncpy(conf_ver, tmp, sizeof(conf_ver));
		tmp = strtok(NULL, chs);
		strncpy(soft_ver, tmp, sizeof(soft_ver));
		free(conf);
	}
}
static int lanxun_cname_query(void *data, int len);
void lanxun_http_register();
static int lanxun_heat_beat(void *data, int len)
{
	static int cdn_probe_type = CDN_PROBE_TYPE_IP;
	int ret = ccapp_heart_beat();
	
	switch (ret) {
		case HB_OK:
			break;
		//引导域名配置文件和日志格式配置文件有更新
		case HB_OK_T_UP:
			simple_timer_add(LX_RIGHT_NOW_SEC, lanxun_cname_query, NULL, 0);
			reload_log_format_file();
			lanxun_http_register();
			break;
		//DNS 探测域名配置文件有更新
		case HB_OK_D_UP:
			simple_timer_add(LX_RIGHT_NOW_SEC, lanxun_dns_probe, NULL, 0);
			break;
		//都有更新
		case HB_OK_A_UP:
			reload_log_format_file();
			simple_timer_add(LX_RIGHT_NOW_SEC, lanxun_cname_query, NULL, 0);
			simple_timer_add(LX_RIGHT_NOW_SEC, lanxun_dns_probe, NULL, 0);
			lanxun_http_register();
			break;
		case HB_CON_ERR:
		//fall through
		default:
			LANXUN_DBG("lanxun_heat_beat error, ret = %d\n", ret);
			break;
	}
	
	cdn_probe_type = cdn_probe_type % 2 + 1;
	//ccapp_cdn_probe(NULL, cdn_probe_type);

	simple_timer_add(LX_HEAT_BEAT_SECS, lanxun_heat_beat, NULL, 0);
	return 0;
}

static int get_file_size(const char *path)  
{  
    int filesize = 0;      
    struct stat statbuff;  
    if(!stat(path, &statbuff)) {  
        filesize = statbuff.st_size;
    } 
    return filesize;  
}

static int lanxun_log_check(void *data, int len)
{
	//check
	int fsize = 0;
	if (pthread_mutex_trylock(&log_file_mutex) == 0) {
		fsize = get_file_size(LOG_REDIRECT_FILE);
		if (fsize >= MAX_LOG_FILE_SIZE) {
			if (!ccapp_upload_log(LOG_REDIRECT_FILE, LOG_TYPE_REDIRECT)) {
				LANXUN_DBG("upload %s succ[%d], remove it\n", LOG_REDIRECT_FILE, fsize);
				remove(LOG_REDIRECT_FILE);
			} else {
				LANXUN_DBG("upload %s failed[%d]!\n", LOG_REDIRECT_FILE, fsize);
				if (fsize > MAX_ERR_LOG_FILE_SIZE){
					LANXUN_ERR("log too big and can`t upload, remove it.\n");
					remove(LOG_REDIRECT_FILE);
				}
			}
		}
		pthread_mutex_unlock(&log_file_mutex);
	} else {
		LANXUN_DBG("lanxun_log_check:pthread_mutex_trylock failed.\n");
	}
	fsize = get_file_size(LOG_DNS_PROBE_FILE);
	if (get_file_size(LOG_DNS_PROBE_FILE) >= MAX_LOG_FILE_SIZE) {
		if (!ccapp_upload_log(LOG_DNS_PROBE_FILE, LOG_TYPE_DNS_PROBE)) {
			LANXUN_DBG("upload %s succ[%d], remove it\n", LOG_DNS_PROBE_FILE, fsize);
			remove(LOG_DNS_PROBE_FILE);
		} else {
			LANXUN_DBG("upload %s failed[%d]!\n", LOG_DNS_PROBE_FILE, fsize);
			if (fsize > MAX_ERR_LOG_FILE_SIZE){
				LANXUN_DBG("log too big and can`t upload, remove it.\n");
				remove(LOG_DNS_PROBE_FILE);
			}
		}
	}
	fsize = get_file_size(LOG_CDN_PROBE_FILE);
	if (get_file_size(LOG_CDN_PROBE_FILE) >= MAX_LOG_FILE_SIZE) {
		if (!ccapp_upload_log(LOG_CDN_PROBE_FILE, LOG_TYPE_CDN_PROBE)) {
			LANXUN_DBG("upload %s succ[%d], remove it\n", LOG_CDN_PROBE_FILE, fsize);
			remove(LOG_CDN_PROBE_FILE);
		} else {
			LANXUN_DBG("upload %s failed[%d]!\n", LOG_CDN_PROBE_FILE, fsize);
			if (fsize > MAX_ERR_LOG_FILE_SIZE){
				LANXUN_DBG("log too big and can`t upload, remove it.\n");
				remove(LOG_CDN_PROBE_FILE);
			}
		}
	}
	simple_timer_add(LX_CHECK_LOG_SECS, lanxun_log_check, NULL, 0);

	return 0;
}

static int lanxun_append_dnsmasq_config(void *data, int len)
{
	FILE *fp;
	char line[256];
	
	fp = fopen("/tmp/dnsmasq.conf", "r+");
	if (fp) {
		while (fgets(line, sizeof(line), fp)) {
			if (strstr(line, "conf-dir=/tmp/dnsmasq.d")) {
				fclose(fp);
				return 0;
			}
		}
		fseek(fp, 0, SEEK_END);
		fprintf(fp, "conf-dir=/tmp/dnsmasq.d\n");
		fclose(fp);
	}	
	reload_dnsmasq_config();
	simple_timer_add(LX_CHECK_DNS_CONFIG_SECS, lanxun_append_dnsmasq_config, NULL, 0);
	return 0;
}
int url_2_ip(struct in_addr *addr, const char *url)
{
	int err, i = 0;
	struct addrinfo hint;
	struct sockaddr_in *sinp;
	struct addrinfo *ailist = NULL, *aip = NULL;

	hint.ai_flags = AI_CANONNAME;
	hint.ai_family = 0;
	hint.ai_socktype = 0;
	hint.ai_protocol = 0;
	hint.ai_addrlen = 0;
	hint.ai_canonname = NULL;
	hint.ai_addr = NULL;
	hint.ai_next = NULL;

	if ((err = getaddrinfo(url, NULL, &hint, &ailist)) != 0)
		return -1;

	for (aip = ailist; aip != NULL && i < LX_DNS_IP_MX; aip = aip->ai_next) {
		if (aip->ai_family == AF_INET && aip->ai_socktype == SOCK_STREAM) {
			sinp = (struct sockaddr_in *)aip->ai_addr;
			addr[i++] = sinp->sin_addr;
		}
	}
	
	freeaddrinfo(ailist);
	return i;
}
static int lanxun_cname_query(void *data, int len)
{
	FILE *fp, *cfg_fp;
	char line[256] = {0}, *ptr, *raw, *cname;
	struct in_addr cname_addr;
	fp = fopen("/etc/app_conf/ccapp/conf/ccapp_domain_cfg.conf", "r");
	cfg_fp = fopen("/tmp/dnsmasq.d/cname_query_result.conf", "w");
	if (fp && cfg_fp) {
		while (fgets(line, sizeof(line), fp)) {
			raw = line;
			if (ptr = strchr(line, '#')) {
				cname = ptr + 1;
				*ptr = '\0';
				if (ptr = strchr(ptr + 1, '#')) {
					*ptr = '\0';
					if (url_2_ip(&cname_addr, cname) > 0) {
						//记录原始域名与cname对应ip的关系
						fprintf(cfg_fp, "address=/%s/%s\n", raw, 
							inet_ntoa(cname_addr));
					}
				}
			}
		}
		fclose(fp);
		fclose(cfg_fp);

		reload_dnsmasq_config();
	}
	simple_timer_add(LX_CNAME_QUERY_SECS, lanxun_cname_query, NULL, 0);
	return 0;
}

int lanxun_http_unregister()
{
	return unregister_http_capture(1);
}
void lanxun_http_register()
{
	int cnt = 0;
	int ret = 0;
	FILE *fp, *cfg_fp;
	char line[256] = {0}, *ptr, *cname, *raw;
	struct in_addr cname_addr;
	lanxun_http_unregister();

	cnt = 0;
	fp = fopen("/etc/app_conf/ccapp/conf/ccapp_domain_cfg.conf", "r");
	if (fp) {
		while (fgets(line, sizeof(line), fp)) {
			raw = line;
			if (ptr = strchr(line, '#')) {
				cname = ptr + 1;
				*ptr = '\0';
				if (ptr = strchr(ptr + 1, '#')) {
					*ptr = '\0';

					if (register_http_capture(1, 1, raw)){
						cnt++;
					}
				}
			}
		}
		fclose(fp);
	}
	LANXUN_DBG("register_http_capture:%d url has add\n", cnt);
}
void lanxun_nlk_init(struct nlk_msg_handler *nlh)
{
	int grp = 0;
	grp |= 1 << (NLKMSG_GRP_HTTP2 - 1);
	nlk_event_init(nlh, grp);
}

int get_vlaue_in_http_head(char *value, int size, char *key, char *head)
{
	char *begin_ptr, *end_ptr;
	begin_ptr = strstr(head, key);
	if (begin_ptr) {
		begin_ptr += strlen(key);
		while (*begin_ptr == ' ') {
			begin_ptr++;
		}
		end_ptr = strchr(begin_ptr, '\r');
		if (end_ptr) {
			int n = LANXUN_MIN(end_ptr - begin_ptr, size - 1);
			memcpy(value, begin_ptr, n);
			value[n] = '\0';
			return 0;
		}
	}
	return -1;
}

void mac_from_u8_to_str(uint8_t *mac, char *str_mac)
{
	sprintf(str_mac, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], 
		mac[3], mac[4], mac[5]);	
}
void lanxun_get_local_mac(char *str)
{
	int uid = 0;
	struct iface_info info;

	if (mu_msg(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &info, sizeof(info)))
		return -1;
	mac_from_u8_to_str(info.mac, str);
}
void get_first_line(char *line, int size, char *head)
{
	char *end_ptr = strchr(head, '\r');
	if (end_ptr) {
		int n = LANXUN_MIN(end_ptr - head, size - 1);
		memcpy(line, head, n);
		line[n] = '\0';
	}
}
void tv_sub(struct timeval *out,struct timeval *in)
{       
	if ((out->tv_usec -= in->tv_usec) < 0) {       
		--out->tv_sec;
	    out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

void log_http_req_to_file(http_info_t *hi)
{
	int i = 0;
	char *log[64] = {0};
	struct timeval tv;
	struct timezone tz;
	time_t t = time(NULL);
	char data[64] = "-";
	char msec[64] = "-";
	char content_type[512] = "-";
	char host[512] = "-";
	char req[2048] = "-";
	char user_agent[512] = "-";
	char cookie[512] = "-";
	char status[64] = "-";
	char rep_len[64] = "-";
	char request_time[64] = "-";
	char refer[512] = "-";
	char local_mac[64] = "-";
	char client_mac[64] = "-";
	char *log_buf = NULL;
	
	if (!hi || hi->rep_code < 100 || hi->rep_code > 600 || hi->rep_len < 0){
		return;
	}
	log[i++] = log_flag;
	log[i++] = conf_ver;
	log[i++] = soft_ver;
	
	strftime(data, sizeof(data), "%d/%b/%Y:%T %z", localtime(&t));
	log[i++] = data;
	
	snprintf(msec, sizeof(msec), "%d.%03d", hi->last_utc.tv_sec, hi->last_utc.tv_usec / 1000);
	log[i++] = msec;

	get_vlaue_in_http_head(content_type, sizeof(content_type), "Accept:", hi->http_req);
	log[i++] = content_type;

	get_vlaue_in_http_head(host, sizeof(host), "Host:", hi->http_req);
	log[i++] = host;

	get_first_line(req, sizeof(req), hi->http_req);
	log[i++] = req;

	get_vlaue_in_http_head(cookie, sizeof(cookie), "Cookie:", hi->http_req);
	log[i++] = cookie;
	
	snprintf(status, sizeof(status), "%d", hi->rep_code);
	log[i++] = status;   //status

	snprintf(rep_len, sizeof(rep_len), "%llu", hi->rep_len);
	log[i++] = rep_len;  //send len

	tv_sub(&hi->last_utc, &hi->req_tv);
	//add us to 1ms
	snprintf(request_time, sizeof(request_time), "%d.%03d", 
				hi->last_utc.tv_sec, (hi->last_utc.tv_usec / 1000) + 1);
	
	log[i++] = request_time;  //request_time
	
	log[i++] = "-";	 //upstream_response_time
	log[i++] = inet_ntoa(hi->dst_ip);	 //upstream_addr
	
	get_vlaue_in_http_head(refer, sizeof(refer), "Referer:", hi->http_req);
	log[i++] = refer;
	
	get_vlaue_in_http_head(user_agent, sizeof(user_agent), "User-Agent:", hi->http_req);
	log[i++] = user_agent;
	
	lanxun_get_local_mac(local_mac);
	log[i++] = local_mac;
	
	mac_from_u8_to_str(hi->mac, client_mac);
	log[i++] = client_mac;
	
	log[i++] = "";	//空
	log[i++] = "-";	//upstream_cache_status

	int n;
	int log_len = 0;
	
	for (n = 0; n < i; n++) {
		//3: 双引号加空格
		log_len += strlen(log[n]) + 3;
	}
	//结束符跟换行符
	log_len += 2;
	log_buf = calloc(1, log_len);

	if(!log_buf) {
		return;
	}
	
	for (n = 0; n < i; n++) {
		strcat(log_buf, "\"");
		strcat(log_buf, log[n]);	
		strcat(log_buf, "\" ");	
	}
	strcat(log_buf, "\n");
	if (pthread_mutex_trylock(&log_file_mutex) == 0) {
		FILE *log_fp = fopen(LOG_REDIRECT_FILE, "a");	
		fputs(log_buf, log_fp);
		fclose(log_fp);
		pthread_mutex_unlock(&log_file_mutex);
	} else {
		LANXUN_DBG("get log_file_mutex failed, give up this log.\n");	
	}
	//LANXUN_DBG("%s\n", log_buf);
	free(log_buf);
}

void log_end_handler(http_info_t *hi)
{
	if (hi) {
		log_http_req_to_file(hi);
		http_info_list_del(hi);
	}
}
#define TYPE_REQ_HEAD	1
#define TYPE_ACK_INFO	2
void  lanxun_nlk_select(struct nlk_msg_handler *nlh)
{
	fd_set fds;
	int max_fd;
	struct nlk_http_header *http;
	struct iphdr *iph;
	struct tcphdr *tcph;
	unsigned char *tmp;
	http_info_t *hi;
	char buf[8192] = {0};
	int rcv_len;

	while (1){
		FD_ZERO(&fds);
		IGD_FD_SET(nlh->sock_fd, &fds);
		struct timeval tv = {1, 0};
		if (select(nlh->sock_fd + 1, &fds, NULL, NULL, &tv) < 0) {
			if (errno == EINTR || errno == EAGAIN){
				continue;
			} else { 
				goto end;
			}
		} else {
			break;
		}
	}
	if (!FD_ISSET(nlh->sock_fd, &fds)) {
		goto end;
	}
	rcv_len = nlk_uk_recv(nlh, buf, sizeof(buf));
	if (rcv_len < 0) {
		goto end;
	}
	http = NLMSG_DATA(buf);
	iph = (void *)http->data;
	tcph = (void *)((char *)iph + iph->ihl *4);
	tmp = (void *)((char *)tcph + tcph->doff * 4);
	
	lanxun_http_info *lhi = http->data;
	
	struct conn_info ci;
	switch (http->mtype) {
		case TYPE_REQ_HEAD:				
			if (!find_http_info_by_id(http->id)) {
				http_info_list_add_req(tmp, http->id, http->mac, iph->daddr);
			}
			break;
		case TYPE_ACK_INFO:
			hi = find_http_info_by_id(http->id);
			if (hi) {
				hi->rep_code = lhi->rep_code;
				hi->rep_len = lhi->rep_len;
				gettimeofday(&hi->last_utc, NULL);
				log_end_handler(hi);
			}
			break;
		default:
			break;
	}
end:
	return;
}
void lanxun_doexit(int ev)
{
	LANXUN_DBG("lanxun stop, .\n");
	http_info_t *node, *next;
	int cnt = lanxun_http_unregister();
	LANXUN_DBG("lanxun_http_unregister:%d has del\n", cnt);
	list_for_each_entry_safe(node, next, &http_info_list, list) {
		http_info_list_del(node);
	}
	exit(ev);
}
static int http_info_list_check()
{
	http_info_t *node, *next;
	struct conn_info ci;
	long uptime = sys_uptime();

	list_for_each_entry_safe(node, next, &http_info_list, list) {
		if (dump_conn_info(node->con_id, &ci)) {
			if (node->failed_times >= 3) {
				http_info_list_del(node);
			} else {
				node->failed_times++;
			}
		} else { 
			if (ci.last && node->last_sysup_time != ci.last) {
				node->last_sysup_time = ci.last;
				gettimeofday(&node->last_utc, NULL);
			}
			if (uptime - ci.last >= 6) {
				node->rep_len = ci.http_req_len;
				node->rep_code = ci.http_rep_code;
				log_end_handler(node);
			}
		}
	}
}

void show_version()
{
	LANXUN_ERR("lanxun plugin for %s version:%s,build time:%s %s.\n", LANXUN_PLATFORM, 
			LANXUN_VER, __DATE__, __TIME__);
}
void *lanxun_timer_pthread(void *data)
{
	while (1) {
		simple_timer_sched();
		sleep(1);
	}
}
void init_lanxun_service()
{
	LANXUN_DBG("init_lanxun_service...\n");
	lanxun_append_dnsmasq_config(NULL, 0);
	lanxun_heat_beat(NULL, 0);
	lanxun_dns_probe(NULL, 0);
	lanxun_cname_query(NULL, 0);
	lanxun_log_check(NULL, 0);
	reload_log_format_file();
	lanxun_http_register();
	LANXUN_DBG("init_lanxun_service sucess.\n");
}
void start_lanxun_service_pthread()
{
	pthread_t tid;
	if (pthread_create(&tid, NULL, lanxun_timer_pthread, NULL)) {
		LANXUN_DBG("lanxun create timer pthread failed, exit.\n");
		exit(0);
	}
}
void main()
{
	signal(SIGTERM, lanxun_doexit);
	signal(SIGQUIT, lanxun_doexit);
	signal(SIGINT, lanxun_doexit);
	
	show_version();
	
	struct nlk_msg_handler nlh;
	lanxun_nlk_init(&nlh);
	
	INIT_LIST_HEAD(&http_info_list);
	LANXUN_DBG("check ntp...\n");
	while (1) {
		if (!ntp_ok()) {
			sleep(5);
			continue;
		} else {
			break;
		}
	}
	LANXUN_DBG("ntp ok.\n");
	init_lanxun_service();
	start_lanxun_service_pthread();
	
	while (1) {
		lanxun_nlk_select(&nlh);
		http_info_list_check();
	}
}
