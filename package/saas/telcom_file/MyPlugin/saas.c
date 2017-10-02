#include "saas_lib.h"
#include "ctSgw.h"
#include "linux_list.h"
#include "saas_cloud.h"

typedef struct  {
	struct list_head list;
	char *name;
	int min_mem;
	int max_mem;
	int now_used;
	int test_exist;
}saas_proc_config_limit_t;

typedef struct {
	int limit_cpu;
	int limit_mem;
	int limit_connect;
	int limit_wifinum;
	struct list_head limit_procs;
}saas_conf_t;

typedef struct {
	int devid;
	unsigned char mac[6];
	char mode[64];
	char ver[64];
	char data[64];
	char mac_str[64];
	int revision;
	char error_msg_head[256];
}saas_software_info_t;


#define SAAS_ERR_FILE "/tmp/saas_err"
#define UPLOAD_PERIOD	(30 * 60)
//#define UPLOAD_PERIOD	(15)

typedef struct {
	struct list_head list;
	char head[128];
	char data[256];
	int errtime;
}saas_error_msg_t;

typedef struct{
	char pname[128];
	int used;
}top_used_t;
static top_used_t top_mem[3];
static top_used_t top_cpu[3];

static saas_conf_t saas_cfg;
saas_system_info_t saas_system_info;
static saas_software_info_t saas_software_info;
struct list_head error_list;
void dump_lan_dev_infos(CtLanDevInfo *dev_infos, int n)
{
	
	for (;n > 0; n--) {
		SAAS_DBG("devName:%s\n", dev_infos[n - 1].devName);
		SAAS_DBG("devType:%d\n", dev_infos[n - 1].devType);
		//SAAS_DBG("macAddr:%s\n", dev_infos[n - 1].macAddr);
		SAAS_DBG("ipAddr:%s\n", dev_infos[n - 1].ipAddr);
		SAAS_DBG("connType:%d\n", dev_infos[n - 1].connType);
		SAAS_DBG("port:%d\n", dev_infos[n - 1].port);
		SAAS_DBG("brandName:%s\n", dev_infos[n - 1].brandName);
		SAAS_DBG("osName:%s\n", dev_infos[n- 1].osName);
		SAAS_DBG("onlineTime:%d\n", dev_infos[n - 1].onlineTime);
	}
}
static int get_total_connects()
{
	char tmp[512];
	int n = 0;
   
	FILE *pipereader = popen("cat /proc/sys/net/netfilter/nf_conntrack_count", "r");
	if (pipereader) {
		fscanf(pipereader, "%d", &n);
		pclose(pipereader);
	}
	return n;
}

static int get_network_info(network_stat_t *ns)
{
	CtLanDevInfo *dev_infos;
	int dev_counts = 0, i;
	int tmp_up_speed = 0, tmp_down_speed = 0;
	CtMacAddr devaddr;
	
	memset(ns, 0, sizeof(network_stat_t));

	if (ctSgw_lanGetDevInfo(&dev_infos, &dev_counts)) {
		SAAS_DBG("ctSgw_lanGetDevInfo failed.\n");
		return -1;
	} else {
		for (i = 0; i < dev_counts; i++) {
			memcpy(devaddr.macAddr, dev_infos[i].macAddr, sizeof(devaddr.macAddr));
			if (!ctSgw_lanGetDevBandwidth(&devaddr, &tmp_up_speed, &tmp_down_speed)) {
				ns->down_speed += tmp_down_speed;
				ns->up_speed += tmp_up_speed;
			}
		}
	}
	free(dev_infos);
	ctSgw_wanGetIfStats(&ns->up_bytes, &ns->down_bytes);
	if (ctSgw_wanGetPppoeAccount(saas_system_info.pppoe_account, 
		sizeof(saas_system_info.pppoe_account))) {
		saas_system_info.network_type = 1;
	} else {
		saas_system_info.network_type = 2;
	}
	return 0;
}

