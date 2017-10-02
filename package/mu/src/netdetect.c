#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<netdb.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/time.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include <resolv.h>
#include "igd_lib.h"
#include "igd_interface.h"

#define TNET_SPORT "80"
#define TNET_HOST_MX 5

int tcp_connect(const char *host)
{
	int sockfd = -1;
	struct sockaddr_in *sinp;
	struct addrinfo hints, *iplist = NULL, *ip = NULL;

	bzero(&hints,sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(host, TNET_SPORT, &hints, &iplist)) {
		MU_DEBUG("netdetect get host ip error\n", host);
		return -1;
	}
	for (ip = iplist; ip != NULL; ip = ip->ai_next) {
		if (ip->ai_family != AF_INET || ip->ai_socktype != SOCK_STREAM) {
			MU_DEBUG("dns ip error\n");
			continue;
		}
		sinp = (struct sockaddr_in *)ip->ai_addr;
		if (!strcmp(inet_ntoa(sinp->sin_addr), "1.127.127.254"))
			continue;
		sockfd = socket(ip->ai_family, ip->ai_socktype, ip->ai_protocol);
		if (sockfd < 0) {
			MU_DEBUG("netdetect create socket error\n");
			continue;
		}
		if (!connect(sockfd, ip->ai_addr, ip->ai_addrlen)) {
			close(sockfd);
			return 0;
		}
		close(sockfd);
	}
	freeaddrinfo(iplist);
	return -1;
}

int main(int argc, char *argv[])
{
	FILE *fp;
	int i, nr;
	int status;
	char buf[128];
	char thost[TNET_HOST_MX][IGD_DNS_LEN];
	
	if (argc < 4) {
		MU_DEBUG("netdetect arg num is eror\n");
		return -1;
	}
	if ((fp = fopen(argv[3], "r")) == NULL) {
		usleep(5000000);
		if ((fp = fopen(argv[3], "r")) == NULL) {
			MU_DEBUG("netdetect open test file error\n");
			return -1;
		}
	}
	nr = 0;
	memset(buf, 0, sizeof(buf));
	memset(thost, 0x0, sizeof(thost));
	while(fgets(buf, sizeof(buf) - 1, fp)) {
		if (strncmp("PING:", buf, 5))
			continue;
		__arr_strcpy_end((unsigned char *)thost[nr],
							(unsigned char *)&buf[5],
							IGD_DNS_LEN, '\n');
		nr++;
		if (nr == TNET_HOST_MX)
			break;
		memset(buf, 0, sizeof(buf));
	}
	if (!nr) {
		MU_DEBUG("netdetect get host error\n");
		return -1;
	}
	while(1) {
		for (i = 0; i < nr; i++) {
			if (!tcp_connect(thost[i])) {
				status = NET_ONLINE;
				MU_DEBUG("netdetect finish status %d\n", status);
				return ipc_send(argv[1], atol(argv[2]), &status, sizeof(int), NULL, 0);
			}
		}
		MU_DEBUG("netdetect status %d\n", status);
		status = NET_OFFLINE;
		ipc_send(argv[1], atol(argv[2]), &status, sizeof(int), NULL, 0);
		usleep(5000000);
	}
	return 0;
}
