#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "igd_share.h"
#include "nf_l7.h"

#define L7_HTTP_FILE	"/tmp/l7_http.txt"
#define L7_FILE	"/tmp/l7.txt"
#define L7_DNS_FILE	"/tmp/l7_dns.txt"
#define L7_EXCLUDE_FILE	"/tmp/l7_exclude.txt"
#define L7_UA_FILE	"/tmp/l7_ua.txt"
#define L7_KO L7_DIR"l7.ko"
#define L7_FILE_LEN_MX	(500 * 1024)

#define goto_error() \
	do{printf("[%d] error" "\n",__LINE__);goto error;}while(0)
#define do_str(tmp,c) do{tmp = strchr(tmp, c); if (!tmp) goto_error();} while(0)
/* #define l7_load_debug(fmt,args...) printf(fmt,##args) */
#define l7_load_debug(fmt,args...) do{}while(0)


#define L7_KEY_MX	8

#define check_number(str) do{int i=0;\
    	   	while (str[i]) {\
			if (!isdigit(str[i])) {\
				printf("%s invalid\n", str);\
				goto_error();\
			}\
			i++;\
		}\
} while(0)

static int read_l7_rule(struct nlk_l7_key *key, unsigned char *tmp)
{
	uint64_t value;
	uint8_t tmp2;
	int len;
	int i;

	key->dir = !!atoi(tmp);
	do_str(tmp, ',');
	tmp++;
	key->pkt_id = atoi(tmp);
	do_str(tmp, ',');
	tmp++;
	key->len_min = atoi(tmp);
	do_str(tmp, ',');
	tmp++;
	key->len_max = atoi(tmp);
	do_str(tmp, ',');
	tmp++;
	key->type = atoi(tmp);
	do_str(tmp, ',');
	if (!key->type) 
		goto ext;
	tmp++;
	key->len = atoi(tmp);
	do_str(tmp, ',');
	if (key->len > 8)
		goto_error();
	tmp++;
	key->offset = atoi(tmp);
	do_str(tmp, ',');
	tmp++;

	switch (key->type) {
	case MATCH_TYPE_MEM:
		value = strtoull(tmp, NULL, 16);
		i = 0;
		len = key->len - 1;
		while (value && i <= len) {
			tmp2 = value & 0xff;
			key->value[len - i] = tmp2;
			value >>= 8;
			i++;
		}
		do_str(tmp, ',');
		tmp++;
		key->end = atoi(tmp);
		break;
	case MATCH_TYPE_RANGE:
		/*  order */
		key->order = atoi(tmp);
		do_str(tmp, ',');
		tmp++;
		i = sscanf(tmp, "%x-%x", &key->range[0], &key->range[1]);
		if (i != 2)
			goto_error();
		if (key->range[0] > key->range[1]) {
			printf("range:%x-%x error\n", key->range[0], key->range[1]);
			goto_error();
		}
		break;
	case MATCH_TYPE_SUM:
		/*  match len support 2 - 4 */
		if (key->len < 2 || key->len > 4) 
			goto_error();
		i = sscanf(tmp, "%x-%x", &key->range[0], &key->range[1]);
		if (i != 2)
			goto_error();
		if (key->range[0] > key->range[1]) {
			printf("range:%x-%x error\n", key->range[0], key->range[1]);
			goto_error();
		}
		break;
	default:
		break;
	}
ext:
	/* read ext match */
	tmp = strchr(tmp, '[');
	if (!tmp) 
		goto out;
	tmp++;
	key->ext_type = atoi(tmp);
	do_str(tmp, ',');
	tmp++;
	key->ext_len = atoi(tmp);
	do_str(tmp, ',');
	tmp++;
	key->ext_offset = atoi(tmp);
out:
	l7_load_debug("%d-%d-%d-%d-%d-%d-%d-%llx-%d,ext:%d-%d-%d\n",
			key->dir, key->pkt_id,
			key->len_min, key->len_max, key->type,
			key->len, key->offset, *(long long *)key->value, key->end,
			key->ext_type, key->ext_len, key->ext_offset);
	return 0;
error:
	return -1;

}

static int read_l7_deps(struct nlk_l7_rule *nlk, unsigned char *tmp)
{
	int type;
	int appid;

	if (!strncmp(tmp, "{}", 2)) 
		return 0;
	tmp++;
	if (sscanf(tmp, "%u,%u", &type, &appid) != 2)
		goto_error();
	switch (type) {
	case 1:
		nlk->deps_type = type;
		nlk->deps_id = appid;
		nlk->comm.mid = 0;
		nlk->comm.id = appid;
		break;
	
	case 2:
		nlk->deps_type = type;
		nlk->deps_id = appid;
		break;
	
	default:
		goto_error();
	}
	return 0;
		
error:
	if (tmp) 
		printf("%s parser error\n", tmp);
	return -1;
}