static int get_wifi_user_count()
{
	CtLanDevInfo *dev_infos;
	int dev_counts = 0, n = 0, i;

	if (ctSgw_lanGetDevInfo(&dev_infos, &dev_counts)) {
		SAAS_DBG("ctSgw_lanGetDevInfo failed.\n");
		return 0;
	} else {
		for (i = 0; i < dev_counts; i++) {
			if (dev_infos[i].connType == 1) {
				n++;
			}
		}
	}
	free(dev_infos);
	return n;
}

static int get_total_mem()
{
	char tmp[512];
	int total_used_mem = 0, total_free_mem = 0;
   
	FILE *pipereader = popen("top -b -n 1", "r");
	if (pipereader) {
		fgets(tmp, sizeof(tmp), pipereader);
		sscanf(tmp, "%*s%d%*s%*s%d", &total_used_mem,  &total_free_mem);
		pclose(pipereader);
	}
	
	return total_used_mem + total_free_mem;
}

static int get_uptime()
{
	struct sysinfo info;
	if (sysinfo(&info)) {
    	SAAS_DBG("failed to get sysinfo, errno:%u, reason:%s\n", 
				errno, strerror(errno));
    	return 0;
    }
	return info.uptime;
}

static void telcom_dev_init()
{
	int sockfd;
	saas_error_msg_t msg;
	
	if (ctSgw_init(&sockfd)) {
		SAAS_DBG("ctSgw_init failed.\n");
		exit(0);
	}
	cc_get_devid(&saas_software_info.devid);
	saas_system_info.total_mem_k = get_total_mem();
	
	if (ctSgw_sysGetMac(saas_software_info.mac)) {
		SAAS_DBG("ctSgw_sysGetMac failed.\n");
		exit(0);
	}
	strcpy(saas_software_info.mode, SAAS_MODE_VENDER);
	saas_software_info.revision = SAAS_SVN;
	strcpy(saas_software_info.ver, SAAS_VER);
	strcpy(saas_software_info.data, "2016-08-24 13:41");
	snprintf(saas_software_info.mac_str, sizeof(saas_software_info.mac_str), 
			"%02x:%02x:%02x:%02x:%02x:%02x", saas_software_info.mac[0], 
				saas_software_info.mac[1], saas_software_info.mac[2],
				saas_software_info.mac[3], saas_software_info.mac[4],
				saas_software_info.mac[5]);
	snprintf(saas_software_info.error_msg_head, sizeof(saas_software_info.error_msg_head),
				"%d\1%02x%02x%02x%02x%02x%02x\1%s\1%s\1%d\1%s\1%d\1%d", 
				saas_software_info.devid, saas_software_info.mac[0], 
				saas_software_info.mac[1], saas_software_info.mac[2],
				saas_software_info.mac[3], saas_software_info.mac[4],
				saas_software_info.mac[5], saas_software_info.mode,
				saas_software_info.ver, saas_software_info.revision, saas_software_info.data,
				saas_system_info.total_mem_k, get_uptime());
	
	SAAS_DBG("error_msg_head:%s\n", saas_software_info.error_msg_head);
}

static int saas_add_limit_proc(char *buf)
{
	char name[512] = {0,};
	int min = 0, max = 0;
	saas_proc_config_limit_t *proc;

	if (3 != sscanf(buf, "%512[^,],%d,%d\n", name, &min, &max)) {
		SAAS_DBG("sscanf fail, %s", buf);
		return -1;
	}
	list_for_each_entry(proc, &saas_cfg.limit_procs, list) {
		if (!strcmp(name, proc->name)) {
			SAAS_DBG("exist, %s", buf);
			return -1;
		}
	}
	proc = malloc(sizeof(saas_proc_config_limit_t));
	if (!proc) {
		SAAS_DBG("malloc fail, %s", buf);
		return -1;
	}
	memset(proc, 0, sizeof(*proc));
	proc->name = strdup(name);
	if (!proc->name) {
		free(proc);
		SAAS_DBG("strdup fail, %s", buf);
		return -1;
	}
	proc->min_mem = min;
	proc->max_mem = max;
	list_add(&proc->list, &saas_cfg.limit_procs);
	return 0;
}

