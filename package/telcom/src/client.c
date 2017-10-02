#include "lib.h"

#include "uci.h"
#include "ioos_uci.h"
#include "uci_fn.h"
#include "nlk_ipc.h"
#include "ipc_msg.h"
#include "module.h"
#include "igd_wifi.h"
#include "igd_system.h"
#include "igd_interface.h"
#include "igd_share.h" 
#include "igd_dnsmasq.h"

#include <netinet/in.h>
#include "cydh.h"
#include "module.h"

#define C_T1	5
#define C_T2	10
#define C_T3	20
#define C_T4	20
#define C_T5	5
#define C_T6	5
#define C_T7	5
#define C_T8	2
#define C_T9	5


#define CGI_BIT_TEST(d,f)  ((d) & (1<<(f)))
#define CGI_BIT_SET(d,f)  (d) |= (1<<(f))

#define TELCOM_CONFIG_FILE	"telcom"
#define TELCOM_CONFIG_FILE_PATH	"/etc/config/telcom"

struct telcom_config_t{
	char disable_web;
};

static struct telcom_config_t telcom_config; 
 static void xielin(const char *content)
{
	FILE *fp;
	if((fp=fopen("/tmp/xielin.txt","a+"))==NULL)
	{
		return ;
	}
	fprintf(fp,"%s\n",content);
	fclose(fp);
}


//client状态变迁主线
#define C_T_TYPE_MAINLINE 1
enum{
	C_STATUS_INIT,
	C_STATUS_WAIT_GW,
	C_STATUS_KEYREQ_SEND,
	C_STATUS_ECDH_SEND,

	C_STATUS_DH_SEND_INI,
	
	C_STATUS_KEEPALIVE,
	C_STATUS_KEEPALIVE_SEND
};

//client状态变迁设备注册分支
#define C_T_TYPE_REG_BRANCH 2
enum{
	C_STATUS_REG_INIT,
	C_STATUS_REG_SEND
};

#define C_T_TYPE_DEV_BRANCH 3
enum{
	C_STATUS_DEVCHANGE_INIT,
	C_STATUS_DEVCHANGE_SEND
};

u_int64_t jiffies = 0;

enum MODE{
	MODE_NONE=0,
	MODE_5G,
	MODE_24G
};
enum FREQ{
	FREQ_NONE=0,
	FREQ_5G,
	FREQ_24G
};

#define USE_LOCAL_HOST 0
static u_int32_t _get_gw()
{
 	struct in_addr addr;
	int uid = 1;
	struct iface_info info;
  	memset(&info, 0x0, sizeof(info));

#if USE_LOCAL_HOST == 1
	inet_aton("127.0.0.1", &addr);
	return ntohl(addr.s_addr);
#endif
  	
	if (mu_msg(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &info, sizeof(info))){
		inet_aton("192.168.1.1", &addr);
		return ntohl(addr.s_addr);
	}else{
		inet_aton(inet_ntoa(info.gw), &addr);
		DEBUG("----gw ip=%s\n", inet_ntoa(info.gw));
 		return ntohl(addr.s_addr);
	}
 }

#if CFG_ENABLE_WIAIR_API
/*
struct host_info {
	struct in_addr addr;
	unsigned char mac[6];
	struct pkt_stats send; //send pkts 
	struct pkt_stats rcv; //  rcv pkts 
	char name[32]; // host name 
	char nick_name[32]; // nick_name set by user
	int conn_cnt; //  conn cnt 
	int seconds; //  online seconds 
	uint16_t vender;
	uint8_t os_type;
	uint8_t is_wifi; 
	uint8_t pid; 
};
*/

static json_t * _host2json(struct host_info * h)
{
	json_t * json;

	json = json_object();
	if (!json) {
		ERROR("json_object() failed\n");
		return NULL;
	}

	json_object_set(json, "devname", json_string(h->name));

	switch (h->os_type){
		case 1:json_object_set(json, "devtype", json_string("pc"));break;
		case 2:json_object_set(json, "devtype", json_string("pc"));break;
		case 3:json_object_set(json, "devtype", json_string("pc"));break;
		case 4:json_object_set(json, "devtype", json_string("phone"));break;
		case 5:json_object_set(json, "devtype", json_string("phone"));break;
		case 6:json_object_set(json, "devtype", json_string("phone"));break;
		case 7:json_object_set(json, "devtype", json_string("pad"));break;
		case 8:json_object_set(json, "devtype", json_string("pad"));break;
		case 9:json_object_set(json, "devtype", json_string("pad"));break;
		default:json_object_set(json, "devtype", json_string("other"));break;
	}
	json_object_set(json, "mac", json_string(mac2str(h->mac)));
	json_object_set(json, "ip", json_string(inet_ntoa(h->addr)));
	json_object_set(json, "connecttype", json_integer(h->is_wifi));
	json_object_set(json, "onlinetime", json_integer(h->seconds));

	return json;
}

/*
"wifi":[
{
"radio":
{
"mode":	"2.4G",		#wifi mode 2.4G/5G
"channel":	number,	
},
}
]
*/
static json_t * _wifiradio2json(int channel,int freq)
{
	json_t * json = json_object(); if(!json) return NULL;

	if(freq==FREQ_24G){
		json_object_set(json, "mode", json_string("2.4G"));
	}
	else if(freq==FREQ_5G){
		json_object_set(json, "mode", json_string("5G"));
	}
	char channel_str[4]={0};
	sprintf(channel_str,"%d",channel);				
	json_object_set(json, "channel", json_string(channel_str));
	return json;
}


//main ap
/*
{"key":"12347890","apidx":"0","auth":"wpapskwpa2psk","ssid":"ChinaNet-TcGF","encrypt":"aes"},
*/
static json_t * _wifiap0_2json(struct wifi_conf *config,char *auth,char*encrypt)
{
		json_t * json = json_object(); if(!json) return NULL;
		json_object_set(json, "apidx", json_string("0"));
		json_object_set(json, "ssid", json_string(config->ssid));
		json_object_set(json, "key", json_string(config->key));
		json_object_set(json, "auth", json_string(auth));
		json_object_set(json, "encrypt", json_string(encrypt));
		return json;
}
//guest ap
static json_t * _wifiap1_2json(struct wifi_conf *config,char *auth,char*encrypt)
{
		json_t * json = json_object(); if(!json) return NULL;
		json_object_set(json, "apidx", json_string("1"));
		json_object_set(json, "ssid", json_string(config->vssid));
		json_object_set(json, "key", json_string(config->key));
		json_object_set(json, "auth", json_string(auth));
		json_object_set(json, "encrypt", json_string(encrypt));
		return json;
}

static json_t * _wifiap2json(struct wifi_conf *config)
{
/*
"ap":[
{
"apidx":		number	#虚拟AP的索引号，从0开始，0为主AP
"ssid":		"ssid",
"key":		"wifi key",
"auth":		"auth mode",	
"encrypt":	"encrypt mode",
},
]

[{"radio":{"channel":0,"mode":"2.4G"},"ap":
[
{"key":"12347890","apidx":"0","auth":"wpapskwpa2psk","ssid":"ChinaNet-TcGF","encrypt":"aes"},
{"key":"SYTUXQSE","apidx":"1","auth":"wpapskwpa2psk","ssid":"iTV-TcGF","encrypt":"aes"},
{"key":"SYTUXQSE","apidx":"2","auth":"wpapskwpa2psk","ssid":"fiberhome-TcGF","encrypt":"aes"},
{"key":"SYTUXQSE","apidx":"3","auth":"wpapskwpa2psk","ssid":"fhpre-TcGF","encrypt":"aes"}]}]}}'

*/
	/*
	open,		->none
	share,		->不支持
	wpa,		->不支持
	wpa2,		->不支持
	wpapsk, 	->psk		+
	wpa2psk,	->psk2		+
	wpapskwpa2psk,	->psk-mixed +
	/////
	none,  不跟东西即可
	wep,		->不支持	
	tkip,		->tkip
	aes,		->ccmp
	aestkip,	->tkip+ccmp
	*/
	//psk-mixed+ccmp
	json_t * json = json_array(); if(!json) return NULL;
	
	char auth[IGD_NAME_LEN]={0};
	char encrypt[IGD_NAME_LEN]={0};
	//
	if (!strncmp(config->encryption, "none", 4)) {
		strcpy(auth, "open");
		strcpy(encrypt, "none");//这样最简单
	} else {
		if (!strncmp(config->encryption, "psk+", 4)){
			strcpy(auth, "wpapsk");
		}else if (!strncmp(config->encryption, "psk2+", 5)){
			strcpy(auth, "wpa2psk");
		}else if (!strncmp(config->encryption, "psk-mixed+", 9)){
			strcpy(auth, "wpapskwpa2psk");
		}else{
			goto out;
		}

		char *p = strchr(config->encryption, '+');
		if (!p)
			goto out;
		p++;
		if ((strlen(p) == 9) && !strncmp(p, "tkip+ccmp", 9)){
			strcpy(encrypt, "aestkip");
		}else if ((strlen(p) == 4) && !strncmp(p, "tkip", 4)){
			strcpy(encrypt, "tkip");
		}else if ((strlen(p) == 4) && !strncmp(p, "ccmp", 4)){
			strcpy(encrypt, "aes");
		}else{
			goto out;
		}
	}
out:
	json_array_append(json, _wifiap0_2json(config,auth,encrypt));
	json_array_append(json, _wifiap1_2json(config,auth,encrypt));
	return json;
}

