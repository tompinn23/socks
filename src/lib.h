#pragma once

#include <sqlite3.h>

typedef struct {
	sqlite3 * db;
} lib_t;

int lib_add_folder(const char* path);
