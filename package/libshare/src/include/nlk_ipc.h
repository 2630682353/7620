#ifndef _NLK_IPC_H_
#define _NLK_IPC_H_
#include "igd_share.h"
#include "nlk_msg_c.h"

struct api_l3 {
	unsigned short type;
	unsigned short gid; /* ip group id or dnsid */
	struct ip_range addr; /* cpu bit order */
	char dns[64];
};

struct api_l4 {
	unsigned short type;
	unsigned short protocol;
	struct port_range src;
	struct port_range dst;
};

struct http_rule_api {
	struct api_l3 l3;
	unsigned long flags;
	unsigned short mid;
	unsigned short period;
	unsigned short times;
	short prio;
	int type;
	union {
		struct url_rd rd;
		char data[512];
	};
};

struct dns_rule_api {
	struct api_l3 l3;
	short prio;
	short target; /*  target type, NF_TARGET_DENY... */
	int type; /* valid when target is NF_TARGET_REDIRECT */
	uint32_t ip;/* valid when target is NF_TARGET_REDIRECT */
	char dns[IGD_DNS_LEN];/* valid when target is NF_TARGET_REDIRECT */
};

struct net_rule_api {
	short prio;
	short target; /* target type, NF_TARGET_ACCEPT, NF_TARGET_DENY... */
	struct api_l3 l3;
	struct api_l4 l4;
};

struct rate_rule_api {
	int prio;
	unsigned short up;/* kB/s*/
	unsigned short down;/*kb/s*/
};

struct qos_rule_api {
	short prio;
	short queue;
};


extern int get_if_statistics(int type, struct if_statistics *stat);
extern unsigned int get_devid(void);
extern unsigned int get_usrid(void);
extern int set_usrid(unsigned int usrid);
extern int del_group(int mid, int id);
//extern int clean_group(int mid);
extern struct nlk_dump_rule *dump_group(int mid, int *nr);
extern int mod_user_grp_by_mac(int id, char *name, int nr, char (*mac)[ETH_ALEN]);
extern int mod_user_grp_by_ip(int id, char *name, int nr, struct ip_range *ip);
extern int mod_url_grp(int id, char *name, int nr, char (*url)[IGD_URL_LEN]);
extern int mod_dns_grp(int id, char *name, int nr, char (*dns)[IGD_DNS_LEN]);
extern int add_url_grp(char *name, int nr, char (*url)[IGD_URL_LEN]);
extern int add_url_grp2(char *name, int nr, struct inet_url *url);
extern int add_dns_grp(char *name, int nr, char (*dns)[IGD_DNS_LEN]);
extern int add_user_grp_by_mac(char *name, int nr, unsigned char (*mac)[ETH_ALEN]);
extern int add_user_grp_by_ip(char *name, int nr, struct ip_range *ip);
extern int mod_filter_rule(int mid, int id, int prio, struct nf_rule_info *info);
//extern int clean_filter_rule(int mid);
extern struct host_info *dump_host_alive(int *nr);
extern int dump_host_info(struct in_addr addr, struct host_info *info);
extern struct app_conn_info *dump_host_app(struct in_addr addr, int *nr);
extern struct conn_info *dump_host_conn(struct in_addr addr, int *nr);
extern int dump_conn_info(int id, struct conn_info *ci);
extern int register_app_filter(struct inet_host *host, struct app_filter_rule *app);
extern int unregister_app_filter(int id);
extern int register_app_rate_limit(struct inet_host *host,
	       	int mid, int id, struct rate_entry *rate);
extern int register_app_qos(struct inet_host *host, int queue, int nr, int *app);
extern int unregister_app_rate_limit(int id);
extern int unregister_app_qos(int id);
extern int get_link_status(unsigned long *bitmap);
extern int igd_safe_read(int fd, unsigned char *dst, int len);
extern int igd_safe_write(int fd, unsigned char *src, int len);
extern int dump_flash_uid(unsigned char *buf);
extern int read_mac(unsigned char *mac);
extern void console_printf(const char *fmt, ...);
extern int register_http_rule(struct inet_host *host, struct http_rule_api *arg);
extern int unregister_http_rule(int mid, int id);
extern void igd_log(char *file, const char *fmt, ...);
extern int igd_aes_dencrypt(const char *path, unsigned char *out, int len, unsigned char *pwd);
extern int igd_aes_dencrypt2(void *in, void *out, int len, void *pwd);
extern int igd_aes_encrypt(const char *path, unsigned char *out, int len, unsigned char *pwd);
extern int igd_aes_encrypt2(void *in, int ilen, void *out, int olen, void *pwd);
extern int set_dev_vlan(char *devname, uint16_t vlan_mask);
extern int set_dev_uid(char *devname, int uid);
extern int set_sysflag(unsigned char bit, unsigned char fg);
extern int igd_time_cmp(struct tm *t, struct time_comm *m);
extern long sys_uptime(void);
extern struct tm *get_tm(void);
extern int set_gpio_led(int action);
extern int set_wifi_led(int action);
extern int set_port_link(int pid, int action);
extern int set_port_speed(int pid, int speed);
extern char *read_firmware(char *key);
extern int set_host_info(struct in_addr ip, char *nick_name, char *name, unsigned char os_type, uint16_t vender);
extern struct http_host_log *dump_http_log(int *nr);
extern int set_host_url_switch(int act);
extern int mtd_set_val(char *val);
extern int mtd_get_val(char *name, char *val, int len);
extern int mtd_clear_data(void);
extern int set_wps_led(int action);
extern int register_http_rsp(int type, struct url_rd *rd);
extern int unregister_http_rsp(int type);
extern int shell_printf(char *cmd, char *dst, int dlen);
extern int get_sys_info(struct sys_info *info);
extern int set_led(int type, int id, int action);
extern int gpio_act(int gpio, int act, int level);
extern int led_act(int type, int act);
extern int gpio_init(int type, int id);
extern int write_gpio(int gpio, int value);
extern int read_gpio(int gpio, int *value);
extern int register_dns_rule(struct inet_host *host, struct dns_rule_api *arg);
extern int unregister_dns_rule(int id);
extern int register_http_capture(int mid, int nr, char (*host)[IGD_DNS_LEN]);
extern int unregister_http_capture(int mid);
extern int register_net_rule(struct inet_host *host, struct net_rule_api *arg);
extern int unregister_net_rule(int id);
extern int register_rate_rule(struct inet_host * host,struct rate_rule_api * arg);
extern int unregister_rate_rule(int id);
extern int register_qos_rule(struct inet_host * host,struct qos_rule_api * arg);
extern int unregister_qos_rule(int id);
extern int register_local_rule(struct inet_host *host, struct net_rule_api *arg);
extern int unregister_local_rule(int id);
extern int nlk_get_msgid(char *name);
#endif
