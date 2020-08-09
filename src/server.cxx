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
	thread = std::thread(&ServerContext::epoll_loop_event, this);
	return 0;
}

int ServerContext::epoll_loop_event() {
	log_set_thread_name("Server Thread");
	int rc = listen(sockfd, SOMAXCONN);
	if(rc == -1) {
			log_perror("listen");
		return -1;
	}
	while(1) {
		int n, i;
		n = epoll_wait(epollfd, events, EPOLL_MAX_EVENTS, -1);
		for(i = 0; i < n; i++) {
			if(events[i].events & EPOLLERR) {
				log_perror("epoll error");
				int error = 0;
				socklen_t errlen = sizeof(error);
				if(getsockopt(events[i].data.fd, SOL_SOCKET, SO_ERROR, (void*)&error, &errlen) == 0) {
					log_error("%s\n", strerror(error));
				}
				close(events[i].data.fd);
				continue;
			}
			else if(sockfd == events[i].data.fd) {
				// Event is from server sock.
				struct sockaddr_in peer_addr;
				socklen_t peer_addr_len = sizeof(peer_addr);
				int newsockfd = accept(ctx->sock, (struct sockaddr*)&peer_addr, &peer_addr_len);
				if(newsockfd < 0) {
					if(errno == EAGAIN || errno == EWOULDBLOCK) {
						log_error("accept returned EAGAIN or EWOULDBLOCK\n");
					} else {
						log_perror("accept");
						return -1;
					}
				} else {
					non_block(newsockfd);
					if(newsockfd >= MAXFDS) {
						log_error("socket fd (%d) >= MAXFDS (%d)", newsockfd, MAXFDS);
						return -1;
					}
				}
			}
		}
	}
}

// vim: set ts=4 sw=4 tw=0 noet :
