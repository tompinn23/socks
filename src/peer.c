#include "peer.h"

#include "log.h"
#include "mem.h"
#include "protocol.h"

#include <assert.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>

static peer_state_t global_state[MAXFDS];
const fd_status_t fd_status_R = {.want_read = true, .want_write = false};
const fd_status_t fd_status_W = {.want_read = false, .want_write = true};
const fd_status_t fd_status_RW = {.want_read = true, .want_write = true};
const fd_status_t fd_status_NORW = {.want_read = false, .want_write = false};



fd_status_t on_peer_connected(int sockfd, const struct sockaddr_in *peer_addr, socklen_t peer_addr_len) {		
    assert(sockfd < MAXFDS);
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int rc = getnameinfo((const struct sockaddr*)peer_addr,peer_addr_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf, NI_NUMERICHOST | NI_NUMERICSERV);
    if(rc == 0) {
        log_info("Client Connected to server: %s:%s", hbuf, sbuf);
    }
    peer_state_t* peerstate = &global_state[sockfd];
    peerstate->state = INITIAL_ACK;
	static const char* inital_msg = "OK socks 0.10";
	strncpy((char*)peerstate->sendbuf, inital_msg, sizeof peerstate->sendbuf);
	peerstate->sendptr = 0;
	peerstate->sendbuf_end = strlen(inital_msg);
	peerstate->current_message.sockfd = sockfd;
	peerstate->current_message.data = xmalloc(SENDBUF_SIZE);
	peerstate->current_message.data_len = SENDBUF_SIZE;
	peerstate->current_message.data_ptr = 0;
	return fd_status_W;
}

fd_status_t on_peer_close(int sockfd) {
	peer_state_t* peerstate = &global_state[sockfd];
	xfree(peerstate->current_message.data);
	peerstate->current_message.data = NULL;
	peerstate->current_message.data_len = 0;
	peerstate->current_message.data_ptr = 0;
	return fd_status_NORW;
}

void clean_on_exit() {
	for(int i = 0; i < MAXFDS; i++) {
		peer_state_t* p = &global_state[i];
		if(p->current_message.data != NULL) {
			xfree(p->current_message.data);
		}
	}
}

fd_status_t on_peer_ready_recv(int sockfd) {
	assert(sockfd < MAXFDS);
	peer_state_t* peerstate = &global_state[sockfd];

	if(peerstate->state == INITIAL_ACK || peerstate->sendptr < peerstate->sendbuf_end) {
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
	for(int i = 0; i < nbytes; i++) {
		switch(peerstate->state) {
			case INITIAL_ACK:
				assert(0 && "cant get here");
			break;
			case WAIT_FOR_MSG:
				if(i == 0 && ((buf[i] >= 'a' && buf[i] <= 'z') || (buf[i] >= 'A' && buf[i] <= 'Z')))
					// Ascii letter start.
					peerstate->state = IN_MSG; //Fallthrough to process first letter.
				else 
					break;
			case IN_MSG:
			{
				message_t* msg = &(peerstate->current_message);
				if(msg->data_ptr + nbytes >= msg->data_len) {
					msg->data = xrealloc(msg->data, msg->data_len * 2);
					msg->data_len *= 2;
				}
				if(buf[i] != '\n') {
					msg->data[msg->data_ptr++] = buf[i];
				} else {
					peerstate->state = WAIT_FOR_MSG;
					msg->data[msg->data_ptr++] = '\0';
					msg->data_ptr = 0;
					log_info("msg recv: %s", msg->data);
					peerstate->sendbuf[0] = 'O';
					peerstate->sendbuf[1] = 'K';
					peerstate->sendptr = 0;
					peerstate->sendbuf_end = 2;
					ready_to_send = true;
				}
			}
			break;
		}	
	}
	return (fd_status_t){.want_read = !ready_to_send,
						 .want_write = ready_to_send};
}

fd_status_t on_peer_ready_write(int sockfd) {
  assert(sockfd < MAXFDS);
  peer_state_t* peerstate = &global_state[sockfd];

  if (peerstate->sendptr >= peerstate->sendbuf_end) {
    // Nothing to send.
    return fd_status_RW;
  }
  int sendlen = peerstate->sendbuf_end - peerstate->sendptr;
  int nsent = send(sockfd, &peerstate->sendbuf[peerstate->sendptr], sendlen, 0);
  if (nsent == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return fd_status_W;
    } else {
      log_perror("send");
	  exit(EXIT_FAILURE);
    }
  }
  if (nsent < sendlen) {
    peerstate->sendptr += nsent;
    return fd_status_W;
  } else {
    // Everything was sent successfully; reset the send queue.
    peerstate->sendptr = 0;
    peerstate->sendbuf_end = 0;

    // Special-case state transition in if we were in INITIAL_ACK until now.
    if (peerstate->state == INITIAL_ACK) {
      peerstate->state = WAIT_FOR_MSG;
    }

    return fd_status_R;
  }
}
