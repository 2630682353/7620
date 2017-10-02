#ifndef _SZXS_H_
#define _SZXS_H_
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "igd_share.h"
#include "igd_lib.h"
#include "igd_cloud.h"
#include "nlk_ipc.h"
#include "ipc_msg.h"
#include "linux_list.h"
#include "igd_system.h"
#include "igd_md5.h"
#include "igd_host.h"
#include "igd_wifi.h"
#include "igd_advert.h"
#include "image.h"
#include "aes.h"
#include <inttypes.h>
#include <linux/tcp.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <igd_interface.h>

struct group_pck_info {
	struct list_head lh;
	uint32_t id;
	uint32_t syn;
	uint32_t seq;
	__be32 tcp_seq;
	uint16_t data_len;
	char *data;
	char buffer[1600];
};

struct id_list {
	struct list_head lh;
	uint32_t id;
	uint16_t tot_len;
	struct list_head gpi_head;
	uint8_t mac[6];
	time_t rcv_time;
};
#endif
