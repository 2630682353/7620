#include "lib.h"
//#include "openssl/aes.h"
 /*
 * ecdh
 */

void print_json(json_t * json)
{
	char data[BUFFER_SIZE];
	char *b = json_dumps(json, 0);
	if (!b) {
		ERROR("json dump failed\n");
		return -1;
	}
	DEBUG("'%s'\n", b);
}


char * oct2string(unsigned char * oct, int len)
{
	int i;
	static char str[4096] = {0};
	char * p;

	if (len >= (sizeof(str)/2)) {
		ERROR("len=%d\n", len);
		return NULL;
	}

	memset(str, 0, sizeof(str));
	p = &str[0];
	for (i = 0; i < len; i++) {
		sprintf(p, "%02X", oct[i]);
		p += 2;
	}

	DEBUG("str=%s\n", str);
	return str;
}

void string2oct(char * str, unsigned char * oct, int len)
{
	int i;
	int l;
	unsigned char * o_p;
	char * s_p;

	DEBUG("str=%s\n", str);

	o_p = oct;
	s_p = str;
	l = strlen(str);
	for (i = 0; i < l; i += 2){
		sscanf(s_p, "%2hhx", o_p);
		o_p ++;
		s_p += 2;
	}

	return str;
}

EC_KEY *ecdh_pub_key(unsigned char * pub_key)
{  
	int len;  
	EC_KEY *ecdh = EC_KEY_new();  

	//Generate Public  
	//ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);//NID_secp521r1  
	ecdh = EC_KEY_new_by_curve_name(NID_secp112r1); /* 112bit */
	//ecdh = EC_KEY_new_by_curve_name(NID_secp112r2); /* 112bit */
	//ecdh = EC_KEY_new_by_curve_name(NID_wap_wsg_idm_ecid_wtls6); /* 112bit */
	//ecdh = EC_KEY_new_by_curve_name(NID_wap_wsg_idm_ecid_wtls8); /* 112bit */

	EC_KEY_generate_key(ecdh);  
	const EC_POINT *point = EC_KEY_get0_public_key(ecdh);  
	const EC_GROUP *group = EC_KEY_get0_group(ecdh);  

	len = EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED, pub_key, ECDH_SIZE + 1, NULL);
	if(len == 0){
		ERROR("EC_POINT_point2oct failed\n");
		//TODO:
	}

	DEBUG("pub key:%s, len=%d\n", oct2string(pub_key, len), len);  

	return ecdh;  
}  

unsigned char *ecdh_shared_secret(EC_KEY *ecdh, unsigned char *peer_pub_key, unsigned char * shared_secret)
{  
	int len;  
	const EC_GROUP *group = EC_KEY_get0_group(ecdh);  

	//ComputeKey
	EC_POINT *point_peer = EC_POINT_new(group); //TODO: need free?
	EC_POINT_oct2point(group, point_peer, peer_pub_key, ECDH_SIZE + 1, NULL);  

	//if (0 != EC_POINT_cmp(group, point2, point2c, NULL)) handleErrors();  
	len = ECDH_compute_key(shared_secret, ECDH_SIZE, point_peer, ecdh, NULL);
	if (len == 0) {
		ERROR("ECDH_compute_key failed\n");
		//TODO:
	}

	DEBUG("shared secret:'%s', len=%d\n", oct2string(shared_secret, len), len);

	return shared_secret;
}  

int ecdh_test(void)
{  
	unsigned char pub_key1[ECDH_SIZE + 1];
	unsigned char pub_key2[ECDH_SIZE + 1];
	unsigned char secret1[ECDH_SIZE + 1];
	unsigned char secret2[ECDH_SIZE + 1];

	EC_KEY *ecdh1 = ecdh_pub_key(pub_key1);  
	EC_KEY *ecdh2 = ecdh_pub_key(pub_key2);  
	ecdh_shared_secret(ecdh2, pub_key1, secret1);  
	ecdh_shared_secret(ecdh1, pub_key2, secret2);  

	printf("To the end\n");  
	EC_KEY_free(ecdh1);  
	EC_KEY_free(ecdh2);  
	return 0;  
}

