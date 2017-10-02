#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <assert.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/if_ether.h>
#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <stdarg.h>
#include <sys/sysinfo.h>
#include "nlk_ipc.h"
#include "ioos_uci.h"
#include "aes.h"
#include "uci_fn.h"

int set_gpio_led(int action)
{
	struct nlk_msg_comm nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_HW_LED;
	nlk.action = action;
	nlk.id = 0;
	return nlk_head_send(NLK_MSG_HW, &nlk, sizeof(nlk));
}

/*  
 *  pid : port id
 *  action: add for link up, del for link down , return 0 for success
*/
int set_port_link(int pid, int action)
{
	struct nlk_msg_comm nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_HW_MII;
	nlk.action = action;
	nlk.id = pid; 
	return nlk_head_send(NLK_MSG_HW, &nlk, sizeof(nlk));
}

/*  
 *  pid : port id
 *  speed: SPEED_DUPLEX_AUTO for auto, SPEED_DUPLEX_10H for 10H
 	SPEED_DUPLEX_10F for 10F, SPEED_DUPLEX_100H for 100H,
 	SPEED_DUPLEX_100F for 100F, return 0 for success
*/
int set_port_speed(int pid, int speed)
{
	struct nlk_msg_comm nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_HW_MII;
	nlk.gid = 1;
	nlk.mid = speed;
	nlk.id = pid;
	nlk.action = NLK_ACTION_DUMP;
	return nlk_head_send(NLK_MSG_HW, &nlk, sizeof(nlk));
}

int set_wifi_led(int action)
{
	switch (action) {
	case NLK_ACTION_ADD:
		system("echo 1 > /proc/wlan0/led");
		break;
	default:
		system("echo 0 > /proc/wlan0/led");
		break;
	}
	return 0;
}

int set_wps_led(int action)
{
	struct nlk_msg_comm nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_HW_LED;
	nlk.action = action;
	nlk.id = 2;
	return nlk_head_send(NLK_MSG_HW, &nlk, sizeof(nlk));
}

/*  return 0 if success, or -1 */
int read_mac(unsigned char *mac)
{
	char buf[64] = {0, };
	int ret;
	char *tmp;

	ret = shell_printf("flash get HW_NIC0_ADDR", buf, sizeof(buf));
	if (ret < 0) 
		return ret;
	if (strncmp(buf, "HW_NIC0_ADDR=", 13))
		return IGD_ERR_DATA_ERR;
	tmp = &buf[13];
	sscanf(tmp, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx", MAC_SPLIT(&mac));

	return 0;
}