static int get_channel(int freq)
{
	int  channel=0;
	int ret=0;
	if(freq==FREQ_24G){
		ret=mu_msg(WIFI_MOD_GET_CHANNEL, NULL, 0, &channel, sizeof(channel));
	}else if(freq==FREQ_5G){
		ret=mu_msg(WIFI_MOD_GET_CHANNEL_5G, NULL, 0, &channel, sizeof(channel));
	}
	DEBUG("channel=%d",channel);
	if(!ret){
		return channel;
	}else{
		return 0;
	}
}

static json_t * _wifi2json(int freq)
{
	json_t * json = json_object(); if(!json) return NULL;
	struct wifi_conf config;
	memset(&config,0,sizeof(config));
	int ret=0;
	if(freq==FREQ_24G){
		ret=(mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config)));
	}
	else if(freq==FREQ_5G){
		ret=(mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config)));
	}
	if(ret){
		return json;
	}
	json_object_set(json, "radio", _wifiradio2json(get_channel(freq),freq));
//	DEBUG("\n encryption=%s",config.encryption);
	json_object_set(json, "ap", _wifiap2json(&config));
//	print_json(json);
	return json;
}

static json_t * _wifi2json_array()
{
	json_t * json = json_array(); if(!json) return NULL;
	json_array_append(json, _wifi2json(FREQ_24G));
	json_array_append(json, _wifi2json(FREQ_5G));
	print_json(json);
	return json;
}

static json_t * _ledswitch2json()
{
	json_t * json = json_object(); if(!json) return NULL;
	int act =NLK_ACTION_DUMP;
	int led;
	char status[8]="ON";
	if(mu_msg(SYSTME_MOD_SET_LED, &act, sizeof(act), &led, sizeof(led))){
	}
	else{
		if(led==0){
			strcpy(status,"OFF");
		}
	}
	json_object_set(json, "status", json_string(status));
	print_json(json);
	
	return json;
}

static json_t * _wifiswitch2json(int freq)
{
	int ret=0;
	json_t * json = json_object(); if(!json) return NULL;
	
	struct wifi_conf config;
	memset(&config,0,sizeof(config));
	if(freq==FREQ_24G){
		ret=mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config));
	}
	else if(freq==FREQ_5G){
		ret=mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config));
	}
	if(!ret){
		if(config.enable){
			json_object_set(json, "status", json_string("ON"));
		}
		else{
			json_object_set(json, "status", json_string("OFF"));
		}
	}
	print_json(json);
	return json;
}

/*
"wifitimer":
[
{"weekday": "day",	#周一至周日，1-7
"time": "time", 	#执行动作的时间，例如"19:30"
"enable": "enable", #0:disable；1
},
]

[
{"weekday":3, "time":"00:00", "enable":1},{"weekday":3, "time":"24:00", "enable":0},
{"weekday":6, "time":"00:00", "enable":1},{"weekday":6, "time":"24:00", "enable":0},
]

*/
//{"weekday":3, "time":"00:00", "enable":1},{"weekday":3, "time":"24:00", "enable":0},

static json_t * _wifitimer_date2json(int weekday,int hour,int min, int enable)
{
		if (weekday==0){
			weekday=7;
		}
		json_t * json = json_object(); if(!json) return NULL;
		char time[24]={0};
		sprintf(time,"%02d:%02d",hour,min);
		json_object_set(json, "weekday", json_integer(weekday));
		json_object_set(json, "time", json_string(time));
		json_object_set(json, "enable", json_integer(enable));
		print_json(json);
		return json;
}

static json_t * _wifitimer2json(int freq)
{
	int ret=0;
	json_t * json = json_array(); if(!json) return NULL;
	
	struct wifi_conf config;
	config.apindex=0;
	memset(&config,0,sizeof(config));

	if(freq==FREQ_24G){
		ret=mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config));
	}
	else if(freq==FREQ_5G){
		ret=mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config));
	}


	struct time_comm *t = &config.tm;

	if(!ret){
		int is_first=0;
		int i=0;
		if(t->loop){
			for (i = 0; i < 7; i++) {
				if (!CGI_BIT_TEST(t->day_flags, i))
					continue;

				json_array_append(json, _wifitimer_date2json(i,t->end_hour,t->end_min,1));
				json_array_append(json, _wifitimer_date2json(i,t->start_hour,t->start_min,0));
				//	json_array_append(json, _wifitimer_date2json(i,t->start_hour,t->start_min,1));
				//	json_array_append(json, _wifitimer_date2json(i,t->end_hour,t->end_min,0));
			}	
		}
	}

	//print_json(json);
	
	return json;

}
#endif /* CFG_ENABLE_WIAIR_API */

static void _client_init(struct client * c)
{
	client_timer_init(c->main_line, C_T_TYPE_MAINLINE, C_STATUS_INIT, jiffies);
	list_add_tail(&c->main_line->list, &c->timers);
	DEBUG("-> C_STATUS_INIT\n");
}

static int _client_connect(struct client * c, u_int32_t gw)
{
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = htonl(gw);

	//struct timeval timeout={20,0};//time out seconds
	//int ret=setsockopt(c->fd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));

	if (connect(c->fd, &addr, sizeof(addr))) {
		ERROR("connect failed\n");
		return -1;
	}


	return 0;
}

static int _client_send_keyreq(struct client * c)
{
	int ret;
	json_t * json;
	json_t * j_data;
	json_t * j_ecdh;
	json_t * j_staitc;

	json = json_header("keyngreq", c->seq++, c->mac);
	if (!json)
		return -1;

	j_data = json_array();
	if (!j_data) {
		ERROR("json_object failed\n");
		goto error;
	}
	json_object_set(json, "keymodelist", j_data);
	//ecdh
	j_ecdh = json_object();
	if (!j_ecdh) {
		ERROR("json_object failed\n");
		goto error;
	}
	json_object_set(j_ecdh, "keymode", json_string("ecdh"));
	json_array_append(j_data, j_ecdh);
	//static
	j_staitc = json_object();
	if (!j_staitc) {
		ERROR("json_object failed\n");
		goto error;
	}
	json_object_set(j_staitc, "keymode", json_string("dh"));

	//json_object_set(j_staitc, "keymode", json_string("static"));
	json_array_append(j_data, j_staitc);

	ret = client_queue_json(c, json, 0, 0);

	if (ret == 0) {
		c->main_line->status = C_STATUS_KEYREQ_SEND;
		c->main_line->last = jiffies + C_T9;
		DEBUG("-> C_STATUS_KEYREQ_SEND\n");
	}

	return ret;

error:
	json_decref(json);
	return -1;
}

static int _client_send_ecdh(struct client * c)
{
	int ret;
	json_t * json;
	json_t * data;

	json = json_header("ecdh", c->seq++, c->mac);
	if (!json)
		return -1;

	data = json_object();
	if (!data) {
		ERROR("json_object failed\n");
		json_decref(json);
		return -1;
	}

	c->ecdh.key = ecdh_pub_key(c->ecdh.my_pub_key);
	json_object_set(data,"ecdh_key", json_string(oct2string(c->ecdh.my_pub_key, ECDH_SIZE + 1)));
	json_object_set(json, "data", data);

	ret = client_queue_json(c, json, 0, 0);

	if (ret == 0) {
		c->main_line->status = C_STATUS_ECDH_SEND;
		c->main_line->last = jiffies + C_T2;
		DEBUG("-> C_STATUS_ECDH_SEND\n");
	}

	return ret;
}

