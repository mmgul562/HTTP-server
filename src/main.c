#include "http/server.h"
#include <stdlib.h>
#include <unistd.h>

#define PORT 8080
#define THREAD_POOL_SIZE 10


int main() {
    Server server;
    if (server_init(&server, PORT, THREAD_POOL_SIZE) != 0) {
        exit(EXIT_FAILURE);
    }

    server_run(&server);

    close(server.server_socket);
    return 0;
}