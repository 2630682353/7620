/* --------------------------------------------------
 * @brief: 
 *
 * @version: 1.0
 *
 * @date: 10/08/2015 09:28:27 AM
 *
 * --------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "joylink.h"
#include "joylink_packets.h"
#include "joylink_extern.h"
#include "joylink_json.h"
#include "joylink_extern_json.h"
#include "crc.h"
#include "igd_share.h"
#include "module.h"
#include "igd_interface.h"

extern void
joylink_dev_set_ver(short ver);

extern short
joylink_dev_get_ver();

int
joylink_get_wifi_attr(WIFICtrl_t *wi);

char * 
joylink_dev_get_client_list();

typedef struct __attr_{
    char name[128];
    E_JL_DEV_ATTR_GET_CB get;
    E_JL_DEV_ATTR_SET_CB set;
}Attr_t;

typedef struct _attr_manage_{
    Attr_t wlan24g;
    Attr_t subdev;
    Attr_t wlanspeed;
    Attr_t uuid;
    Attr_t feedid;
    Attr_t accesskey;
    Attr_t localkey;
    Attr_t server_st;
	Attr_t macaddr;	
	Attr_t server_info;	
	Attr_t version;	
}WIFIManage_t;

WIFIManage_t _g_am, *_g_pam = &_g_am;

WIFICtrl_t _g_wifi = {
#ifdef __LINUX_UB2__
    .online_state = 1,
    .downspeed = 500,
    .upspeed = 600,
    ._24g.on = 0,
    ._24g.ssid = "tesssid",
    ._24g.encryption = "mixed-psk",
    ._24g.key = "tessskey",
    ._5g.on = 2,
    ._5g.encryption = "mixed-psk",
    ._5g.ssid = "5gtesssid",
    ._5g.key = "5gtessskey",
#endif
};

WIFICtrl_t *_g_pwifi = &_g_wifi;

E_JLRetCode_t
joylink_dev_set_attr_example(WIFICtrl_t *wi, JLPInfo_t *jlp);

int 
joylink_util_cut_ip_port(const char *ipport, char *out_ip, int *out_port);

E_JLRetCode_t
joylink_dev_is_net_ok()
{
    /**
     *FIXME:must to do
     */
	/*
	int uid = 1;
	char macstr[18];
	struct iface_info info;
	int ret = 0;
	memset(&info, 0x0, sizeof(info));
	mu_msg(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &info, sizeof(info));
		
	if (info.link) {
		ret = E_RET_TRUE;
	} else {
		ret = E_RET_FAIL;
	}
	*/
	int ret = E_RET_TRUE;
    
    
    return ret;
}

E_JLRetCode_t
joylink_dev_set_connect_st(int st)
{
    /**
     *FIXME:must to do
     */
    char buff[64] = {0};
    int ret = -1;
    sprintf(buff, "{\"conn_status\":\"%d\"}", st);
    if(_g_pam->server_st.set){
        if(_g_pam->server_st.set(buff)){
            log_error("set error");
        }else{
            ret = 0;
        }
    }

    return ret;
}

E_JLRetCode_t
joylink_dev_set_attr_jlp(JLPInfo_t *jlp)
{
    if(NULL == jlp){
        return E_RET_ERROR;
    }
    /**
     *FIXME:must to do
     */
    int ret = E_RET_ERROR;
    char buff[256];

    bzero(buff, sizeof(buff));
    if(strlen(jlp->feedid)){
        sprintf(buff, "{\"feedid\":\"%s\"}", jlp->feedid);
        log_debug("--set buff:%s", buff);
        if(_g_pam->feedid.set){
            if(_g_pam->feedid.set(buff)){
                log_error("set error");
                goto RET;
            }
        }
    }
    char cmd[128] = {0};
    snprintf(cmd, 128, "jd_config.basic.feedid=%s", jlp->feedid);
    uuci_set(cmd);

    bzero(buff, sizeof(buff));
    if(strlen(jlp->accesskey)){
        sprintf(buff, "{\"accesskey\":\"%s\"}", jlp->accesskey);
        log_debug("--set buff:%s", buff);
        if(_g_pam->accesskey.set){
            if(_g_pam->accesskey.set(buff)){
                log_error("set error");
                goto RET;
            }
        }
    }
	
	memset(cmd, 0 ,128);
      snprintf(cmd, 128, "jd_config.basic.accesskey=%s", jlp->accesskey);
    uuci_set(cmd);

    bzero(buff, sizeof(buff));
    if(strlen(jlp->localkey)){
        sprintf(buff, "{\"localkey\":\"%s\"}", jlp->localkey);
        log_debug("--set buff:%s", buff);
        if(_g_pam->localkey.set){
            if(_g_pam->localkey.set(buff)){
                log_error("set error");
                goto RET;
            }
        }
    }

     memset(cmd, 0 ,128);
      snprintf(cmd, 128, "jd_config.basic.localkey=%s", jlp->localkey);
    uuci_set(cmd);

    bzero(buff, sizeof(buff));
    if(strlen(jlp->joylink_server)){
        sprintf(buff, "%s:%d", jlp->joylink_server, jlp->server_port);
        log_debug("--set buff:%s", buff);
        if(_g_pam->server_info.set){
            if(_g_pam->server_info.set(buff)){
                log_error("set error");
                goto RET;
            }
        }
    }

	printf("serverport:%d\n", jlp->server_port);
    memset(cmd, 0 ,128);
      snprintf(cmd, 128, "jd_config.basic.server_port=%d", jlp->server_port);
    uuci_set(cmd);

    bzero(buff, sizeof(buff));
    sprintf(buff, "{\"version\":%d}", jlp->version);
    log_debug("--set buff:%s", buff);
    if(_g_pam->version.set){
        if(_g_pam->version.set(buff)){
            log_error("set error");
            goto RET;
        }
    }
 memset(cmd, 0 ,128);
      snprintf(cmd, 128, "jd_config.basic.version=%d", jlp->version);
    uuci_set(cmd);
	
    ret = E_RET_OK;
RET:

    return ret;
}

