#include <stdio.h>
#include "csp.h"
#include "connection.h"
#include "protocol.h"
#include "server.h"
#include "nlk_ipc.h"

char *get_env_value(char *prefix, char *name)
{
	char env[512];

	snprintf(env, sizeof(env), "%s%s",
		prefix ? prefix : "", name);
	return getenv(env);
}

static int create_head(connection_t *con)
{
	int len = 0;

	if (con->http_head)
		return 0;

	con->http_head = malloc(NC_HTTP_HEAD_MAX);
	if (!con->http_head) {
		CSP_ERR("malloc head fail, %s\n", con->request);
		return -1;
	}
	len += snprintf(con->http_head + len,
		NC_HTTP_HEAD_MAX - len,
		"%s 200 OK\r\n"
		"Server: %s\r\n"
		"Connection: close\r\n"
		"Content-Type: %s; charset=utf-8\r\n",
		con->http_ver, NC_VER, con->http_ctype ? 
		con->http_ctype : "application/json");
	if (con->gzip) {
		len += snprintf(con->http_head + len,
			NC_HTTP_HEAD_MAX - len,
			"Content-Encoding: gzip\r\n");
	}
	if (con->http_body_len > 0) {
		len += snprintf(con->http_head + len,
			NC_HTTP_HEAD_MAX - len,
			"Content-Length: %d\r\n", con->http_body_len);
	}
	len += snprintf(con->http_head + len,
		NC_HTTP_HEAD_MAX - len,
		"\r\n");
	if (len >= NC_HTTP_HEAD_MAX) {
		CSP_ERR("http head too large\n");
		free(con->http_head);
		con->http_head = NULL;
		return -1;
	}
	con->http_head_len = len;
	return 0;
}

extern int deflate_file_to_buffer_gzip(char*dest,int len, char *start, off_t st_size, time_t mtime);
static int create_body(connection_t *con)
{
	int len;
	char *json, *ptr;

	if (con->http_body)
		return 0;

	json = json_object_to_json_string(con->response);
	if (!json)
		return -1;
	if (con->gzip) {
		ptr = malloc(8192*2);
		if (!ptr)
			return -1;
		len = deflate_file_to_buffer_gzip(ptr, 8192*2, json, strlen(json), 0);
		if (len < 0)
			return -1;
	} else {
		ptr = json;
		len = strlen(json);
	}
	con->http_body = ptr;
	con->http_body_len = len;
	return 0;
}

static int send_reply(connection_t *con)
{
	if (write(con->sock, con->http_head,
			con->http_head_len) < 0) {
		CSP_ERR("send head fail, %s\n", con->request);
		return -1;
	}
	if (write(con->sock, con->http_body,
			con->http_body_len) < 0) {
		CSP_ERR("send body fail, %s\n", con->request);
		return -1;
	}
	return 0;
}

static void send_err(connection_t *con)
{
	char buf[2048];

	snprintf(buf, sizeof(buf), 
		"HTTP/1.1 200 OK\r\n"
		"Server: NC 1.0.0\r\n"
		"Connection: close\r\n"
		"Content-Type: application/json; charset=utf-8\r\n"
		"Content-Length: %d\r\n\r\n"
		"%s",
		strlen("{\"error\": 1000}"), "{\"error\": 1000}");
	write(con->sock, buf, strlen(buf));
}

int main_ffff(int argc, char *argv[])
{
	int ret = 0;
	connection_t con;
	char *ptr, *opt, *fname, *function;
	cgi_protocol_t *cgi_fun;
	struct json_object *obj;

	memset(&con, 0, sizeof(con));
	con.request = argv[0];
	con.http_ver = env_com_value(NC_ENV_HTTP_VER);
	con.encoding = env_com_value(NC_ENV_HTTP_ENCODING);
	ptr = env_com_value(NC_ENV_SOCK);
	con.sock = ptr ? atoi(ptr) : -1;
	con.response = json_object_new_object();
	if (!con.request || !con.http_ver ||
		con.sock <= 0 || !con.response) {
		CSP_ERR("req err, %p,%p,%d,%p\n", con.request,
			con.http_ver, con.sock, con.response);
		goto err;
	}
	con.function = -1;
	con.ip_from = env_com_value(NC_ENV_IP);
	con.login = 1;
	if (strstr(con.encoding, "gzip"))
		con.gzip = 1;

	opt = env_key_value("opt");
	fname = env_key_value("fname");
	function = env_key_value("function");
	if (!opt || !fname || !function) {
		ret = PRO_BASE_ARG_ERR;
		goto reply;
	}
	cgi_fun = find_pro_handler(opt);
	if (!cgi_fun) {
		ret = PRO_NET_NO_CABLE;
		goto reply;
	}

	obj= json_object_new_string(opt);
	json_object_object_add(con.response, "opt", obj);
	obj= json_object_new_string(fname);
	json_object_object_add(con.response, "fname", obj);
	obj= json_object_new_string(function);
	json_object_object_add(con.response, "function", obj);

	ret = cgi_fun->handler(NULL, &con, con.response);
	obj= json_object_new_int(ret);
	json_object_object_add(con.response, "error", obj);

reply:
	if (create_body(&con))
		goto err;
	if (create_head(&con))
		goto err;
	if (send_reply(&con))
		goto err;
	close(con.sock);
	return 0;
err:
	if (con.sock > 0) {
		send_err(&con);
		close(con.sock);
	}
	return -1;
}
