#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <strings.h>
#include <stdio.h>

int tcp_connect(const char *host, const char *serv)
{
	int sockfd = -1;
	struct addrinfo hints, *iplist = NULL, *ip = NULL;

	bzero(&hints,sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if(getaddrinfo(host, serv, &hints, &iplist) != 0 ){
		return -1;
	}

	for (ip = iplist; ip != NULL; ip = ip->ai_next) {
		sockfd = socket(ip->ai_family, ip->ai_socktype, ip->ai_protocol);
		if (sockfd < 0)
			continue;
		if (connect(sockfd, ip->ai_addr, ip->ai_addrlen) == 0)
			break;
		close(sockfd);
	}
	freeaddrinfo(iplist);
	return sockfd;
}
