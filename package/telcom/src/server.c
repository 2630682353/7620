#include <jansson.h>

#include "lib.h"
#include "cydh.h"

#define SC_T1	5
#define SC_T2	5
#define SC_T3	5
#define SC_T4	10
#define SC_T5	5
#define SC_T7	5



//client状态变迁主线
#define SC_T_TYPE_MAINLINE 1
enum{
	SC_STATUS_INIT,
	SC_STATUS_WAIT_KEYREQ,
	SC_STATUS_WAIT_ECDH,
	SC_STATUS_WAIT_DH,
	SC_STATUS_WAIT_REG,
	SC_STATUS_KEEPALIVE
};


//client状态变迁配置检测分支
#define SC_T_TYPE_CFG_BRANCH 2
enum{
	SC_STATUS_CFG_INIT,
	SC_STATUS_CFG_SEND
};

u_int64_t jiffies = 0;
static int max_fd = 0;
static struct server server;

static int _client_send_keyreq_ack(struct client * c, int seq);
static int _client_send_ecdh(struct client * c);
static int _client_send_dh(struct client * c,char *key);

static void _client_init(struct client * c)
{
	client_timer_init(c->main_line, SC_T_TYPE_MAINLINE, SC_STATUS_INIT, jiffies);
	list_add_tail(&c->main_line->list, &c->timers);
	DEBUG_S("client(%p): -> SC_STATUS_INIT\n", c);
}

static void timer_loop(void)
{
	struct client * c;
	struct client * p;

	list_for_each_entry_safe(c, p, &server.clients, list){
		struct client_timer * t;
		struct client_timer * tp;
		list_for_each_entry_safe(t, tp, &c->timers, list){
			if (jiffies >= t->last) {
				if (t->type == SC_T_TYPE_MAINLINE) {
					if (t->status == SC_STATUS_WAIT_KEYREQ) {
						DEBUG("T7 timeout\n");
						client_del(c); client_free(c);
					}
					if (t->status == SC_STATUS_WAIT_ECDH) {
						DEBUG("T1 timeout\n");
						client_del(c); client_free(c);
					}
					if (t->status == SC_STATUS_WAIT_REG) {
						DEBUG("T2 timeout\n");
						client_del(c); client_free(c);
					}
				}
				else if (t->type == SC_T_TYPE_CFG_BRANCH) {
					if (t->status == SC_STATUS_CFG_SEND) {
						DEBUG("T5 timeout\n");
						client_del(c); client_free(c);
					}
				}
				else
					ERROR_S("wrong type:%d\n", t->type);

			}
		}
	}
}

static void server_accept()
{
	struct client * c;
	int fd;
	struct sockaddr_in addr;
	int len = sizeof(addr);
	
	fd = accept(server.fd, (struct sockaddr *)&addr, &len);
	if (fd < 0) {
		ERROR_S("accept failed\n");
		return;
	}

	c = client_new(_client_init);
	if (!c)
		close(fd);

	c->fd = fd;
	c->main_line->last = jiffies + SC_T7;
	c->main_line->status = SC_STATUS_WAIT_KEYREQ;
	DEBUG("client(%p): -> SC_STATUS_WAIT_KEYREQ\n", c);

	list_add_tail(&c->list, &server.clients);
}

static void _update_max_fd(int fd)
{
	if (fd > max_fd)
		max_fd = fd;
}

static void _client_read_fd_set(struct client * c, void * data)
{
	fd_set * fds = (fd_set *)data;
	FD_SET(c->fd, fds); _update_max_fd(c->fd);
}

static void _client_write_fd_set(struct client * c, void * data)
{
	fd_set * fds = (fd_set *)data;

	if (!list_empty(&c->snd_queue))
		FD_SET(c->fd, fds); _update_max_fd(c->fd);
}

static void _client_read_fd_check(struct client * c, void * data)
{
	int len;
	fd_set * fds = (fd_set *)data;

	if (FD_ISSET(c->fd, fds)){
		len = client_recv_server(c);
		if (len == -1){
			client_del(c); client_free(c);
		}
	}
}

