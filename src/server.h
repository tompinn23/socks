#pragma once

#include "peer.h"
#include "protocol.h"
#include "concurrentqueue.h"


#include <thread>
#include <string>
#include <vector>
#define EPOLL_MAX_EVENTS 64
#define MAX_MESSAGE_STORE 8

class ServerContext {
public:
	ServerContext(std::string addr, uint16_t port);
	int run_server();
	int stop_server();
	
	moodycamel::ConcurrentQueue<message_t> message_queue_out;
	moodycamel::ConcurrentQueue<message_t> message_queue_in;
private:
	int epoll_loop_event();

	std::vector<message_t> out_message_store;
	int messages_left;
	Peer* peer_state;
	int sockfd;
	std::thread thread;
	int epollfd;
	struct epoll_event* events;

	friend class Peer;
};