E_JLRetCode_t
joylink_dev_get_jlp_info(JLPInfo_t *jlp)
{
    if(NULL == jlp){
        return -1;
    }
    /**
     *FIXME:must to do
     */
    int ret = -1;
    char buff[256];
	unsigned char mac[6];
   	read_mac(mac);
	snprintf(jlp->mac,128,"%02x:%02x:%02x:%02x:%02x:%02x", 
		mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	//snprintf(jlp->mac, 128, "94:8B:03:FE:05:3D");
	printf("mac is:%s\n",jlp->mac);
	int num;
	char **val;
	
	if(!uuci_get("jd_config.basic.accesskey", &val, &num)) {
		
		if (val[0]) {
			strcpy(jlp->accesskey, val[0]);
			}
		uuci_get_free(val, num);
	}
	
	if(!uuci_get("jd_config.basic.feedid", &val, &num)) {
		if (val[0])
			strncpy(jlp->feedid, val[0], strlen(val[0]));
		uuci_get_free(val, num);
	}
	if(!uuci_get("jd_config.basic.localkey", &val, &num)) {
	
		if (val[0])
			strncpy(jlp->localkey, val[0], strlen(val[0]));
		uuci_get_free(val, num);
	}

	if(!uuci_get("jd_config.basic.server_port", &val, &num)) {
		
		if (val[0])
			jlp->server_port = atoi(val[0]);
		uuci_get_free(val, num);
	}
	
	if(!uuci_get("jd_config.basic.version", &val, &num)) {
		
		if (val[0])
			jlp->version = (short)atoi(val[0]);
		uuci_get_free(val, num);
	}	

    if(_g_pam->accesskey.get){
        bzero(buff, sizeof(buff));
        if(!_g_pam->accesskey.get(buff, sizeof(buff))){
            log_debug("-->accesskey:%s\n", buff);
            jl_parse_jlp(jlp, buff);
        }else{
            log_error("get accesskey error");
        }
    }

    if(_g_pam->localkey.get){
        bzero(buff, sizeof(buff));
        if(!_g_pam->localkey.get(buff, sizeof(buff))){
            log_debug("-->localkey:%s\n", buff);
            jl_parse_jlp(jlp, buff);
        }else{
            log_error("get localkey error");
        }
    }

    if(_g_pam->feedid.get){
        bzero(buff, sizeof(buff));
        if(!_g_pam->feedid.get(buff, sizeof(buff))){
            log_debug("-->feedid:%s\n", buff);
            jl_parse_jlp(jlp, buff);
        }else{
            log_error("get feedid error");
        }
    }

    if(_g_pam->uuid.get){
        bzero(buff, sizeof(buff));
        if(!_g_pam->uuid.get(buff, sizeof(buff))){
            log_debug("-->uuid:%s\n", buff);
            jl_parse_jlp(jlp, buff);
        }else{
            log_error("get uuid error");
        }
    }

    if(_g_pam->macaddr.get){
        bzero(buff, sizeof(buff));
        if(!_g_pam->macaddr.get(buff, sizeof(buff))){
            log_info("-->mac:%s\n", buff);
            jl_parse_jlp(jlp, buff);
        }else{
            log_error("get mac error");
        }
    }

    if(_g_pam->server_info.get){
        bzero(buff, sizeof(buff));
        if(!_g_pam->server_info.get(buff, sizeof(buff))){
            log_info("-->server_info:%s\n", buff);
            joylink_util_cut_ip_port(buff, jlp->joylink_server, &jlp->server_port);
        }else{
            log_error("get mac error");
        }
    }

    if(_g_pam->version.get){
        bzero(buff, sizeof(buff));
        if(!_g_pam->version.get(buff, sizeof(buff))){
            log_info("-->version:%s\n", buff);
            jl_parse_jlp(jlp, buff);
        }else{
            log_error("get mac error");
        }
    }

    return ret;
}

E_JLRetCode_t
joylink_dev_set_attr_example(WIFICtrl_t *wi, JLPInfo_t *jlp)
{
    if(NULL == wi && NULL == jlp){
        return -1;
    }
    /**
     *FIXME:must to do
     */
    int ret = 0;
    char buff[256];
    char *w24gt ="{\"enabled\": \"%d\",\"ssid\":\"%s\",\"type\": \"%d\",\"passphrase\":\"%s\"}";
    int type;

    if(NULL != wi){
        int on_off = wi->_24g.on? 0:1;
        if(strlen(wi->_24g.ssid)){
            if(!strcmp("none", wi->_24g.encryption)){
                type = 0;
            }else{
                type = 1;
            }
            sprintf(buff, w24gt, on_off, wi->_24g.ssid, type, wi->_24g.key);
            log_info("24g.on:%d", wi->_24g.on);
            log_info("set buff:%s", buff);
            if(_g_pam->wlan24g.set){
                if(_g_pam->wlan24g.set(buff)){
                    log_error("set error");
                    ret = -1;
                }
            }
        }
    }

    if(NULL != jlp){
        bzero(buff, sizeof(buff));
        if(strlen(jlp->feedid)){
            sprintf(buff, "{\"feedid\":\"%s\"}", jlp->feedid);
            log_debug("--set buff:%s", buff);
            if(_g_pam->feedid.set){
                if(_g_pam->feedid.set(buff)){
                    log_error("set error");
                    ret = -1;
                }
            }
        }

        bzero(buff, sizeof(buff));
        if(strlen(jlp->accesskey)){
            sprintf(buff, "{\"accesskey\":\"%s\"}", jlp->accesskey);
            log_debug("--set buff:%s", buff);
            if(_g_pam->accesskey.set){
                if(_g_pam->accesskey.set(buff)){
                    log_error("set error");
                    ret = -1;
                }
            }
        }

        bzero(buff, sizeof(buff));
        if(strlen(jlp->localkey)){
            sprintf(buff, "{\"localkey\":\"%s\"}", jlp->localkey);
            log_debug("--set buff:%s", buff);
            if(_g_pam->localkey.set){
                if(_g_pam->localkey.set(buff)){
                    log_error("set error");
                    ret = -1;
                }
            }
        }
    }

    return ret;
}

int
joylink_dev_get_snap_shot(char *out_snap, int32_t out_max)
{
    if(NULL == out_snap || out_max < 0){
        return 0;
    }
    /**
     *FIXME:must to do
     */
    int len = 0;
    joylink_get_wifi_attr(_g_pwifi);
    char *p = joylink_dev_get_client_list();
    char *packet_data =  packageGWInfo(
            "you are ok!",
            0,
            _g_pwifi,
            p);

    /*if(p != NULL && packet_data != NULL){*/
        len = strlen(packet_data);
        printf("here is snap_shotddd********\n");
        char * tmp = "{\"code\":0,\"streams\":[{\"stream_id\":\"url\",\"current_value\":\"test1\"},"
		"{\"stream_id\":\"result\",\"current_value\":\"test2\"}]}";
        printf("------>%s:len:%d\n", tmp, strlen(tmp));
        if(len < out_max){
            memcpy(out_snap, tmp, strlen(tmp)); 
        }else{
            len = 0;
        }
    /*}*/

    if(NULL !=  packet_data){
        free(packet_data);
    }
    if(NULL != p){
        free(p);
    }

    return len;
}

int
joylink_dev_get_json_snap_shot(char *out_snap, int32_t out_max, int code, char *feedid)
{
    /**
     *FIXME:must to do
     */
    sprintf(out_snap, "{\"code\":%d, \"feedid\":\"%s\"}", code, feedid);

    return strlen(out_snap);
}

E_JLRetCode_t 
joylink_dev_lan_json_ctrl(const char *json_cmd)
{
    /**
     *FIXME:must to do
     */
    log_debug("json ctrl:%s", json_cmd);

    return E_RET_OK;
}

char * 
joylink_dev_script_ctrl(const char *recPainText, JLContrl_t *ctr, int from_server)
{
    if(NULL == recPainText || NULL == ctr){
        return -1;
    }
    /**
     *FIXME:must to do
     */
    int ret = -1;
    ctr->biz_code = (int)(*((int *)(recPainText + 4)));
    ctr->serial = (int)(*((int *)(recPainText +8)));
    char * out_str = NULL;

    WIFICtrl_t wctr;
    bzero(&wctr, sizeof(wctr));
    wctr._24g.on = -1;
    time_t tt = time(NULL);
    log_info("serial:%d:bcode:%d:server:%d:time:%ld", ctr->serial, ctr->biz_code, from_server,(long)tt);

    if(ctr->biz_code == JL_BZCODE_GET_SNAPSHOT){
        /*
         *Nothing to do!
         */
        ret = 0;
    }else if(ctr->biz_code == JL_BZCODE_CTRL){
	out_str = joylink_dev_parse_ctrl(recPainText + 12);
    	/*
        joylink_dev_parse_wifi_ctrl(&wctr, recPainText + 12);
        if(strlen(wctr._24g.encryption)){
            if(!strcmp(wctr._24g.encryption, "none")){
                strcpy(_g_pwifi->_24g.key, "");
            }else if(!strcmp(wctr._24g.encryption, "mixed-psk")){
                strcpy(_g_pwifi->_24g.key, wctr._24g.key);
            }

            strcpy(_g_pwifi->_24g.encryption, wctr._24g.encryption);
        }
        if(strlen(wctr._24g.ssid)){
            if(strcmp(_g_pwifi->_24g.ssid, wctr._24g.ssid)){
                strcpy(_g_pwifi->_24g.ssid, wctr._24g.ssid); 
            }
        }
        if(wctr._24g.on != -1 ){
            _g_pwifi->_24g.on = wctr._24g.on;
        }

        if(!joylink_dev_set_attr_example(_g_pwifi, NULL)){
            log_info("set atrr ok");
        }else{
            log_error("set attr error");
        }
        ret = 0;
    }else{
        log_error("unKown biz_code:%d", ctr->biz_code);
        */
    }
    
    return out_str;
}

E_JLRetCode_t
joylink_dev_ota(JLOtaOrder_t *otaOrder)
{
    if(NULL == otaOrder){
        return -1;
    }
    int ret = E_RET_OK;

    log_debug("serial:%d | feedid:%s | productuuid:%s | version:%d | versionname:%s | crc32:%d | url:%s\n",
     otaOrder->serial, otaOrder->feedid, otaOrder->productuuid, otaOrder->version, 
     otaOrder->versionname, otaOrder->crc32, otaOrder->url);

    char cmd_download[1024] = {0};
    char *folder = "/home/yn/workspace/";
    sprintf(cmd_download, "wget -b %s -P %s", otaOrder->url, folder);
    system(cmd_download);

    return ret;
}

void
joylink_dev_ota_status_upload()
{
    JLOtaUpload_t otaUpload;
    int i;
    strcpy(otaUpload.feedid, _g_pdev->jlp.feedid);
    strcpy(otaUpload.productuuid, _g_pdev->jlp.uuid);

    for(i = 0; i <= 100; i++){
        i += 30;
        if(i > 100){
            /**
             *FIXME:must to do
             *judge crc32 of the image
             */
            
            otaUpload.status = 1;
            otaUpload.progress = 100;
            strcpy(otaUpload.status_desc, "固件安装中");
        }else{
            otaUpload.status = 0;
            otaUpload.progress = i;
            strcpy(otaUpload.status_desc, "固件下载中");
        }      
        joylink_server_ota_status_upload_req(&otaUpload);
        sleep(2);
    }
    otaUpload.status = 2;
    otaUpload.progress = 0;
    strcpy(otaUpload.status_desc, "固件升级完成");
    joylink_server_ota_status_upload_req(&otaUpload);
}

int 
joylink_dev_register_attr_cb(
        const char *name,
        E_JL_DEV_ATTR_TYPE type,
        E_JL_DEV_ATTR_GET_CB attr_get_cb,
        E_JL_DEV_ATTR_SET_CB attr_set_cb)
{
    if(NULL == name){
        return -1;
    }
    int ret = -1;
    log_debug("regster %s", name);
    if(!strcmp(name, JL_ATTR_WLAN24G)){
       _g_pam->wlan24g.get = attr_get_cb; 
       _g_pam->wlan24g.set = attr_set_cb; 
       ret = 0;
    }else if(!strcmp(name, JL_ATTR_SUBDEVS)){
       _g_pam->subdev.get = attr_get_cb; 
       _g_pam->subdev.set = attr_set_cb; 
       ret = 0;
    }else if(!strcmp(name, JL_ATTR_WAN_SPEED)){
       _g_pam->wlanspeed.get = attr_get_cb; 
       _g_pam->wlanspeed.set = attr_set_cb; 
       ret = 0;
    }else if(!strcmp(name, JL_ATTR_UUID)){
       _g_pam->uuid.get = attr_get_cb; 
       _g_pam->uuid.set = attr_set_cb; 
       ret = 0;
    }else if(!strcmp(name, JL_ATTR_FEEDID)){
       _g_pam->feedid.get = attr_get_cb; 
       _g_pam->feedid.set = attr_set_cb; 
       ret = 0;
    }else if(!strcmp(name, JL_ATTR_ACCESSKEY)){
       _g_pam->accesskey.get = attr_get_cb; 
       _g_pam->accesskey.set = attr_set_cb; 
       ret = 0;
    }else if(!strcmp(name, JL_ATTR_LOCALKEY)){
       _g_pam->localkey.get = attr_get_cb; 
       _g_pam->localkey.set = attr_set_cb; 
       ret = 0;
    }else if(!strcmp(name, JL_ATTR_CONN_STATUS)){
       _g_pam->server_st.set = attr_set_cb; 
       ret = 0;
    }else if(!strcmp(name, JL_ATTR_MACADDR)){
       _g_pam->macaddr.get = attr_get_cb; 
       ret = 0;
    }else if(!strcmp(name, JL_ATTR_VERSION)){
       _g_pam->version.get = attr_get_cb; 
       _g_pam->version.set = attr_set_cb; 
       ret = 0;
    }

    log_info("regster %s:ret:%d", name, ret);
    return ret;
}



//=========== example ==================
int
joylink_get_wifi_attr(WIFICtrl_t *wi)
{
    if(NULL == wi){
        return -1;
    }
    int ret = -1;
    char buff[256];

    if(_g_pam->wlan24g.get){
        bzero(buff, sizeof(buff));
        if(!_g_pam->wlan24g.get(buff, sizeof(buff))){
            log_debug("-->wlan24g:%s\n", buff);
            jl_parse_24g(wi, buff);
        }else{
            log_error("get wlan24g error");
        }
    }else{
        log_error("nofoud wlan24g cb");
    }

    if(_g_pam->wlanspeed.get){
        bzero(buff, sizeof(buff));
        if(!_g_pam->wlanspeed.get(buff, sizeof(buff))){
            log_debug("get wlanspeed:%s", buff);
            jl_parse_wan_speed(wi, buff);
        }else{
            log_error("get wlanspeed error");
        }
    }else{
        log_error("nofoud wlan24g cb");
    }

    return ret;
}

char *
joylink_dev_get_client_list()
{
    char buff[1024];
    int count = 0;
    char *p = NULL;

    SubDev_t sdev[10];
    bzero(sdev, sizeof(sdev));

    if(_g_pam->subdev.get){
        bzero(buff, sizeof(buff));
        if(!_g_pam->subdev.get(buff, sizeof(buff))){
            log_debug("-->devs:%s\n", buff);
            count = joylink_parse_client_list(sdev, 10, buff);
            
            p = joylink_package_client_list_(sdev, count);
        }else{
            log_error("get subdev error");
        }
    }

    return p;
}

int
joylink_test_crc32()
{
    FILE* pFile;
    long lSize;
    char* buffer;
    size_t result;

    pFile = fopen("/home/yn/workspace/1448518667891_H5.zip" , "r");
    if (pFile == NULL){
        log_error("File error\n");
        return -1;;
    }

    fseek(pFile , 0 , SEEK_END);
    lSize = ftell(pFile);
    rewind(pFile);

    buffer = (char*) malloc(sizeof(char) * lSize);
    if (buffer == NULL){
        log_error("Memory error\n");
        return -1;
    }

    result = fread(buffer, 1, lSize, pFile);
    if (result != lSize){
        log_error("Reading error\n");
        return -1;
    }

    make_crc32_table();
    uint32_t crc = {0};
    crc = make_crc(crc, (unsigned char*)buffer, (uint32_t)lSize);

    log_debug("lSize:%ld\ncrc:%d\nsizeof_crc:%d\n", lSize, crc, sizeof(crc));
    
    fclose(pFile);
    free(buffer);
    return 0;
}