static int read_l7_pkts(struct nlk_l7_rule *nlk, 
		struct nlk_l7_key *key, unsigned char *tmp)
{
	int cnt = 0;

	tmp = strtok(tmp, "|");
	if (!tmp) 
		goto_error();
	if (read_l7_rule(key, tmp)) 
		goto_error();
	cnt++;
	key++;
	while ((tmp = strtok(NULL, "|")) != NULL) {
		if (read_l7_rule(key, tmp)) 
			goto_error();
		cnt++;
		key++; 
		if (cnt >= 8) 
			break;
	}

	return cnt;
error:
	if (tmp) 
		printf("%s parser error\n", tmp);
	return -1;
}

static int __read_l7_key(struct l7_key *key, unsigned char **res)
{
	unsigned char *tmp = *res;
	uint32_t value;
	uint8_t tmp2;
	int i;
	int len;

	key->type = !!atoi(tmp);
	do_str(tmp, ',');
	tmp++;
	key->len = atoi(tmp);
	if (key->len > 8 || key->len < 0)
		goto_error();
	do_str(tmp, ',');
	tmp++;
	key->offset = atoi(tmp);
	do_str(tmp, ',');
	tmp++;
	value = (uint32_t)strtoull(tmp, NULL, 16);

	i = 0;
	len = key->len - 1;
	while (value && i <= len) {
		tmp2 = value & 0xff;
		key->value[len - i] = tmp2;
		value >>= 8;
		i++;
	}
	do_str(tmp, ',');
	tmp++;
	/*key->end = atoi(tmp);*/
	*res = tmp;
	return 0;
error:
	return -1;
}

static int read_l7_basic(struct nlk_l7_rule *nlk, unsigned char *tmp)
{
	unsigned char *org = tmp;
	int type;
	struct l7_key key = {0, };
	struct l7_pkt_len klen = {0, };
	int type1 = 0;
	int type2 = 0;
	uint32_t value;
	int ret;

	/*tmp = {1,1,4,0,30005747,0|1,1,4,-4,30005747,0|2,1,0,0,0} */
	/*  skip {} */
	if (!strncmp(tmp, "{}", 2)) 
		return 0;
repeat:
	tmp++;
	type = atoi(tmp);
	tmp++;
	tmp++;
	switch (type) {
	case 0:
		break;
	case 1:
		if (type1 >= 2)
			goto_error();
		if (__read_l7_key(&key, &tmp))
			return -1;
		nlk->fdt_key[type1] = key;
		type1++;
		break;
	case 2:
		if (type2 >= 1)
			goto_error();
		klen.len = atoi(tmp);
		if (klen.len < 1 || klen.len > 4) 
			goto_error();
		do_str(tmp, ',');
		tmp++;
		klen.offset = atoi(tmp);
		do_str(tmp, ',');
		tmp++;
		klen.order = atoi(tmp);
		do_str(tmp, ',');
		tmp++;
		klen.adjust = atoi(tmp);
		nlk->fdt_len = klen;
		type2++;
		break;
	default:
		goto_error();
	}

	tmp = strchr(tmp, '|');
	if (tmp)
		goto repeat;

	if (type1 && type2)
		nlk->fdt_type = 3;
	else if (type1) 
		nlk->fdt_type = 1;
	else if (type2)
		nlk->fdt_type = 2;
	else
		nlk->fdt_type = 0;
	l7_load_debug("basic: type %d {1:%d-%d-%d-%x %d-%d-%d-%x}"
			"{2:%d %d %d %d}\n",
			nlk->fdt_type,
		       	nlk->fdt_key[0].type,
			nlk->fdt_key[0].len, 
			nlk->fdt_key[0].offset, *(int *)nlk->fdt_key[0].value,
			nlk->fdt_key[1].type,
			nlk->fdt_key[1].len, 
			nlk->fdt_key[1].offset, *(int *)nlk->fdt_key[1].value,
			nlk->fdt_len.len, nlk->fdt_len.offset,
			nlk->fdt_len.order, nlk->fdt_len.adjust);
		
	return 0;
error:
	printf("%s error\n", org);
	return -1;
}

