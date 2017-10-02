#include <stdio.h>
#include <string.h>
#include "igd_share.h"
#include "host_type.h"

struct os_type_trait {
	unsigned char type;
	struct host_dhcp_trait *dhcp;
};

static struct host_dhcp_trait dhcp_pc_windows[] = {
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,15,3,6,44,46,47,31,33,249,43,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,15,3,6,44,46,47,31,33,43,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,77,251,61,12,60,55,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,12,81,60,55,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,251,61,12,60,55,255,},
	},
	{
		.type = REQ_OPS | DIS_OPS,
		.op = {53,61,50,12,60,55,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,251,61,50,12,60,55,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,50,12,81,60,55,255,},
	},
	{
		.type = REQ_OPS | DIS_OPS,
		.op = {53,61,50,54,12,81,60,55,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,15,3,6,44,46,47,31,33,121,249,43,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,61,12,60,55,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,116,61,12,60,55,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,15,3,6,44,46,47,31,33,121,249,252,43,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,61,12,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,50,54,12,55,43,255,},
	},
	{
		.type = REQ_OP55,
		.op = {1,3,15,6,44,46,47,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,50,12,55,43,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,61,50,12,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,12,55,43,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,3,6,15,44,46,47,57,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,61,12,55,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,61,50,54,12,55,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,61,50,12,55,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,15,3,6,44,46,47,43,77,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,3,6,15,44,46,47,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,61,55,60,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,61,12,55,60,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,50,61,55,60,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,50,61,12,55,60,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,54,50,61,12,55,60,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,54,50,61,55,60,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,15,3,6,44,46,47,31,33,43,77,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,50,54,12,60,55,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,15,3,44,46,47,6,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,116,61,50,12,60,55,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,77,61,50,12,60,55,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,116,61,12,60,55,43,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,50,54,12,81,60,55,43,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,77,61,50,12,81,60,55,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,77,61,50,54,12,81,60,55,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,12,55,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,54,50,12,55,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,77,116,61,12,60,55,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,77,61,50,54,12,60,55,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,50,12,60,55,43,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,50,54,12,60,55,43,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,12,60,55,43,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,116,61,50,12,60,55,43,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,12,81,60,55,43,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,50,12,81,60,55,43,255,},
	},
	{
		.type = END_OP,
		.op = {255,},
	},
};

static struct host_dhcp_trait dhcp_phone_android[] = {
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,3,6,28,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,55,50,54,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,3,6,28,33,51,58,59,121,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,61,60,55,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,57,61,60,50,54,55,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,57,61,60,55,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,121,33,3,6,28,51,58,59,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,57,60,12,55,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,50,54,57,60,12,55,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,121,33,3,6,15,28,51,58,59,119,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,33,3,6,28,51,58,59,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,50,57,60,12,55,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,61,57,60,12,55,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,33,3,6,15,28,51,58,59,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,61,50,54,57,60,12,55,255,},
	},
	{
		.type = END_OP,
		.op = {255,},
	},
};

static struct host_dhcp_trait dhcp_pc_macos[] = {
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,3,6,15,119,95,252,44,46,255,},
	},
	{
		.type = END_OP,
		.op = {255,},
	},
};

