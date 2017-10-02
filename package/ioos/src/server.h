#ifndef _http_server_h_
#define _http_server_h_

typedef struct server server_t;
extern server_t srv;

void server_init(server_t *srv);
void server_doing(server_t *srv);
void server_clean(server_t *srv);
int server_list_update(char *ip, server_t *srv);
int server_list_add(char*ip, server_t *srv);
int server_list_del(char*ip, server_t *srv);
#endif
