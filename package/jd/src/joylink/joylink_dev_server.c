#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>

#include "joylink.h"
#include "joylink_utils.h"
#include "joylink_packets.h"
#include "joylink_crypt.h"
#include "joylink_json.h"
#include "joylink_extern.h"
#include "joylink_sub_dev.h"

#define JL_MAX_CUT_PACKET_LEN  (1024)
static void
joylink_proc_server_ctrl(uint8_t* recPainText);


static void 
joylink_server_st_time_out_check()
{
    log_debug("server st :%d", _g_pdev->server_st);
	if(_g_pdev->hb_lost_count > JL_MAX_SERVER_HB_LOST){
		if (_g_pdev->server_st == JL_SERVER_ST_WORK){
			log_info("Server HB Lost, Retry Auth!\r\n");
			_g_pdev->hb_lost_count = 0;
			_g_pdev->server_st = JL_SERVER_ST_AUTH;
		}

		if(_g_pdev->server_st == JL_SERVER_ST_AUTH){
			log_info("Auth ERR, Reconnect!\r\n");
			_g_pdev->hb_lost_count = 0;
			_g_pdev->server_st = JL_SERVER_ST_INIT;
			close(_g_pdev->server_socket);
			_g_pdev->server_socket = -1;
		}
	}
}

static void 
joylink_server_st_init()
{
    int ret = -1;
    struct sockaddr_in saServer; 
    bzero(&saServer, sizeof(saServer)); 

    saServer.sin_family = AF_INET;

    saServer.sin_port = htons(_g_pdev->jlp.server_port);

#ifdef _GET_HOST_BY_NAME_
    struct hostent *host;
    if((host=gethostbyname(_g_pdev->jlp.joylink_server)) == NULL) {
        perror("gethostbyname error");
        return ;
    }
    saServer.sin_addr = *((struct in_addr *)host->h_addr);
#else
   saServer.sin_addr.s_addr = inet_addr(_g_pdev->jlp.joylink_server);
#endif

    _g_pdev->server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_g_pdev->server_socket < 0 ){
        log_error("socket() failed!\n");
        return;
    }
    ret = connect(_g_pdev->server_socket, 
            (struct sockaddr *)&saServer,
            sizeof(saServer));

    if(ret < 0){
        log_error("connect() server failed!\n");
        close(_g_pdev->server_socket);
        _g_pdev->server_st = 0;
    }else{
        _g_pdev->server_st = 1;
    }
    _g_pdev->hb_lost_count = 0;
}

static void 
joylink_server_st_auth()
{
    int ret;
    int len = joylink_packet_server_auth_rsp();

    ret = send(_g_pdev->server_socket, _g_pdev->send_buff, len, 0);
    if(ret < 0){
        log_error("send error ret:%d", ret);
    }
    log_debug("Auth with server----->ret:%d\n", ret);
}

static void 
joylink_server_st_work()
{
    int ret;
    int len = 0; 
    if(E_JLDEV_TYPE_GW != _g_pdev->jlp.devtype){
        len = joylink_packet_server_hb_req();
    }else{
        len = joylink_packet_server_sub_hb_req();
    }    

    ret = send(_g_pdev->server_socket, _g_pdev->send_buff, len, 0);
    if(ret < 0){
        log_error("send error ret:%d", ret);
    }
    log_debug("HB----->");
    
 //   joylink_dev_parse_ctrl("{\"streams\":[{\"stream_id\":\"url\",\"current_value\":"
  //	"\"fname=system&opt=main&function=get&login=1\"}]}");
  /*
  char *temp = malloc(200);
 memset(temp, 0 ,200);
  time_t tt = time(NULL);
  *(int *)temp = (int)tt;
  *(int *)(temp+4) = JL_BZCODE_CTRL;
  *(int *)(temp+8) = 2;
   char * temp2 = "{\"streams\":[{\"stream_id\":\"url\",\"current_value\":\""
		"fname=system&opt=main&function=get\"}]}";
  memcpy(temp + 12, temp2, strlen(temp2));

 joylink_proc_server_ctrl(temp);
 free(temp);
*/
 
 //joylink_dev_parse_ctrl("{\"streams\":[{\"stream_id\":\"url\",\"current_value\":\""
//		"fname=system&opt=deviceid&function=get\"}]}");
    
    _g_pdev->hb_lost_count ++;
}