static int _client_send_dh(struct client * c)
{
	int ret;
	int ret0=0;
	json_t * json=NULL;
	json_t * j_data=NULL;

	json_t * j_p=NULL;
	json_t * j_g=NULL;
	json_t * j_key=NULL;

	json = json_header("dh", c->seq++, c->mac);
	if (!json)
		return -1;

	j_data = json_object();
	if (!j_data) {
		ERROR("json_object failed\n");
		goto ERROR;
	}
	//DH *k1 = NULL;
	if(c->dh.k1){
		DH_free(c->dh.k1);
		c->dh.k1= NULL;
	}

	uint8_t g_int= DH_GENERATOR_2;
	BIGNUM *p_bn = NULL, *g_bn = NULL;

	if(dh_new_key(&c->dh.k1, BIT_LEN_OF_PRIME, p_bn, g_int) < 0){
		goto ERROR;
	} 
 
	unsigned char p_str[1024]={0};
	unsigned char g_str[1024]={0};
	unsigned char pub_key_str[1024]={0};
 	DEBUG("\n begin\n");
	disp_bn(c->dh.k1->p, "-----Public key1:\n");
	disp_bn(c->dh.k1->g, "-----Public key1:\n");
	disp_bn(c->dh.k1->pub_key, "-----Public key1:\n");
	
 	DEBUG("\n begin\n");

	ret0=cybn_to_base64(c->dh.k1->p,p_str); if(ret0){goto ERROR;}
	ret0=cybn_to_base64(c->dh.k1->g,g_str);if(ret0){goto ERROR;}
	ret0=cybn_to_base64(c->dh.k1->pub_key,pub_key_str);if(ret0){goto ERROR;}	

	json_object_set(j_data, "dh_key", json_string(pub_key_str));
	json_object_set(j_data, "dh_p", json_string(p_str));
	json_object_set(j_data, "dh_g", json_string(g_str));


	json_object_set(json, "data", j_data);

//send
	ret = client_queue_json(c, json, 0, 0);
	if (ret == 0) {
		c->main_line->status = C_STATUS_DH_SEND_INI;
		c->main_line->last = jiffies + C_T2;
		DEBUG("-> C_STATUS_DH_SEND_INI\n");
	}

	return ret;

ERROR:
	if(json){json_decref(json);}
	if(j_data){json_decref(j_data);}
	//if(j_p){json_decref(j_p);}
	//if(j_g){json_decref(j_g);}
	//if(j_key){json_decref(j_key);}
	return -1;
}


static int _client_send_dh_old(struct client * c)
{
	int ret;
	int ret0=0;
	json_t * json=NULL;
	json_t * j_data=NULL;

	json_t * j_p=NULL;
	json_t * j_g=NULL;
	json_t * j_key=NULL;

	json = json_header("dh", c->seq++, c->mac);
	if (!json)
		return -1;

	j_data = json_array();
	if (!j_data) {
		ERROR("json_object failed\n");
		goto ERROR;
	}

	//DH *k1 = NULL;
	if(c->dh.k1){
		DH_free(c->dh.k1);
		c->dh.k1= NULL;
	}

	uint8_t g_int= DH_GENERATOR_2;
	BIGNUM *p_bn = NULL, *g_bn = NULL;

	if(dh_new_key(&c->dh.k1, BIT_LEN_OF_PRIME, p_bn, g_int) < 0){
		goto ERROR;
	} 
//disp_bn(k1->g, "-----G:\n");
//disp_bn(k1->pub_key, "-----Public key:\n");

	unsigned char p_str[1024]={0};
	unsigned char g_str[1024]={0};
	unsigned char pub_key_str[1024]={0};


	ret0=cybn_to_base64(c->dh.k1->p,p_str); if(ret0){goto ERROR;}
	ret0=cybn_to_base64(c->dh.k1->g,g_str);if(ret0){goto ERROR;}
	ret0=cybn_to_base64(c->dh.k1->p,pub_key_str);if(ret0){goto ERROR;}	


	j_key = json_object();if (!j_key) {goto ERROR;}
	json_object_set(j_key, "dh_key", json_string(pub_key_str));
	json_array_append(j_data, j_key );

	j_p = json_object();if (!j_p) {goto ERROR;}
	json_object_set(j_p, "dh_p", json_string(pub_key_str));
	json_array_append(j_data, j_p);

	j_g = json_object();if (!j_g) {goto ERROR;}
	json_object_set(j_g, "dh_g", json_string(pub_key_str));
	json_array_append(j_data, j_g);

	json_object_set(json, "data", j_data);

//send
	ret = client_queue_json(c, json, 0, 0);
	if (ret == 0) {
		c->main_line->status = C_STATUS_DH_SEND_INI;
		c->main_line->last = jiffies + C_T2;
		DEBUG("-> C_STATUS_ECDH_SEND\n");
	}

	return ret;

ERROR:
	if(json){json_decref(json);}
	if(j_data){json_decref(j_data);}
	if(j_p){json_decref(j_p);}
	if(j_g){json_decref(j_g);}
	if(j_key){json_decref(j_key);}
	return -1;
}



static int _client_send_dev_reg(struct client * c)
{
	int ret;
	json_t * json;
	json_t * j_data;

	json = json_header("dev_reg", c->seq++, c->mac);
	if (!json){
		DEBUG("\n");
		return -1;
	}

	//data
	j_data = json_object();
	if (!j_data) {
		ERROR("json_object failed\n");
		json_decref(json);
		return -1;
	}
	json_object_set(j_data, "vendor", json_string("WIAIR")/*TODO*/);
	json_object_set(j_data, "model", json_string("I1")/*TODO*/);
	json_object_set(j_data, "url", json_string("http://www.wiair.com/"));
	json_object_set(j_data, "wireless", json_string("yes")/*TODO*/);

	json_object_set(json, "data", j_data);

 	ret = client_queue_json(c, json, 1, 1);

	if (ret == 0) {
		struct client_timer * t;

		t = client_timer_find_by_type(c, C_T_TYPE_REG_BRANCH);
		if(t){
			t->status = C_STATUS_REG_SEND;
			t->last = jiffies + C_T3;
		}
		DEBUG("-> C_STATUS_REG_SEND\n");
	}

	return ret;
}

static int _client_send_keepalive(struct client * c)
{
	int ret;
	json_t * json;
	//json_t * j_data;

	json = json_header("keepalive", c->seq++, c->mac);
	if (!json)
		return -1;

	ret = client_queue_json(c, json, 1, 1);

	if (ret == 0) {
		c->main_line->status = C_STATUS_KEEPALIVE_SEND;
		c->main_line->last = jiffies + C_T4;
		DEBUG("-> C_STATUS_KEEPALIVE_SEND\n");
	}

	return ret;
}

static int _client_send_dev_report(struct client * c)
{
	int ret;
	json_t * json;
	json_t * j_data;
	json_t * j_host;
	int i;
#if CFG_ENABLE_WIAIR_API
	int nr;
	struct host_info * h;
	struct host_info * hosts;
#endif /*CFG_ENABLE_WIAIR_API*/

	json = json_header("dev_report", c->seq++, c->mac);
	if (!json)
		return -1;

	j_data = json_array();
	if (!j_data){
		ERROR("json_array() failed\n");
		goto error;
	}
	json_object_set(json, "dev", j_data);

#if CFG_ENABLE_WIAIR_API
	nr = 0;
	hosts = dump_host_alive(&nr);
	h = hosts;
	if (h) {
		for (i = 0; i < nr; i++) {
			j_host = _host2json(h++);
			if (!j_host) {
				free(hosts);
				goto error;
			}
			json_array_append(j_data, j_host);
		}
	}
	free(hosts);
#endif /*CFG_ENABLE_WIAIR_API*/

	ret = client_queue_json(c, json, 1, 1);
	if (ret == 0) {
		struct client_timer * t;
		
		t = client_timer_find_by_type(c, C_T_TYPE_DEV_BRANCH);
		if(t){
			t->status = C_STATUS_DEVCHANGE_SEND;
			t->status = jiffies + C_T5;
		}
		DEBUG("-> C_STATUS_DEVCHANGE_SEND\n");
	}

	return ret;

error:
	json_decref(json);
	return -1;
}

