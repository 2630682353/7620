#ifndef __IGD_ROUTE_H__
#define __IGD_ROUTE_H__

#include "nlk_ipc.h"
#include "uci.h"
#include "uci_fn.h"
#include "linux_list.h"
#include "module.h"

enum {
	ROUTE_MOD_INIT = DEFINE_MSG(MODULE_ROUTE, 0),
	ROUTE_MOD_ADD,
	ROUTE_MOD_DEL,
	ROUTE_MOD_DUMP,
	ROUTE_MOD_RELOAD,
};

struct igd_route_param {
	struct list_head list;
	char dst_ip[16];
	char gateway[16];
	char netmask[16];
	int dev_flag;
	int is_reject;
	int enable;
};

#define ROUTE_MAX 10

extern int route_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
#endif
