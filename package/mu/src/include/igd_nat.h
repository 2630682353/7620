#ifndef __IGD_NAT_H__
#define __IGD_NAT_H__

#include <arpa/inet.h>
#include "module.h"
#include "linux_list.h"

#define NAT_PROTO_TCP 1
#define NAT_PROTO_UDP 2
#define NAT_PROTO_TCPUDP 3
#define NAT_PROTO_DMZ 4

struct igd_nat_param {
	struct list_head list;
	unsigned char enable;
	unsigned char mac[6];
	struct in_addr ip;
	unsigned char proto;
	unsigned short out_port;
	unsigned short out_port_end;
	unsigned short in_port;
	unsigned short in_port_end;
};

#define IGD_NAT_MAX  10
#define STATIC_DNAT_CHAIN  "STATIC_DNAT"

enum {
	NAT_MOD_INIT = DEFINE_MSG(MODULE_NAT, 0),
	NAT_MOD_ADD,
	NAT_MOD_DEL,
	NAT_MOD_DUMP,
	NAT_MOD_RESTART,
	NAT_MOD_HOSTUP,
	NAT_MOD_HOSTDOWN,
	NAT_MOD_MAC_DEL,
};

extern int nat_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
#endif