static int _client_recv_keyreq_ack(struct client * c, json_t * json, int seq, json_t * j_data)
{
	char * mode;
	if (!j_data){
		ERROR("no 'data'\n");
		return -1;
	}

	if (c->main_line->status != C_STATUS_KEYREQ_SEND) {
		ERROR("wrong status, should be C_STATUS_KEYREQ_SEND\n");
		return -1;
	}

	mode = json_string_value(j_data);

	if (!strcmp(mode, "static")) {
		struct client_timer * t;

		string2oct(ECDH_STATIC_SECRET, c->ecdh.secret, ECDH_SIZE);

		//main line to keepalive
		c->main_line->status = C_STATUS_KEEPALIVE;
		c->main_line->last = jiffies + C_T6;
		DEBUG("-> C_STATUS_KEEPALIVE\n");

		//reg branch
		t = client_timer_new();
		if (!t) 
			return -1;

		client_timer_init(t, C_T_TYPE_REG_BRANCH, C_STATUS_REG_INIT, jiffies);
		list_add_tail(&t->list, &c->timers);
		DEBUG("-> C_STATUS_REG_INIT\n");

		return _client_send_dev_reg(c);
	}
	else if(!strcmp(mode, "ecdh")){
		return _client_send_ecdh(c);
	}
	else if(!strcmp(mode, "dh")){
		return _client_send_dh(c);
	}
	else{
	}
}
static int _client_recv_dh_ack(struct client * c, json_t * json, int seq, json_t * j_data)
{
	struct client_timer * t;
	unsigned pub_bin[BYTE_LEN_OF_PRIME * 2], share_bin[BYTE_LEN_OF_PRIME * 2], str[1024]={0};
	int p_bin_len, g_bin_len, pub_bin_len, share_bin_len;
	BIGNUM *pub_bn = NULL, *share_bn = NULL;

	if (!j_data){
		ERROR("no 'data'\n");
		return -1;
	}

	if (c->main_line->status != C_STATUS_DH_SEND_INI) {
		ERROR("wrong status, should be C_STATUS_DH_SEND_INI\n");
		return -1;
	}

	json_t * j_key = json_object_get(j_data, "dh_key");
	if (!j_key){
		ERROR("no 'dh_key'\n");
		return -1;
	}
//解析收到的数据并运算
	char * tmp=json_string_value(j_key);
	if(!tmp){
		ERROR("no json_string_value(j_key) error\n");
	}
	if(strlen(tmp)<=sizeof(str)){
		strcpy(str,tmp);
	}else{
		ERROR("no 'key_too_long'\n");
		goto ERROR;
	}
	pub_bin_len = BASE64_Decode(str, strlen((char*)str), pub_bin);
	if(NULL == (pub_bn = BN_bin2bn(pub_bin, pub_bin_len, NULL))) {
		ERROR("get 'pub_bn' failed\n");
		goto ERROR;
	}
	//disp_bn(pub_bn, "-----pair's public key:\n");
	if((share_bin_len = get_share(pub_bn,c->dh.k1, share_bin, sizeof(share_bin))) < 0) {
		ERROR("get 'get_share' failed\n");
		goto ERROR;
	}
	if(NULL == (share_bn = BN_bin2bn(share_bin, share_bin_len, NULL))) {
		//handleErrors(__FUNCTION__, __LINE__);
		goto ERROR;
	}
	disp_bn(share_bn, "-----share key\n");

	/*
	DEBUG("change begin");
	if(NULL == (share_bn = BN_bin2bn(share_bin, share_bin_len, NULL))){
		ERROR("BN_bin2bn failed\n");
		goto ERROR;
	}
	//disp_bn(share_bn, "-----share key\n");
	*/
	//c->ecdh.secret

	DEBUG("dstlen=%d,share_bin_len=%d,BYTE_LEN_OF_PRIME * 2=%d",sizeof(c->ecdh.secret),share_bin_len,BYTE_LEN_OF_PRIME * 2);
	memset(c->ecdh.secret,0,sizeof(c->ecdh.secret));
	memcpy(c->ecdh.secret,share_bin,share_bin_len);

	/*
	//生成的key是怎样的?
	if(NULL == (share_bn = BN_bin2bn(share_bin, share_bin_len, NULL))) {
		ERROR("get 'BN_bin2bn share key' failed\n");
		goto ERROR;
	}
	*/
	//生成KEY
/////

	c->main_line->status = C_STATUS_KEEPALIVE;
	c->main_line->last = jiffies + C_T6;
	DEBUG("-> C_STATUS_KEEPALIVE\n");

	t = client_timer_new();
	if (!t) {
		goto ERROR;
	}

	client_timer_init(t, C_T_TYPE_REG_BRANCH, C_STATUS_REG_INIT, jiffies);
	list_add_tail(&t->list, &c->timers);
	DEBUG("-> C_STATUS_REG_INIT\n");

	if(c->dh.k1){
		DH_free(c->dh.k1);
		c->dh.k1=NULL;
	}
	
	if(pub_bn)
		BN_free(pub_bn);
	if(share_bn)
		BN_free(share_bn);

	return _client_send_dev_reg(c);
ERROR:

	if(c->dh.k1){
		DH_free(c->dh.k1);
		c->dh.k1=NULL;
	}
	if(pub_bn)
		BN_free(pub_bn);
	if(share_bn)
		BN_free(share_bn);
		return -1;
	
}

static int _client_recv_ecdh(struct client * c, json_t * json, int seq,  json_t * j_data)
{
	char * ecdh_key;
	json_t * j_ecdh_key;
	struct client_timer * t;

	if (!j_data){
		ERROR("no 'data'\n");
		return -1;
	}

	j_ecdh_key = json_object_get(j_data, "ecdh_key");
	if (!j_ecdh_key) {
		ERROR("no 'ecdh_key'\n");
		return -1;
	}
	ecdh_key = json_string_value(j_ecdh_key);
	string2oct(ecdh_key, c->ecdh.peer_pub_key, ECDH_SIZE + 1);

	ecdh_shared_secret(c->ecdh.key, c->ecdh.peer_pub_key, c->ecdh.secret);

	if (c->main_line->status != C_STATUS_ECDH_SEND) {
		ERROR("wrong status, should be C_STATUS_ECDH_SEND\n");
		return -1;
	}
	c->main_line->status = C_STATUS_KEEPALIVE;
	c->main_line->last = jiffies + C_T6;
	DEBUG("-> C_STATUS_KEEPALIVE\n");

	_client_send_ack(c, seq, 1); //FIXME: send ack ? and encrypt ?

	t = client_timer_new();
	if (!t) 
		return -1;

	client_timer_init(t, C_T_TYPE_REG_BRANCH, C_STATUS_REG_INIT, jiffies);
	list_add_tail(&t->list, &c->timers);
	DEBUG("-> C_STATUS_REG_INIT\n");

	return _client_send_dev_reg(c);
}

static int _client_recv_ack(struct client * c, json_t * json, int seq, json_t * j_data)
{
	struct buffer * b;
	struct buffer * p;

	list_for_each_entry_safe(b, p, &c->wait_ack_queue, list){
		struct client_timer * t;
		char * _type;
		int _seq;
		char * _mac;
		
		json_parse_head(b->json, &_type, &_seq, &_mac);
		if (_seq == seq) {
			if(!strcmp(_type, "dev_reg")){
				t = client_timer_find_by_type(c, C_T_TYPE_REG_BRANCH);
				if(t){
					list_del(&t->list);
					client_timer_free(t); //NOTE: recv ack, reg branch is done
				}
				t = client_timer_new();
				if (!t)
					return -1;
				client_timer_init(t, C_T_TYPE_DEV_BRANCH, C_STATUS_DEVCHANGE_INIT, jiffies + C_T7);
				list_add_tail(&t->list, &c->timers);
				DEBUG("-> C_STATUS_DEVCHANGE_INIT\n");
			}
			else if(!strcmp(_type, "keepalive")){
				t = c->main_line;
				t->status = C_STATUS_KEEPALIVE;
				t->last = jiffies + C_T6;
				DEBUG("-> C_STATUS_KEEPALIVE\n");
			}
			else if(!strcmp(_type, "cfg")){
				t = client_timer_find_by_type(c, C_T_TYPE_DEV_BRANCH);
				if(t){
					if (t->status == C_STATUS_DEVCHANGE_SEND) {
						t->status = C_STATUS_DEVCHANGE_INIT;
						t->last = jiffies + C_T7;
						DEBUG("-> C_STATUS_DEVCHANGE_INIT\n");
					}
					else
						ERROR("wrong status:%d\n", t->status);
				}
			}
			else
				ERROR("type:%s\n", _type);
			list_del(&b->list);
			buffer_free(b);
		}
	}

	return 0;
}

static void set_vap(char * ssid,int freq)//
{
	return;
	struct wifi_conf config;
	memset(&config,0,sizeof(config));
	config.apindex=0;
	int ret=0;
	if(freq==FREQ_24G){
		ret=mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config));
	}else if(freq==FREQ_5G){
		ret=mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config));
	}else{
		return ;
	}
	
	if(!ret){
		if(strlen(ssid)<IGD_NAME_LEN){
			strcpy(config.vssid,ssid);
		}
		if (mu_msg(WIFI_MOD_SET_VAP, &config, sizeof(config), NULL, 0))	{
			return ;
		}
		DEBUG("\n set_vap_24g");
	}
}