#define AES_BLOCK_SIZE 16
static void myprintf(const char *content)
{
	FILE *fp;
	if((fp=fopen("/tmp/xielin.txt","a+"))==NULL)
	{
		return ;
	}
	fprintf(fp,"%s\n",content);
	fclose(fp);
}
/*
int _aes_encrypt_new2(char * key, int key_len, char * in, int in_len,char * out)
{
		AES_KEY aes;
	   unsigned char iv[AES_BLOCK_SIZE];		// init vector
	   unsigned char* input_string;
	   unsigned char* encrypt_string;
	   unsigned char* decrypt_string;
	   unsigned int len;		// encrypt length (in multiple of AES_BLOCK_SIZE)
	   unsigned int i;

		  // Set encryption key
		  for (i=0; i<AES_BLOCK_SIZE; ++i) {
			  iv[i] = 0;
		  }
		  if (AES_set_encrypt_key(key, 128, &aes) < 0) {
			  fprintf(stderr, "Unable to set encryption key in AES\n");
			  exit(-1);
		  }
	   
		  // encrypt (iv will change)
		//  AES_cbc_encrypt(in, encrypt_string, len, &aes, iv, AES_ENCRYPT);

		return 0;

}
*/

int _aes_encrypt_new(char * key, int key_len, char * in, int in_len,char * out)
{
		//unsigned char ivec[16] = { 0x8,0xfe,0x78,0xcc,0x19,0x79,0,0xce,0xfe};
		unsigned char ivec[16] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
		AES_KEY aes;
		struct stat st;
		char *src = in;
		int size;
		int fd;
		int ret;
		int len;
		size = in_len;
		ret = size % 16;
		len = size;
		if (ret)
		{
			len = len - ret + 16;
		}
		if (AES_set_encrypt_key(key, 128, &aes) < 0)
			goto error;
		AES_cbc_encrypt(src, out, len, &aes, ivec, AES_ENCRYPT);

		return len;
	error:
	    return -1;
}

int _aes_encrypt(char * key, int key_len, char * in, int in_len,char * out)
{
	int enc_len;
	AES_KEY enc_key;
	char * _in;
	char * _out;

	assert(key);
	assert(key_len == 128 /* 128, 192 or 256 */);


	return _aes_encrypt_new(key,key_len,in,in_len,out);
	
	AES_set_encrypt_key(key, key_len, &enc_key);

	enc_len = 0;
	_in = in;
	_out = out;
	while(enc_len < in_len){
		AES_encrypt(_in, _out, &enc_key);

		_in += AES_BLOCK_SIZE;
		_out += AES_BLOCK_SIZE;
		enc_len += AES_BLOCK_SIZE;
	}

	DEBUG("in='%s' enc_len=%d\n", in, enc_len);
	return enc_len;
}


/*
if (AES_set_decrypt_key(pwd, 128, &aes) < 0)
	goto_error();
AES_cbc_encrypt(src, out, len, &aes, ivec, AES_DECRYPT);
*/
int _aes_decrypt_new(char * key, int key_len, char * in, int in_len, char * out)
{
	unsigned char ivec[16] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	AES_KEY aes;
	struct stat st;
	char *src = in;
	int size;
	int fd;
	int ret;
	int len;
	size = in_len;
	ret = size % 16;
	len = size;
	if (ret)
	{
		len = len - ret + 16;
	}
	if (AES_set_decrypt_key(key, 128, &aes) < 0)
		goto error;
	AES_cbc_encrypt(src, out, len, &aes, ivec, AES_DECRYPT);

	return 0;
error:
	return -1;
}


int _aes_decrypt(char * key, int key_len, char * in, int in_len, char * out)
{
	int dec_len;
	AES_KEY dec_key;
	char * _in;
	char * _out;

	assert(key);
	assert(key_len == 128 /* 128, 192 or 256 */);

	return _aes_decrypt_new(key,key_len,in,in_len,out);

	AES_set_decrypt_key(key, key_len, &dec_key);

	dec_len = 0;
	_in = in;
	_out = out;
	while(dec_len < in_len){
		AES_decrypt(_in, _out, &dec_key);

		_in += AES_BLOCK_SIZE;
		_out += AES_BLOCK_SIZE;
		dec_len += AES_BLOCK_SIZE;
	}

	DEBUG("out='%s' enc_len=%d\n", out, dec_len);
	return dec_len;
}

