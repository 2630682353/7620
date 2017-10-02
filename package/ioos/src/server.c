#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uci.h"
#include "common.h"
#include "log.h"
#include "ioos_uci.h"
#include "igd_share.h"
#include "server.h"
#include "linux_list.h"

typedef struct auth_list{
	struct list_head node;
	char ip[16];
	time_t last_time;
}auth_list_t;

void auth_list_init(auth_list_t *node)
{
	memset(node, 0, sizeof(auth_list_t));
	INIT_LIST_HEAD(&node->node);
}

static inline void auth_list_add(auth_list_t *new, auth_list_t *head)
{
	list_add(&new->node, &head->node);
}

static inline void auth_list_del(auth_list_t *node)
{
	list_del(&node->node);
}

static inline auth_list_t * auth_list_search(const char*ip,auth_list_t *head)
{
	struct list_head *pos;
	auth_list_t *ptr;

	list_for_each(pos, &head->node) {
		ptr = list_entry(pos, auth_list_t, node);
		if (strcmp(ptr->ip , ip) == 0)
			return ptr;
	}
	return NULL;
}

static inline auth_list_t * auth_list_timeout(time_t now, time_t maxtime, auth_list_t *head)
{
	auth_list_t *ptr;
	struct list_head *pos;

	list_for_each(pos, &head->node) {
		ptr = list_entry(pos, auth_list_t, node);
		if (ptr->last_time + maxtime < now)
			return ptr;
	}
	return NULL;
}

void auth_list_up(time_t now, auth_list_t *head)
{
	 auth_list_t * ptr;
	 struct list_head *pos;
       
        list_for_each(pos, &head->node) {
		ptr = list_entry(pos, auth_list_t, node);
		ptr->last_time = now;
        }

}
int auth_list_add_ip(const char*ip, time_t now, auth_list_t *head)
{
	auth_list_t *node;

	node = auth_list_search(ip, head);
	if (node != NULL) {
		node->last_time = now;
		return 0;
	}
	node = (auth_list_t *)malloc(sizeof(auth_list_t));
	if (node == NULL)
		return -1;
	auth_list_init(node);
	strncpy(node->ip, ip, 15);
	node->last_time = now;
	auth_list_add(node, head);
	return 0;
}

int auth_list_del_ip(const char*ip, auth_list_t *head)
{
	auth_list_t *node;
	
	node = auth_list_search(ip, head);
	if(node == NULL)
		return 0;
	auth_list_del(node);
	free(node);
	return 0;
}

int auth_list_clean(time_t now, time_t maxtime, auth_list_t *head)
{
	auth_list_t *node;
	
	while ((node = auth_list_timeout(now, maxtime, head))) {
		auth_list_del(node);
		free(node);
	}
	return 0;
}
int auth_list_update_ip(const char*ip, time_t time, auth_list_t *head)
{
	auth_list_t *node;

	node = auth_list_search(ip, head);
	if (node == NULL)
		return -1;
	node->last_time = time;
	return 0;
}

struct server{
	time_t last_time;
	time_t max_time;
	auth_list_t head;
};

server_t srv;

void server_list_clean(server_t *srv)
{
	time_t now;
	now = time(NULL);
	
	if (srv->last_time + 100 < now )
		auth_list_up(now, &srv->head);
	srv->last_time = now;
	auth_list_clean(srv->last_time, srv->max_time, &srv->head);
}

int server_list_update(char *ip, server_t *srv)
{
	return auth_list_update_ip(ip, srv->last_time, &srv->head);
}

int server_list_add(char*ip, server_t *srv)
{
	return auth_list_add_ip(ip, srv->last_time, &srv->head);
}

int server_list_del(char*ip, server_t *srv)
{
	return auth_list_del_ip(ip, &srv->head);
}

void server_clean(server_t *srv)
{
	auth_list_t *alt, *_alt;

	list_for_each_entry_safe(alt, _alt, &srv->head.node, node) {
		list_del(&alt->node);
		free(alt);
	}
}
void server_doing(server_t *srv)
{
	server_list_clean(srv);
}

void server_init(server_t *srv)
{
	memset(srv, 0, sizeof(server_t));
	auth_list_init(&srv->head);
	srv->max_time = 60 * 60 * 12;
	return; 
}
