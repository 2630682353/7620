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

int gpio_act(int gpio, int act, int level)
{
	struct nlk_msg_comm nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_HW_GPIO_ACT;
	nlk.id = gpio;
	nlk.action = act;
	nlk.index = level;
	return nlk_head_send(NLK_MSG_HW, &nlk, sizeof(nlk));
}

static int led_gpio_load(int *gpio, int *level)
{
	int i = 0, ret = 0;
	char *flag = NULL, *ptr;

	for (i = (LED_MIN + 1); i < LED_MAX; i++) {
		gpio[i] = -1;
		if (i == LED_POWER) {
			flag = "GPIO_POWER";
		} else if (i == LED_WAN) {
			flag = "GPIO_WAN";
		} else if (i == LED_LAN1) {
			flag = "GPIO_LAN1";
		} else if (i == LED_LAN2) {
			flag = "GPIO_LAN2";
		} else if (i == LED_LAN3) {
			flag = "GPIO_LAN3";
		} else if (i == LED_LAN4) {
			flag = "GPIO_LAN4";
		} else if (i == LED_INTERNET) {
			flag = "GPIO_INTERNET";
		} else if (i == LED_WIFI24G) {
			flag = "GPIO_WIFI24G";
		} else if (i == LED_WIFI5G) {
			flag = "GPIO_WIFI5G";
		} else if (i == LED_WPS) {
			flag = "GPIO_WPS";
		} else if (i == LED_USB) {
			flag = "GPIO_USB";
		} else {
			continue;
		}
		ptr = read_firmware(flag);
		if (!ptr) {
			ret = 1;
			console_printf("read fail flag: %s\n", flag);
			continue;
		} else if (!ptr[0]) {
			;
		} else {
			gpio[i] = atoi(ptr);
			ptr = strchr(ptr, ',');
			if (ptr && (ptr[1] == '1'))
				level[i] = 1;
		}
	}
	return ret;
}

static int led_wifi5g_act(int ret, int act)
{
	if (act == LED_ACT_ON) {
		if (ret)
			return 0;
		system("iwpriv "WIFI_5G_NAME"0 set WlanLed=2");
	} else if (act == LED_ACT_OFF) {
		if (ret)
			return 0;
		system("iwpriv "WIFI_5G_NAME"0 set WlanLed=0");
	} else if (act == LED_ACT_FLASH) {
		if (ret)
			return 0;
		system("iwpriv "WIFI_5G_NAME"0 set WlanLed=1");
	} else if (act == LED_ACT_ENABLE) {
		if (ret == LED_ACT_ENABLE)
			return -1;
		return led_wifi5g_act(0, ret);
	} else if (act == LED_ACT_DISABLE) {
		if (ret)
			return 0;
		system("iwpriv "WIFI_5G_NAME"0 set WlanLed=0");
	}
	return 0;
}

int led_act(int type, int act)
{
	int ret;
	static int gpio[LED_MAX] = {0,};
	static int level[LED_MAX] = {0,};

	if (type <= LED_MIN || type >= LED_MAX)
		return IGD_ERR_DATA_ERR;

	if (gpio[LED_MIN] == 0) {
		if (!led_gpio_load(gpio, level)) {
			gpio[LED_MIN] = 1;
		}
	}
	if (gpio[type] < 0)
		return IGD_ERR_NOT_SUPPORT;
	ret = gpio_act(gpio[type], act, level[type]);
	if (type == LED_WIFI5G)
		ret = led_wifi5g_act(ret, act);
	return ret;
}

int gpio_init(int type, int id)
{
	struct nlk_msg_comm nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_HW_INIT;
	nlk.id = type;
	nlk.index = id;
	return nlk_head_send(NLK_MSG_HW, &nlk, sizeof(nlk));
}

/*  return < 0 if error */
int read_gpio(int gpio, int *value)
{
	struct nlk_msg_comm nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_HW_GPIO;
	nlk.action = NLK_ACTION_DUMP;
	nlk.id = gpio;
	return nlk_head_send_recv(NLK_MSG_HW, &nlk,
		       	sizeof(nlk), value, sizeof(*value));
}

/*  return 0 if success */
int write_gpio(int gpio, int value)
{
	struct nlk_msg_comm nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_HW_GPIO;
	nlk.action = NLK_ACTION_ADD;
	nlk.id = gpio;
	nlk.prio = !!value;
	return nlk_head_send(NLK_MSG_HW, &nlk, sizeof(nlk));
}

int set_led(int type, int id, int action)
{
	struct nlk_msg_comm nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_HW_LED;
	nlk.action = action;
	nlk.id = type;
	nlk.index = id;
	return nlk_head_send(NLK_MSG_HW, &nlk, sizeof(nlk));
}

int set_gpio_led(int action)
{
	struct nlk_msg_comm nlk;

#ifdef GPIO_38_HIGH
	set_led(LED_TYPE_7620_38, 1, action);
#else
	set_led(LED_TYPE_7620_38, 0, action);
#endif
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

int set_port_led(int pid, int action)
{
	struct nlk_msg_comm nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_HW_LED;
	nlk.action = action;
	nlk.id = 3;
	nlk.index = pid;
	return nlk_head_send(NLK_MSG_HW, &nlk, sizeof(nlk));
}

int set_wifi_led(int action)
{
	struct nlk_msg_comm nlk;

	memset(&nlk, 0x0, sizeof(nlk));
	nlk.key = NLK_HW_LED;
	nlk.action = action;
	nlk.id = 1;
	return nlk_head_send(NLK_MSG_HW, &nlk, sizeof(nlk));
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

#define FACTORY_DEV	"/dev/mtd3"
/*  return 0 if success, or -1 */
int read_mac(unsigned char *mac)
{
	unsigned char tmp[64];
	int fd;
	int len;

	fd = open(FACTORY_DEV, O_RDONLY);
	if (fd < 0) 
		return -1;
	len = read(fd, tmp, sizeof(tmp));
	if (len != sizeof(tmp)) 
		goto error;
	memcpy(mac, tmp + 0x28, ETH_ALEN);
	close(fd);
	return 0;
		
error:
	close(fd);
	return -1;
}

