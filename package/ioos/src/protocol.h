#ifndef _http_protocol_h_
#define _http_protocol_h_
#include "connection.h"
#include "uci.h"
#define PRO_BASE_ARG_ERR 20100000
#define PRO_BASE_IDENTITY_ERR                   20100002
#define PRO_BASE_SYS_ERR 30100000
#define PRO_NET_NO_CABLE 30100001
#define PRO_NET_PASSWD_SHORT 30100002
#define PRO_NET_ACCOUNT_NOTREADY 30100003
#define PRO_NET_ACCOUNT_TIMEOUT 30100004
#define PRO_NET_PPPOE_ERR 30100005
#define PRO_SEC_LOGIN_ERR			20104030

typedef struct _cgi_protocol_t{
        const char *name;                 /* function name*/
        int (*handler)(server_t*, connection_t *, struct json_object*); /*function handler*/
}cgi_protocol_t, *cgi_protocol_tp;

extern cgi_protocol_t *find_pro_handler(const char *pro_opt);

//#define CHECK_LOGIN
#endif
