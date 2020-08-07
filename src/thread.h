#pragma once

#include <threads.h>
#include <pthread.h>


struct thread_handle {
	char name[128];
	pthread_t thread_id;
};
