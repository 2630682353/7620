#include "connection.h"
#include "ioos_uci.h"
#include "log.h"
#include "server.h"
#include "csp.h"

connection_t list;
void connection_response(connection_t* con);
void connection_parse(connection_t *con,char *src);
void connection_write(connection_t* con);
void connection_free(connection_t *con);
extern int cgi_protocol_handler(server_t*srv,connection_t *con,struct json_object* response);
extern int deflate_file_to_buffer_gzip(char*dest,int len, char *start, off_t st_size, time_t mtime);

void v_list_init (v_list_t *head)
{
	head->next = NULL;
}

int v_list_add(v_list_t *head, char*key, char*value)
{
	v_list_t *list;
	
	list = malloc(sizeof(v_list_t));
	if(list == NULL)
		return -1;
	list->key = strdup(key);
	if (list->key== NULL) {
		free(list);
		return -1;
	}
	list->value = strdup(value);
	list->next = head->next;
	if (list->value== NULL) {
		free(list->key);
		free(list);
		return -1;
	}
	head->next = list;
	return 0;
}

char *v_list_get(v_list_t *head, char*key)
{
	v_list_t *p;
	
	for(p = head->next; p; p = p->next) {
		if (strcmp(key, p->key) == 0) {
			return p->value;
		}
	}
	return NULL;
}
void v_list_free(v_list_t *head)
{
	v_list_t *p;
	v_list_t *q;
	
	for(p = head->next; p; p = q) {
		free(p->key);
		free(p->value);
		q = p->next;
		free(p);
	}
	head->next = NULL;
}

static int getgzip(char*url)
{
	char *p;
	char*end;
	char buf[1024];
	
	if ((p = strstr(url, "Accept-Encoding")) == NULL)
		return 0;
	
	if((end=strstr(p,"\r\n")) == NULL)
		return 0;

	sprintf(buf, "%.*s", end - p - strlen("Accept-Encoding"), p+strlen("Accept-Encoding"));
	printf("Accept-Encoding=%s\n", buf);

	if ((p = strstr(buf, "gzip")) == NULL)
		return 0;
	return 1;
}

void connection_reset(connection_t *con)
{
	con->state = 0;
	con->num = 0;

	if (con->ranged_req) {
		free(con->ranged_req);
		con->ranged_req = NULL;
	}

	if (con->ip_from != NULL) {
		free(con->ip_from);
		con->ip_from = NULL;
	}
	
	if (con->req.size > 0) {
		free(con->req.ptr);
	        con->req.ptr = NULL;	
		con->req.size = 0;
		con->req.used = 0;
	}
	
	if (con->rhead) {
		free(con->rhead);
		con->rhead_len = 0;
	        con->rhead = NULL;	
	}
	
	con->rsize = 0;
	con->function = 0;
	if (con->csp) {
		free(con->csp);
		con->csp = NULL;
	}
	
	if(con->res.size > 0) {
		free(con->res.ptr);
		con->res.ptr = NULL;
	}
	con->res.size = 0;
	con->res.used = 0;
	v_list_free(&con->head);
	json_object_put(con->response);
}

int con_p_requst(request_t *req)
{
	if (req->used > BUFFER_PR_SIZE_MAX) {
		DEBUG_E("used is to long,req->used = %d",req->used);
		return -1;
	}
	if (req->size == 0) {
		req->ptr = malloc(BUFFER_PR_SIZE);
		if (req->ptr == NULL) {
			DEBUG_E("malloc error,req->ptr = NULL");
			return -1;
		}
		req->used = 0;
		req->size = BUFFER_PR_SIZE;
	} else if(req->size - req->used < BUFFER_PR_SIZE) {
		req->size += BUFFER_PR_SIZE;
		req->ptr = realloc(req->ptr,req->size);
		if (req->ptr == NULL) {
			DEBUG_E("req->ptr = NULL");
			return -1;
		}
	}
	return req->size - req->used -1;
}

char* get_head_from(const char*url, const char*key)
{
	char *p;
	char buf[1024];
	char*end;
	
	if((p=strstr(url,key)) == NULL)
		return NULL;

	if ((end=strstr(p,"\r\n")) == NULL)
		return NULL;
	sprintf(buf,"%.*s",end - p - strlen(key)- 2,p+strlen(key)+2);
	return strdup(buf);
}