/*
 ///////
open,		->none
share,		->不支持
wpa,		->不支持
wpa2,		->不支持
wpapsk,		->psk+
wpa2psk,	->psk2+
wpapskwpa2psk,	->psk-mixed+
/////
none,  不跟东西即可
wep,		->不支持	
tkip,		->tkip
aes,		->ccmp
aestkip,	->tkip+ccmp
*/
static void set_ap(char * ssid,char * key,char * auth,char * encrypt,int freq)
{
	struct nlk_sys_msg nlk;
	struct wifi_conf config;
	memset(&config,0,sizeof(config));

	int ret=0;
	if(freq==FREQ_24G){
		ret=mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config));
	}
	else if(freq==FREQ_5G){
		ret=mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config));
	}else {
		return;
	}

	DEBUG("\nold key=%s encryption=%s",config.key,config.encryption);
	if(!ret){
		if(strlen(ssid)<IGD_NAME_LEN){
			arr_strcpy(config.ssid, ssid);
		}
		if(strlen(key)<IGD_NAME_LEN){
			arr_strcpy(config.key, key);
		}
		char au[IGD_NAME_LEN]="psk2";//default value
		char enc[IGD_NAME_LEN]="+ccmp";//default value

		int is_jiami=1;

		if(!strcmp(auth,"open")){
			is_jiami=0;
			strcpy(au,"none");
			strcpy(enc,"");
		}else if(!strcmp(auth,"wpapsk")){
			strcpy(au,"psk");
		}else if(!strcmp(auth,"wpa2psk")){
			strcpy(au,"psk2");
		}else if(!strcmp(auth,"wpapskwpa2psk")){
			strcpy(au,"psk-mixed");
		}
		if(is_jiami){
			if(!strcmp(encrypt,"none")){
				strcpy(enc,"");
			}else if(!strcmp(encrypt,"aestkip")){
				strcpy(enc,"+tkip+ccmp");
			}else if(!strcmp(encrypt,"aes")){
				strcpy(enc,"+ccmp");
			}else if(!strcmp(encrypt,"aestkip")){
				strcpy(enc,"+tkip+ccmp");
			}
		}
		strcat(au,enc);
		arr_strcpy(config.encryption,au);
		DEBUG("\n enc=%s",config.encryption);
		if (mu_msg(WIFI_MOD_SET_AP, &config, sizeof(config), NULL, 0))	{
			DEBUG("\n set_ap failed");
			return ;
		}

		memset(&nlk, 0x0, sizeof(nlk));
		nlk.comm.gid = NLKMSG_GRP_SYS;
		nlk.comm.mid = SYS_GRP_MID_WIFI;
		nlk.msg.wifi.type = 2;
		nlk_event_send(NLKMSG_GRP_SYS, &nlk, sizeof(nlk));

		
		DEBUG("\n set_ap ok");
	}
}

static void _set_wifi_swich(int enable, int freq, int is_vap)
{
	struct wifi_conf config;
	memset(&config,0,sizeof(config));
	config.apindex=0;
	enable = enable > 0?1:0;
	int ret=0;
	if(freq==FREQ_24G){
		ret=mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config));
	}
	else if(freq==FREQ_5G){
		ret=mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config));
	}else {
		return;
	}
	if (ret) {
		DEBUG("get wifi config error[freq=%d].\n", freq);
		return;
	}
	if(is_vap && config.vap != enable) {
		config.vap = enable;
		if (mu_msg(WIFI_MOD_SET_TIME, &config, sizeof(config), NULL, 0))	{
			return ;
		}
	} 
	if (!is_vap && config.enable != enable) {
		config.enable = enable;
		if (mu_msg(WIFI_MOD_SET_TIME, &config, sizeof(config), NULL, 0))	{
			return ;
		}
	}
	DEBUG("\n freq[%d] wifi_swich[vap=%d] set as[%d]", freq, is_vap, enable);
}
void set_wifi_swich(int enable, int freq)
{
	_set_wifi_swich(enable, freq, 0);
}
void set_wifi_vap_swich(int enable, int freq)
{
	_set_wifi_swich(enable, freq, 1);
}

//act 1 open  0 close 
static void telcom_set_led(int status)
{	
	int act=0;
	int led;
	if(status==1)
	{
		 act=NLK_ACTION_ADD;
	}
	else
	{
		act=NLK_ACTION_DEL;
	}
	mu_msg(SYSTME_MOD_SET_LED, &act, sizeof(act), &led, sizeof(led));
}

static void set_wifitimer_switch(int status,int freq)
{
	struct wifi_conf config;
	memset(&config,0,sizeof(config));
	config.apindex=0;
	int ret=0;

	if(freq==FREQ_24G){
		ret=mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config));
	}
	else if(freq==FREQ_5G){
		ret=mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config));
	}
	else{
		return;
	}
	
	if(!ret){
		if(status==0){
			config.time_on=0;
		}else{
			config.time_on=1;
		}
		if (mu_msg(WIFI_MOD_SET_TIME, &config, sizeof(config), NULL, 0))	{
			return ;
		}
	}
}


static void set_wifi_time(char *day,char *begin_time,char* end_time,int freq)
{
	int start_hour=0;
	int start_min=0;	
	int end_hour=0;
	int end_min=0;		
	
	struct wifi_conf config;
	memset(&config,0,sizeof(config));
	config.apindex=0;
	int ret=0;

	sscanf(begin_time,"%d:%d",&start_hour,&start_min);	
	sscanf(end_time,"%d:%d",&end_hour,&end_min);	

	if(freq==FREQ_24G){
		ret=mu_msg(WIFI_MOD_GET_CONF, NULL, 0, &config, sizeof(config));
	}
	else if(freq==FREQ_5G){
		ret=mu_msg(WIFI_MOD_GET_CONF_5G, NULL, 0, &config, sizeof(config));
	}
	else{
		return;
	}
	//公司的参数是 0100000 星期日-星期1
	
	if(!ret){
		int i=0;
		struct time_comm *t = &config.tm;
		for (i = 0; i < 7; i++) {
			if (*(day + i) == '1')
				CGI_BIT_SET(t->day_flags, i);
		}

/*
		t->start_hour=start_hour;
		t->start_min=start_min;
		t->end_hour=end_hour;
		t->end_min=end_min;		
*/
		t->start_hour=end_hour;
		t->start_min=end_min;
		t->end_hour=start_hour;
		t->end_min=start_min;	

		t->loop = 1;
		//config.enable=1;
		if (mu_msg(WIFI_MOD_SET_TIME, &config, sizeof(config), NULL, 0))	{
			return ;
		}
	}
}

