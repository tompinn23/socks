#include "server.h"

#include "log.h"


#include <exception>
#include <stdexcept>

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>

using namespace std;

static int non_block(int sfd) {
	int flags, rc;
	flags = fcntl(sfd, F_GETFL, 0);
	if(flags == -1 ) {
		log_perror("fcntl error");
		return -1;
	}
	flags |= O_NONBLOCK;
	rc = fcntl(sfd, F_SETFL, flags);
	if(rc == -1) {
		log_perror("fcntl error");
		return -1;
	}
	return 0;
}


ServerContext::ServerContext(std::string addr, uint16_t port) {
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1) {
		log_perror("socket");
		throw runtime_error("failed to create server context");
	}
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof servaddr);
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(addr.c_str());
	servaddr.sin_port = htons(port);

	int rc = bind(sockfd, (struct sockaddr*)&servaddr, sizeof servaddr);
	if(rc == -1) {
		log_perror("bind error");
		throw runtime_error("failed to create server context");
	}
	rc = non_block(sockfd);
	if(rc == -1) {
		throw runtime_error("failed to make socket non blocking");
	}

	epollfd = epoll_create1(0);
	if(epollfd == -1) {
		log_perror("epoll_create1");
		throw runtime_error("failed to create server context");
	}

	struct epoll_event event;
	struct epoll_event* events;
	event.data.fd = sockfd;
	event.events = EPOLLIN;
	rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event);
	if(rc == -1) {
		log_perror("epoll_ctl");
		throw runtime_error("failed to create server context");
	}
	
	events = new struct epoll_event[EPOLL_MAX_EVENTS];
}

int ServerContext::run_server() {
		
}
