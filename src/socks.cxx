#include "log.h"

#include "server.h"

int main(int argc, char** argv) {
	log_set_thread_name("Main Thread");
	log_error("HELLO");
	ServerContext serv = ServerContext("127.0.0.1", 8080);
	serv.run_server();
	for(;;) {
		message_t msg;
		if(serv.message_queue_in.try_dequeue(msg)) {
			serv.message_queue_out.enqueue(msg);
		}
	}
}
