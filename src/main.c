#include "http/server.h"
#include <stdlib.h>
#include <unistd.h>

#define PORT 8080


int main() {
    Server server;
    if (!server_init(&server, PORT)) {
        exit(EXIT_FAILURE);
    }

    server_run(&server);

    close(server.server_socket);
    cleanup_connection_pool(&server.conns);

    return 0;
}