int
joylink_proc_server_st()
{
	int interval = 5000;
    if(!strlen(_g_pdev->jlp.feedid)){
        log_debug("feedid is empty");
        return interval;
    }

    joylink_server_st_time_out_check();

	switch (_g_pdev->server_st){
        case JL_SERVER_ST_INIT:
            joylink_server_st_init();
            interval = 2000;
            break;
        case JL_SERVER_ST_AUTH:
            joylink_server_st_auth();
            if (_g_pdev->hb_lost_count == 0){
                interval = 500;
            }
            interval = 500;
            _g_pdev->hb_lost_count++;
            break;
        case JL_SERVER_ST_WORK:
            joylink_server_st_work();
            interval = 15000;
            break;
        default:
            log_debug("Unknow server st:%d", _g_pdev->server_st);
            break;
    }

    joylink_dev_set_connect_st(_g_pdev->server_st);

	return interval;
}

int
joylink_server_upload_req()
{
    int ret = -1;
    int time_v;
    int len;
    char data[JL_MAX_PACKET_LEN] = {0};

    ret = joylink_dev_get_snap_shot(data + 4, JL_MAX_PACKET_LEN - 4);

    if(ret > 0){
        time_v = time(NULL);
        memcpy(data, &time_v, 4);

        len = joylink_encypt_server_rsp(
                _g_pdev->send_buff,
                JL_MAX_PACKET_LEN, PT_UPLOAD,
                _g_pdev->jlp.sessionKey, 
                (uint8_t*)&data,
                ret + 4);

        if(len > 0 && len < JL_MAX_PACKET_LEN){
            ret = send(_g_pdev->server_socket, _g_pdev->send_buff, len, 0);
            if(ret < 0){
                log_error("send error ret:%d", ret);
            }
            log_debug("send to server len:%d:ret:%d\n", len, ret);
        }else{
            log_error("send data too big or packe error:ret:%d", ret);
        }
    }

    return ret;
}

static int
joylink_server_subdev_upload_req()
{
    int ret = -1;
    int time_v;
    int len;
    char data[JL_MAX_PACKET_LEN] = {0};
    int i;
    JLDevInfo_t *alldevinfo = NULL;
    int count;
    int scan_type = 0;
    char *snap_shot = NULL;

    alldevinfo = joylink_dev_sub_devs_get(&count, scan_type);

    for(i = 0; i < count; i++){
        snap_shot = joylink_dev_sub_get_snap_shot(alldevinfo[i].jlp.feedid, &ret);
    
        if(snap_shot != NULL){
            ret = joylink_encrypt_sub_dev_data(
                    (uint8_t*)(data + 4 + 32), 
                    sizeof(data) - 4 -32,
                    ET_ACCESSKEYAES, 
                    (uint8_t*)alldevinfo[i].jlp.accesskey, 
                    (uint8_t*)snap_shot, 
                    ret);

            free(snap_shot);
            snap_shot = NULL;
        }
        if(ret > 0){
            time_v = time(NULL);
            memcpy(data, &time_v, 4);
            memcpy(data+4, alldevinfo[i].jlp.feedid, 32);

            len = joylink_encypt_server_rsp(_g_pdev->send_buff, JL_MAX_PACKET_LEN, 
                PT_SUB_UPLOAD, _g_pdev->jlp.sessionKey, 
                (uint8_t*)data, ret + 4 + 32) ;

            if(len > 0 && len < JL_MAX_PACKET_LEN){
                ret = send(_g_pdev->server_socket, _g_pdev->send_buff, len, 0);
                if(ret < 0){
                    log_error("send error ret:%d", ret);
                }
                log_debug("send to server len:%d:ret:%d\n", len, ret);
            }else{
                log_error("send data too big or packet error:ret:%d", ret);
            }
        }
    }

    if(NULL == alldevinfo){
        free(alldevinfo);
    }

    return ret;
}