int aes_test(void)
{
	static const unsigned char key[] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	};

	unsigned char text[]="hello world!";
	unsigned char enc_out[80];
	unsigned char dec_out[80];
#if 0
	AES_KEY enc_key, dec_key;

	AES_set_encrypt_key(key, 128, &enc_key);
	AES_encrypt(text, enc_out, &enc_key);      

	AES_set_decrypt_key(key,128,&dec_key);
	AES_decrypt(enc_out, dec_out, &dec_key);

	_aes_encrypt(key, 128, text, enc_out);
	_aes_encrypt(key, 128, enc_out, dec_out);
#endif

	int i;

	printf("original:\t");
	for(i=0;*(text+i)!=0x00;i++)
		printf("%X ",*(text+i));
	printf("\nencrypted:\t");
	for(i=0;*(enc_out+i)!=0x00;i++)
		printf("%X ",*(enc_out+i));
	printf("\ndecrypted:\t");
	for(i=0;*(dec_out+i)!=0x00;i++)
		printf("%X ",*(dec_out+i));
	printf("\n");

	return 0;
}

#define MAC_FORMAT "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
#define MAC_SPLIT(mac) (mac)[0],(mac)[1],(mac)[2],(mac)[3],(mac)[4],(mac)[5]

char * mac2str(char * mac)
{
	static char str[128] = { 0 };

	sprintf(str, MAC_FORMAT, MAC_SPLIT(mac));

	return str;
}

char * str2mac(char * str)
{
	static int mac[6] = { 0, };

	sscanf(str, MAC_FORMAT, &mac[0], &mac[1], &mac[2], &mac[3],&mac[4], &mac[5]);
	
	return mac;
}

char * get_mac(char * ifname)
{
	int sk;
	struct ifreq ifr;
	struct sockaddr addr;
	static char mac[6];

	sk = socket(AF_INET,SOCK_DGRAM,0);
	if(sk < 0){
		ERROR("socket error\n");
		return NULL;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name,ifname);

	if( ioctl(sk, SIOCGIFHWADDR, &ifr) < 0 ){
		ERROR("ioctl error\n");
		close(sk);
		return NULL;
	}

	memcpy(&addr, &ifr.ifr_hwaddr, sizeof(struct sockaddr));
	memcpy(mac, (void *)&addr.sa_data, 6);
	close(sk);

	return mac;
}

char * json_str_get(json_t * json, char * key)
{
	json_t * v;

	v = json_object_get(json, key);
	if (!v) {
		ERROR("get 'key' failed\n");
		return NULL;
	}

	return json_string_value(v);
}

int json_parse_head(json_t * json, char ** type, int * seq, char ** mac)
{
	json_t * j_type;
	json_t * j_seq;
	json_t * j_mac;

	//type
	j_type = json_object_get(json, "type");
	if (!j_type) {
		ERROR("no 'type'\n");
		return -1;
	}
	*type = json_string_value(j_type);
	//seq
	j_seq = json_object_get(json, "sequence");
	if (!j_seq) {
		ERROR("no 'seq'\n");
		return -1;
	}
	*seq = json_integer_value(j_seq);
	//mac
	j_mac = json_object_get(json, "mac");
	if (!j_mac) {
		ERROR("no 'mac'\n");
		return -1;
	}
	mac = json_string_value(j_mac);

	return 0;
}

int json_is_ack(json_t * json)
{
	char * type;
	int seq;
	char * mac;

	if (json_parse_head(json, &type, &seq, &mac))
		return 0;

	if (!strcmp(type, "ack"))
		return 1;

	return 0;
}

struct buffer * buffer_new(void)
{
	struct buffer * b;

	b = (struct buffer *)malloc(sizeof(struct buffer));
	if (!b){
		ERROR("alloc buffer failed");
		return NULL;
	}