/*  tmp is {0|1-65535}  or {1|80,90,100}*/
static int read_l7_l4(struct nlk_l7_rule *nlk, unsigned char *tmp)
{
	int type;
	int ret;
	int i;

	tmp++;
	type = atoi(tmp);
	tmp++;
	tmp++;
	switch (type) {
	case 0:
		nlk->l4.type = L7_L4_RANGE; 
		ret = sscanf(tmp, "%hu-%hu",
			&nlk->l4.port[0], &nlk->l4.port[1]);
		if (ret != 2)
			return -1;
		break;
	case 1:
		nlk->l4.type = L7_L4_ENUM; 
		nlk->l4.port[0] = atoi(tmp);
		i = 1;
		while ((tmp = strchr(tmp, ',')) != NULL && i < L7_L4_PORT_MX) {
			tmp++;
			nlk->l4.port[i++] = atoi(tmp);
		}
		break;
	default:
		return -1;
	}

	l7_load_debug("mid %d id %d proto %d %d-%d\n",
			nlk->comm.mid, nlk->comm.id, nlk->proto,
			nlk->l4.port[0], nlk->l4.port[1]);
	return 0;
}

static int do_l7_rule(unsigned char *buf)
{
	struct nlk_l7_rule nlk;
	struct nlk_l7_key data[L7_KEY_MX];
	char *save;
	char *tmp;
	int ret;
	int cnt = 0;
	int pkts_nr = 0;


	memset(&nlk, 0, sizeof(nlk));
	memset(data, 0, sizeof(data));
	tmp = strtok_r(buf, ";", &save);
	if (!tmp) 
		goto_error();
	while ((tmp = strtok_r(NULL, ";", &save)) != NULL) {
		switch (cnt++) {
		case 0:
			check_number(tmp);
			nlk.comm.mid = atoi(tmp);
			break;
		case 1:
			check_number(tmp);
			nlk.comm.id = atoi(tmp);
			break;
		case 2:
			check_number(tmp);
			nlk.proto = atoi(tmp);
			break;
		case 3:
			break;
		case 4:
			if (!strncmp(tmp, "{}", 2)) 
				break;
			tmp++;
			nlk.l4_src.type = atoi(tmp);
			break;
		case 5:
			if (read_l7_l4(&nlk, tmp)) 
				goto_error();
			break;
		case 6:
			if (read_l7_basic(&nlk, tmp)) 
				goto_error();
			break;
		case 7:
			tmp++;
			pkts_nr = read_l7_pkts(&nlk, data, tmp);
			if (pkts_nr < 0) 
				goto_error();
			break;
		case 8:
			if (read_l7_deps(&nlk, tmp) < 0) 
				goto_error();
			break;
			
		default:
			break;
		}
	}

	nlk.comm.key = L7_NLK_RULE;
	nlk.comm.obj_nr = pkts_nr;
	nlk_data_send(NLK_MSG_L7, &nlk, sizeof(nlk), data, sizeof(data));
	return 0;
error:
	printf("error:%s\n", buf);
	return -1;
}

static int read_http_key(unsigned char *tmp, struct nlk_http_key *key)
{
	int cnt = 0;

	key->dir = !!atoi(tmp);
	do_str(tmp, ',');
	tmp++;
	arr_strcpy_end(key->key, tmp, ',');
	do_str(tmp, ',');
	tmp++;
	key->type = atoi(tmp);
	do_str(tmp, ',');
	tmp++;
	key->len = atoi(tmp);
	do_str(tmp, ',');
	tmp++;
	key->offset = atoi(tmp);
	do_str(tmp, ',');
	tmp++;
	arr_strcpy_end(key->value, tmp, ',');
	do_str(tmp, ',');
	tmp++;
	key->end = atoi(tmp);
	l7_load_debug("key:%s:dir%d:type:%d:len:%d:offset:%d:end:%d:value:%s\n",
		key->key, key->dir, key->type,
		key->len, key->offset, key->end, key->value);
	return 0;
error:
	return -1;
}

static int read_http_url(struct nlk_l7_rule *nlk, unsigned char *org)
{
	unsigned char *tmp;
	char buf[512];
	char *save;

	/*  skip {} */
	if (!strncmp(org, "{}", 2)) 
		return 0;

	arr_strcpy(buf, org + 1);
	tmp = strchr(buf, '}');
	if (!tmp) 
		goto_error();
	*tmp = 0;
	tmp = strtok_r(buf, "|", &save);
	arr_strcpy(nlk->url[0], buf);
	nlk->url_nr = 1;

	if (!tmp)
		goto out;
		
	while ((tmp = strtok_r(NULL, "|", &save)) != NULL) {
		if (nlk->url_nr > HTTP_RULE_URL_MX) 
			break;
		arr_strcpy(nlk->url[nlk->url_nr], tmp);
		nlk->url_nr++;
	}
out:
	loop_for_each(0, nlk->url_nr) {
		l7_load_debug("url:%s\n", nlk->url[i]);
	} loop_for_each_end();
	return 0;
error:
	return -1;
}