static void
joylink_proc_server_auth(uint8_t* recPainText)
{
    JLAuthRsp_t* p = (JLAuthRsp_t*)recPainText;
    bzero(_g_pdev->jlp.sessionKey, 33); 
    memcpy(_g_pdev->jlp.sessionKey, p->session_key, 32);

    log_debug("OK===>Rand=%u,Time:%u!\r\n", 
            p->random_unm, p->timestamp);

    log_info("AUTH OK\n");

    if(E_JLDEV_TYPE_GW != _g_pdev->jlp.devtype){
        //joylink_server_upload_req();
    }
    _g_pdev->server_st = JL_SERVER_ST_WORK;
}

static void
joylink_proc_server_hb(uint8_t* recPainText)
{
    _g_pdev->server_st = JL_SERVER_ST_WORK;
    JLHearBeatRst_t* p = (JLHearBeatRst_t*)recPainText;
    _g_pdev->hb_lost_count = 0;
    log_info("HEAT BEAT OK===>Code=%u,Time:%u!\r\n",
            p->code, p->timestamp);
    
//	 joylink_dev_parse_ctrl("{\"streams\":[{\"stream_id\":\"url\",\"current_value\":\""
//		"fname=net&opt=wan_conf&function=get\"}]}");

    if(E_JLDEV_TYPE_GW != _g_pdev->jlp.devtype){
       // joylink_server_upload_req();
    }else{
        joylink_server_subdev_upload_req();
    }
}

void
joylink_send_big_pkg_server()
{
    int ret = -1;
    int len;
    int i;
    int offset = 0;

	JLPacketHead_t *pt = (JLPacketHead_t*)_g_pdev->send_p;
    short res = pt->crc;

    int total = (int)(pt->payloadlen/JL_MAX_CUT_PACKET_LEN) 
        + (pt->payloadlen%JL_MAX_CUT_PACKET_LEN? 1:0);

    for(i = 1; i <= total; i ++){
        if(i == total){
            len = pt->payloadlen%JL_MAX_CUT_PACKET_LEN;
        }else{
            len = JL_MAX_CUT_PACKET_LEN;
        }
        
        offset = 0;
        bzero(_g_pdev->send_buff, sizeof(_g_pdev->send_buff));

        memcpy(_g_pdev->send_buff, _g_pdev->send_p, sizeof(JLPacketHead_t) + pt->optlen);
        offset  = sizeof(JLPacketHead_t) + pt->optlen;

        memcpy(_g_pdev->send_buff + offset, 
                _g_pdev->send_p + offset + JL_MAX_CUT_PACKET_LEN * (i -1), len);

        pt = (JLPacketHead_t*)_g_pdev->send_buff; 
        pt->total = total;
        pt->index = i;
        pt->payloadlen = len;
        pt->crc = CRC16(_g_pdev->send_buff + sizeof(JLPacketHead_t), pt->optlen + pt->payloadlen);
        pt->reserved = (unsigned char)res;

        ret = send(_g_pdev->server_socket, 
                _g_pdev->send_buff,
                len + sizeof(JLPacketHead_t) + pt->optlen, 0);

        if(ret < 0){
            log_error("send error");
        }
        log_info("send to server:ret:%d", ret);
    }

    if(NULL != _g_pdev->send_p){
        free(_g_pdev->send_p);
        _g_pdev->send_p = NULL;
    }
}



