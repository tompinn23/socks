#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>

static int
make_socket_non_blocking (int sfd)
{
  int flags, s;

  flags = fcntl (sfd, F_GETFL, 0);
  if (flags == -1)
    {
      perror ("fcntl");
      return -1;
    }

  flags |= O_NONBLOCK;
  s = fcntl (sfd, F_SETFL, flags);
  if (s == -1)
    {
      perror ("fcntl");
      return -1;
    }

  return 0;
}



int main(int argc, char** argv) {
	int sock;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1) {
		printf("Failed to create socket.");
	}
	
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(8080);
	
	bind(sock, (struct sockaddr*)&servaddr, sizeof(servaddr));
	int s;
	s = make_socket_non_blocking(sock);
	if(s == -1)
		return -1;

	s = listen(sock, SOMAXCONN);
	if(s == -1) {
		perror("listen");
		return -1;
	}

	int	efd = epoll_create1(0);
	if(efd == -1) {
		perror("epoll_create1");
		return -1;
	}
	struct epoll_event event;
	struct epoll_event* events;
	event.data.fd = sock;
	event.events = EPOLLIN | EPOLLET;
	s = epoll_ctl(efd, EPOLL_CTL_ADD,  sock, &event);
	if(s == -1) {
		perror("epoll_ctl");
		return -1;
	}

	events = calloc(64, sizeof event);
	while(1) {
		int n, i;
		n = epoll_wait(efd, events, 64, -1);
		for(i = 0; i < n; i++) {
			if((events[i].events & EPOLLERR) ||
			   (events[i].events & EPOLLHUP) ||
			   (!(events[i].events & EPOLLIN))) {
				fprintf(stderr, "epoll error\n");
				close(events[i].data.fd);
				continue;
			}
			else if(sock == events[i].data.fd) {
				while(1){
					struct sockaddr in_addr;
					socklen_t in_len;
					int infd;
					char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

					in_len = sizeof in_addr;
					infd = accept(sock, &in_addr, &in_len);
					if(infd == -1) {
						if((errno == EAGAIN) ||
						   (errno == EWOULDBLOCK)) {
							break;
						}
						else {
							perror("accept");
							break;
						}

					}
					s = getnameinfo(&in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf, NI_NUMERICHOST | NI_NUMERICSERV);
					if(s == 0) {
						printf("Accepted connection on descriptor %d, (host=%s:%s)\n", infd, hbuf, sbuf);
						
					}
					s = make_socket_non_blocking(infd);
					if(s == -1)
						return -1;
					
					event.data.fd = infd;
					event.events = EPOLLIN | EPOLLET;
					s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
					if(s == -1) {
						perror("epoll_ctl");
						return -1;
					}
				}
				continue;
			} else {
				int done = 0;
				while(1) {
					ssize_t count;
					char buf[512];
					count = read(events[i].data.fd, buf, sizeof buf);
					if(count == -1) {
						if(errno != EAGAIN) {
							perror("read");
							done =1;
						}
						break;
					}
					else if(count == 0) {
						done = 1;
						break;
					}
					s = write(1, buf, count);
					if(s == -1) {
						perror("write");
						return -1;
					}
				}
				if(done) {
					printf("Closed connection on descriptor %d\n", events[i].data.fd);
					close(events[i].data.fd);
				}

			}
		}
	}
	free(events);
	close(sock);
	return 0;
}