static void _client_write_fd_check(struct client * c, void * data)
{
	int len;
	char * d;
	struct fs_task * task;

	fd_set * fds = (fd_set *)data;
	if (FD_ISSET(c->fd, fds)){
		len = client_send_server(c);
		if (len == -1){
			client_del(c); client_free(c);
		}
	}
}

static int _client_recv_keyreq(struct client * c, json_t * json, int seq, json_t * j_data)
{
	int index;
	json_t * j_kmode;

	if (!j_data){
		ERROR_S("no 'data'\n");
		return -1;
	}

	if (c->main_line->status != SC_STATUS_WAIT_KEYREQ) {
		ERROR_S("wrong status, should be SC_STATUS_WAIT_KEYREQ\n");
		return -1;
	}

	index = 0;
	json_array_foreach(j_data, index, j_kmode){
		char * mode = json_str_get(j_kmode, "keymode");
	}

	return _client_send_keyreq_ack(c, seq);
}

static int _client_recv_ecdh(struct client * c, json_t * json, int seq, json_t * j_data)
{
	char * ecdh_key;
	json_t * j_ecdh_key;

	if (!j_data){
		ERROR_S("no 'dat'\n");
		return -1;
	}

	j_ecdh_key = json_object_get(j_data, "ecdh_key");
	if (!j_ecdh_key) {
		ERROR_S("no 'ecdh_key'\n");
		return -1;
	}
	ecdh_key = json_string_value(j_ecdh_key);
	string2oct(ecdh_key, c->ecdh.peer_pub_key, ECDH_SIZE + 1);

	if (c->main_line->status != SC_STATUS_WAIT_ECDH) {
		ERROR_S("wrong status, should be SC_STATUS_WAIT_ECDH\n");
		return -1;
	}
	//FIXME: no ack ?
	//_client_send_ack(c, seq, 0);
	return _client_send_ecdh(c);
}

static int _client_recv_dh(struct client * c, json_t * json, int seq, json_t * j_data)
{
	char pub_key_str[1024]={0};
	char * char_key;
	char * char_p;
	char * char_g;	
	json_t * key;
	json_t * p;
	json_t * g;

	if (!j_data){
		ERROR_S("no 'dat'\n");
		return -1;
	}

	key = json_object_get(j_data, "dh_key");
	if (!key) {
		ERROR_S("no 'dh_key'\n");
		return -1;
	}

	p = json_object_get(j_data, "dh_p");
	if (!p) {
		ERROR_S("no 'dh_p'\n");
		return -1;
	}

	g = json_object_get(j_data, "dh_g");
	if (!g) {
		ERROR_S("no 'dh_g'\n");
		return -1;
	}
	
	char_key = json_string_value(key);
	char_p = json_string_value(p);
	char_g = json_string_value(g);

	//���key
	unsigned char public_key[1024];
	int p_bin_len, g_bin_len, pub_bin_len, share_bin_len;
	uint8_t g_int=2;
	uint8_t p_bin[BYTE_LEN_OF_PRIME * 2], g_bin[BYTE_LEN_OF_PRIME * 2];
	BIGNUM *p_bn = NULL, *g_bn = NULL;
	BIGNUM *pub_bn = NULL, *share_bn = NULL;
	uint8_t pub_bin[BYTE_LEN_OF_PRIME * 2], share_bin[BYTE_LEN_OF_PRIME * 2], str[1024];
	DH *k1 = NULL;

	p_bin_len = BASE64_Decode(char_p, strlen(char_p), p_bin);
	if(NULL == (p_bn = BN_bin2bn(p_bin, p_bin_len, NULL))){ 
		ERROR_S(" change p_bn error\n");
		goto ERROR;
		//handleErrors(__FUNCTION__, __LINE__)
	};
	//disp_bn(p_bn, "-----pair's P:\n");
	
	g_bin_len = BASE64_Decode(char_g, strlen(char_g), g_bin);
	if(NULL == (g_bn = BN_bin2bn(g_bin, g_bin_len, NULL))){
		ERROR_S(" change g_bn error\n");
		goto ERROR;
	}
	//disp_bn(g_bn, "-----pair's G:\n");
	
	pub_bin_len = BASE64_Decode(char_key, strlen(char_key), pub_bin);
	if(NULL == (pub_bn = BN_bin2bn(pub_bin, pub_bin_len, NULL))){
		ERROR_S(" change pub_bn error\n");
		goto ERROR;
	}
	//disp_bn(pub_bn, "-----pair's public key:\n");
	if (c->main_line->status != SC_STATUS_WAIT_DH) {
		ERROR_S("wrong status, should be SC_STATUS_WAIT_ECDH\n");
		return -1;
	}

	if(dh_new_key(&k1, BIT_LEN_OF_PRIME, p_bn, g_int) < 0){
		ERROR_S(" dh_new_key error\n");
		goto ERROR;
	}

	//disp_bn(k1->p, "-----P:\n");
	//disp_bn(k1->g, "-----G:\n");
	//disp_bn(k1->pub_key, "-----Public key:\n");

	if((share_bin_len = get_share(pub_bn, k1, share_bin, sizeof(share_bin))) < 0) {
		ERROR_S(" get_share error\n");
		goto ERROR;
	}

	memcpy(c->ecdh.secret,share_bin,share_bin_len);


	/*
	if(NULL == (share_bn = BN_bin2bn(share_bin, share_bin_len, NULL))){
		ERROR_S(" change share_bn error\n");
		goto ERROR;
	}*/

	/*
	disp_bn(c->dh.k1->p, "-----P2:\n");
	disp_bn(c->dh.k1->g, "-----G2:\n");
	disp_bn(c->dh.k1->pub_key, "-----Public key2:\n");*/

	//json_object_set(j_data, "dh_key", json_string(pub_key_str));

	disp_bn(share_bn, "-----private key\n");

	//FIXME: no ack ?
	//_client_send_ack(c, seq, 0);

	int ret0=cybn_to_base64(k1->pub_key,pub_key_str);if(ret0){
		ERROR_S(" cybn_to_base64 error\n");
		goto ERROR;
	}	
	DEBUG_S("server:public_key=%s",pub_key_str);
	
	return _client_send_dh(c,pub_key_str);
ERROR:
//	DEBUG_S("exit");
//	exit(0);

	return -1;

}

