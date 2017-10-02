#include "net_wifi.h"

int check_ssid(char *ssid)
{
	int i = 0,len = 0;
	
	if(ssid == NULL)
		return 0;

	len = strlen(ssid);
	if(len <= 0 || len > 32)
		return 0;

	while(i < len-1)
	{
		if(!isalnum(ssid[i]) && ssid[i] != '_'  && ssid[i] != '-')
			return 0;

		i++;
	}

	return 1;
}

int check_passwd(char *passwd)
{
	int i = 0,len = 0;

	if(NULL == passwd)
		return 0;

	len = strlen(passwd);
	if(len <= 0 || len > 64)
		return 0;

	while(passwd[i] != '\0')
	{
		if(!isalnum(passwd[i]) && passwd[i] != '.' \
			&& passwd[i] != ',' && passwd[i] != '_' && passwd[i] != '-')
			return 0;

		i++;
	}


	return 1;
}

int check_rangeip(char *ipnum)
{
	int num = -1;
	int i = 0;
	
	if(ipnum == NULL)
		return 0;
	
	while(ipnum[i] != '\0')
	{
		if(!isdigit(ipnum[i]))
			return 0;
		i++;
	}

	num = atoi(ipnum);
	if(num > 255 || num < 0)
		return 0;

	return 1;
}

int check_Legalip(char *ip)
{
	char *first = NULL;
	char *second = NULL;
	char *three = NULL;
	char *four = NULL;
	char *tmp = NULL;
        char *tmp_ip = NULL;
        char *str_ip = NULL;

	if(NULL == ip)
		return 0;

        tmp_ip = strdup(ip);
        str_ip = tmp_ip;
        first = strsep(&tmp_ip,".");
        if(!first || !check_rangeip(first))
                goto error;

        second = strsep(&tmp_ip,".");
        if(!second || !check_rangeip(second))
                goto error;

        three = strsep(&tmp_ip,".");
        if(!three || !check_rangeip(three))
                goto error;

        four = strsep(&tmp_ip,".");
        if(!four || !check_rangeip(four))
                goto error;

        tmp = strsep(&tmp_ip,".");
        if(tmp)
                goto error;

        if(str_ip)
                free(str_ip);

        return 1;

error :
        if(str_ip)
                free(str_ip);

        return 0;
}


int net_wifi_set_local_ap(local_ap_info *ap_info)
{
	if(ap_info == NULL)
		return FAILURE;

	char *security = NULL,*wifimode = NULL;
	char cmd[128] = {0};

	if(!check_ssid(ap_info->ssid))
		printf("check ssid error!\n");
	else
	{
		sprintf(cmd,"wireless.@wifi-iface[0].ssid=%s",ap_info->ssid);
		uuci_set(cmd);
	}
	if(ap_info->channel[0] == '\0' || atoi(ap_info->channel) < 0 || atoi(ap_info->channel) > 14)
		printf("check channel error!\n");
	else{
		uci_setval(WIFIPACKAGE,"mt7620","channel",ap_info->channel);
	}
	if(ap_info->hide[0] = '\0' || atoi(ap_info->hide) < 0 || atoi(ap_info->hide) > 1)
		printf("check hiden error!\n");
	else{
		sprintf(cmd,"wireless.@wifi-iface[0].hidden=%s",ap_info->hide);
		uuci_set(cmd);
	}
	switch(ap_info->wifi_mode)
	{
		case PHY_11B:
			wifimode = "1";
			break;
		case PHY_11G:
			wifimode = "4";
			break;
		case PHY_11N:
			wifimode = "6";
			break;
		case PHY_11BGN_MIXD:
			wifimode = "9";
			break;
		default:
			break;
	}

	if(!wifimode)
		printf("check wifimode error!\n");
	else
		uci_setval(WIFIPACKAGE,"mt7620","wifimode",wifimode);
	
	switch(ap_info->security)
	{
		case WPAPSK:
			security = "psk+ccmp";
			break;
		case WPAPSK2:
			security = "psk2+ccmp";
			break;
		case WPA1PSKWPA2PSK:
			security = "mixed-psk+tkip";
			break;
		case DISABLE:
			security = "none";
			break;
		default:
			return FAILURE;
	}

	if(!security)
		printf("check security error!\n");
	else{
		sprintf(cmd,"wireless.@wifi-iface[0].encryption=%s",security);
		uuci_set(cmd);
	}
	if(security && ap_info->security == DISABLE)
	{
		sprintf(cmd,"wireless.@wifi-iface[0].key");
		uuci_delete(cmd);
	}
	else if(security){
		if(!check_passwd(ap_info->passwd))
			printf("check passwd error!\n");
		else{
			sprintf(cmd,"wireless.@wifi-iface[0].key=%s",ap_info->passwd);
			uuci_set(cmd);
	}
	}
	return SUCCESS;
	
}


