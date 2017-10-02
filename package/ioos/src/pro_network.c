#include "net_wifi.h"



#if 1
int pro_net_wifi_ap2_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	char *ssid = NULL,*passwd=NULL,*mode=NULL;
	char *channel=NULL,*hidessid=NULL,*security=NULL;
	//char *wifimode = NULL,*wifisecurity = NULL;
	local_ap_info ap_info;
	local_ap_info *get_ap_info = NULL;
	struct json_object* obj;

	 if(connection_is_set(con))
	 {
	 	ssid = con_value_get(con,WIFI_SSID);
		passwd = con_value_get(con,WIFI_PASSWD);
		mode = con_value_get(con,WIFI_MODE);
		channel = con_value_get(con,WIFI_CHANNEL);
		hidessid = con_value_get(con,WIFI_HIDESSID);
		security = con_value_get(con,WIFI_SECURIRY);

		if(!check_ssid(ssid))
			return GET_PROVALUE_ERROR;
		
		if(!channel || atoi(channel) < 0 || atoi(channel) > 14)
			return GET_PROVALUE_ERROR;
			
		if(!hidessid || atoi(hidessid) > 1 || atoi(hidessid) < 0)
			return GET_PROVALUE_ERROR;
			
		if(!mode || atoi(mode) < 1 || atoi(mode) > 4)
			return GET_PROVALUE_ERROR;
		
		if(!security || atoi(security) < 0 || atoi(security) > 3)
			return GET_PROVALUE_ERROR;
		
		memset(&ap_info,0,sizeof(local_ap_info));

		if(security)
		{
			switch(atoi(security))
			{
				case 0:
					ap_info.security = DISABLE;
					break;
				case 1:
					ap_info.security = WPAPSK;
					break;
				case 2:
					ap_info.security = WPAPSK2;
					break;
				case 3:
					ap_info.security = WPA1PSKWPA2PSK;
					break;
				default:
					break;
			}
			
		}
		
		if(ap_info.security != DISABLE && !check_passwd(passwd))
			return GET_PROVALUE_ERROR;

		if(mode)
		{
			switch(atoi(mode))
			{
				case 1:
					ap_info.wifi_mode = PHY_11B;
					break;
				case 2:
					ap_info.wifi_mode = PHY_11G;
					break;
				case 3:
					ap_info.wifi_mode = PHY_11N;
					break;
				case 4:
					ap_info.wifi_mode = PHY_11BGN_MIXD;
					break;
				default:
					break;
			}
			
		}

		strcpy(ap_info.ssid,ssid);
		strcpy(ap_info.passwd,passwd);
		strcpy(ap_info.hide,hidessid);
		strcpy(ap_info.channel,channel);

		if(net_wifi_set_local_ap(&ap_info) != SUCCESS)
	 		return SET_WIFIAP_ERROR;

		server_set_wireless(srv);

	 }
	 get_ap_info = calloc(1,sizeof(local_ap_info));
	 if(NULL == get_ap_info)
	 	return GET_WIFIAP_ERROR;
	 
	 if(net_wifi_get_local_ap(get_ap_info) != SUCCESS){
	 	free(get_ap_info);
	 	return GET_WIFIAP_ERROR;
	 }

	if(get_ap_info->ssid[0] != '\0'){
		obj= json_object_new_string(get_ap_info->ssid);
		json_object_object_add(response, "ssid",obj);
	}
	if(get_ap_info->passwd[0] != '\0'){
		obj= json_object_new_string(get_ap_info->passwd);
		json_object_object_add(response, "passwd",obj);
	}
	if(get_ap_info->wifi_mode){
		obj= json_object_new_int(get_ap_info->wifi_mode);
		json_object_object_add(response, "mode",obj);
	}
	if(get_ap_info->channel[0] != '\0'){
		obj= json_object_new_string(get_ap_info->channel);
		json_object_object_add(response, "channel",obj);
	}
	if(get_ap_info->hide[0] != '\0'){
		obj= json_object_new_string(get_ap_info->hide);
		json_object_object_add(response, "hidessid",obj);
	}	
	if(get_ap_info->security){
		obj= json_object_new_int(get_ap_info->security);
		json_object_object_add(response, "security",obj);
	}	

	free(get_ap_info);

	return 0;
}
#endif

int pro_net_wifi_lan_ip_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	char *ip = NULL,*mask=NULL;
	struct json_object* obj;
	lan_ip_info lan_ip;
	lan_ip_info *get_lan_ip = NULL;
	
	if(connection_is_set(con))
	{
		ip = con_value_get(con,LAN_IP);
		mask = con_value_get(con,LAN_MASK);

		if(!check_Legalip(ip) || !check_Legalip(mask))
			return GET_PROVALUE_ERROR;

		memset(&lan_ip,0,sizeof(lan_ip_info));

		strcpy(lan_ip.ip,ip);
		strcpy(lan_ip.mask,mask);

		if(wifi_set_lan_ip_info(&lan_ip) != SUCCESS)
		return SET_LANIP_ERROR;
	
		server_set_lanflag(srv);
	}
	
	get_lan_ip = calloc(1,sizeof(lan_ip_info));
	if(NULL == get_lan_ip)
		return SET_LANIP_ERROR;
		
	if(wifi_get_lan_ip_info(get_lan_ip) != SUCCESS){
		free(get_lan_ip);
		return SET_LANIP_ERROR;
	}
	
		
	if(get_lan_ip->ip[0] != '\0'){
		obj= json_object_new_string(get_lan_ip->ip);
		json_object_object_add(response, "ip",obj);
	}
	if(get_lan_ip->mask[0] != '\0'){
		obj= json_object_new_string(get_lan_ip->mask);
		json_object_object_add(response, "mask",obj);
	}
	if(get_lan_ip->mac[0] != '\0'){
		obj= json_object_new_string(get_lan_ip->mac);
		json_object_object_add(response, "mac",obj);
	}

	free(get_lan_ip);
	
	return 0;
}



