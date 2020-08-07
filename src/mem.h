#pragma once

#include <stdint.h>
#include <stddef.h>

void* xmalloc(size_t sz);
void* xrealloc(void* ptr, size_t sz);
void* xzalloc(size_t sz);
void* xcalloc(size_t num, size_t amt);
void xfree(void* ptr);

