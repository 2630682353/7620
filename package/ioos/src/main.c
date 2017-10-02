#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include "linux_list.h"
#include <stdio.h>
#include "connection.h"
#include "server.h"
#include "log.h"
#include "nlk_ipc.h"

int ioos_nlk_call(struct nlk_msg_handler *nlh)
{
	char buf[1024] = {0,};
	struct nlk_host *host = NULL;
	struct nlk_msg_comm *comm;
	
	if (nlk_event_recv(nlh, buf, sizeof(buf)) <= 0)
		return -1;
	comm = (struct nlk_msg_comm *)buf;
	switch (comm->gid) {
	case NLKMSG_GRP_HOST:
		DEBUG("ioos_nlk_call NLKMSG_GRP_HOST start, action %d\n", comm->action);
		host = (struct nlk_host *)buf;
		if (comm->action == NLK_ACTION_DEL)
			server_list_del(inet_ntoa(host->addr), &srv);
		DEBUG("ioos_nlk_call NLKMSG_GRP_HOST end\n");
		break;
	default:
		break;
	}
	return 0;
}

int http_start(int port)
{
	int optflag;
	int grp;
	connection_t *con;
	socklen_t clilen;
	struct nlk_msg_handler nlh;
	struct epoll_event ev,events[20];
	struct sockaddr_in cliaddr, servaddr;
	int i, listenfd, connfd, epfd, nfds;

	grp = 1 << (NLKMSG_GRP_SYS - 1);
	grp |= 1 << (NLKMSG_GRP_HOST - 1);
	nlk_event_init(&nlh, grp);
	
	server_init(&srv);
	connections_init(&list);
	clilen = sizeof(struct sockaddr);

	optflag = 1;
	epfd = epoll_create(256);
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optflag,  sizeof(optflag));
	ev.data.fd = listenfd;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

	ev.data.fd = nlh.sock_fd;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, nlh.sock_fd, &ev);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl (INADDR_ANY);
	servaddr.sin_port = htons (port);
	bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	listen(listenfd, 20);

	while (1) {
		server_doing(&srv);
		nfds = epoll_wait(epfd, events, 20, 1000);
		for (i = 0;i < nfds; ++i) {
			if (events[i].data.fd == listenfd) {
				connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
				fcntl(connfd, F_SETFL, O_NONBLOCK | O_RDWR);
				if (connfd < 0)
					continue;
				connections_add(&list, connfd);
				ev.data.fd = connfd;
				ev.events = EPOLLIN;
				epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
				continue;
			}

			if (events[i].data.fd == nlh.sock_fd) {
				ioos_nlk_call(&nlh);
				continue;
			}
			
			con = NULL;
			if ((con = connections_search(&list, events[i].data.fd)) == NULL) {
				epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
				continue;
			}
			
			if (con->state == CON_STATE_READ) {
				con->read(con);
				switch (con->state) {
					case CON_STATE_WRITE:
						ev.data.fd=events[i].data.fd;
						ev.events=EPOLLOUT;
						epoll_ctl(epfd, EPOLL_CTL_MOD, events[i].data.fd,&ev);
					case CON_STATE_READ:
						break;
					case CON_STATE_ERROR:
					default:
						epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
						connections_del(&list, con);
						break;
				}
			} else if(con->state == CON_STATE_WRITE) {
				con->write(con);
				switch (con->state) {
					case CON_STATE_WRITE:
						break;
					case CON_STATE_READ:
						ev.data.fd=events[i].data.fd;
						ev.events=EPOLLIN;
						epoll_ctl(epfd, EPOLL_CTL_MOD, events[i].data.fd, &ev);
					case CON_STATE_ERROR:
					default:
						epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
						connections_del(&list, con);
						break;
				}
			} else {
				epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
				connections_del(&list, con);
			}
		}
	}
	return 0;
}

int main(int argc ,char**argv)
{
	if (argc <= 2)
		dm_daemon();
	else {
		//signal(SIGCHLD, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
		signal(SIGPIPE, SIG_IGN);
		closelog();
	}
	http_start(81);
	return 0;
}
