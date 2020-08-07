#pragma once

#include <pthread.h>
#include <sys/epoll.h>

#define MAX_EPOLL_EVT 64

struct epoll_ctx {
	int epoll_fd;
	struct epoll_event event;
	struct epoll_event* events;
};

struct net_ctx {
	int sock;
	pthread_t tid;
	int return_code;
	struct epoll_ctx e_ctx;;
};




struct net_ctx* create_server();
int run_server(struct net_ctx* ctx);
void destroy_server(struct net_ctx* ctx);