static int saas_conf(char *file)
{
	FILE *fp = NULL;
	char buf[2048];

	memset(&saas_cfg, 0, sizeof(saas_cfg));
	INIT_LIST_HEAD(&saas_cfg.limit_procs);

	fp = fopen(file, "rb");
	if (!fp) {
		SAAS_DBG("fopen fail, %s, %s\n", file, strerror(errno));
		return -1;
	}

	while (1) {
		memset(buf, 0, sizeof(buf));
		if (!fgets(buf, sizeof(buf) - 1, fp))
			break;
		if (!memcmp(buf, "CPU:", 4)) {
			saas_cfg.limit_cpu = atoi(buf + 4);
		} else if (!memcmp(buf, "MEM:", 4)) {
			saas_cfg.limit_mem = atoi(buf + 4);
		} else if (!memcmp(buf, "PROC:", 5)) {
			saas_add_limit_proc(buf + 5);
		} else if (!memcmp(buf, "CONNECT:", 8)) {
			saas_cfg.limit_connect = atoi(buf + 8);
		} else if (!memcmp(buf, "WIFINUM:", 8)) {
			saas_cfg.limit_wifinum = atoi(buf + 8);
		}
	}
	fclose(fp);
	
	return 0;
}

/*
  PID  PPID USER     STAT   VSZ %MEM CPU %CPU COMMAND
16510 16509 admin    R     1072   0%   1  13% top
*/
static int get_min_index(top_used_t *top3)
{
	int min_index;
	if (top3[0].used > top3[1].used) {
		if (top3[1].used > top3[2].used) {
			min_index = 2;
		} else {
			min_index = 1;
		}
	} else {
		if (top3[0].used > top3[2].used) {
			min_index = 2;
		} else {
			min_index = 0;
		}
	}
	return min_index;
}
static void top_mem_cpu_handler(char *pname, int memused, int cpuused)
{
	int cpu_min_index = get_min_index(top_cpu);
	int mem_min_index = get_min_index(top_mem);
	
	if (cpuused > top_cpu[cpu_min_index].used) {
		strcpy(top_cpu[cpu_min_index].pname, pname);
		top_cpu[cpu_min_index].used = cpuused;
	}

	if (memused > top_mem[mem_min_index].used) {
		strcpy(top_mem[mem_min_index].pname, pname);
		top_mem[mem_min_index].used = memused;
	}
	
}

static int error_msg_add(char *head, char *data)
{
	saas_error_msg_t *msg = malloc(sizeof(saas_error_msg_t));
	if (!msg) {
		return -1;
	}
	memset(msg, 0, sizeof(saas_error_msg_t));
	if (head) {
		strcpy(msg->head, head);
	}
	if (data) {
		strcpy(msg->data, data);
	}
	msg->errtime = get_uptime();
	list_add(&msg->list, &error_list);
}
static void limit_proc_handler_init()
{
	saas_proc_config_limit_t *proc;
	list_for_each_entry(proc, &saas_cfg.limit_procs, list) {
		proc->test_exist = 0;
	}
}

static void limit_proc_handler_end()
{
	char head[128], msgdata[256];
	saas_proc_config_limit_t *proc;
	list_for_each_entry(proc, &saas_cfg.limit_procs, list) {
		if (proc->test_exist == 0) {
			snprintf(head, sizeof(head), "PROC:%s", proc->name);
			snprintf(msgdata, sizeof(msgdata), "%d,%d", -1, -1);
			error_msg_add(head, msgdata);
		}
	}
}