static void
joylink_proc_server_ctrl(uint8_t* recPainText)
{
    int ret = -1;
    int len;
    char data[JL_MAX_PACKET_LEN] = {0};
    char *pout = NULL;
    int max_out = 0;
	char *data2 = NULL;	
    JLContrl_t ctr;
    bzero(&ctr, sizeof(ctr));

    log_debug("Control from server:%s:len:%d\n",
            recPainText + 12, strlen((char*)(recPainText + 12)));
    char * response = NULL;

    if((response = joylink_dev_script_ctrl((const char *)recPainText, &ctr, 2)) == NULL){
        return;
    }   
    printf("response is :%s\n", response);
    ret = joylink_packet_script_ctrl_rsp(&data2, &ctr, response);    
    free(response);
    	if(data2 != NULL){
		//printf("data2:%s  ret:%d\n", data2 + 12, ret);
		 _g_pdev->payload_total = ret; 

		if(_g_pdev->payload_total < JL_MAX_PAYLOAD_LEN){
			len = joylink_encypt_server_rsp(_g_pdev->send_buff, JL_MAX_PACKET_LEN, 
            				PT_SERVERCONTROL, _g_pdev->jlp.sessionKey, 
            				(uint8_t*)data2, ret);
			
	 	}else{
			_g_pdev->send_p = (char *)malloc(_g_pdev->payload_total + JL_MAX_AES_EXTERN + JL_MAX_PHEAD_LEN);
	 		if(_g_pdev->send_p == NULL)
	 			goto RET;
			pout = _g_pdev->send_p;
			max_out = _g_pdev->payload_total + JL_MAX_AES_EXTERN;
			len = joylink_encypt_server_rsp((uint8_t*)pout, max_out, 
            				PT_SERVERCONTROL, _g_pdev->jlp.sessionKey, 
            				(uint8_t*)data2, ret);	
			printf("big data encrypt  len:%d\n", len);
		}		
	
	 }


//    log_debug("rsp data:%s:len:%d",
//            (char *)(data2 + 12), 
//            strlen((char *)(data2 + 12)));

//    len = joylink_encypt_server_rsp(_g_pdev->send_buff, JL_MAX_PACKET_LEN, 
//           PT_SERVERCONTROL, _g_pdev->jlp.sessionKey, 
//            (uint8_t*)data2, ret);

    if(len > 0 &&  len < JL_MAX_PACKET_LEN){
        ret = send(_g_pdev->server_socket, _g_pdev->send_buff, len, 0);
        if(ret < 0){
            log_error("send error ret:%d", ret);
        }
        log_info("send to server len:%d:ret:%d\n", len, ret);
    }else{
	joylink_send_big_pkg_server();
    }
    
RET:
    if (NULL != data2) {
    	free(data2);
	data2 = NULL;
    }
}

static void
joylink_proc_server_upload(uint8_t* recPainText)
{
    JLDataUploadRsp_t* p = (JLDataUploadRsp_t *)recPainText;
    log_info("UPLOAD OK timestamp %d code %d\r\n", p->timestamp, p->code);
}

static void
joylink_proc_server_sub_hb(uint8_t* recPainText)
{
    JLDataSubHbRsp_t* p = (JLDataSubHbRsp_t *)recPainText;
    log_info("SUB_HB OK -->timestamp %d code %d\r\n", p->timestamp, p->code);

    _g_pdev->server_st = JL_SERVER_ST_WORK;
    _g_pdev->hb_lost_count = 0;

    if(E_JLDEV_TYPE_GW != _g_pdev->jlp.devtype){
        joylink_server_upload_req();
    }else{
        joylink_server_subdev_upload_req();
    }
}

static void
joylink_proc_server_sub_upload(uint8_t* recPainText)
{
    JLDataSubUploadRsp_t* p = (JLDataSubUploadRsp_t *)recPainText;
    log_info("SUB DEV UPLOAD OK timestamp %d feedid %s code %d\r\n", 
        p->timestamp, p->feedid, p->code);
}

