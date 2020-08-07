#pragma once

#include <stdint.h>

typedef struct {
	int sockfd;
	uint8_t* data;
	int data_ptr;
	int data_len;
} message_t;