static int read_http_keys(struct nlk_l7_rule *nlk, 
		struct nlk_http_key *key, unsigned char *org)
{
	unsigned char *tmp = org;
	int cnt = 0;
	int ret;

	/*  for skip \n */
	if (!strncmp(tmp, "{}", 2)) 
		goto out;
	tmp++;
	if (read_http_key(tmp, key))
		goto_error();
	cnt++;
	tmp = strtok(tmp, "|");
	if (!tmp)
		goto out;
	key++;
	while ((tmp = strtok(NULL, "|")) != NULL) {
		if (read_http_key(tmp, key))
			goto_error();
		key++;
		cnt++;
		if (cnt >= HTTP_RULE_KEY_MX) 
			break;
	}
out:
	return cnt;
error:
	printf("read error %s\n", org);
	return -1;
}

static int do_http_rule(unsigned char *buf)
{
	struct nlk_l7_rule nlk;
	struct nlk_http_key data[HTTP_RULE_KEY_MX];
	int type;
	char *tmp;
	char *save;
	int mid;
	int appid;
	int proto;
	int ret;
	int cnt = 0;
	int key_nr = 0;

	memset(&nlk, 0, sizeof(nlk));
	memset(&data, 0, sizeof(data));
	tmp = strtok_r(buf, ";", &save);
	if (!tmp)
		goto_error();
	/*  ignore app name */
	while ((tmp = strtok_r(NULL, ";", &save)) != NULL) {
		switch (cnt++) {
		case 0:
			check_number(tmp);
			nlk.comm.mid = atoi(tmp);
			break;
		case 1:
			check_number(tmp);
			nlk.comm.id = atoi(tmp);
			break;
		case 2:
			if (read_http_url(&nlk, tmp)) 
				goto_error();
			break;
		case 3:
			key_nr = read_http_keys(&nlk, data, tmp);
			if (key_nr < 0) 
				goto_error();
			break;
		case 4:
			if (read_l7_deps(&nlk, tmp) < 0) 
				goto_error();
			break;
		default:
			break;
		}
	}
	nlk.comm.obj_nr = key_nr;
	nlk.comm.key = L7_NLK_HTTP;
	nlk_data_send(NLK_MSG_L7, &nlk, sizeof(nlk), data, sizeof(data));
	return 0;
error:
	printf("load error:%s\n", buf);
	return -1;
}

static int read_ua_key(struct nlk_l7_rule *nlk, unsigned char *org)
{
	unsigned char *tmp;
	char buf[512];
	char *save;

	/*  skip {} */
	if (!strncmp(org, "{}", 2)) 
		goto_error();

	arr_strcpy(buf, org + 1);
	tmp = strchr(buf, '}');
	if (!tmp) 
		goto_error();
	*tmp = 0;
	tmp = strtok_r(buf, "|", &save);
	arr_strcpy(nlk->url[0], buf);
	nlk->url_nr = 1;
	if (!tmp)
		goto out;

	while ((tmp = strtok_r(NULL, "|", &save)) != NULL) {
		if (nlk->url_nr > HTTP_RULE_URL_MX) 
			break;
		arr_strcpy(nlk->url[nlk->url_nr], tmp);
		nlk->url_nr++;
	}
out:
	loop_for_each(0, nlk->url_nr) {
		l7_load_debug("url:%s\n", nlk->url[i]);
	} loop_for_each_end();


	return 0;
error:
	printf("read error %s\n", org);
	return -1;
}

static int do_dns_rule(unsigned char *buf)
{
	struct nlk_l7_rule nlk;
	int type;
	char *tmp;
	char *save;
	int mid;
	int appid;
	int proto;
	int ret;
	int cnt = 0;

	memset(&nlk, 0, sizeof(nlk));
	tmp = strtok_r(buf, ";", &save);
	if (!tmp)
		goto_error();
	/*  ignore app name */
	while ((tmp = strtok_r(NULL, ";", &save)) != NULL) {
		switch (cnt++) {
		case 0:
			check_number(tmp);
			nlk.comm.mid = atoi(tmp);
			break;
		case 1:
			check_number(tmp);
			nlk.comm.id = atoi(tmp);
			break;
		case 2: /* proto */
			break;
		case 3:
			if (read_ua_key(&nlk, tmp))
				goto_error();
			break;
		case 4: /* src port */
		case 5: /*  dst port */
		default:
			break;
		}
	}
	nlk.comm.key = L7_NLK_DNS;
	nlk_head_send(NLK_MSG_L7, &nlk, sizeof(nlk));
	return 0;
error:
	printf("load error:%s\n", buf);
	return -1;
}

