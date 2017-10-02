#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "wd_com.h"
#include "igd_share.h"

#define CHECK_FILE  "/proc/intreg"

#define RALINK_GPIO_REG_IRQ		0x0A
#define RALINK_GPIO_ENABLE_INTP		0x08
#define RALINK_GPIO_SET_DIR_IN		0x11
#define RALINK_GPIO3924_SET_DIR_IN	0x13
#define RALINK_GPIO7140_SET_DIR_IN	0x15
#define RALINK_GPIO72_SET_DIR_IN	0x17


typedef struct {
	unsigned int irq;   //request irq pin number
	pid_t pid;			//process id to notify
} ralink_gpio_reg_info;

int resetd_init(int flag)
{
	int fd = 0, r = 0;
	ralink_gpio_reg_info info;

	WD_DBG("flag:%d\n", flag);

	system("mknod /dev/gpio c 252 0");
	fd = open("/dev/gpio", O_RDONLY);
	if (fd < 0) {
		perror("/dev/gpio");
		return -1;
	}
	/* set gpio direction to input */
	if (flag < 24) {
		r = ioctl(fd, RALINK_GPIO_SET_DIR_IN, 1 << flag);
	} else if (flag < 40) {
		r = ioctl(fd, RALINK_GPIO3924_SET_DIR_IN, 1 << (flag - 24));
	} else if (flag < 72) {
		r = ioctl(fd, RALINK_GPIO7140_SET_DIR_IN, 1 << (flag - 40));
	} else if (flag == 72) {
		r = ioctl(fd, RALINK_GPIO72_SET_DIR_IN, 1 << (flag - 72));
	} else {
		WD_DBG("flag:%d, err\n", flag);
		return -1;
	}
	if (r)
		WD_DBG("dir in fail:%d, %s\n", r, strerror(errno));

	/* enable gpio interrupt */
	ioctl(fd, RALINK_GPIO_ENABLE_INTP);
	/* register my information */
	info.pid = getpid();
	info.irq = flag;

	ioctl(fd, RALINK_GPIO_REG_IRQ, &info);
	close(fd);
	fd = 0;
	return 0;
}

int resetd_led(void)
{
	int i;
	static unsigned char time = 0, state = 0;

	time++;
	if (time%10)
		return 0;
	WD_DBG("led %d\n", state);
	for (i = (LED_MIN + 1); i < LED_MAX; i++)
		led_act(i, state ? LED_ACT_ON : LED_ACT_OFF);
	WD_DBG("led %d end\n", state);

	state = state ? 0 : 1;
	return 0;
}

static int resetd_fd = 0;
static int resetd_time = 0;
int resetd_loop(int rts)
{
	char buf[64] = {0};
	int rbyts = 0;
	pid_t pid = 0;

	if (resetd_time > 1200) {
		resetd_time = 0;
	} else if (resetd_time >= 600) {
		resetd_led();
		resetd_time++;
		return 0;
	}

	if (resetd_fd <= 0)
		resetd_fd = open(CHECK_FILE, O_RDONLY);
	if (resetd_fd <= 0)
		return -1;
	memset(buf, 0, sizeof(buf));
	//press:0
	lseek(resetd_fd, 0, SEEK_SET);
	rbyts = read(resetd_fd, buf, sizeof(buf));
	if (rbyts < 0) {
		close(resetd_fd);
		resetd_fd = -1;
		return -1;
	} else if (rbyts < 7) {
		return -1;
	}
	if (atoi(&buf[6]) == 1) {
		WD_DBG("press reset\n");
		resetd_time++;
	} else {
		resetd_time = 0;
	}

	if (resetd_time > rts) {
		pid = fork();
		if (pid < 0) {
			return;
		} else if (pid > 0) {
			resetd_time = 600;
			return 0;
		}
		system("killall mu");
		system("killall pppd");
		usleep(2000000);
		set_sysflag(SYSFLAG_RESET, 1);
		set_sysflag(SYSFLAG_FIRST, 1);
		if (!access("/etc/init.d/ali_cloud.init", F_OK))
			set_sysflag(SYSFLAG_ALIRESET, 1);
		system("mtd -r erase \"rootfs_data\"");
		sleep(30);
		system("reboot");
		exit(0);
	}
	return 0;
}
