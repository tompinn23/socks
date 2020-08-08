#pragma once

#include <thread>
#include <string>
#define EPOLL_MAX_EVENTS 64

class ServerContext {
public:
	ServerContext(std::string addr, uint16_t port);
	int run_server();
	int stop_server();
private:
	int epoll_loop_event(void* udata);

	int sockfd;
	std::thread thread;
	int epollfd;
	struct epoll_event* events;
};
