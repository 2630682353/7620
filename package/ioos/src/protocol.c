#include"protocol.h"
#include "log.h"
#include "nlk_ipc.h"
#include "uci_fn.h"

#define PRO_SYS_SETTING   "setting"
#define PRO_NET_WAN_CONF "wan_conf"
#define PRO_NET_WAN_INFO   "wan_info"
#define PRO_NET_WAN_ACCOUNT   "wan_account"
#define PRO_NET_WIFI_AP   "wifi_ap"
#define PRO_NET_WIFI_AP_5G   "wifi_ap_5g"
#define PRO_NET_WIFI_LT "wifi_lt"
#define PRO_NET_WIFI_LT_5G "wifi_lt_5g"
#define PRO_NET_WIFI_VAP "wifi_vap"
#define PRO_NET_WIFI_VAP_HOST "vap_host"
#define PRO_NET_WIFI_ACL_BLACK "acl_black"
#define PRO_NET_WIFI_ACL_WHITE "acl_white"
#define PRO_NET_DHCPD   "dhcpd"
#define PRO_SYS_FIRMSTATUS   "firmstatus"
#define PRO_SYS_FIRMUP   "firmup"
#define PRO_SYS_LOCAL_FIRMUP   "local_firmup"
#define PRO_SYS_UPLOAD_STATUS "upload_status"
#define PRO_SYS_DEVICE_CHECK   "device_check"
#define PRO_SYS_DEVICEID   "deviceid"
#define PRO_NET_RULE_TABLE_SECURITY   "rule_table_security"
#define PRO_NET_URL_SAFE_REDIRECT   "url_safe_redirect"//used for url safe redirect param send
#define PRO_SYS_LOGIN   "login"
#define PRO_SYS_WAN_DETECT   "wan_detect"
#define PRO_NET_QOS   "qos"
#define PRO_NET_TESTSPEED   "testspeed"
#define PRO_NET_CHANNEL   "channel"
#define PRO_NET_CHANNEL_5G   "channel_5g"
#define PRO_NET_AP_LIST   "ap_list"
#define PRO_NET_AP_LIST_5G   "ap_list_5g"
#define PRO_NET_TXRATE    "txrate"
#define PRO_NET_TXRATE_5G   "txrate_5g"
#define PRO_NET_DDNS       "ddns"
#define PRO_NET_ADVERT    "advert"
#define PRO_RESET_FIRST_LOGIN    "check_first"
#define PRO_SYS_LOG_DUMP "sys_log"
#define PRO_SYS_RETIME "sys_retime"
#define PRO_SYS_TIMEZONE "timezone"
#define PRO_USB_LOGIN "usb_login"

#define PRO_SYS_MAIN  "main"
#define PRO_NET_HOST_APP  "host_app"
#define PRO_NET_HOST_MODE  "host_mode"
#define PRO_NET_HOST_LMT  "host_lmt"
#define PRO_NET_HOST_LS  "host_ls"
#define PRO_NET_HOST_STUDY_TIME  "host_study_time"
#define PRO_NET_HOST_KING  "host_king"
#define PRO_NET_HOST_BLACK  "host_black"
#define PRO_NET_APP_BLACK  "app_black"
#define PRO_NET_HOST_NICK  "host_nick"
#define PRO_NET_APP_LT  "app_lt"
#define PRO_NET_STUDY_URL  "study_url"
#define PRO_NET_HOST_STUDY_URL  "host_study_url"
#define PRO_NET_HOST_DEL_HISTORY  "host_del_history"
#define PRO_NET_HOST_DBG  "host_dbg"
#define PRO_SYS_RCONF  "rconf"
#define PRO_NET_HOST_URL  "host_url"
#define PRO_NET_STUDY_URL_VAR  "study_url_var"
#define PRO_NET_HOST_PUSH  "host_push"
#define PRO_NET_NEW_PUSH  "new_push"
#define PRO_NET_HOST_PIC  "host_pic"
#define PRO_NET_HOST_FILTER  "host_filter"
#define PRO_SYS_LED  "led"
#define PRO_NET_HOST_NAT  "host_nat"
#define PRO_SYS_LOGIN_ACCOUNT  "login_account"
#define PRO_NET_HOST_IF  "host_if"
#define PRO_SYS_CMDLINE  "sys_cmd"
#define PRO_SYS_APP_INFO  "sys_app_info"
#define PRO_NET_INTERCEPT_URL_BLACK  "intercept_url_black"
#define PRO_NET_HOST_LM   "host_lm"
#define PRO_NET_HOST_LT   "host_lt"
#define PRO_NET_APP_WRITE   "app_white"
#define PRO_NET_APP_QUEUE   "app_queue"
#define PRO_NET_VPN_INFO  "vpn_info"
#define PRO_SYS_MTD_PARAM "mtd_param"
#define PRO_SYS_UP_READY  "up_ready"
#define PRO_SYS_LANXUN  "lanxun"
#define PRO_NET_WECHAT_ALLOW  "wechat_allow"
#define PRO_NET_WECHAT_HOST  "wechat_host"
#define PRO_NET_WECHAT_CLEAN  "wechat_clean"
#define CGI_SYS_STORAGE 	"storage"
#define CGI_COMMAND_LOG		"command_log"
#define PRO_SYS_TELCOM			"telcom"
#define PRO_FIREWALL		"firewall"
#define PRO_NET_STATIC_DHCP	"static_dhcp"
#define PRO_NET_IPMAC_BIND	"ipmac_bind"
#define PRO_SYSINFO      "saas"
#define PRO_NET_LINK_STATUS "link_status"
#define PRO_NET_UPNPD_SWITCH "upnpd_switch"
#define PRO_NET_TUNNEL      "tunnel"
#define PRO_SYS_CONFIG      "sys_config"
#define PRO_CONFIG_STATUS      "config_status"
#define PRO_SYS_GUEST      "sys_guest"
#define PRO_NET_ROUTE "net_route"
#define PRO_SYS_FIRMINFO "firminfo"

