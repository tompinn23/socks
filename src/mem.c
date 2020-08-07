#include "mem.h"

#include "log.h"

#include <stdlib.h>
#include <string.h>


void* xmalloc(size_t sz) {
	void* mem;
	if(sz == 0) return NULL;
	mem = malloc(sz);
	if(!mem) {
		log_fatal("Failed to allocate memory of sz: %d", sz);
		exit(EXIT_FAILURE);
	}
	return mem;
}

void* xrealloc(void* ptr, size_t sz) {
	void* mem = ptr;
	if(sz == 0) {
		log_warn("Your memory was just free'd by the way dont call realloc with 0");
		xfree(ptr);
		return NULL;
	}
	mem = realloc(mem, sz);
	if(!mem) {
		log_fatal("Failed to reallocate memory to sz: %d", sz);
		exit(EXIT_FAILURE);
	}
	return mem;
}

void* xcalloc(size_t num, size_t amt) {
	void* ptr = xmalloc(num * amt);
	memset(ptr, 0, amt * num);
	return ptr;
}

void* xzalloc(size_t sz) {
	void* ptr = xmalloc(sz);
	memset(ptr, 0, sz);
	return ptr;
}

void xfree(void* ptr) { 
	free(ptr);
}
