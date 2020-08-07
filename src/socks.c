#include "net.h"
#include <stdio.h>

int main(int argc, char** argv) {
	struct net_ctx* ctx = create_server();
	run_server(ctx);
	while(1) {
		char s[256];
		fgets(s, 256, stdin);
		printf("%s", s);
	}

}