static int _client_send_keyreq_ack(struct client * c, int seq)
{
	int ret;
	json_t * json;
	int is_ecdh = 0;

	json = json_header("keyngack", seq/*ack*/, c->mac);
	if (!json)
		return -1;

	if (is_ecdh) {
		json_object_set(json, "keymode", json_string("ecdh"));

		ret = client_queue_json(c, json, 0, 0);
		if (ret == 0) {
			c->main_line->status = SC_STATUS_WAIT_ECDH;
			c->main_line->last = jiffies + SC_T1;
			DEBUG("client(%p): -> SC_STATUS_WAIT_ECDH\n", c);
		}

		return ret;
	}
	else{
		json_object_set(json, "keymode", json_string("dh"));

		/* xielin
		string2oct(ECDH_STATIC_SECRET, c->ecdh.secret, ECDH_SIZE);
		*/
		ret = client_queue_json(c, json, 0, 0);
		if (ret == 0) {
			c->main_line->status = SC_STATUS_WAIT_DH;
			c->main_line->last = jiffies + SC_T2;
			DEBUG("client(%p): -> SC_STATUS_WAIT_REG\n", c);
		}

		return ret;
	}
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
		ERROR_S("json_object failed\n");
		json_decref(json);
		return -1;
	}

	c->ecdh.key = ecdh_pub_key(c->ecdh.my_pub_key);
	json_object_set(data,"ecdh_key", json_string(oct2string(c->ecdh.my_pub_key, ECDH_SIZE + 1)));
	json_object_set(json, "data", data);

	ecdh_shared_secret(c->ecdh.key, c->ecdh.peer_pub_key, c->ecdh.secret);

	ret = client_queue_json(c, json, 1, 0); //FIXME: wait ack ?

	if (ret == 0) {
		c->main_line->status = SC_STATUS_WAIT_REG;
		c->main_line->last = jiffies + SC_T2;
		DEBUG("client(%p): -> SC_STATUS_WAIT_REG\n", c);
	}

	return ret;
}