#define MONI_TEST 0
static int _client_recv_cfg(struct client * c, json_t * json, int seq, json_t * j_data)
{
	json_t * j_status;
	json_t * j_set;
	json_t * j_get;
#if MONI_TEST ==1
	DEBUG("HERE");
	///////
	char buffer[4096]="{\"type\":\"cfg\",\"status\":{\"wifi\":[{\"radio\":{\"channel\":2,\"mode\":\"2.4G\"}},{\"radio\":{\"channel\":2,\"mode\":\"5G\"}}]},\"sequence\":1,\"mac\":\"60B617F6C188\",\"set\":{\"wifi\":[{\"radio\":{\"channel\":0,\"mode\":\"2.4G\"},\"ap\":[{\"key\":\"123456789\",\"apidx\":0,\"auth\":\"wpa2psk\",\"ssid\":\"ChinaNet-6RLN\",\"encrypt\":\"aes\"},{\"key\":\"3FRJb4F3\",\"apidx\":1,\"auth\":\"wpapskwpa2psk\",\"ssid\":\"iTV-6RLN\",\"encrypt\":\"aes\"},{\"key\":\"3FRJb4F3\",\"apidx\":2,\"auth\":\"wpapskwpa2psk\",\"ssid\":\"fiberhome-6RLN\",\"encrypt\":\"aes\"},{\"key\":\"3FRJb4F3\",\"apidx\":3,\"auth\":\"wpapskwpa2psk\",\"ssid\":\"fhpre-6RLN\",\"encrypt\":\"aes\"}]},{\"radio\":{\"channel\":0,\"mode\":\"5G\"},\"ap\":[{\"apidx\":0,\"key\":\"123456789\",\"auth\":\"wpa2psk\",\"ssid\":\"ChinaNet-6RLN\",\"mode\":\"5G\",\"encrypt\":\"aes\"},{\"apidx\":1,\"key\":\"3FRJb4F3\",\"auth\":\"wpapskwpa2psk\",\"ssid\":\"iTV-6RLN\",\"mode\":\"5G\",\"encrypt\":\"aes\"},{\"apidx\":2,\"key\":\"3FRJb4F3\",\"auth\":\"wpapskwpa2psk\",\"ssid\":\"fiberhome-6RLN\",\"mode\":\"5G\",\"encrypt\":\"aes\"},{\"apidx\":3,\"key\":\"3FRJb4F3\",\"auth\":\"wpapskwpa2psk\",\"ssid\":\"fhpre-6RLN\",\"mode\":\"5G\",\"encrypt\":\"aes\"}]}]}}";
	//char buffer[2048]="{\"status\":{\"wifi\":[{\"radio\":{\"channel\":6,\"mode\":\"2.4G\"}},{\"radio\":{\"channel\":6,\"mode\":\"5G\"}}]},\"mac\":\"60B617F6BF44\",\"set\":{\"wifi\":[{\"radio\":{\"channel\":0,\"mode\":\"2.4G\"},\"ap\":[{\"key\":\"\",\"apidx\":0,\"auth\":\"open\",\"ssid\":\"ChinaNet-TXCA-ame-4\",\"encrypt\":\"aes\"},{\"key\":\"\",\"apidx\":1,\"auth\":\"open\",\"ssid\":\"iTV-TXCA\",\"encrypt\":\"aes\"},{\"key\":\"87654321\",\"apidx\":2,\"auth\":\"wpapskwpa2psk\",\"ssid\":\"ChinaNet-GRa6\",\"encrypt\":\"aes\"},{\"key\":\"87654321\",\"apidx\":3,\"auth\":\"wpapskwpa2psk\",\"ssid\":\"fhpre-TXCA\",\"encrypt\":\"aes\"}]},{\"radio\":{\"channel\":0,\"mode\":\"5G\"},\"ap\":[{\"apidx\":4,\"key\":\"\",\"auth\":\"open\",\"ssid\":\"ChinaNet-TXCA-ame-4\",\"mode\":\"5G\",\"encrypt\":\"aes\"},{\"apidx\":5,\"key\":\"\",\"auth\":\"open\",\"ssid\":\"iTV-TXCA\",\"mode\":\"5G\",\"encrypt\":\"aes\"},{\"apidx\":6,\"key\":\"87654321\",\"auth\":\"wpapskwpa2psk\",\"ssid\":\"ChinaNet-GRa6\",\"mode\":\"5G\",\"encrypt\":\"aes\"},{\"apidx\":7,\"key\":\"87654321\",\"auth\":\"wpapskwpa2psk\",\"ssid\":\"fhpre-TXCA\",\"mode\":\"5G\",\"encrypt\":\"aes\"}]}]}}";
	//char buffer[2048]="{\"status\":{\"wifi\":[{\"radio\":{\"channel\":6,\"mode\":\"2.4G\"}},{\"radio\":{\"channel\":6,\"mode\":\"5G\"}}]},\"mac\":\"60B617F6BF44\",\"set\":{\"wifi\":[{\"radio\":{\"channel\":0,\"mode\":\"2.4G\"},\"ap\":[{\"key\":\"\",\"apidx\":0,\"auth\":\"open\",\"ssid\":\"ChinaNet-TXCA-ame-4\",\"encrypt\":\"aes\"},{\"key\":\"\",\"apidx\":1,\"auth\":\"open\",\"ssid\":\"iTV-TXCA\",\"encrypt\":\"aes\"},{\"key\":\"87654321\",\"apidx\":2,\"auth\":\"wpapskwpa2psk\",\"ssid\":\"ChinaNet-GRa6\",\"encrypt\":\"aes\"},{\"key\":\"87654321\",\"apidx\":3,\"auth\":\"wpapskwpa2psk\",\"ssid\":\"fhpre-TXCA\",\"encrypt\":\"aes\"}]},{\"radio\":{\"channel\":0,\"mode\":\"5G\"},\"ap\":[{\"apidx\":4,\"key\":\"\",\"auth\":\"open\",\"ssid\":\"ChinaNet-TXCA-ame-4\",\"mode\":\"5G\",\"encrypt\":\"aes\"},{\"apidx\":5,\"key\":\"\",\"auth\":\"open\",\"ssid\":\"iTV-TXCA\",\"mode\":\"5G\",\"encrypt\":\"aes\"},{\"apidx\":6,\"key\":\"87654321\",\"auth\":\"wpapskwpa2psk\",\"ssid\":\"ChinaNet-GRa6\",\"mode\":\"5G\",\"encrypt\":\"aes\"},{\"apidx\":7,\"key\":\"87654321\",\"auth\":\"wpapskwpa2psk\",\"ssid\":\"fhpre-TXCA\",\"mode\":\"5G\",\"encrypt\":\"aes\"}]}]}}";
	json_error_t err;
	json = json_loads(buffer, 0, &err);
	xielin(buffer);
	///////
#endif
	int is_set_wifi_switch=0;
	int is_set_wifi_timer=0;
	j_status = json_object_get(json, "status");
	if (j_status) {
		json_t * j_wifis;

		j_wifis = json_object_get(j_status, "wifi");
		if (j_wifis) {
			int index = 0;
			json_t * j_wifi;

			json_array_foreach(j_wifis, index, j_wifi){
				json_t * j_radio;
				char * mode;
				json_t * j_channel;
				int channel;
				j_radio = json_object_get(j_wifi, "radio"); 
				if(!j_radio){
					ERROR("no 'radio'"); 
					return -1;
				};
				mode = json_str_get(j_radio, "mode");
				j_channel = json_object_get(j_radio, "channel");
				if(!j_channel){ 
					ERROR("no 'channel'"); 
					return -1;
				};
				channel = json_integer_value(j_channel);
			}
		}
	}
	
	//璁剧疆淇℃伅
	j_set = json_object_get(json, "set");
	if (j_set) {
		json_t * j_wifis;
		json_t * j_wifiswitch;
		json_t * j_ledswitch;
		json_t * j_wifitimers;

		j_wifis = json_object_get(j_set, "wifi");
		if (j_wifis) {
			int index;
			json_t * j_wifi;
			int freq=FREQ_NONE;
			index = 0;
			json_array_foreach(j_wifis, index, j_wifi){
				json_t * j_radio;
				char * mode;
				json_t * j_channel;
				int channel;

				json_t * j_aps;
				json_t * j_ap;
				int _index;
				
				//radio
				j_radio = json_object_get(j_wifi, "radio"); 
				
				if(!j_radio) { 
					ERROR("no 'radio'"); 
					return -1;
				};
				mode = json_str_get(j_radio, "mode");
				j_channel = json_object_get(j_radio, "channel"); 
				if(!j_channel){
					ERROR("no 'channel'"); 
					return -1;
				};
				channel = json_integer_value(j_channel);
				
				if(!strcmp(mode,"2.4G")){
					freq=FREQ_24G;
				}
				else if(!strcmp(mode,"5G")){
					freq=FREQ_5G;
				}
				//ap
 				j_aps = json_object_get(j_wifi, "ap"); 
				if(!j_aps) {
					print_json(j_wifi);
					ERROR("no 'ap'"); 
					return -1;
				};
 				_index = 0;
				json_array_foreach(j_aps, _index, j_ap){
 					json_t * j_apidx;
					int apidx;
					char * ssid;
					char * key;
					char * auth;
					char * encrypt;
					char *enable_str = NULL;
					int enable = 1;
					
					j_apidx = json_object_get(j_ap, "apidx");
 					if(!j_apidx){
 						ERROR("no 'apidx'"); return -1;
					};
					apidx = json_integer_value(j_apidx);
					DEBUG("apidx=%d",apidx);

 					ssid = json_str_get(j_ap, "ssid");
					if(!ssid){ERROR("no 'ssid'");continue;}
					
 					key = json_str_get(j_ap, "key");
 					if(!key){ERROR("no 'key'");continue;}
 					
					auth = json_str_get(j_ap, "auth");
					if(!auth){ERROR("no 'auth'");continue;}
					
					encrypt = json_str_get(j_ap, "encrypt");
					if(!encrypt){ERROR("no 'encrypt'");continue;}

					enable_str = json_str_get(j_ap, "enable");
					if (enable_str) {
						if(!strcmp(enable_str,"no")) {
							enable = 0;
						}
					}
					
					if(apidx==0){
						DEBUG("\nbegin set ap freq=%d,apidx=%d,apidx=%d,ssid=%s,key=%s,auth=%s,encrypt=%s\n",
							freq,apidx,apidx,ssid,key,auth,encrypt
						);
						set_ap(ssid,key,auth,encrypt,freq);
						set_wifi_swich(enable, freq);
					}
					else if(apidx==1){

						DEBUG("\nbegin set vap freq=%d,apidx=%d,apidx=%d,ssid=%s,key=%s,auth=%s,encrypt=%s\n",
							freq,apidx,apidx,ssid,key,auth,encrypt
						);
						set_vap(ssid,freq);
						set_wifi_vap_swich(enable, freq);
					}
 				}
			}
		}

 		j_wifiswitch = json_object_get(j_set, "wifiswitch");
		//wifi 是否开启
		if (j_wifiswitch) {
			char * status = json_str_get(j_wifiswitch, "status");
			if(status){
				is_set_wifi_switch=1;
				if (!strcmp(status, "ON")){
					DEBUG("\n cmd:open wifi\n");
					set_wifi_swich(1,FREQ_24G);
					set_wifi_swich(1,FREQ_5G);
				}else if (!strcmp(status,"OFF")){
					DEBUG("\n cmd:close wifi\n");
					set_wifi_swich(0,FREQ_24G);
					set_wifi_swich(0,FREQ_5G);
				}
			}
		}
 		j_ledswitch = json_object_get(j_set, "ledswitch");
		if (j_ledswitch) {
			char * status = json_str_get(j_ledswitch, "status");
			if(status){
				if (!strcmp(status, "ON")) {
					DEBUG("LED ON");
					telcom_set_led(1);
				}else{
					DEBUG("LED OFF");
					telcom_set_led(0);
				}
			}
		}
 		j_wifitimers = json_object_get(j_set, "wifitimer");
		char begin_time[32]="0000000";//ri,1,2
		char end_time[32]={0};
		char day_str[32]={0};
		if (j_wifitimers) {
			int index;
			json_t * j_wifitimer;
			is_set_wifi_timer=1;
			index = 0;
			json_array_foreach(j_wifitimers, index, j_wifitimer){
				char * day; /*FIXME: integer or string ?*/
				char * time;
				char * enable; /*FIXME: integer or sting ? */
	
				day = json_str_get(j_wifitimer, "weekday");
				if(!day){ERROR("no 'day'") ;continue;}
				
				time = json_str_get(j_wifitimer, "time");
				if(!time){ERROR("no 'time'");continue;}
				
				enable = json_str_get(j_wifitimer, "enable");
				if(!enable){ERROR("no 'enable'");continue;}
				
				if(!strcmp(day,"1")){
					day_str[1]='1';
				}else if(!strcmp(day,"2")){
					day_str[2]='1';
				}else if(!strcmp(day,"3")){
					day_str[3]='1';
				}else if(!strcmp(day,"4")){
					day_str[4]='1';
				}else if(!strcmp(day,"5")){
					day_str[5]='1';
				}else if(!strcmp(day,"6")){
					day_str[6]='1';
				}else if(!strcmp(day,"7")){
					day_str[0]='1';
				}
				
				if(!strcmp(enable,"1")){
					strcpy(begin_time,time);
				}else{
					strcpy(end_time,time);
				}
			}
			//获取所有的时间数
			DEBUG("day_str=%s,begin_time=%s,end_time=%s",day_str,begin_time,end_time);
			set_wifi_time(day_str,begin_time,end_time,FREQ_24G);
			set_wifi_time(day_str,begin_time,end_time,FREQ_5G);
		}
	}

	if(is_set_wifi_switch){
		if(is_set_wifi_timer){
			set_wifitimer_switch(1,FREQ_24G);
			set_wifitimer_switch(1,FREQ_5G);
		}else{
			set_wifitimer_switch(0,FREQ_24G);
			set_wifitimer_switch(0,FREQ_5G);
		}
	}
	
 	//鏌ヨ淇℃伅
	j_get = json_object_get(json, "get_status");
	int ret_wifi=0;
	int ret_wifiswitch=0;
	int ret_ledswitch=0;
	int ret_wifitimer=0;
	if (j_get) {
		int index = 0;
		json_t * j_name;
		json_array_foreach(j_get, index, j_name){
			char * name = json_str_get(j_name, "name");
			if(name){
				if (!strcmp(name, "wifi"))	 {
					ret_wifi=1;
				}
				else if (!strcmp(name, "wifiswitch")) {
					ret_wifiswitch=1;
				}
				else if (!strcmp(name, "ledswitch")) {
					ret_ledswitch=1;
				}
				else if (!strcmp(name, "wifitimer")) {
					ret_wifitimer=1;
				}
				else{
					ERROR("unknown name:%s\n", name);
				}
			}
		}
	}
	
	if (!j_get){ //闈炴煡璇俊鎭繑鍥瀉ck
		if(c){
			return _client_send_ack(c, seq, 1);
		}
	}
	else{
			json_t * response;
	
			json_t * j_status;
			json_t * j_wifis;
			json_t * j_wifi;
			json_t * j_aps;
			json_t * j_ap;
	
			json_t * j_wifiswitch;
			json_t * j_ledswitch;
			json_t * j_wifitimers;
	
			response = json_header("status", seq/*ack*/, c->mac); if(!response) return -1;
			//status
			j_status = json_object(); if(!j_status) goto json_error;
			json_object_set(response, "status", j_status);
	
			if(ret_wifi){
				json_object_set(response, "wifi", _wifi2json_array());
			}
	
			if(ret_wifiswitch){
#if CFG_ENABLE_WIAIR_API
				json_object_set(response, "wifiswitch", _wifiswitch2json(FREQ_24G));
#endif
			}
			if(ret_ledswitch){
#if CFG_ENABLE_WIAIR_API
				json_object_set(response, "ledswitch", _ledswitch2json());
#endif
			}
			if(ret_wifitimer){
				json_object_set(response, "wifitimer", _wifitimer2json(FREQ_24G));
			}
			
			if(c){
				return client_queue_json(c, response, 0, 1);
			}
	
	json_error:
			json_decref(response);
			return -1;
		}
}

