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

#include "wd_com.h"
#include "nlk_ipc.h"

void wd_dbg(const char *fmt, ...)
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

static int signal_broadcast(int arg)
{
	struct nlk_sys_msg nlk;
	
	memset(&nlk, 0x0, sizeof(nlk));
	nlk.comm.gid = NLKMSG_GRP_SYS;
	nlk.comm.mid = SYS_GRP_MID_SIGNAL;
	nlk.comm.id = arg;
	nlk_event_send(NLKMSG_GRP_SYS, &nlk, sizeof(nlk));
	return 0;
}

static unsigned char _running = 1;
static void sigterm_hdl(int arg)
{
    _running = 0;
}

static void sigsur_hdl(int arg)
{
#ifndef WPS_NOT_SUPPORT
	wps_run();
#endif
	signal_broadcast(arg);
}
#ifdef WPS_REPEATER
//#if 1
static int wps_repeater_pid = 0;

static void sigsur2_hdl(int arg)
{
	if (wps_repeater_pid) {
		WD_DBG("wps repeater is running, kill it!\n");
		kill(wps_repeater_pid, SIGKILL);
	}
	WD_DBG("begin wps repeater!\n");
	if ((wps_repeater_pid = fork()) == 0) {
		wps_repeater_run();
	}
}
static void sigchld_hdl(int arg)
{
	int pid = wait(NULL);
	if (pid == wps_repeater_pid) {
		wps_repeater_pid = 0;
		WD_DBG("wps repeater end,child pid=%d\r\n", pid);
	}
}
#endif
int main(int argc, char *const argv[])
{
	pid_t pid = 0;
	int flag = argv[1] ? atoi(argv[1]) : 1;
	int rts = 30;
	char *ptr;

	pid = fork();
	if (pid < 0) {
		WD_DBG("fork fail!,%m\n");
		return -1;
	} else if (pid > 0) {
		exit(0);
	}

	while (sys_uptime() < 30)
		sleep(1);
	WD_DBG("reset start\n");

	signal(SIGTERM, sigterm_hdl);
	signal(SIGUSR1, sigsur_hdl);
	
	
#ifdef WPS_REPEATER
//#if 1
	signal(SIGUSR2, sigsur2_hdl);
	signal(SIGCHLD, sigchld_hdl);
#else
	signal(SIGUSR2, sigsur_hdl);
#endif

#ifndef WPS_NOT_SUPPORT
	wps_init();
#endif

	resetd_init(flag ? flag : 1);

	ptr = read_firmware("RESET_TIME");
	if (ptr)
		rts = atoi(ptr)*10 ? : 30;
	WD_DBG("reset_time: %d\n", rts);

	while(_running) {
		resetd_loop(rts);
#ifndef WPS_NOT_SUPPORT
		wps_loop();
#endif
		usleep(100000);
	}

	WD_DBG("reset quit\n");
	return 0;
}