static struct host_dhcp_trait dhcp_pc_linux[] = {
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,28,2,3,15,6,12,255,},
	},
	{
		.type = REQ_OPS | DIS_OPS,
		.op = {53,54,50,55,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,28,2,3,15,6,12,44,47,26,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,50,55,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,28,2,3,15,6,119,12,44,47,26,121,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,28,2,3,15,6,119,12,44,47,26,121,42,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,50,12,55,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,28,2,3,15,6,12,40,41,42,26,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,28,2,121,15,6,12,40,41,42,26,119,3,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,28,2,121,15,6,12,40,41,42,26,119,3,121,249,252,42,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,50,12,55,61,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,54,50,12,55,61,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,28,2,3,15,6,12,40,41,42,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,28,2,121,3,15,6,12,255,},
	},
	{
		.type = REQ_OPS | DIS_OPS,
		.op = {53,50,61,55,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,50,61,12,55,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,54,50,61,12,55,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,3,6,12,15,17,23,28,29,31,33,40,41,42,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,57,51,55,60,61,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,57,54,50,41,55,60,61,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,57,51,55,12,60,61,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,57,54,50,51,55,12,60,61,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,3,6,12,15,17,23,28,29,31,33,40,41,42,119,255,},
	},
	{
		.type = REQ_OPS | DIS_OPS,
		.op = {53,57,50,51,55,12,60,61,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,57,55,12,60,61,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,57,54,50,51,55,60,61,255,},
	},
	{
		.type = DIS_OP55,
		.op = {6,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,55,12,60,61,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,3,6,12,15,28,42,40,255,},
	},
	{
		.type = DIS_OPS,
		.op = {53,57,55,60,255,},
	},
	{
		.type = REQ_OPS,
		.op = {50,53,57,55,60,255,},
	},
	{
		.type = DIS_OPS | REQ_OPS,
		.op = {53,57,50,51,55,60,61,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,3,6,12,15,23,28,29,31,33,40,41,42,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,3,6,12,15,17,23,28,29,31,33,40,41,42,9,7,200,44,255,},
	},
	{
		.type = REQ_OPS,
		.op = {53,57,55,60,61,255,},
	},
	{
		.type = DIS_OP55 | REQ_OP55,
		.op = {1,28,2,3,15,6,119,12,44,47,26,121,42,121,249,252,42,255,},
	},
	{
		.type = END_OP,
		.op = {255,},
	},
};

static struct host_dhcp_trait dhcp_phone_ios[] = {
	{
		.type = END_OP,
		.op = {255,},
	},
};

static struct host_dhcp_trait dhcp_phone_wp[] = {
	{
		.type = END_OP,
		.op = {255,},
	},
};

static struct host_dhcp_trait dhcp_tablet_ios[] = {
	{
		.type = END_OP,
		.op = {255,},
	},
};

static struct host_dhcp_trait dhcp_tablet_android[] = {
	{
		.type = END_OP,
		.op = {255,},
	},
};

static struct host_dhcp_trait dhcp_tablet_surface[] = {
	{
		.type = END_OP,
		.op = {255,},
	},
};

#define OS_TYPE_NUM  (sizeof(os_type_info)/sizeof(os_type_info[0]))

static struct os_type_trait os_type_info[] = {
	{
		.type = OS_PC_WINDOWS,
		.dhcp = dhcp_pc_windows,
	},
	{
		.type = OS_PHONE_ANDROID,
		.dhcp = dhcp_phone_android,
	},
	{
		.type = OS_PHONE_IOS,
		.dhcp = dhcp_phone_ios,
	},
	{
		.type = OS_PHONE_WP,
		.dhcp = dhcp_phone_wp,
	},
	{
		.type = OS_PC_MACOS,
		.dhcp = dhcp_pc_macos,
	},
	{
		.type = OS_PC_LINUX,
		.dhcp = dhcp_pc_linux,
	},
	{
		.type = OS_TABLET_IOS,
		.dhcp = dhcp_tablet_ios,
	},
	{
		.type = OS_TABLET_ANDROID,
		.dhcp = dhcp_tablet_android,
	},
	{
		.type = OS_TABLET_SURFACE,
		.dhcp = dhcp_tablet_surface,
	},
};

static int data_cmp(unsigned char *dst, unsigned char *src, unsigned char flag)
{
	unsigned char *d = dst, *s = src;

	while (*d == *s) {
		if (*d == flag)
			return 0;
		d++, s++;
	}
	return -1;
}

unsigned char get_host_os_type(struct host_dhcp_trait *dhcp)
{
	int i = 0, j = 0;
	struct os_type_trait *oti = os_type_info;

	if (!dhcp || dhcp->type == 0)
		return OS_TYPE_NONE;

	for (i = 0; i < OS_TYPE_NUM; i++) {
		for (j = 0; oti[i].dhcp[j].type != END_OP; j++) {
			if (!(oti[i].dhcp[j].type & dhcp->type))
				continue;
			if (!data_cmp(dhcp->op, oti[i].dhcp[j].op, 255))
				return oti[i].type;
		}
	}
	return OS_TYPE_NONE;
}
