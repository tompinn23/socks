#include "log.h"

int main(int argc, char** argv) {
	log_set_thread_name("Main Thread");
	log_error("HELLO");
}