static void
joylink_proc_server_sub_ctrl(uint8_t* recPainText, unsigned short payloadlen)
{
    int ret = -1;
    int len;
    char data[JL_MAX_PACKET_LEN] = {0};
    JLSubContrl_t ctr;
    bzero(&ctr, sizeof(ctr));
 
    if(-1 == joylink_subdev_script_ctrl(recPainText, &ctr, payloadlen)){
        return;
    }

    ret = joylink_packet_subdev_script_ctrl_rsp(data, sizeof(data), &ctr);

    int *org_s = (int*)(recPainText + 8);
    int *p_ser = (int*)(data + 8);
    log_debug("---org serial:%d:packet serial:%d, %d", *org_s, *p_ser,  ctr.serial);

    log_debug("rsp data:%s:len:%d",
            (char *)(data + 12), 
            strlen((char *)(data + 12)));

    len = joylink_encypt_server_rsp(_g_pdev->send_buff, JL_MAX_PACKET_LEN, 
            PT_SUB_CLOUD_CTRL, _g_pdev->jlp.sessionKey, 
            (uint8_t*)data, ret);

    if(len > 0 &&  len < JL_MAX_PACKET_LEN){
        ret = send(_g_pdev->server_socket, _g_pdev->send_buff, len, 0);
        if(ret < 0){
            log_error("send error ret:%d", ret);
        }
        log_info("send to server len:%d:ret:%d\n", len, ret);
    }else{
        log_error("packet error ret:%d", ret);
    }
}

static void
joylink_proc_server_sub_unbind(uint8_t* recPainText, unsigned short payloadlen)
{
    int ret = -1;
    int len;
    char data[JL_MAX_PACKET_LEN] = {0};

    JLSubUnbind_t unbind;
    bzero(&unbind, sizeof(unbind));

    if(-1 == joylink_subdev_unbind(recPainText, &unbind)){
        return;
    }
    ret = joylink_packet_subdev_unbind_rsp(data, sizeof(data), &unbind);
    if(-1 == ret){
        return;
    }
    len = joylink_encypt_server_rsp(_g_pdev->send_buff, JL_MAX_PACKET_LEN,
        PT_SUB_UNBIND, _g_pdev->jlp.sessionKey,
        (uint8_t*)data, payloadlen);

    if(len > 0 &&  len < JL_MAX_PACKET_LEN){
        ret = send(_g_pdev->server_socket, _g_pdev->send_buff, len, 0);
        if(ret < 0){
            log_error("send error ret:%d", ret);
        }
        log_info("send to server len:%d:ret:%d\n", len, ret);
    }else{
        log_error("packet error ret:%d", ret);
    }

    if(NULL != unbind.info){
        free(unbind.info);
    }
}

void
joylink_server_ota_status_upload_req(JLOtaUpload_t *otaUpload)
{
    int ret = -1;
    int len = 0;
    len = joylink_package_ota_upload_req(otaUpload);

    if(len > 0 && len <JL_MAX_PACKET_LEN){
        ret = send(_g_pdev->server_socket, _g_pdev->send_buff, len, 0);
        if(ret < 0){
            log_error("send error ret:%d\n", ret);
        }
        log_info("send to server len:%d:ret:%d\n", len, ret);
    }else{
        log_error("packet error ret:%d\n", ret);
    }
}

static void
joylink_proc_server_ota_order(uint8_t* json, int json_len)
{
    int ret = -1;
    int len = 0;
    JLOtaOrder_t otaOrder;
    bzero(&otaOrder, sizeof(otaOrder));

    ret = joylink_parse_server_ota_order_req(&otaOrder, (char *)json);
    if(ret != 0){
        return;
    }
    _g_pdev->jlp.crc32 = otaOrder.crc32;
    
    ret = joylink_dev_ota(&otaOrder);
    if(ret != 0){
        log_error("dev ota error:ret:%d\n", ret);
        return;
    }

    len = joylink_packet_server_ota_order_rsp(&otaOrder);

    if(len > 0 && len < JL_MAX_PACKET_LEN){ 
        ret = send(_g_pdev->server_socket, _g_pdev->send_buff, len, 0);
        if(ret < 0){
            log_error("send error ret:%d", ret);
        }else{
            log_info("send to server len:%d:ret:%d\n", len, ret);
        }
    }else{
        log_error("packet error ret:%d", ret);
    }

    joylink_dev_ota_status_upload();
}

