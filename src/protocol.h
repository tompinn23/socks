#pragma once

#include <stdint.h>
#include <deque>

struct message_t {
	int sockfd;
	std::deque<uint8_t> buf;
};
