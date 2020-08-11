#pragma once

#include <stddef.h>
#include <stdint.h>
#include <netdb.h>
#include <stdbool.h>
#include <array>

#include "protocol.h"

typedef enum { INITIAL_ACK, WAIT_FOR_MSG, IN_MSG } SockState;


#define MAXFDS 16 * 1024
#define SENDBUF_SIZE 1024

class ServerContext;

typedef struct {
	bool want_read;
	bool want_write;
	bool no_change;
} fd_status_t;

class Peer {
public:
	fd_status_t on_peer_connected(int sockfd, const struct sockaddr_in* peer_addr, socklen_t peer_addr_len);
	fd_status_t on_peer_close(int sockfd);
	fd_status_t on_peer_ready_recv(ServerContext& ctx, int sockfd);
	fd_status_t on_peer_ready_write(ServerContext& ctx, int sockfd);
	
	int sendptr;
	std::array<uint8_t, SENDBUF_SIZE> sendbuf;
	int sendbuf_end;
private:
	SockState state;
};