static void timer_loop(struct client * c)
{
	struct client_timer * t;

	list_for_each_entry(t, &c->timers, list){
		if (jiffies >= t->last) {
			if(t->type == C_T_TYPE_MAINLINE){
				if (t->status == C_STATUS_KEYREQ_SEND) {
					DEBUG("T9 timeout\n");
					goto timeout;
				}
				else if (t->status == C_STATUS_ECDH_SEND) {
					DEBUG("T2 timeout\n");
					goto timeout;
				}
				else if(t->status == C_STATUS_KEEPALIVE){
					DEBUG("T6 timeout, send keepalive\n");
					//xielin_client_send_keepalive(c);
					_client_send_keepalive(c);
				}
				else if(t->status == C_STATUS_KEEPALIVE_SEND){
					DEBUG("T4 timeout\n");
					goto timeout;
				}
			}
			else if(t->type == C_T_TYPE_REG_BRANCH){
				if (t->status == C_STATUS_REG_SEND) {
					DEBUG("T3 timeout\n");
					//xielingoto timeout;
					goto timeout;
				}
			}
			else if(t->type == C_T_TYPE_DEV_BRANCH){
				if (t->status == C_STATUS_DEVCHANGE_INIT) {
					DEBUG("T7 timeout\n");
					_client_send_dev_report(c);
				}
				if (t->status == C_STATUS_DEVCHANGE_SEND) {
					DEBUG("T5 timeout\n");
					goto timeout;
				}
			}
			else
				ERROR("wrong type:%d\n", t->type);
		}
	}

	return;

timeout:
	client_del(c);
	c->main_line->status = C_STATUS_INIT;
	DEBUG("-> C_STATUS_INIT\n");
	return;
}