static void limit_proc_handler(char *pname, int pid, int mem_used)
{
	char head[128], msgdata[256];
	saas_proc_config_limit_t *proc;
	list_for_each_entry(proc, &saas_cfg.limit_procs, list) {
		if (!strcmp(proc->name, pname)) {
			if (mem_used > proc->max_mem || mem_used < proc->min_mem) {
				snprintf(head, sizeof(head), "PROC:%s", proc->name);
				snprintf(msgdata, sizeof(msgdata), "%d,%d", pid, mem_used);
				error_msg_add(head, msgdata);
			}
			proc->test_exist = 1;
		}
	}
}
static void top_mem_cpu_handler_init()
{
	memset(top_cpu, 0, sizeof(top_cpu));
	memset(top_mem, 0, sizeof(top_mem));
}
static void top_mem_cpu_handler_end()
{
	int i;
	for (i = 0; i < 3; i++) {
		if (!top_cpu[i].used) {
			strcpy(top_cpu[i].pname, "none");
		}
	}
	for (i = 0; i < 3; i++) {
		if (!top_mem[i].used) {
			strcpy(top_mem[i].pname, "none");
		}
	}
}


static void get_procs_data()
{	
	char tmp[512];
	char memused[64], cpuused_pecent[64], pname[256], last_name[256] = {0};
	char *realname = NULL;
	int pid, total_used_mem, total_free_mem, mu, cu;
   	
	FILE *pipereader = popen("top -b -n 1", "r");
	if (!pipereader) {
		return;
	}
	fgets(tmp, sizeof(tmp), pipereader);
	sscanf(tmp, "%*s%d%*s%*s%d", &saas_system_info.mem_used,  &saas_system_info.mem_free);
	fgets(tmp, sizeof(tmp), pipereader);
	fgets(tmp, sizeof(tmp), pipereader);
	fgets(tmp, sizeof(tmp), pipereader);
	limit_proc_handler_init();
	top_mem_cpu_handler_init();
	saas_system_info.cpu_percent = 0;
	saas_system_info.mem_percent = ((float)(saas_system_info.mem_used * 100) / saas_system_info.total_mem_k);
	while (fgets(tmp, sizeof(tmp), pipereader)) {
		sscanf(tmp, "%d%*s%*s%*s%s%*s%*s%s%s", &pid, 
			memused, cpuused_pecent, pname);
		if (strchr(memused, '<')) {
			sscanf(tmp, "%d%*s%*s%*s%*s%s%*s%*s%s%s", &pid, 
				memused, cpuused_pecent, pname);
		}
		
		
		if (!strchr(pname, '[')) {
			if (strchr(pname, '/')) {
				for (realname = pname + strlen(pname); realname >= pname; realname--) {
					if (*realname == '/') {
						realname++;
						break;
					}
				}
			} else {
				realname = pname;
			}
		} else {
			pname[strlen(pname) - 1] = '\0';
			realname = pname + 1;
		}

		if (strcmp(last_name, realname)) {
			strcpy(last_name, realname);
		} else {
			continue;
		}
		
		sscanf(memused, "%d", &mu);
		if (strchr(memused, 'm')) {
			mu *= 1000;
		}
		limit_proc_handler(realname, pid, mu);
		sscanf(cpuused_pecent, "%d", &cu);
		saas_system_info.cpu_percent += cu;
		top_mem_cpu_handler(realname, mu, cu);
	}
	limit_proc_handler_end();
	top_mem_cpu_handler_end();
	pclose(pipereader);
}
static void dump_system_info(saas_system_info_t *si)
{
	int i;
	SAAS_DBG("cpu_percent=%d\n", si->cpu_percent);
	SAAS_DBG("mem_percent=%d\n", si->mem_percent);
	SAAS_DBG("connect_counts=%d\n", si->connect_counts);
	SAAS_DBG("wifi_user_count=%d\n", si->wifi_user_count);
	SAAS_DBG("total_mem_k=%d\n", si->total_mem_k);
	SAAS_DBG("mem_used=%d\n", si->mem_used);
	SAAS_DBG("mem_free=%d\n", si->mem_free);
	SAAS_DBG("down_bytes=%d\n", si->ns.down_bytes);
	SAAS_DBG("up_bytes=%d\n", si->ns.up_bytes);
	SAAS_DBG("down_speed=%d\n", si->ns.down_speed);
	SAAS_DBG("up_speed=%d\n", si->ns.up_speed);
	SAAS_DBG("network_type=%d\n", si->network_type);
	SAAS_DBG("pppoe_account=%s\n", si->pppoe_account);

	for (i = 0; i < 3; i++) {
		SAAS_DBG("cpu used----");
		SAAS_DBG("%s[%d] ", top_cpu[i].pname, top_cpu[i].used);
	}
	SAAS_DBG("\n");
	for (i = 0; i < 3; i++) {
		SAAS_DBG("mem used----");
		SAAS_DBG("%s[%d] ", top_mem[i].pname, top_mem[i].used);
	}
	SAAS_DBG("\n");
	
}
static int system_check_timer_callback(void *data, int len)
{
	int i;
	char head[128], msgdata[256], json_buf[1024] = {0};
	
	saas_system_info.connect_counts = get_total_connects();
	saas_system_info.wifi_user_count = get_wifi_user_count();
	get_network_info(&saas_system_info.ns);
	get_procs_data();
	//gen_query_reply_data(json_buf, sizeof(json_buf));
	//SAAS_DBG("msg:%s\n", json_buf);
	
	//dump_system_info(&saas_system_info);
	
	if (saas_system_info.cpu_percent > saas_cfg.limit_cpu) {
		snprintf(head, sizeof(head), "CPU:%d", saas_system_info.cpu_percent);
		snprintf(msgdata, sizeof(msgdata), "%s:%d;%s:%d;%s:%d", 
				top_cpu[0].pname, top_cpu[0].used, top_cpu[1].pname, top_cpu[1].used, 
				top_cpu[2].pname, top_cpu[2].used);
		error_msg_add(head, msgdata);
	}
	if (saas_system_info.mem_percent > saas_cfg.limit_mem) {
		snprintf(head, sizeof(head), "MEM:%d", saas_system_info.mem_used);
		snprintf(msgdata, sizeof(msgdata), "%s:%d;%s:%d;%s:%d", 
				top_mem[0].pname, top_mem[0].used, top_mem[1].pname, top_mem[1].used, 
				top_mem[2].pname, top_mem[2].used);
		error_msg_add(head, msgdata);
	}
	if (saas_system_info.connect_counts > saas_cfg.limit_connect) {
		snprintf(head, sizeof(head), "CONNECT:%d", saas_system_info.connect_counts);
		error_msg_add(head, NULL);
	}
	if (saas_system_info.wifi_user_count > saas_cfg.limit_wifinum) {
		snprintf(head, sizeof(head), "WIFINUM:%d", saas_system_info.wifi_user_count);
		error_msg_add(head, NULL);
	}

	simple_timer_add(10, system_check_timer_callback, NULL, 0);
	return 0;
}
static int buf_apend_data(char *buf, int size, int *used, char *data)
{
	int dlen = strlen(data);
	if (dlen + *used > size -1) {
		return -1;
	}
	strcpy(buf + *used, data);
	*used += dlen;
	return 0;
}
static int buf_apend_kv_str(char *buf, int size, int *used, char *key, 
		char *value, int end)
{
	char tmp_key[256], tmp_value[256];
	snprintf(tmp_key, sizeof(tmp_key), "\"%s\": ", key);
	snprintf(tmp_value, sizeof(tmp_value), "\"%s\"", value);
	buf_apend_data(buf, size, used, tmp_key);
	buf_apend_data(buf, size, used, tmp_value);
	if (!end) {
		buf_apend_data(buf, size, used, ", ");
	}
}
static int buf_apend_kv_int(char *buf, int size, int *used, char *key, 
		int value, int end)
{
	char tmp_key[256], tmp_value[256];
	snprintf(tmp_key, sizeof(tmp_key), "\"%s\": ", key);
	snprintf(tmp_value, sizeof(tmp_value), "%d", value);
	buf_apend_data(buf, size, used, tmp_key);
	buf_apend_data(buf, size, used, tmp_value);
	if (!end) {
		buf_apend_data(buf, size, used, ", ");
	}
}
int get_dns(char *dns1, char *dns2)
{
	FILE *fp;
	if (!dns1 || !dns2) {
		return -1;
	}
	fp = fopen("/etc/resolv.conf", "r");
	if (!fp) {
		return -1;
	}
	fscanf(fp, "nameserver %s\nnameserver %s", dns1, dns2);
	fclose(fp);
    return 0;
}

