/*****************************************************************************
* $File:   watchdog.c
*
* $Author: Hua Shao
* $Date:   Feb, 2014
*
* The dog needs feeding.......
*
*****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <linux/watchdog.h>
#include "wd_com.h"

void watchdog_dbg(const char *fmt, ...)
{
	FILE *fp = NULL;
	va_list ap;

	fp = fopen("/dev/console", "w");
	if (fp) {
		va_start(ap, fmt);
		vfprintf(fp, fmt, ap);
		va_end(ap);
		fclose(fp);
	}
}

static unsigned char _running = 1;
static void sigterm_hdl(int arg)
{
    _running = 0;
}

static void sigsur_hdl(int arg)
{
	if (arg == SIGUSR1)
		wps_run();
}

int main(int argc, char *const argv[])
{
	pid_t pid = 0;
	int fd = 0, opt = 0, ret = 0, time = 20;
	int flag = argv[1] ? atoi(argv[1]) : 1;

	pid = fork();
	if (pid < 0) {
		WD_DBG("fork fail!,%m\n");
		return -1;
	} else if (pid > 0) {
		exit(0);
	}

	/* avoid reseting syste before fully inited. */
	sleep(10);
	WD_DBG("wake watchdog up, %d\n", pid);

	/* open the device */
	fd = open("/dev/watchdog", O_WRONLY);
	if (fd == -1) {
		WD_DBG("open /dev/watchdog fail! %m\n");
		exit(1);
	}

	/* set signal term to call sigterm_handler() */
	/* to make sure fd device is closed */
	signal(SIGTERM, sigterm_hdl);
	signal(SIGUSR1, sigsur_hdl);
	signal(SIGUSR2, sigsur_hdl);

	wps_init();
	resetd_init(flag ? flag : 1);

	/* main loop: feeds the dog every <tint> seconds */
	while(_running) {
		if (time > 10) {
			if (write(fd, "\0", 1) < 0)
				WD_DBG("feed fail\n");
			time = 0;
		}
		resetd_loop();
		usleep(100000);
		time++;
	}

	WD_DBG("watchdog will quit, sending WDIOS_DISABLECARD to driver. \n");

	opt = WDIOS_DISABLECARD;
__retry:
	ret = ioctl(fd, WDIOC_SETOPTIONS, &opt);
	if (ret == EINTR) {
		goto __retry;
	} else if (ret < 0) {
		WD_DBG("ioctl fail, %m\n");
	} else {
		WD_DBG("ioctl succ, %m\n");
	}

	WD_DBG("app stop feeding.\n");
	if (close(fd) == -1)
		WD_DBG("close fail\n");
}