	memset(b, 0, sizeof(struct buffer));
	b->json = NULL;

	return b;
}

void buffer_free(struct buffer * b)
{
	if (b->json) {
		json_decref(b->json);
		b->json = NULL;
	}

	free(b);
}

static int _socket_init(void)
{
	int sk;
	struct timeval tv;
	tv.tv_sec = 20;
	tv.tv_usec = 0;

	sk = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sk < 0) {
		ERROR("create socket failed\n");
		return -1;
	}
	if (setsockopt(sk, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
		ERROR("set socket option SO_RCVTIMEO error!\n");
	}
	
	struct timeval to = {3, 0};
	//connect函数超时，处理拔网线重连的情况
	if (setsockopt(sk, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to)) < 0){
		ERROR("set socket option SO_SNDTIMEO error!\n");
	}

	DEBUG("sk=%d\n", sk);
	return sk;
}

int client_socket_init(void)
{
	return _socket_init();
}

int server_socket_init(void)
{
	int sk;
	int reuse;
	struct sockaddr_in addr;

	sk = _socket_init();
	if (sk < 0)
		return sk;

	reuse = 1;
	if (setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) == -1) {
		ERROR("set SO_REUSEADDR error\n");
		close(sk);
		return -1;
	}

	//bind
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(PORT);
	if (bind(sk, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		ERROR("bind failed\n");
		close(sk);
		return -1;
	}

	//listen
	if (listen(sk, 10) < 0) {
		ERROR("listen failed\n");
		close(sk);
		return -1;
	}

	return sk;
}

struct client_timer * client_timer_new(void)
{
	struct client_timer * t;

	t = (struct client_timer *)malloc(sizeof(struct client_timer));
	if (!t) {
		ERROR("alloc timer failed\n");
		return NULL;
	}

	memset(t, 0, sizeof(struct client_timer));

	return t;
}

void client_timer_init(struct client_timer * t, int type, int status, u_int64_t last)
{
	INIT_LIST_HEAD(&t->list);
	t->type = type;
	t->status = status;
	t->last = last;
}

void client_timer_free(struct client_timer * t)
{
	DEBUG("free timer:%p\n", t);
	free(t);
}

struct client_timer * client_timer_find_by_type(struct client * c, int type)
{
	struct client_timer * t;

	list_for_each_entry(t, &c->timers, list){
		if (t->type == type) 
			return t;
	}

	ERROR("timer not found, type=%d\n", type);
	return NULL;
}

struct client * client_new(void (*init)(struct client *))
{
	struct client * c;

	c = (struct client *)malloc(sizeof(struct client));
	if (!c) {
		ERROR("alloc client failed\n");
		return NULL;
	}

	memset(c, 0, sizeof(struct client));
	c->fd = -1;
	INIT_LIST_HEAD(&c->list);
	INIT_LIST_HEAD(&c->timers);
	INIT_LIST_HEAD(&c->snd_queue);
	INIT_LIST_HEAD(&c->rcv_queue);
	INIT_LIST_HEAD(&c->wait_ack_queue);

	//timer main line
	c->main_line = client_timer_new();
	if (!c->main_line){
		free(c);
		return NULL;
	}

	DEBUG("new client:%p\n", c);

	init(c);

	return c;
}

void client_free(struct client *c)
{
	if (c->main_line) {
		list_del(&c->main_line->list);
		c->main_line = NULL;
	}

	DEBUG("client(%p)\n", c);
	free(c);
}

void client_del(struct client *c)
{
	struct buffer * b;
	struct buffer * p;
	struct client_timer * t;
	struct client_timer * tp;

	list_del(&c->list); INIT_LIST_HEAD(&c->list);

	if (c->fd != -1){
		close(c->fd);
		c->fd = -1;
	}

	if(c->dh.k1){
		DH_free(c->dh.k1);
		c->dh.k1= NULL;
	}

	//rcv_queue
	list_for_each_entry_safe(b, p, &c->rcv_queue, list){
		list_del(&b->list);
		buffer_free(b);
	}
	
	//snd_queue
	list_for_each_entry_safe(b, p, &c->snd_queue, list){
		list_del(&b->list);
		buffer_free(b);
	}

	//timers
	list_for_each_entry_safe(t, tp, &c->timers, list){
		if (t != c->main_line) {
			list_del(&t->list);
			client_timer_free(t);
		}
	}

	DEBUG("client(%p)\n", c);
}

