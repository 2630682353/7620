#include "common.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#define IP_CHECK(i) do{if((i)< '0' || (i) > '9' ){ printf("i=%c\n",i);return -1;}}while(0)
#define IP_CHECK2(i) do{if((i)< '0' || (i) > '5' ){ printf("i=%c\n",i);return -1;}}while(0)
#define MAC_CHECK(i) do{if(!(((i)<= 'F'&& (i)>= 'A') ||  ((i)<= '9' && (i) >= '0') || ((i)<= 'f'&& (i)>= 'a')))return -1;}while(0)

int checkmask(unsigned int mask)
{
	while (mask & (0x1<<31)) {
		mask <<= 1;
	}
	return mask;
}

int ip_check2(unsigned int ips, unsigned int ipe, unsigned int mask)
{
	unsigned int tmp;
	if (ips == ipe)
		return -1;
	if ((ips & mask) != (ipe & mask))
		return -1;
	tmp = ips & (~mask);
	if (!tmp || (tmp == (~mask)))
		return -1;
	tmp = ipe & (~mask);
	if (!tmp || (tmp == (~mask)))
		return -1;
	return 0;
}

int checkip(char *ip)
{
	char * p = ip;
	int nr = 0, i = 0;

	if (p == NULL)
		return -1;
	i = strlen(p);
	if (i < 7 || i > 15)
		return -1;
	if (*p == '.') 
		return -1;
	while (*p) {
		if ((*p > '9' || *p < '0') && *p != '.')
			return -1;
		if (*p == '.')
			nr++;
		p++;
	}
	if(nr != 3)
		return -1; 
	p = ip;
	for (i = 0; i <= 3; i++) {
		nr = atoi(p);
		if (nr > 255 ||nr < 0)
			return -1;
		if (i == 3)
			break;
		while (*p != '.') {
			p++; 
		}
		p++; 
	}
	return 0;
}

/*
	check mac
*/
int checkmac(char*mac)
{
	MAC_CHECK(mac[0]);
	MAC_CHECK(mac[1]);
	if(mac[2] != ':')return -1;
	MAC_CHECK(mac[3]);
	MAC_CHECK(mac[4]);
	if(mac[5] != ':')return -1;
	MAC_CHECK(mac[6]);
	MAC_CHECK(mac[7]);
	if(mac[8] != ':')return -1;
	MAC_CHECK(mac[9]);
	MAC_CHECK(mac[10]);
	if(mac[11] != ':')return -1;
	MAC_CHECK(mac[12]);
	MAC_CHECK(mac[13]);
	if(mac[14] != ':')return -1;
	MAC_CHECK(mac[15]);
	MAC_CHECK(mac[16]);
	if(mac[17] != '\0')return -1;
	return 0;
}

int dm_daemon(void)
{
	pid_t pid;
	int i;
	if((pid = fork())<0)
	{
		exit(-1);
	}else if(pid){
		exit(0);
	}
	if(setsid()<0){
		exit(-1);
	}
	pid = fork();
	if(pid < 0){
		exit(-1);
	}else if(pid){
		exit(0);
	}
	if(chdir("/"))
	{
		exit(-1);
	}
	for(i=0;i<256;i++)
		close(i);
	i = open("/dev/null", O_RDWR); /* open stdin */
	dup(i); /* stdout */
	dup(i); /* stderr */
	umask(0);
	//signal(SIGCHLD,SIG_IGN);
	signal(SIGHUP,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	return 0;
}

int checktimestr(char *str)
{
	if(str[0] < '0' || str[0] > '9')return 0;
	if(str[1] < '0' || str[1] > '9')return 0;
	if(str[3] < '0' || str[3] > '9')return 0;
	if(str[4] < '0' || str[4] > '9')return 0;
	if(str[2] != ':')return 0;
	if(str[5] != '\0')return 0;
	return 1;
}

time_t strtotime(char*str)
{
	char buf[6];
	int hours;
	int min;
	
	if (!checktimestr(str))
		return 0;
	buf[2] = '\0';
	hours = atoi(buf);
	min = atoi(&buf[3]);
	return hours*60*60 + min * 60;
}

int timetostr(time_t t, char*str, int len)
{
	time_t less;
	int hours;
	int min;
	
	if (len < 6)
		return -1;
	less = t%(24*60*60);
	hours = less/(60*60);
	min = (less%(60*60))/60;
	snprintf(str,6,"%02d:%02d",hours,min);
	return 0;	
}

void str_decode_url(const char *src, int src_len, char *dst, int dst_len)
{
        int     i, j, a, b;
    if(src == NULL){/*add by wangsn*/
        return ;
    }
#define HEXTOI(x)  (isdigit(x) ? x - '0' : x - 'W')

        for (i = j = 0; i < src_len && j < dst_len - 1; i++, j++)
                switch (src[i]) {
                case '%':
                        if (isxdigit(((unsigned char *) src)[i + 1]) &&
                            isxdigit(((unsigned char *) src)[i + 2])) {
                                a = tolower(((unsigned char *)src)[i + 1]);
                                b = tolower(((unsigned char *)src)[i + 2]);
                                dst[j] = (HEXTOI(a) << 4) | HEXTOI(b);
                                i += 2;
                        } else {
                                dst[j] = '%';
                        }
                        break;
                default:
                        dst[j] = src[i];
                        break;
                }

        dst[j] = '\0';  /* Null-terminate the destination */
}