static int _client_send_dh(struct client * c,char *key)
{

	int ret;
	json_t * json;
	json_t * data;

	json = json_header("dh", c->seq++, c->mac);
	if (!json)
		return -1;

	data = json_object();
	if (!data) {
		ERROR_S("json_object failed\n");
		json_decref(json);
		return -1;
	}
	json_object_set(data,"dh_key", json_string(key));
	json_object_set(json, "data", data);


	//ecdh_shared_secret(c->ecdh.key, c->ecdh.peer_pub_key, c->ecdh.secret);

	/*
	char * b;

	b = json_dumps(json, 0);
	if (!b) {
		ERROR("json dump failed\n");
		return -1;
	}
	*/
	ret = client_queue_json(c, json, 1, 0); //FIXME: wait ack ?

	if (ret == 0) {
		c->main_line->status = SC_STATUS_WAIT_REG;
		c->main_line->last = jiffies + SC_T2;
		DEBUG("client(%p): -> SC_STATUS_WAIT_REG\n", c);
	}

	return ret;
}



static int _client_recv_dev_reg(struct client * c, json_t * json, int seq, json_t * j_data)
{
	char * vendor;
	char * model;
	char * url;
	char * wireless;
	json_t * j_vendor;
	json_t * j_model;
	json_t * j_url;
	json_t * j_wireless;

	if (!j_data){
		ERROR_S("no 'data'\n");
		return -1;
	}
	DEBUG_S("rec reg info_xielin");
	//vendor
	j_vendor = json_object_get(j_data, "vendor");
	if (!j_vendor) {
		ERROR_S("no 'vendor'\n");
		return -1;
	}
	vendor = json_string_value(j_vendor);
	//model
	j_model = json_object_get(j_data, "model");
	if (!j_model) {
		ERROR_S("no 'model'\n");
		return -1;
	}
	model = json_string_value(j_model);
	//url
	j_url = json_object_get(j_data, "url");
	if (!j_url) {
		ERROR_S("no 'url'\n");
		return -1;
	}
	url = json_string_value(j_url);
	//wireless
	j_wireless = json_object_get(j_data, "wireless");
	if (!j_wireless) {
		ERROR_S("no 'wireless'\n");
		return -1;
	}
	wireless = json_string_value(j_wireless);

	c->main_line->status = SC_STATUS_KEEPALIVE;
	c->main_line->last = jiffies + SC_T4;
	DEBUG_S("server(%p): -> SC_STATUS_KEEPALIVE\n");

	return _client_send_ack(c, seq, 1);
}

static int _client_recv_keepalive(struct client * c, json_t * json, int seq, json_t * j_data)
{
	if (c->main_line->status != SC_STATUS_KEEPALIVE) {
		ERROR_S("wrong status, should be SC_STATUS_KEEPALIVE\n");
		return -1;
	}

	c->main_line->last = jiffies +  SC_T4;

	return _client_send_ack(c, seq, 1);
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
			if(!strcmp(_type, "cfg")){
				t = client_timer_find_by_type(c, SC_T_TYPE_CFG_BRANCH);
				if(t){
					if (t->status == SC_STATUS_CFG_SEND) {
						t->status = SC_STATUS_CFG_INIT;
						t->last = jiffies + SC_T5;
						DEBUG_S("client(%p): -> SC_STATUS_CFG_INIT\n", c);
					}
					else{
						ERROR_S("wrong status:%d\n", t->status);
					}
				}
			}
			else{
				ERROR_S("type:%s\n", _type);
			}
			list_del(&b->list);
			buffer_free(b);
		}
	}

	return 0;
}

static int _client_recv_cfg(struct client *c, json_t * json, int seq, json_t * j_data)
{
	return _client_send_ack(c, seq, 1);
}

static int _client_recv_dev_report(struct client *c, json_t * json, int seq, json_t * j_data)
{
	return _client_send_ack(c, seq, 1);
}