static void
joylink_proc_server_ota_upload(uint8_t* json)
{
    int ret = -1;
    JLOtaUploadRsp_t otaUpload;
    bzero(&otaUpload, sizeof(JLOtaUploadRsp_t));

    ret = joylink_parse_server_ota_upload_req(&otaUpload, (const char*)json);
    
    if(ret != 0){
        return;
    }

    log_info("OTA UPLOAD OK code %d msg %s\r\n", 
        otaUpload.code, otaUpload.msg);
}

int
joylink_server_recv(char fd, char *rec_buff, int max)
{
    JLPacketHead_t head;
    bzero(&head, sizeof(head));
    E_PT_REV_ST_t st = E_PT_REV_ST_MAGIC;
    int ret;
    
    do{
        switch(st){
            case E_PT_REV_ST_MAGIC:
                ret = recv(fd, &head.magic, sizeof(head.magic), 0);
                if(head.magic == 0x123455CC){
                    st = E_PT_REV_ST_HEAD;
                }
                break;
            case E_PT_REV_ST_HEAD:
                ret = recv(fd, &head.optlen, sizeof(head) - sizeof(head.magic), 0);
                if(ret > 0){
                    st = E_PT_REV_ST_DATA;
                }
                break;
            case E_PT_REV_ST_DATA:
                if(head.optlen + head.payloadlen < max - sizeof(head)){
                    ret = recv(fd, rec_buff + sizeof(head), head.optlen + head.payloadlen , 0);
                    if(ret > 0){
                        memcpy(rec_buff, &head, sizeof(head));
                        ret = ret + sizeof(head);
                        st = E_PT_REV_ST_END;
                    }

                }else{
                    ret = -1;
                    log_fatal("recev buff is too small");
                }
                break;
            default:
                break;
        }
    }while(ret>0 && st != E_PT_REV_ST_END);

    if (ret == -1 || ret == 0){
        return -1;
    }

    return ret;
}

void
joylink_proc_server()
{
    uint8_t recBuffer[JL_MAX_PACKET_LEN * 2 ] = { 0 };
    uint8_t recPainText[JL_MAX_PACKET_LEN * 2 + 16] = { 0 };
    int ret;

    ret = joylink_server_recv(_g_pdev->server_socket, (char*)recBuffer, JL_MAX_PACKET_LEN * 2);
    if (ret == -1 || ret == 0){
        close(_g_pdev->server_socket);
        _g_pdev->server_socket = -1;
        _g_pdev->server_st = 0;
        log_info("Server close, Reconnect!\r\n");
        return;
    }

    JLPacketParam_t param;
    ret = joylink_dencypt_server_req(&param, recBuffer, ret, recPainText, JL_MAX_PACKET_LEN);

    if (ret < 1){
        return;
    }

    log_info("Server org ctrl type:%d", param.type);
    switch (param.type){
        case PT_AUTH:
            joylink_proc_server_auth(recPainText);
            break;
        case PT_BEAT:
            joylink_proc_server_hb(recPainText);
            break;
        case PT_SERVERCONTROL:
            joylink_proc_server_ctrl(recPainText);
            break;
        case PT_UPLOAD:
            joylink_proc_server_upload(recPainText);
            break;
        case PT_SUB_HB:
            joylink_proc_server_sub_hb(recPainText);
            break;
        case PT_SUB_UPLOAD:
            joylink_proc_server_sub_upload(recPainText);
            break;
        case PT_SUB_CLOUD_CTRL:
            joylink_proc_server_sub_ctrl(recPainText, ret);
            break;
        case PT_SUB_UNBIND:
            joylink_proc_server_sub_unbind(recPainText, ret);
            break;
        case PT_OTA_ORDER:
            joylink_proc_server_ota_order((recPainText + 4), (ret - 4));
            break;
        case PT_OTA_UPLOAD:
            joylink_proc_server_ota_upload(recPainText + 4);
            break;
        default:
            log_debug("Unknow param type.\r\n");
            break;
    }
}