/*
{
    "opt": "saas",
    "fname": "system",
    "function": "get",
    "freemem": 34201600,  //剩余内存  B
    "runtime": 268, //运行时间
    "mode": 1,   //上网方式  1.dhcp 上网， 2.pppoe拨号上网， 3.静态ip上网， 4.无线中继上网方式
    "DNS1": "192.168.9.1",   //dns地址
    "DNS2": "223.6.6.6",   //dns 地址
    "mac": "94:8B:03:FF:50:21",   //mac 地址 （lan口）
    "down_speed": 10710,  //下载速度  kb/s
    "up_speed": 630,  //上传速度 kb/s
    "down_bytes": 371922, //下载流量  kb
    "up_bytes": 21440,  //上传流量  kb
    "pppoe": "",   //上网账号
    "appnum": 1,   //应用数量
    "curver": "1.26.2", // 固件版本
    "cpuused": 24,   // cpu使用率
    "error": 0
}
*/
int gen_query_reply_data(char *buf, int len)
{
	int used = 0;
	char tmp[256];
	char dns1[64] = {0}, dns2[64] = {0};
	get_network_info(&saas_system_info.ns);

	buf_apend_data(buf, len, &used, "{");
	buf_apend_kv_str(buf, len, &used, "opt", "saas", 0);
	buf_apend_kv_str(buf, len, &used, "fname", "system", 0);
	buf_apend_kv_str(buf, len, &used, "function", "get", 0);
	
	buf_apend_kv_int(buf, len, &used, "freemem", saas_system_info.mem_free * 1024, 0);
	buf_apend_kv_int(buf, len, &used, "runtime", get_uptime(), 0);
	buf_apend_kv_int(buf, len, &used, "mode", saas_system_info.network_type, 0);

	get_dns(dns1, dns2);
	buf_apend_kv_str(buf, len, &used, "DNS1", dns1, 0);
	buf_apend_kv_str(buf, len, &used, "DNS2", dns2, 0);
	buf_apend_kv_str(buf, len, &used, "mac", saas_software_info.mac_str, 0);
	
	buf_apend_kv_int(buf, len, &used, "down_speed", saas_system_info.ns.down_speed, 0);
	buf_apend_kv_int(buf, len, &used, "up_speed", saas_system_info.ns.up_speed, 0);
	buf_apend_kv_int(buf, len, &used, "down_bytes", saas_system_info.ns.down_bytes, 0);
	buf_apend_kv_int(buf, len, &used, "up_bytes", saas_system_info.ns.up_bytes, 0);
	
	buf_apend_kv_str(buf, len, &used, "pppoe", saas_system_info.pppoe_account, 0);
	buf_apend_kv_int(buf, len, &used, "appnum", 0, 0);
	buf_apend_kv_str(buf, len, &used, "curver", saas_software_info.ver, 0);
	buf_apend_kv_int(buf, len, &used, "cpuused", saas_system_info.cpu_percent, 0);
	buf_apend_kv_int(buf, len, &used, "error", 0, 1);
	buf_apend_data(buf, len, &used, "}");
}
static int system_info_upload_timer_callback(void *data, int len)
{
	FILE *fp;
	saas_error_msg_t *msg ,*next;
	char buf[1024];
	int has_msg = 0;
	int uptime = get_uptime();

	fp = fopen(SAAS_ERR_FILE, "w");
	if (!fp) {
		SAAS_DBG("fopen fail, give up msgs\n");
		list_for_each_entry_safe(msg, next, &error_list, list) {
			list_del(&msg->list);
			free(msg);
		}	
	} else {
		list_for_each_entry_safe(msg, next, &error_list, list) {
			if (!has_msg) {
				fprintf(fp, "%s\1", saas_software_info.error_msg_head);
				SAAS_DBG("msg:%s\1", saas_software_info.error_msg_head);
				has_msg = 1;
			}
			fprintf(fp, "%s,%d;%s;\1", msg->head, uptime - msg->errtime, msg->data);
			SAAS_DBG("%s,%d;%s;\1", msg->head, uptime - msg->errtime, msg->data);
			list_del(&msg->list);
			free(msg);
		}		
		//give up last '\1'
		fseek(fp, -1L, SEEK_CUR);
		fprintf(fp, "\2\2\2\2\n");
		SAAS_DBG("\n");
		fclose(fp);
		if (has_msg) {
			system("cp /tmp/saas_err /tmp/saas_err_bak");
			if (saas_send(SAAS_ERR_FILE)) {
				SAAS_DBG("send err file fail\n");
			} else {
				SAAS_DBG("send err file sucess\n");
			}
		}
	}
	simple_timer_add(UPLOAD_PERIOD, system_info_upload_timer_callback, NULL, 0);
	return 0;
}