static int do_exlude_rule(unsigned char *buf)
{
	struct nlk_l7_rule nlk;

	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.obj_nr = 1;
	nlk.comm.key = L7_NLK_EXCLUDE;
	arr_strcpy(nlk.url[0], buf);
	return nlk_head_send(NLK_MSG_L7, &nlk, sizeof(nlk));
}

static int read_file(char *path, char *out, int len)
{
	struct stat st;
	int size;
	int fd;
	int ret;

	fd = open(path, O_RDONLY);
	if (fd < 0) 
		return -1;

	if (fstat(fd, &st))
		goto_error();
	size = st.st_size;
	if (size > len)
		goto_error();

	ret = igd_safe_read(fd, out, size);
	if (ret != size) {
		printf("read %d, need %d\n", ret, size);
		goto_error();
	}
	close(fd);

	return 0;
error:
    printf("read %s error\n", path);
    if (fd > 0) 
	    close(fd);
    return -1;
}

static int do_ua_rule(unsigned char *buf)
{
	struct nlk_l7_rule nlk;
	int type;
	char *tmp;
	char *save;
	int mid;
	int appid;
	int proto;
	int ret;
	int cnt = 0;

	memset(&nlk, 0, sizeof(nlk));
	tmp = strtok_r(buf, ";", &save);
	if (!tmp)
		goto_error();
	/*  ignore app name */
	while ((tmp = strtok_r(NULL, ";", &save)) != NULL) {
		switch (cnt++) {
		case 0:
			check_number(tmp);
			nlk.comm.mid = atoi(tmp);
			break;
		case 1:
			check_number(tmp);
			nlk.comm.id = atoi(tmp);
			break;
		case 2:
			if (read_ua_key(&nlk, tmp))
				goto_error();
			break;
		case 3:
			check_number(tmp);
			nlk.comm.gid = atoi(tmp);
			break;
		case 4:
		case 5:
			break;
		case 6:
			if (read_l7_deps(&nlk, tmp) < 0) 
				goto_error();
			break;
		default:
			break;
		}
	}

	nlk.comm.key = L7_NLK_UA;
	nlk_head_send(NLK_MSG_L7, &nlk, sizeof(nlk));

	return 0;
error:
	printf("load error:%s\n", buf);
	return -1;
}

static int load_file(char *file, int (*parser)(unsigned char *buf))
{
	char txt[L7_FILE_LEN_MX];
	unsigned char buf[4096];
	unsigned char *tmp;
	unsigned char *org;
	int cnt;

	memset(txt, 0, sizeof(txt));
	if (read_file(file, txt, sizeof(txt)))
		return -1;
	cnt = 0;
	org = txt;
	while ((tmp = strchr(org, '\n')) != NULL) {
		if (tmp - org > sizeof(buf) - 1) 
			continue;
		memset(buf, 0, sizeof(buf));
		memcpy(buf, org, tmp - org);
		org = tmp + 1;
		cnt++;
		l7_load_debug("%s\n", buf);
		if (buf[0] == '#') 
			continue;
		if ((*parser)(buf) < 0)
			printf("%s line %d parser error\n", file, cnt);
	}

	return 0;
}

int main(int argc, const char *argv[])
{
	struct nlk_l7_rule nlk;
	int ret = -1;

	system("rmmod "L7_KO);
	system("insmod "L7_KO);
	memset(&nlk, 0, sizeof(nlk));
	nlk.comm.key = L7_NLK_SET;
	nlk.comm.action = NLK_ACTION_ADD;
	nlk_head_send(NLK_MSG_L7, &nlk, sizeof(nlk));

	load_file(L7_FILE, do_l7_rule);
	load_file(L7_HTTP_FILE, do_http_rule);
	load_file(L7_DNS_FILE, do_dns_rule);
	load_file(L7_UA_FILE, do_ua_rule);
	load_file(L7_EXCLUDE_FILE, do_exlude_rule);

	/*  load private l7 */
	/*load_file(P_L7_FILE, do_l7_rule);*/
	/*load_file(P_L7_HTTP_FILE, do_http_rule);*/

out:
	nlk.comm.key = L7_NLK_SET;
	nlk.comm.action = NLK_ACTION_DEL;
	nlk_head_send(NLK_MSG_L7, &nlk, sizeof(nlk));
	
	return 0;
}