int client_recv(struct client * c)
{
	struct buffer * b;
	int ret;

	b = buffer_new();
	if (!b)
		return -1;

	unsigned char  rec_buf[BUFFER_SIZE]={0};
	//unsigned char  rec_tmp[BUFFER_SIZE]={0};
	
	//ret = recv(c->fd, &b->data[0], BUFFER_SIZE, 0);
	ret = recv(c->fd, rec_buf, BUFFER_SIZE, 0);
	if (ret < 0) {
		ERROR("recv error\n");
		return -1;
	}
	if (ret == 0) {
		ERROR("peer close the socket\n");
		return -1;
	}

	char info_head[8]={0};
	long head_fix=htonl(0x3f721fb5);
	memcpy(info_head,&head_fix,4);
	
	if(memcmp(rec_buf,info_head,4)){
		ERROR("invalid fix head\n");
		return -1;
	}

	/*
	int i;
	DEBUG("\nrec info\n");
	for(i=0;i<8;i++){
		DEBUG("\nc[%d]=%x\n",i,rec_buf[i]);
	}
	*/
	long l0=rec_buf[7];
	long l1=rec_buf[6];
	
	long package_len=l0+l1*256;
	
	DEBUG("lenxielin=%u\n", package_len);

	if(package_len>=BUFFER_SIZE-1){
		return -1;
	}
	//解析得到数据的长度
	//去掉包头信息
	//memcpy(&b->data[0],rec_buf[8],ret);
	memcpy(&b->data[0],&rec_buf[8],package_len);

	//BASE64_Encode(&rec_buf[8],package_len,rec_tmp);
	//DEBUG("\n%s\n",rec_tmp);
	
	b->len = package_len;
	list_add_tail(&b->list, &c->rcv_queue);
	DEBUG("client(%p): lenxielin=%u\n", c, package_len);
	return ret;
}


int client_recv_server(struct client * c)
{
	struct buffer * b;
	int ret;

	b = buffer_new();
	if (!b)
		return -1;

	char  rec_buf[BUFFER_SIZE]={0};
	
	//ret = recv(c->fd, &b->data[0], BUFFER_SIZE, 0);
	ret = recv(c->fd, rec_buf, BUFFER_SIZE, 0);
	if (ret < 0) {
		ERROR_S("recv error\n");
		return -1;
	}
	if (ret == 0) {
		ERROR_S("peer close the socket\n");
		return -1;
	}

	char info_head[8]={0};
	long head_fix=htonl(0x3f721fb5);
	memcpy(info_head,&head_fix,4);
	
	if(memcmp(rec_buf,info_head,4)){
		ERROR("invalid fix head\n");
		return -1;
	}

	int i;
	DEBUG_S("\nrec info\n");
	for(i=0;i<8;i++){
		DEBUG_S("c[%d]=%x",i,rec_buf[i]);
	}
	unsigned short l0=rec_buf[7];
	unsigned short l1=rec_buf[6];
	
	long package_len=l0+l1*255;
	package_len=ret-8;
	DEBUG_S("lenxielin =%u l0=%d,l1=%d\n", package_len,l0,l1);
	//解析得到数据的长度
	//去掉包头信息
	//memcpy(&b->data[0],rec_buf[8],ret);
	memcpy(&b->data[0],&rec_buf[8],package_len);
	
	b->len = ret-8;
	list_add_tail(&b->list, &c->rcv_queue);
	DEBUG_S("client(%p): lenxielin=%u\n", c, package_len);
	return ret;
}