int con_check_read_finish(connection_t* con)
{
	char*pend;
	char *clenp;
	char *uri;
	char *uriend;
	char *query;
	char *endquery;
	int plen = 0;
	
	if ((pend =strstr(con->req.ptr,"\r\n\r\n")) == NULL)
		return 0;
	clenp = strstr(con->req.ptr, "Content-Length:");
	if (clenp != NULL) {
		if (clenp > pend) {
			DEBUG_E("request = %s\n",con->req.ptr);
		} else {
			clenp = clenp+strlen("Content-Length:");
			plen = atoi(clenp);
			if (strlen(pend) < plen+4)
				return 0;
		}
	}

	if (con->req.used != strlen(con->req.ptr)) {
		DEBUG_E("len is not right");
		DEBUG_E("confd=%d,request = %s,len=%d,used=%d\n",con->fd,con->req.ptr,strlen(con->req.ptr),con->req.used);
	}
	uri = strchr(con->req.ptr, '/');
	if (uri == NULL) {
		DEBUG("error1\n");
		con->state = CON_STATE_ERROR;
		return 1;
	}
	uriend = strchr(uri, ' ');
	if (uriend == NULL) {
		DEBUG("error2\n");
		con->state = CON_STATE_ERROR;
		return 1;
	}
	*uriend++ = '\0';
	DEBUG("uri=%s,fd=%d\n", uri, con->fd);
	query =strchr(uri, '?');
	if (query == NULL) {
		DEBUG("error3\n");
		con->state = CON_STATE_ERROR;
		return 1;
	}
	*query++='\0';
	*pend = '\0';

	endquery=pend+4;
	if (*endquery != '\0') {
		if (plen != strlen(endquery))
			DEBUG_E("plen=%d,endquery=%d",plen,strlen(endquery));
		DEBUG("enduri=%s\n",endquery);
	}
	con->ip_from = get_head_from(uriend, "IP-FROM");
	con->gzip = getgzip(uriend);
	con->csp = strdup(uri);
	connection_parse(con, query);
	connection_parse(con, endquery);
	return 1;
}

void connection_read(connection_t* con)
{
	int ret;
	int n;
	
	while (1) {
		ret = con_p_requst(&con->req);
		if (ret <= 0) {
			DEBUG_E("read error\n");
			con->state = CON_STATE_ERROR;
			return;
		}
		
		n = read(con->fd, con->req.ptr + con->req.used, ret);
		DEBUG("confd=%d,nread=%d\n", con->fd, n);
		if (n <= 0) {
			DEBUG_E("read error2\n");
			con->state = CON_STATE_ERROR;
			return;
		} else if(n == ret) {
			con->req.used += n;
			continue;
		} else {
			con->req.used += n;
			break;
		}
	}
	con->req.ptr[con->req.used] = '\0';
	if (con_check_read_finish(con)) {
		if (con->state != CON_STATE_ERROR) {
			int error = 0;
			struct json_object* obj_error;;
			error = cgi_protocol_handler(&srv,con,con->response);
			obj_error= json_object_new_int(error);
			json_object_object_add(con->response, "error",obj_error);
			connection_response(con);
			connection_write(con);
			DEBUG("error=%d\n",error);
		}
		return;
	}
	return;
}

void connection_write(connection_t* con)
{
	int n;
	int towrite;

	if (con->rsize < con->rhead_len) {
		towrite = con->rhead_len - con->rsize;
		n = write(con->fd, &con->rhead[con->rsize], towrite);
		 if (n < 0) {
			DEBUG_E("write error,n=%d\n",n);
			 con->state = CON_STATE_ERROR;
			 return;
		 } else {
			con->rsize += n;
			if (n != towrite) {
				con->state = CON_STATE_WRITE;
				DEBUG_E("should to write again,write error,n=%d\n",n);
				return;
			}
		 }
	}
	
	towrite = con->res.used + con->rhead_len - con->rsize;
	n = write(con->fd, &con->res.ptr[con->rsize - con->rhead_len], towrite);
	if (n < 0) {
		DEBUG("write error\n");
		con->state = CON_STATE_ERROR;
		return;
	}
	
	if (n != towrite) {
		DEBUG("go to write agin");
		con->rsize += n;
		con->state = CON_STATE_WRITE;
	} else
		con->state = CON_STATE_CLOSE;

	return;
}

void connections_init(connection_t *list)
{
	memset(list,0,sizeof(connection_t));
	INIT_LIST_HEAD(&list->node);
	list->num = 0;
	list->read = connection_read;
	list->write = connection_write;
	list->free = connection_free;
	list->handle = NULL;
	list->response = json_object_new_object();
	list->function = -1;
	list->fd = -1;
	list->ranged_req = NULL;
	return;
}