void doexit(int sig)
{
	saas_error_msg_t *msg ,*next;
	saas_proc_config_limit_t *node, *next_node;

	list_for_each_entry_safe(msg, next, &error_list, list) {
		list_del(&msg->list);
		free(msg);
	}	
	list_for_each_entry_safe(node, next_node, &saas_cfg.limit_procs, list) {
		list_del(&node->list);
		free(node->name);
		free(node);
	}	
	ctSgw_cleanup();
	exit(0);
}
void cc_sig(void)
{
	sigset_t sig;

	sigemptyset(&sig);
	sigaddset(&sig, SIGABRT);
	sigaddset(&sig, SIGPIPE);
	sigaddset(&sig, SIGQUIT);
	sigaddset(&sig, SIGUSR1);
	sigaddset(&sig, SIGUSR2);
	sigaddset(&sig, SIGHUP);
	sigprocmask(SIG_BLOCK, &sig, NULL);

	//signal(SIGBUS, doexit);
	signal(SIGINT, doexit);
	signal(SIGTERM, doexit);
	//signal(SIGFPE, doexit);
	//signal(SIGSEGV, doexit);
	signal(SIGKILL, doexit);
}

void reporter_init()
{
	cc_sig();
	if (saas_conf("saas.conf")) {
		SAAS_DBG("read config file error.\n");
	}
	INIT_LIST_HEAD(&error_list);
	telcom_dev_init();
	simple_timer_add(0, system_check_timer_callback, NULL, 0);
	simple_timer_add(UPLOAD_PERIOD, system_info_upload_timer_callback, NULL, 0);
}

int main(int argc, char *argv[])
{
	fd_set fds;
	int max_fd = 0;
	struct timeval tv;
	struct cloud_client_info cci;
	
	reporter_init();
	cc_sig();
	cc_init(&cci);
	cc_mesg(CSO_LINK, &cci);
	while (1) {
		max_fd = 0;
		FD_ZERO(&fds);
		FD_SET(cci.sock, &fds);
		max_fd = N_MAX(max_fd, cci.sock);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		if (select(max_fd + 1, &fds, NULL, NULL, &tv) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
		}
		
		if (FD_ISSET(cci.sock, &fds)) {
			cc_mesg(CSO_WAIT, &cci);
		}
	
		simple_timer_sched();
		cc_timeout(&cci);
	}
	return 0;
}