int client_send(struct client * c)
{
	struct buffer * b;
	struct buffer * p;
	int len;

	len = 0;
	list_for_each_entry_safe(b, p, &c->snd_queue, list){
		DEBUG("b->len=%d b->offset=%d\n", b->len, b->offset);

		len = send(c->fd, &b->data[0] + b->offset, b->len - b->offset, 0);
		if (len == -1) {
			ERROR("send error\n");
			return -1;
		}
		b->offset += len;
		if (b->offset == b->len) {
			list_del(&b->list);
			if (b->wait_ack)
				list_add_tail(&b->list, &c->wait_ack_queue);
			else
				buffer_free(b);
			DEBUG("send done ");
		}
		DEBUG("client(%p): len=%d\n", c, len);
		return len;
	}

	return 0;
}


int client_send_server(struct client * c)
{
	struct buffer * b;
	struct buffer * p;
	int len;

	len = 0;
	list_for_each_entry_safe(b, p, &c->snd_queue, list){
		DEBUG_S("b->len=%d b->offset=%d\n", b->len, b->offset);

		len = send(c->fd, &b->data[0] + b->offset, b->len - b->offset, 0);
		if (len == -1) {
			ERROR_S("send error\n");
			return -1;
		}
		b->offset += len;
		if (b->offset == b->len) {
			list_del(&b->list);
			if (b->wait_ack)
				list_add_tail(&b->list, &c->wait_ack_queue);
			else
				buffer_free(b);
			DEBUG_S("send done ");
		}
		DEBUG_S("client(%p): len=%d\n", c, len);
		return len;
	}

	return 0;
}


json_t * json_header(char * type, int seq, char * mac)
{
	json_t * json;

	json = json_object();
	if (!json) {
		ERROR("json_object_failed\n");
		return NULL;
	}

	json_object_set(json, "type", json_string(type));
	json_object_set(json, "sequence",json_integer(seq));
	json_object_set(json, "mac",json_string(mac2str(mac)));

	return json;
}

int _client_send_ack_jiami(struct client * c, int seq, int encrypt)
{
	json_t * json;
	json_t * data;

	json = json_header("ack", seq, c->mac);
	if (!json)
		return -1;

	return client_queue_json(c, json, 0, encrypt);
}


int _client_send_ack(struct client * c, int seq, int encrypt)
{
	json_t * json;
	json_t * data;

	json = json_header("ack", seq, c->mac);
	if (!json)
		return -1;

	return client_queue_json(c, json, 0, encrypt);
}

#define IS_PRINT_HEAD 1

int client_queue_send(struct client * c, char * buffer, int len, json_t * json, int wait_ack)
{
	struct buffer * b;

//add head
	char head[8]={0};
    long head_fix=htonl(0x3f721fb5);
    memcpy(head,&head_fix,4);

	long head_len=htonl(len);
    memcpy(head+4,&head_len,4);

	//int 
	if (len > BUFFER_SIZE-8) {
		ERROR("len=%d, too large\n", len);
		return -1;
	}

	b = buffer_new();
	if (!b)
		return -1;
	
	memcpy(b->data,head,8);
	memcpy(b->data+8, buffer, len);
	
	b->len += len+8;
	b->json = json;
	b->wait_ack = wait_ack;
	DEBUG("b->len=%d b->offset=%d\n", b->len+8, b->offset);

	list_add_tail(&b->list, &c->snd_queue);

	DEBUG("client(%p): len=%d wait_ack=%d\n", c, len, wait_ack);

	return 0;
}


int client_queue_json(struct client * c, json_t * json, int wait_ack, int encrypt)
{
	char * b;
	int ret;
	char data[BUFFER_SIZE];

	b = json_dumps(json, 0);
	if (!b) {
		ERROR("json dump failed\n");
		return -1;
	}

	DEBUG("'%s'\n", b);

#if CFG_ENABLE_AES
	if (encrypt) {
		int len = _aes_encrypt(c->ecdh.secret, 128, b, strlen(b) + 1, data);
		ret = client_queue_send(c, data, len, json, wait_ack);
	}
	else
#endif
		ret = client_queue_send(c, b, strlen(b) + 1, json, wait_ack);

	free(b);
	return ret;
}

void server_init(struct server * s)
{
	memset(s, 0, sizeof(struct server));
	INIT_LIST_HEAD(&s->clients);
}