int  connections_add(connection_t *list,int fd)
{
	connection_t *con = malloc(sizeof(connection_t));
	if (con == NULL)
		return -1;
	connections_init(con);
	con->fd = fd;
	list_add(&con->node,&list->node);
	return 0;
}

void connections_del(connection_t *list,connection_t*con)
{
	list_del(&con->node);
	con->free(con);
	return;
}

connection_t* connections_search(connection_t *list,int fd)
{
	struct list_head *pos;
	connection_t *ptr;
	
	list_for_each(pos, &list->node) {
		if ((ptr = list_entry(pos,connection_t, node))) {
			if (ptr->fd == fd) {
				return ptr;
			}
		}
	}
	return NULL;
}

char *con_value_get(connection_t *con, char*key)
{
	return v_list_get(&con->head, key);
	//return env_key_value(key);
}

void connection_free(connection_t *con)
{
	if (con->ranged_req) {
		free(con->ranged_req);
		con->ranged_req = NULL;
	}

	if (con->req.size > 0)
		free(con->req.ptr);
	
	if (con->res.size > 0)
		free(con->res.ptr);
	
	if (con->ip_from != NULL) {
		free(con->ip_from);
		con->ip_from = NULL;
	}
	
	if (con->rhead != NULL)
		free(con->rhead);
	if (con->fd > 0) {
		DEBUG("fd=%d to close",con->fd);
		close(con->fd);
		con->fd = -1;
	}
	con->function = -1;
	if (con->csp != NULL) {
		free(con->csp);
		con->csp = NULL;
	}
	v_list_free(&con->head);
	json_object_put(con->response);
	free(con);   
}

void connection_parse(connection_t *con, char *src)
{
	char*key = src;
	char *pend;
	char *value;
	char tmp[1024];
	
	while (key != NULL) {
		pend=strchr(key,'&');
		if(pend != NULL){
			*pend = '\0';
			pend++;
		}
		value = strchr(key,'=');
		if(value == NULL){
			key = pend;
			continue;
		}
		*value++ = '\0';
		str_decode_url(value, strlen(value), tmp, 1024);
		v_list_add(&con->head,key,tmp);
		key = pend;
	}
	return;

}

//#define HEAD_EXAMP "HTTP/1.0 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %d\r\n\r\n"
//#define HEAD_EXAMP_GZIP "HTTP/1.0 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %d\r\nContent-Encoding: gzip\r\n\r\n"
#define HEAD_EXAMP "HTTP/1.0 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %d\r\nConnection: Keep-Alive\r\n\r\n"
#define HEAD_EXAMP_GZIP "HTTP/1.0 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %d\r\nConnection: Keep-Alive\r\nContent-Encoding: gzip\r\n\r\n"
void connection_response(connection_t* con)
{
	int len = 0;
	char *text = NULL, *opt = NULL;

	text = json_object_to_json_string(con->response);
	if (text == NULL)
		DEBUG("text == NULL\n");
	opt = con_value_get(con,"opt");
	if (opt && !strcmp(opt, "study_url_var")) {
		len = strlen(text) + 1024;
		con->ranged_req = malloc(len);
		if (!con->ranged_req)
			return;
		memset(con->ranged_req, 0, len);
		snprintf(con->ranged_req, len - 1, "var %s = %s;", opt, text);
	}

	if (con->gzip) {
		con->res.ptr = malloc(8192*2);
		con->res.size = 8192*2;
		if (len) {
			con->res.used = deflate_file_to_buffer_gzip(con->res.ptr, 8192*2, con->ranged_req, strlen(con->ranged_req), 0);
		} else {
			con->res.used = deflate_file_to_buffer_gzip(con->res.ptr, 8192*2, text, strlen(text), 0);
		}
		con->rhead = malloc(1024);
		sprintf(con->rhead, HEAD_EXAMP_GZIP,con->res.used);
		con->rhead_len = strlen(con->rhead);
		return;
	} else {
		if (len) {
			con->res.ptr = con->ranged_req;
			con->res.used = strlen(con->ranged_req);
		} else {
			con->res.ptr = text;
			con->res.used = strlen(text);
		}
		con->rhead = malloc(1024);
		sprintf(con->rhead, HEAD_EXAMP, strlen(text));
		con->rhead_len = strlen(con->rhead);
	}
}

int connection_is_set(connection_t *con)
{
	char* function = NULL;
	
	if (con->function != -1)
		return con->function;
	function = con_value_get(con, "function");
	if (function == NULL)
		return -1;
	if (strcmp(function, "get") == 0) {
		con->function = 0;
		return 0;
	} else if (strcmp(function, "set") == 0) {
		con->function = 1;
		return 1;
	}
	return -1;
}