int pro_net_wifi_wan_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	char *OperationMode = NULL;
	char *ip = NULL,*mask = NULL,*gateway=NULL;
	char *dns1=NULL,*dns2=NULL,*mode = NULL;
	struct json_object* obj;

	if(connection_is_set(con))
	{
		OperationMode = uci_getval(NETWORKPACKAGE,"wan","proto");
		if(OperationMode == NULL)
			return UCI_GETVAL_ERROR;

		mode = con_value_get(con,WAN_OPERATIONMODE);
		if(!mode)
			return GET_PROVALUE_ERROR;

		switch(atoi(mode))
		{
			case 0://static
				ip = con_value_get(con,WAN_IP);
				mask = con_value_get(con,WAN_MASK);
				gateway = con_value_get(con,WAN_GATEWAY);
				dns1 = con_value_get(con,WAN_DNS1);
				dns2 = con_value_get(con,WAN_DNS2);

				if(ip && check_Legalip(ip))
					uci_setval(NETWORKPACKAGE,"wan","ipaddr",ip);
				if(mask && check_Legalip(mask))
					uci_setval(NETWORKPACKAGE,"wan","mask",mask);
				if(gateway && check_Legalip(gateway))
					uci_setval(NETWORKPACKAGE,"wan","gateway",gateway);
				if(dns1 && check_Legalip(dns1))
					uci_setval(NETWORKPACKAGE,"wan","dns1",dns1);
				if(dns2 && check_Legalip(dns2))
					uci_setval(NETWORKPACKAGE,"wan","dns2",dns2);

				uci_setval(NETWORKPACKAGE,"wan","proto","static");

				break;
			case 1://dhcp
				uci_setval(NETWORKPACKAGE,"wan","proto","dhcp");
				break;
			//case 2://pppoe
			default:
				break;
				
		}

		do_cmd(RELOADNETWORK);
		
	}
	
	OperationMode = uci_getval(NETWORKPACKAGE,"wan","proto");
	if(OperationMode == NULL)
		return UCI_GETVAL_ERROR;
	
	if(strncmp(OperationMode,"dhcp",strlen(OperationMode)) == 0){
		obj= json_object_new_int(1);
		json_object_object_add(response, "mode",obj);
	}else if(strncmp(OperationMode,"static",strlen(OperationMode)) == 0){
		ip = uci_getval(NETWORKPACKAGE,"wan","ipaddr");
		mask = uci_getval(NETWORKPACKAGE,"wan","mask");
		gateway = uci_getval(NETWORKPACKAGE,"wan","gateway");
		dns1 = uci_getval(NETWORKPACKAGE,"wan","dns1");
		dns2 = uci_getval(NETWORKPACKAGE,"wan","dns2");
		
		obj= json_object_new_int(0);
		json_object_object_add(response, "mode",obj);

		if(ip){
			obj= json_object_new_string(ip);
			json_object_object_add(response, "ip",obj);
		}
		if(mask){
			obj= json_object_new_string(mask);
			json_object_object_add(response, "mask",obj);
		}
		if(gateway){
			obj= json_object_new_string(gateway);
			json_object_object_add(response, "gateway",obj);
		}
		if(dns1){
			obj= json_object_new_string(dns1);
			json_object_object_add(response, "dns1",obj);
		}
		if(dns2){
			obj= json_object_new_string(dns2);
			json_object_object_add(response, "dns2",obj);
		}	

	}
	return 0;
}




int pro_net_wifi_dhcpd_handler(server_t* srv, connection_t *con, struct json_object*response)
{
	struct json_object* obj;
	char *start = NULL,*end = NULL,*leasetime =NULL;
	dhcp_info info;
	dhcp_info *get_info;
 
	if(connection_is_set(con))
	{
		start = con_value_get(con,WAN_IP);
		end = con_value_get(con,WAN_IP);
		leasetime = con_value_get(con,WAN_IP);

		if(!check_rangeip(start))
			return GET_PROVALUE_ERROR;
		if(!check_rangeip(end) || atoi(start) > atoi(end))
			return GET_PROVALUE_ERROR;
		if(!leasetime || atoi(leasetime) <=0 || atoi(leasetime) > 12)
			return GET_PROVALUE_ERROR;
		
		memset(&info,0,sizeof(dhcp_info));
		strcpy(info.end,end);
		strcpy(info.start,start);
		strcpy(info.leasetime,leasetime);
			
		if(wifi_set_dhcp(&info) != SUCCESS)
			return SET_DHCP_ERROR;
		
	}
	
	get_info = calloc(1,sizeof(dhcp_info));
	if(NULL == get_info)
		return GET_DHCP_ERROR;
	
	if(wifi_get_dhcp(get_info) != SUCCESS){
		free(get_info);
		return GET_DHCP_ERROR;
	}

	if(get_info->start[0] != '\0'){
		obj= json_object_new_string(get_info->start);
		json_object_object_add(response, "start",obj);
	}
	if(get_info->end[0] != '\0'){
		obj= json_object_new_string(get_info->end);
		json_object_object_add(response, "end",obj);
	}
	if(get_info->leasetime[0] != '\0'){
		obj= json_object_new_string(get_info->leasetime);
		json_object_object_add(response, "leasetime",obj);
	}

	free(get_info);

	return 0;
}