int net_wifi_get_local_ap(local_ap_info *ap_info)
{
	char *ssid = NULL,*passwd=NULL,*mode=NULL;
	char *channel=NULL,*hidessid=NULL,*security=NULL;
	
	ssid = uci_getval_cmd("wireless.@wifi-iface[0].ssid");
	if(ssid){
		strcpy(ap_info->ssid,ssid);
		free(ssid);
	}
		
	passwd = uci_getval_cmd("wireless.@wifi-iface[0].key");
	if(passwd){
		strcpy(ap_info->passwd,passwd);
		free(passwd);
	}
	
	mode = uci_getval(WIFIPACKAGE,"mt7620","hwmode");
	if(mode){
		ap_info->wifi_mode = atoi(mode);
		free(mode);
	}
	channel = uci_getval(WIFIPACKAGE,"mt7620","channel");
	if(channel){
		strcpy(ap_info->channel,channel);
		free(channel);
	}
		
	hidessid = uci_getval_cmd("wireless.@wifi-iface[0].hidden");
	if(hidessid){
		strcpy(ap_info->hide,hidessid);
		free(hidessid);
	}
	
	security = uci_getval_cmd("wireless.@wifi-iface[0].encryption");
	if(security){
		if(!strncmp(security,"none",strlen(security)))
			ap_info->security = DISABLE;
		else if(!strncmp(security,"psk+ccmp",strlen(security)))
			ap_info->security = WPAPSK;
		else if(!strncmp(security,"psk2+ccmp",strlen(security)))
			ap_info->security = WPAPSK2;
		else if(!strncmp(security,"mixed-psk+tkip",strlen(security)))
			ap_info->security = WPA1PSKWPA2PSK;

		free(security);
	}

	return SUCCESS;
}



int wifi_set_lan_ip_info(lan_ip_info *lan_ip)
{
	if(lan_ip == NULL)
		return FAILURE;

	if(!check_Legalip(lan_ip->ip) || !check_Legalip(lan_ip->mask))
		return FAILURE;

	uci_setval(NETWORKPACKAGE,"lan","ipaddr",lan_ip->ip);
	uci_setval(NETWORKPACKAGE,"lan","mask",lan_ip->mask);
	

	return SUCCESS;
}


int wifi_get_lan_ip_info(lan_ip_info *lan_ip)
{
	char *ip = NULL,*mask=NULL,*mac = NULL;
	
	ip = uci_getval(NETWORKPACKAGE,"lan","ipaddr");
	if(ip)
	{
		strcpy(lan_ip->ip,ip);
		free(ip);
	}
	
	mask = uci_getval(NETWORKPACKAGE,"lan","netmask");
	if(mask)
	{
		strcpy(lan_ip->mask,mask);
		free(mask);
	}
	
	mac = uci_getval(NETWORKPACKAGE,"lan","bssid");
	if(mac)
	{
		strcpy(lan_ip->mac,mac);
		free(mac);
	}

	return SUCCESS;
}

int wifi_set_dhcp(dhcp_info *info)
{
	int limit = 0;
	char leasetime[4] = {0},limit_ip[4] = {0};
	
	if(!info)
		return FAILURE;

	if(!check_rangeip(info->start))
		return FAILURE;
	if(!check_rangeip(info->end) || atoi(info->start) >= atoi(info->end))
		return FAILURE;

	limit = atoi(info->start) - atoi(info->end);
	sprintf(limit_ip,"%d",limit);

	sprintf(leasetime,"%sh",info->leasetime);

	uci_setval(DHCPPACKAGE,"lan","start",info->start);
	uci_setval(DHCPPACKAGE,"lan","limit",limit_ip);
	uci_setval(DHCPPACKAGE,"lan","leasetime",leasetime);

	return SUCCESS;
}

int wifi_get_dhcp(dhcp_info *info)
{
	char *start = NULL,*end = NULL,*leasetime =NULL;
	int limit = 0,len = 0;
	char limit_ip[4] = {0};
	
	start = uci_getval(DHCPPACKAGE,"lan","start");
	if(start)
	{
		strcpy(info->start,start);
		free(start);
	}
	
	end = uci_getval(DHCPPACKAGE,"lan","limit");
	if(end)
	{
		if(start){
			limit = atoi(info->start) + atoi(info->end);
			sprintf(limit_ip,"%d",limit);
			strcpy(info->end,limit);
		}
		free(end);
	}
	
	leasetime = uci_getval(DHCPPACKAGE,"lan","leasetime");
	if(leasetime)
	{
		len = strlen(leasetime) -1;
		while(!isdigit(leasetime[len]))
			len--;
		
		leasetime[len] = '\0';

		if(strlen(leasetime) > 0)
			strcpy(info->leasetime,leasetime);
		
		free(leasetime);
	}

	return SUCCESS;
}




