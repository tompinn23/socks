#include "peer.h"


#include "log.h"
#include "mem.h"
#include "protocol.h"
#include "server.h"

#include <assert.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>

#include <string>

const fd_status_t fd_status_R = {.want_read = true, .want_write = false};
const fd_status_t fd_status_W = {.want_read = false, .want_write = true};
const fd_status_t fd_status_RW = {.want_read = true, .want_write = true};
const fd_status_t fd_status_NORW = {.want_read = false, .want_write = false};
const fd_status_t fd_status_NOCHG = {.want_read = false, .want_write = false, .no_change = true };


fd_status_t Peer::on_peer_connected(int sockfd, const struct sockaddr_in *peer_addr, socklen_t peer_addr_len) {		
    assert(sockfd < MAXFDS);
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int rc = getnameinfo((const struct sockaddr*)peer_addr,peer_addr_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf, NI_NUMERICHOST | NI_NUMERICSERV);
    if(rc == 0) {
        log_info("Client Connected to server: %s:%s", hbuf, sbuf);
    }
    state = INITIAL_ACK;
	static const char* inital_msg = "OK socks 0.10";
	strncpy((char*)&sendbuf[0], inital_msg, sizeof sendbuf);
	sendptr = 0;
	sendbuf_end = strlen(inital_msg);
	//current_message.sockfd = sockfd;
	//current_message.data = xmalloc(SENDBUF_SIZE);
	//current_message.data_len = SENDBUF_SIZE;
	//current_message.data_ptr = 0;
	return fd_status_W;
}

fd_status_t Peer::on_peer_close(int sockfd) {
	return fd_status_NORW;
}


fd_status_t Peer::on_peer_ready_recv(ServerContext& ctx, int sockfd) {
	assert(sockfd < MAXFDS);

	if(state == INITIAL_ACK || sendptr < sendbuf.size()) {
		return fd_status_W;
	}

	uint8_t buf[1024];
	int nbytes = recv(sockfd, buf, sizeof buf, 0);
	if(nbytes == 0) {
		// disconnected peer
		return fd_status_NORW;
	} else if(nbytes < 0) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			//Socket is not really read for recv.
			return fd_status_R;
		} else {
			log_perror("on_recv");
			exit(EXIT_FAILURE);
		}
	}
	bool ready_to_send = false;
	message_t msg = {0};
	msg.sockfd = sockfd;
	for(int i = 0; i < nbytes; i++) {
		switch(state) {
			case INITIAL_ACK:
				assert(0 && "cant get here");
			break;
			case WAIT_FOR_MSG:
				if(i == 0 && ((buf[i] >= 'a' && buf[i] <= 'z') || (buf[i] >= 'A' && buf[i] <= 'Z')))
					// Ascii letter start.
					state = IN_MSG; //Fallthrough to process first letter.
				else 
					break;
			case IN_MSG:
			{
				if(buf[i] != '\n') {
					msg.buf.push_back(buf[i]);
				} else {
					state = WAIT_FOR_MSG;
					msg.buf.push_back('\0');
					ctx.message_queue_in.enqueue(msg);
					log_info("msg recv: %s", std::string(msg.buf.begin(), msg.buf.end()).c_str());
					sendbuf[0] = 'O';
					sendbuf[1] = 'K';
					sendbuf[2] = '\n';
					sendptr = 0;
					sendbuf_end = 3;
					ready_to_send = true;
				}
			}
			break;
		}	
	}
	return (fd_status_t){.want_read = !ready_to_send,
						 .want_write = ready_to_send};
}

fd_status_t Peer::on_peer_ready_write(ServerContext& ctx, int sockfd) {
	assert(sockfd < MAXFDS);

	if (sendptr >= sendbuf.size()) {
		// Nothing to send.
		return fd_status_RW;
	}
	int sendlen = sendbuf.size() - sendptr;
	int nsent = send(sockfd, &sendbuf[sendptr], sendlen, 0);
	if (nsent == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return fd_status_W;
		} else {
			log_perror("send");
			exit(EXIT_FAILURE);
		}
	}
	if (nsent < sendlen) {
		sendptr += nsent;
		return fd_status_W;
	} else {
		// Everything was sent successfully; reset the send queue.
		sendptr = 0;
		
		// Special-case state transition in if we were in INITIAL_ACK until now.    
		if (state == INITIAL_ACK) {
			state = WAIT_FOR_MSG;
		}
	    return fd_status_R; 
	}
}

