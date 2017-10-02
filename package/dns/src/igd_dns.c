#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <resolv.h>
#include "igd_lib.h"

int url_2_ip(struct in_addr *addr, const char *url)
{
	int err, i = 0;
	struct addrinfo hint;
	struct sockaddr_in *sinp;
	struct addrinfo *ailist = NULL, *aip = NULL;

	hint.ai_flags = AI_CANONNAME;
	hint.ai_family = 0;
	hint.ai_socktype = 0;
	hint.ai_protocol = 0;
	hint.ai_addrlen = 0;
	hint.ai_canonname = NULL;
	hint.ai_addr = NULL;
	hint.ai_next = NULL;

	if ((err = getaddrinfo(url, NULL, &hint, &ailist)) != 0)
		return -1;

	for (aip = ailist; aip != NULL && i < DNS_IP_MX; aip = aip->ai_next) {
		if (aip->ai_family == AF_INET && aip->ai_socktype == SOCK_STREAM) {
			sinp = (struct sockaddr_in *)aip->ai_addr;
			addr[i++] = sinp->sin_addr;
		}
	}
	
	freeaddrinfo(ailist);
	return i;
}

int main(int argc, char **argv)
{
	struct igd_dns data;
	
	if (argc != 4)
		return -1;
	memset(&data, 0x0, sizeof(data));
	data.nr = url_2_ip(&data.addr[0], argv[1]);
	ipc_send(argv[2], atol(argv[3]), &data, sizeof(data), NULL, 0);
	return 0;
}

