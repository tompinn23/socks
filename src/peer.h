#pragma once

#include <stddef.h>
#include <stdint.h>
#include <netdb.h>
#include <stdbool.h>


#include "protocol.h"

typedef enum { INITIAL_ACK, WAIT_FOR_MSG, IN_MSG } SockState;


#define MAXFDS 16 * 1024
#define SENDBUF_SIZE 1024

typedef struct {
	SockState state;
	uint8_t sendbuf[SENDBUF_SIZE];
	int sendbuf_end;
	int sendptr;
	message_t current_message;
} peer_state_t;

typedef struct {
	bool want_read;
	bool want_write;
} fd_status_t;



void clean_on_exit();

fd_status_t on_peer_connected(int sockfd, const struct sockaddr_in* peer_addr, socklen_t peer_addr_len);
fd_status_t on_peer_close(int sockfd);
fd_status_t on_peer_ready_recv(int sockfd);
fd_status_t on_peer_ready_write(int sockfd);
