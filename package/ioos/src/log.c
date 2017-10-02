#include<stdio.h>
#include<string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "log.h"

static int ioos_print;
static int logfd;
static int log_len;
static char *logfile_tmp;

static void log_init(void)
{
	char tmp[1024];
	
	if (logfile_tmp == NULL) {
		sprintf(tmp,"%s.tmp",LOGFILE);
		logfile_tmp=strdup(tmp);
	}
	logfd = open(LOGFILE, O_CREAT|O_TRUNC|O_RDWR, 0777);
	log_len = 0;
}

int log_write(char*string)
{
	int len;
	
	if (ioos_print) {
		printf("%s",string);
		return 0;
	}
	
	if (logfd <= 0)
		log_init();
	len = strlen(string);
	if (log_len + len > LOG_LEN_LEN) {
		close(logfd);
		rename(LOGFILE, logfile_tmp);
		log_init();
	}
	log_len += write(logfd, string, len);
	return 0;
}

void closelog(void)
{
	ioos_print = 1;
}

void openlog(void)
{
	ioos_print = 0;
}