static int _client_deal_recinfo(struct client * c)
{
	json_t * json;
	struct buffer * b;
	struct buffer * p;

	list_for_each_entry_safe(b, p, &c->rcv_queue, list){
		json_error_t err;
		char * type;
		int seq;
		char * mac;
		json_t * j_data;

#if CFG_ENABLE_AES
		if (c->main_line->status >= C_STATUS_KEEPALIVE) {
			char data[BUFFER_SIZE];
			memset(data, 0, sizeof(data));
			_aes_decrypt(c->ecdh.secret, 128, b->data, b->len, data);
			DEBUG("data='%s'\n", data);
			json = json_loads(data, 0, &err);
		}
		else{
			DEBUG("data='%s'\n", b->data);
			json = json_loads(b->data, 0, &err);
		}
#else
		DEBUG("data='%s'\n", b->data);
	
		json = json_loads(b->data, 0, &err);

#endif /*CFG_ENABLE_AES*/
		if (!json) {
			ERROR("invalid json data\n");
			return -1;
		}

		if (json_parse_head(json, &type, &seq, &mac)){
			DEBUG("\n");
			goto error;
		}

		j_data = NULL;

		if (!strcmp(type, "keyngack")) {
			j_data = json_object_get(json, "keymode");
			if (_client_recv_keyreq_ack(c, json, seq, j_data)){
				DEBUG("\n");
				goto error;
			}
		}
		else if (!strcmp(type, "ecdh")) {
			j_data = json_object_get(json, "data");
			if (_client_recv_ecdh(c, json, seq, j_data)){
				DEBUG("\n");
				goto error;
			}
		}
		else if (!strcmp(type, "ack")) {
			if (_client_recv_ack(c, json, seq, j_data)){
				DEBUG("\n");
				goto error;
			}
		}
		else if (!strcmp(type, "cfg")) {
			if (_client_recv_cfg(c, json, seq, j_data)){
				DEBUG("\n");
				//goto error;
			}else{
				to_bridge();
			}
		}
		else if (!strcmp(type, "dh")) {
			j_data = json_object_get(json, "data");
			if (_client_recv_dh_ack(c, json, seq, j_data)){
				DEBUG("\n");
				goto error;
			}
		}

		json_decref(json);
		list_del(&b->list);
		buffer_free(b);
	}

	return 0;

error:
	json_decref(json);
	list_del(&b->list);
	buffer_free(b);
	return -1;
}

static int is_bridge()
{
	struct iface_conf config;
 	int uid = 1;
	
	memset(&config, 0x0, sizeof(config));
	if (mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &config, sizeof(config))) {
		return 0;
	}
	return config.isbridge;
}

int to_nat()
{
	system("brctl delif br-lan eth0.2");
}

int close_dhcp()
{
	system("killall -kill dnsmasq");
	
	//return 1;
	struct dhcp_conf config;
	//close dhcp
	if (mu_msg(DNSMASQ_DHCP_SHOW, NULL, 0, &config, sizeof(config))){
		return 1;
	}
	if(config.enable){
		config.enable=0;
		mu_msg(DNSMASQ_DHCP_SET, &config, sizeof(config), NULL, 0);
	}
	return 0;
}
int to_bridge()
{
	if(is_bridge()){
		return 0;
	}
	//close dhcp
	close_dhcp();
	if (mu_msg(IF_MOD_WAN_BRIDAGE, NULL, 0, NULL, 0)) {
		return 1;
	}
	return 0;
}

int nlk_telcomc_call(struct nlk_msg_handler *nlh)
{
	char buf[4096] = {0,};
	struct nlk_msg_comm *comm;
	struct nlk_host *host = NULL;
	struct nlk_msg_l7 *app = NULL;
	struct ua_msg *ua = NULL;
	struct nlk_dhcp_msg *dhcp = NULL;
	struct nlk_alone_config *alone = NULL;

	if (nlk_event_recv(nlh, buf, sizeof(buf)) <= 0)
		return -1;
	comm = (struct nlk_msg_comm *)buf;

	switch (comm->gid) {
	case NLKMSG_GRP_HOST:
		break;
	default:
		break;
	}
	return 0;
}

static int nlk_call(struct nlk_msg_handler *nlh,struct client * c)
{
	char type[32] = {0};
	char name[32] = {0};
	char vender[32];
	char macstr[32] = {0};
	char buf[1024] = {0,};
	struct host_info info;
	struct nlk_host *host;
	struct nlk_msg_comm *comm;
	struct nlk_sys_msg *sys;
	struct ua_msg *ua;
	struct nlk_sys_msg *if_msg = NULL;
	struct nlk_msg_l7 *app = NULL;

	if (nlk_event_recv(nlh, buf, sizeof(buf)) <= 0){
		return -1;
	}
	comm = (struct nlk_msg_comm *)buf;
	switch (comm->gid) {
	case NLKMSG_GRP_HOST:
		DEBUG("\n have change \n");
		host = (struct nlk_host *)buf;
		if (comm->action == NLK_ACTION_DEL 
			|| comm->action == NLK_ACTION_ADD) {
			
			_client_send_dev_report(c);
//			ali_host_timer = (sys_uptime() - 8);
//	ALI_DBG(MAC_FORMART", %d,%d\n",MAC_SPLIT(host->mac), ali_host_timer, comm->action);
		}
		break;
	default:
		break;
	}
	return 0;
}

int network_is_pppoe()
{
	int uid = 1;	
	static struct iface_conf config;
	mu_msg(IF_MOD_PARAM_SHOW, &uid, sizeof(uid), &config, sizeof(config));
	if (config.mode == MODE_PPPOE) {
		DEBUG("network is pppoe.\n");
		return 1;
	} else {
		return 0;
	}
}
int web_disable_sync()
{
	char *value = NULL;
	int disable = 0;
	value = uci_getval("telcom", "sync", "disabled");
	if (value) {
		disable = atoi(value);
		free(value);
	}
	if (disable) {
		DEBUG("web disable sync.\n");
	}
	return disable;
}
int main(int argc, char * argv[])
{
	fd_set fds;

	int status, grp, max_fd, ipc_fd;
	struct nlk_msg_handler nlh;
	struct timeval tv;

	fd_set read_fds;
	fd_set write_fds;
	struct timeval tv_host;
	grp = 1 << (NLKMSG_GRP_HOST - 1);
	nlk_event_init(&nlh, grp);
	
	char * mac;
	struct client * c=NULL;
	u_int32_t gw, last_gw = 0;

	c = client_new(_client_init);
	if (!c){
		return -1;
	}

	mac = get_mac("eth0");
	if (!mac){
		client_del(c);
		client_free(c);
		return -1;
	}
	memcpy(c->mac, mac, 6);

	while(1){
		int ret;
		int len;

		if (network_is_pppoe() || web_disable_sync()) {
			if (c->fd != -1) {
				client_del(c);
				c->main_line->status = C_STATUS_INIT;
			}
			sleep(3);
			continue;
		}
	
		if (c->main_line->status == C_STATUS_INIT) {
			c->fd = client_socket_init(); 
			if (c->fd != -1){
				c->main_line->status = C_STATUS_WAIT_GW;
				DEBUG("-> C_STATUS_WAIT_GW\n");
			}
		}

		if (c->main_line->status == C_STATUS_WAIT_GW) {
			gw = _get_gw();
			if (gw != last_gw){
				if(is_bridge()){
					//to_nat();
				}
				last_gw = gw;
			}
			if (gw) {
				if (_client_connect(c, gw) == 0) {
					_client_send_keyreq(c);
				}
				else{
					//DEBUG("------connect time out!\n");
					sleep(C_T1);
					goto error;
				}
			}
			else{
				sleep(C_T8);
			}
		}

		if (c->main_line->status > C_STATUS_WAIT_GW) {
			//down is for get host change info
			FD_ZERO(&fds);
			IGD_FD_SET(nlh.sock_fd, &fds);
			//tv.tv_sec = 1;
			tv_host.tv_sec = 0;
			tv_host.tv_usec = 0;
			if (select(max_fd + 1, &fds, NULL, NULL, &tv_host) < 0) {
				//if (errno == EINTR || errno == EAGAIN){
					//continue;
					//goto COMHERE;
				//}
			}
			if (FD_ISSET(nlh.sock_fd, &fds)){
				if (c->main_line->status > C_STATUS_WAIT_GW) {		
					//nlk_call(&nlh,c);
				}
			}
	COMHERE:
			//continue;
			///up is for get host change info
		
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			//read_fds
			FD_ZERO(&read_fds);
			FD_SET(c->fd, &read_fds);

			//write_fds
			FD_ZERO(&write_fds);
			if (!list_empty(&c->snd_queue))
				FD_SET(c->fd, &write_fds);

			ret = select(c->fd + 1, &read_fds, &write_fds, NULL, &tv);
			if (ret == -1) {
				if (errno == EINTR || errno == EAGAIN){
					continue;
				}
			}
			else if (ret == 0) { /* timer */
				jiffies ++;
				timer_loop(c);
			}
			else{
				//read_fds
				if (FD_ISSET(c->fd, &read_fds)){
					len = client_recv(c);
					if (len == -1){
						DEBUG("\n");
						goto error;
					}
				}
				//do recved buffer
				if (_client_deal_recinfo(c)){
					DEBUG("\n");
					goto error;
				}

				//check write_fds
				if (FD_ISSET(c->fd, &write_fds)){
					len = client_send(c);
					if (len == -1){
						DEBUG("\n");
						goto error;
					}
				}

				continue;
error:
				client_del(c);
				c->main_line->status = C_STATUS_INIT;
				DEBUG("-> C_STATUS_INIT\n");
			}
		}
	}

	return 0;
}
