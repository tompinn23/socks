#include "net.h"
#include "log.h"
#include "mem.h"
#include "peer.h"

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

int create_non_block_socket(const char* addr, uint16_t port) {
	int sock, rc;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1) {
		log_perror("socket error");
		return -1;
	}
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(8080);

	rc = bind(sock, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if(rc == -1) {
		log_perror("bind error");
		return -1;
	}
	rc = non_block(sock);
	if(rc == -1) {
		log_error("Failed to make socket non blocking.");
		return -1;
	}
	return sock;
}


struct net_ctx* create_server() {
	struct net_ctx* ctx;
	ctx = xzalloc(sizeof(*ctx));

	int sock, efd, rc;
	sock = create_non_block_socket("127.0.0.1", 8080);
	if(sock == -1) {
		log_error("Failed to create socket");
		goto err;
	}
	efd = epoll_create1(0);
	if(efd == -1){
		log_perror("epoll_create1 error");
		goto err;
	}
	struct epoll_event event;
	struct epoll_events* events;
	event.data.fd = sock;
	event.events = EPOLLIN;
	rc = epoll_ctl(efd, EPOLL_CTL_ADD, sock, &event);
	if(rc == -1) {
		log_perror("epoll_ctl");
		goto err;
	}
	struct epoll_ctx e_ctx;
	e_ctx.events = xcalloc(MAX_EPOLL_EVT, sizeof event);
	e_ctx.epoll_fd = efd;
	ctx->e_ctx = e_ctx;
	ctx->sock = sock;
	return ctx;
err:
	xfree(ctx);
	return NULL;
// End error handling.
}

static void* epoll_event_loop(void* udata) {
	struct net_ctx* ctx = (struct net_ctx*)udata;
	int s = listen(ctx->sock, SOMAXCONN);
	if(s == -1) {
		log_perror("[Server] listen");
		ctx->return_code = -1;
		return NULL;
	}
	while(1) {
		int n, i;
		n = epoll_wait(ctx->e_ctx.epoll_fd, ctx->e_ctx.events, MAX_EPOLL_EVT, -1);
		for(i = 0; i < n; i++) {
			if(ctx->e_ctx.events[i].events & EPOLLERR) { 
				log_perror("[Server] epoll error");
				int error = 0;
				socklen_t errlen = sizeof(error);
				if (getsockopt(ctx->e_ctx.events[i].data.fd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen) == 0){
					log_error("%s\n", strerror(error));
				}
				close(ctx->e_ctx.events[i].data.fd);
				continue;
			}
			else if(ctx->sock == ctx->e_ctx.events[i].data.fd) {
				// The event is coming on server socket.
				// Connection is handled on main server thread.
					struct sockaddr_in peer_addr;
					socklen_t peer_addr_len = sizeof(peer_addr);
					int newsockfd = accept(ctx->sock, (struct sockaddr*)&peer_addr, &peer_addr_len);
					if (newsockfd < 0) {
						if (errno == EAGAIN || errno == EWOULDBLOCK) {
							// This can happen due to the nonblocking socket mode; in this
							// case don't do anything, but print a notice (since these events
							// are extremely rare and interesting to observe...)
							printf("accept returned EAGAIN or EWOULDBLOCK\n");
						} else {
							log_perror("accept");
							ctx->return_code = -1;
							return NULL;
						}
					} else {
						non_block(newsockfd);
						if (newsockfd >= MAXFDS) {
							log_error("socket fd (%d) >= MAXFDS (%d)", newsockfd, MAXFDS);
							ctx->return_code = -1;
							return NULL;
						}
						fd_status_t status = on_peer_connected(newsockfd, &peer_addr, peer_addr_len);
						struct epoll_event event = {0};
						event.data.fd = newsockfd;
						if (status.want_read) {
							event.events |= EPOLLIN;
						}
						if (status.want_write) {
							event.events |= EPOLLOUT;
						}
	
						if (epoll_ctl(ctx->e_ctx.epoll_fd, EPOLL_CTL_ADD, newsockfd, &event) < 0) {
							log_perror("epoll_ctl EPOLL_CTL_ADD");
							ctx->return_code = -1;
							return NULL;
						}
					}
				continue;
			} // else if(ctx->sock == ctx->e_ctx.events[i].data.fd)
			else {
				if (ctx->e_ctx.events[i].events & EPOLLIN) {
					// Ready for reading.
					int fd = ctx->e_ctx.events[i].data.fd;
					fd_status_t status = on_peer_ready_recv(fd);
					struct epoll_event event = {0};
					event.data.fd = fd;
					if (status.want_read) {
						event.events |= EPOLLIN;
					}
					if (status.want_write) {
						event.events |= EPOLLOUT;
					}
					if (event.events == 0) {
						printf("socket %d closing\n", fd);
						if (epoll_ctl(ctx->e_ctx.epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
							log_perror("epoll_ctl EPOLL_CTL_DEL");            
							ctx->return_code = -1;
							return NULL;
						}
						on_peer_close(fd);
						close(fd);
					} else if (epoll_ctl(ctx->e_ctx.epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0) {
						log_perror("epoll_ctl EPOLL_CTL_MOD");
						ctx->return_code = -1;
						return NULL;
					}
				} else if (ctx->e_ctx.events[i].events & EPOLLOUT) {
					// Ready for writing.
					int fd = ctx->e_ctx.events[i].data.fd;
					fd_status_t status = on_peer_ready_write(fd);
					struct epoll_event event = {0};
					event.data.fd = fd;
					if (status.want_read) {
						event.events |= EPOLLIN;
					}
					if (status.want_write) {
						event.events |= EPOLLOUT;
					}
					if (event.events == 0) {
						printf("socket %d closing\n", fd);
						if (epoll_ctl(ctx->e_ctx.epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
							log_perror("epoll_ctl EPOLL_CTL_DEL");
							ctx->return_code = -1;
							return NULL;
						}
						on_peer_close(fd);
						close(fd);
					} else if (epoll_ctl(ctx->e_ctx.epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0) {
						log_perror("epoll_ctl EPOLL_CTL_MOD");
						ctx->return_code = -1;
						return NULL;
					}
				}   
			}
		}
	}
	ctx->return_code = 0;
	return NULL;
}


int run_server(struct net_ctx* ctx) {
	int rc = pthread_create(&ctx->tid, 0, epoll_event_loop, ctx);
	if(rc != 0 ){
		log_error("Failed creating server thread: %s", strerror(rc));
		return -1;
	}
	return 0;
}