extern int pro_net_ap_list_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_ap_list_5g_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wan_set_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wan_account_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_wan_account_active_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_rule_table_security_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wan_info_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_sys_setting_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wifi_ap_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wifi_ap_5g_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wifi_vap_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wifi_vap_host_hander(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wifi_acl_black_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wifi_acl_white_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wifi_lt_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int pro_net_wifi_lt_5g_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int pro_net_dhcpd_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_sys_firmstatus_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_sys_firmup_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_sys_local_firmup_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_sys_upload_status_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_sys_timezone_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int pro_sys_sdevice_check_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_sys_deviceid_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_url_safe_redirect_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_sys_login_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wan_detect_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_qos_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_testspeed_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_channel_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_channel_5g_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_txrate_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_txrate_5g_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_reset_first_login_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int cgi_net_ddns_handler(server_t* srv, connection_t *con, struct json_object*response);

extern int cgi_sys_main_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_app_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_mode_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_lmt_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_ls_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_study_time_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_king_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_black_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_app_black_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_nick_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_app_lt_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_study_url_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_study_url_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_del_history_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_dbg_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_sys_rconf_ver_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_url_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_study_url_var_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_push_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_new_push_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_pic_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_filter_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_sys_led_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_host_nat_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_sys_account_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_host_if_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_system_cmdline_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_sys_app_info_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_intercept_url_black_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_advert_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_abandon_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_app_queue_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_vpn_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_mtd_param_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_up_ready_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_sys_lanxun_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_sys_log_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int pro_net_wifi_vap_allowmin_hander(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wifi_wechat_clean_hander(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_wifi_wechat_host_hander(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_sys_retime_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int pro_usb_login_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_sys_storage(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_command_log(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_sys_telcom_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_firewall_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int pro_net_static_dhcp_hander(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_net_ipmac_bind_hander(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_saas_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_net_tunnel_hander(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_sys_local_config_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int pro_sys_config_status_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int cgi_sys_guest_account_handler(server_t* srv, connection_t *con, struct json_object*response);
extern int cgi_link_status_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_upnpd_switch_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_route_handler(server_t *srv, connection_t *con, struct json_object *response);
extern int cgi_sys_firminfo_handler(server_t *srv, connection_t *con, struct json_object *response);

static cgi_protocol_t pro_list[] ={
	{PRO_SYS_MAIN, cgi_sys_main_handler},
	{PRO_NET_HOST_APP, cgi_net_host_app_handler},
	{PRO_NET_HOST_MODE, cgi_net_host_mode_handler},
	{PRO_NET_HOST_LMT, cgi_net_host_lmt_handler},
	{PRO_NET_HOST_LS, cgi_net_host_ls_handler},
	{PRO_NET_HOST_STUDY_TIME, cgi_net_host_study_time_handler},
	{PRO_NET_HOST_KING, cgi_net_host_king_handler},
	{PRO_NET_HOST_BLACK, cgi_net_host_black_handler},
	{PRO_NET_APP_BLACK, cgi_net_app_black_handler},
	{PRO_NET_HOST_NICK, cgi_net_host_nick_handler},
	{PRO_NET_APP_LT, cgi_net_app_lt_handler},
	{PRO_NET_STUDY_URL, cgi_net_study_url_handler},
	{PRO_NET_HOST_STUDY_URL, cgi_net_host_study_url_handler},
	{PRO_NET_HOST_DEL_HISTORY, cgi_net_host_del_history_handler},
	{PRO_NET_HOST_DBG, cgi_net_host_dbg_handler},
	{PRO_SYS_RCONF, cgi_sys_rconf_ver_handler},
	{PRO_NET_HOST_URL, cgi_net_host_url_handler},
	{PRO_NET_STUDY_URL_VAR, cgi_net_study_url_var_handler},
	{PRO_NET_HOST_PUSH, cgi_net_host_push_handler},
	{PRO_NET_NEW_PUSH, cgi_net_new_push_handler},
	{PRO_NET_HOST_PIC, cgi_net_host_pic_handler},
	{PRO_NET_DDNS, cgi_net_ddns_handler},
	{PRO_NET_HOST_FILTER, cgi_net_host_filter_handler},
	{PRO_SYS_LED, cgi_sys_led_handler},
	{PRO_NET_HOST_NAT, cgi_host_nat_handler},
	{PRO_SYS_LOGIN_ACCOUNT, cgi_sys_account_handler},
	{PRO_NET_HOST_IF, cgi_net_host_if_handler},
	{PRO_SYS_CMDLINE, cgi_system_cmdline_handler},
	{PRO_SYS_APP_INFO, cgi_sys_app_info_handler},
	{PRO_NET_INTERCEPT_URL_BLACK, cgi_net_intercept_url_black_handler},
	{PRO_NET_ADVERT, cgi_net_advert_handler},
	{PRO_NET_HOST_LM, cgi_net_abandon_handler},
	{PRO_NET_HOST_LT, cgi_net_abandon_handler},
	{PRO_NET_APP_WRITE, cgi_net_abandon_handler},
	{PRO_NET_APP_QUEUE, cgi_net_app_queue_handler},
	{PRO_SYS_UP_READY, cgi_up_ready_handler},

	{PRO_NET_TXRATE, pro_net_txrate_handler},
	{PRO_NET_TXRATE_5G, pro_net_txrate_5g_handler},
	{PRO_NET_AP_LIST, pro_net_ap_list_handler},
	{PRO_NET_AP_LIST_5G, pro_net_ap_list_5g_handler},
	{PRO_NET_CHANNEL, pro_net_channel_handler},
	{PRO_NET_CHANNEL_5G, pro_net_channel_5g_handler},
	{PRO_SYS_WAN_DETECT, pro_net_wan_detect_handler},
	{PRO_NET_WAN_CONF, pro_net_wan_set_handler},
	{PRO_NET_WAN_INFO, pro_net_wan_info_handler},
	{PRO_NET_WAN_ACCOUNT, pro_net_wan_account_handler},
	{PRO_NET_WIFI_AP, pro_net_wifi_ap_handler},
	{PRO_NET_WIFI_AP_5G, pro_net_wifi_ap_5g_handler},
	{PRO_NET_WIFI_LT, pro_net_wifi_lt_handler},
	{PRO_NET_WIFI_LT_5G, pro_net_wifi_lt_5g_handler},
	{PRO_NET_WIFI_VAP, pro_net_wifi_vap_handler},
	{PRO_NET_WIFI_VAP_HOST, pro_net_wifi_vap_host_hander},
	{PRO_NET_WIFI_ACL_BLACK, pro_net_wifi_acl_black_handler},
	{PRO_NET_WIFI_ACL_WHITE, pro_net_wifi_acl_white_handler},
	{PRO_NET_DHCPD, pro_net_dhcpd_handler},
	{PRO_NET_QOS, pro_net_qos_handler},
	{PRO_NET_TESTSPEED, pro_net_testspeed_handler},
	{PRO_NET_URL_SAFE_REDIRECT, pro_net_url_safe_redirect_handler},
	{PRO_NET_RULE_TABLE_SECURITY, pro_net_rule_table_security_handler},
	{PRO_NET_VPN_INFO, cgi_net_vpn_handler},
	
	{PRO_SYS_DEVICEID, pro_sys_deviceid_handler},
	{PRO_SYS_FIRMSTATUS, pro_sys_firmstatus_handler},
	{PRO_SYS_FIRMUP, pro_sys_firmup_handler},
	{PRO_SYS_LOCAL_FIRMUP, pro_sys_local_firmup_handler},
	{PRO_SYS_UPLOAD_STATUS, pro_sys_upload_status_handler},
	{PRO_SYS_DEVICE_CHECK, pro_sys_sdevice_check_handler},
	{PRO_SYS_SETTING, pro_sys_setting_handler},
	{PRO_SYS_LOGIN, pro_sys_login_handler},
	{PRO_SYS_TIMEZONE,pro_sys_timezone_handler},
	{PRO_SYS_MTD_PARAM, cgi_mtd_param_handler},
	{PRO_RESET_FIRST_LOGIN, pro_reset_first_login_handler},
	{PRO_SYS_LOG_DUMP, cgi_sys_log_handler},
	{PRO_NET_WECHAT_ALLOW, pro_net_wifi_vap_allowmin_hander},
	{PRO_NET_WECHAT_CLEAN, pro_net_wifi_wechat_clean_hander},	
	{PRO_NET_WECHAT_HOST, pro_net_wifi_wechat_host_hander},
	{PRO_SYS_RETIME, pro_sys_retime_handler},
	{PRO_USB_LOGIN, pro_usb_login_handler},

	{PRO_SYS_LANXUN, cgi_sys_lanxun_handler},
	{CGI_SYS_STORAGE, cgi_sys_storage},
	{CGI_COMMAND_LOG, cgi_command_log},
	{PRO_SYS_TELCOM, cgi_sys_telcom_handler},
	{PRO_FIREWALL, cgi_firewall_handler},
	{PRO_NET_STATIC_DHCP, pro_net_static_dhcp_hander},
	{PRO_NET_IPMAC_BIND, pro_net_ipmac_bind_hander},
	{PRO_SYSINFO, pro_saas_handler},
	{PRO_NET_TUNNEL, cgi_net_tunnel_hander},
	{PRO_SYS_CONFIG, pro_sys_local_config_handler},
	{PRO_CONFIG_STATUS, pro_sys_config_status_handler},
	{PRO_CONFIG_STATUS, pro_sys_config_status_handler},
	{PRO_SYS_GUEST, cgi_sys_guest_account_handler},
	{PRO_NET_LINK_STATUS, cgi_link_status_handler},
	{PRO_NET_UPNPD_SWITCH, cgi_upnpd_switch_handler},
	{PRO_NET_ROUTE, cgi_route_handler},
	{PRO_SYS_FIRMINFO, cgi_sys_firminfo_handler},
	{NULL, NULL},
};

cgi_protocol_t* find_pro_handler(const char * pro_opt);

int cgi_protocol_handler(server_t*srv,connection_t *con,struct json_object* response);

static void update(connection_t * con,server_t *srv)
{
#if 1
	if (con->ip_from && strcmp(con->ip_from, "127.0.0.1")) {
		if (server_list_update(con->ip_from,srv) == 0) {
			con->login = 1;
		} else {
			if (get_usrid() == 0) {	
				server_list_add(con->ip_from,srv);
				con->login = 1;
			} else {
				con->login = 0;
			}
		}
	} else {
		con->login = 1;
	}
#endif
}

int cgi_protocol_handler(server_t*srv,connection_t *con,struct json_object* response)
{
	struct json_object* obj;
	cgi_protocol_t *cur_protocol;
	char* opt = con_value_get(con,"opt");
	char* fname = con_value_get(con,"fname");

	if(!opt)
		return PRO_BASE_ARG_ERR; 

	if(!fname || !fname[0])
		fname = "net";

	if (strcmp(fname, "net") 
		&& strcmp(fname, "system")
		&& strcmp(fname, "sys")) {
		return PRO_BASE_ARG_ERR;
	}

	cur_protocol = find_pro_handler(opt);
	if(!cur_protocol)
		return PRO_BASE_ARG_ERR;

	obj= json_object_new_string(opt);
	json_object_object_add(response, "opt",obj);
	obj= json_object_new_string(fname);
	json_object_object_add(response, "fname",obj);
	if (connection_is_set(con) == -1)
		return PRO_BASE_ARG_ERR; 

	if (connection_is_set(con)) {
		obj= json_object_new_string("set");
#ifdef ZXHN_ER300_SUPPORT
		if (strcmp(opt, "login"))
			uuci_set("qos_rule.status.cfg=1");
#endif
	}else{
		obj= json_object_new_string("get");
	}
	json_object_object_add(response, "function",obj);
	update(con,srv);
	DEBUG("go to hanle function\n");
	return cur_protocol->handler(srv,con,response);
}

cgi_protocol_t *find_pro_handler(const char *pro_opt)
{
    int i;
    if(pro_opt == NULL)
        return NULL;
    i = 0;
    while(1){
        if(pro_list[i].name == NULL)
            return NULL;
        if(strcmp(pro_list[i].name, pro_opt) == 0){
            return &pro_list[i];
        }
        i++;
    }
        return NULL;
}