static void _client_do(struct client * c, void * data)
{
	struct buffer * b;
	struct buffer * p;
	json_t * json;

	list_for_each_entry_safe(b, p, &c->rcv_queue, list){
		json_error_t err;

#if CFG_ENABLE_AES
		if (c->main_line->status >= SC_STATUS_WAIT_REG) {
			DEBUG_S("now begin_jieji");
			char data[BUFFER_SIZE];
			memset(data, 0, sizeof(data));
			_aes_decrypt(c->ecdh.secret, 128, b->data, b->len, data);
			DEBUG_S("data='%s'\n", data);
			json = json_loads(data, 0, &err);
		}
		else{
			DEBUG_S("data='%s'\n", b->data);
			json = json_loads(b->data, 0, &err);
		}

#else
		DEBUG_S("data='%s'\n", b->data);
		json = json_loads(b->data, 0, &err);
#endif /*CFG_ENABLE_AES*/
		if (json) {
			char * type;
			int seq;
			char * mac;
			json_t * j_data;

			if (json_parse_head(json, &type, &seq, &mac))
				goto error;

			//data
			if (!strcmp(type, "keyngreq")) {
				j_data = json_object_get(json, "keymodelist");
				if (_client_recv_keyreq(c, json, seq, j_data))
					goto error;
			}
			else if (!strcmp(type, "ecdh")) {
				j_data = json_object_get(json, "data");
				if (_client_recv_ecdh(c, json, seq, j_data))
					goto error;
			}
			else if (!strcmp(type, "dh")) {//xielin
				j_data = json_object_get(json, "data");
				if (_client_recv_dh(c, json, seq, j_data))
					goto error;
			}
			else if (!strcmp(type, "dev_reg")) {
				DEBUG_S("receive dev_reg");
				j_data = json_object_get(json, "data");
				if (_client_recv_dev_reg(c, json, seq, j_data))
					goto error;
			}
			else if (!strcmp(type, "keepalive")) {
				j_data = json_object_get(json, "data");
				if (_client_recv_keepalive(c, json, seq, j_data))
					goto error;
			}
			else if (!strcmp(type, "ack")) {
				DEBUG_S("receive ack");
				if (_client_recv_ack(c, json, seq, j_data))
					goto error;
			}
			else if (!strcmp(type, "cfg")) {
				if (_client_recv_cfg(c, json, seq, j_data))
					goto error;
			}
			else if (!strcmp(type, "dev_report")) {
				j_data = json_object_get(json, "dev");
				if (_client_recv_dev_report(c, json, seq, j_data))
					goto error;
			}
			else
				ERROR("unknown type:%s\n", type);
		}

		json_decref(json);
		list_del(&b->list);
		buffer_free(b);
	}

	return;

error:
	json_decref(json);
	client_del(c); client_free(c);
}

void for_each_client(void (*callback)(struct client * , void *), void * data)
{
	struct client * c;
	struct client * p;

	list_for_each_entry_safe(c, p, &server.clients, list){
		callback(c, data);
	}
}

int main(int argc, char * argv[])
{

	int pid = fork();	
	if (pid < 0) {
		return;	
	} else if (pid > 0) {
		return 0;
	}

	DEBUG_S("server begin");
	server_init(&server);

	server.fd = server_socket_init();
	if (server.fd < 0)
		return -1;

	while(1){
		fd_set read_fds;
		fd_set write_fds;
		struct timeval tv;
		int ret;

		max_fd = 0;

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		//read_fds
		FD_ZERO(&read_fds);
		FD_SET(server.fd, &read_fds);
		_update_max_fd(server.fd);
		for_each_client(_client_read_fd_set, &read_fds);

		//write_fds
		FD_ZERO(&write_fds);
		for_each_client(_client_write_fd_set, &write_fds);

		ret = select(max_fd + 1, &read_fds, &write_fds, NULL, &tv);

		if (ret == -1) {
			if (errno == EINTR || errno == EAGAIN)
			    continue;
		}
		else if (ret == 0) { /* timer */
			jiffies ++;
			timer_loop();
		}
		else{
			//read_fds
			if (FD_ISSET(server.fd, &read_fds)){
				server_accept();
			}
			for_each_client(_client_read_fd_check, &read_fds);

			//do recved buffer
			for_each_client(_client_do, NULL);

			//check write_fds
			for_each_client(_client_write_fd_check, &write_fds);
		}
	}
	return 0;
}
