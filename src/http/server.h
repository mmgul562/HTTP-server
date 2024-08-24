#ifndef HTTP_SERVER_SERVER_H
#define HTTP_SERVER_SERVER_H

#include <arpa/inet.h>
#include <pthread.h>


typedef struct {
    int server_socket;
    struct sockaddr_in server_addr;
    int port;
    int thread_pool_size;
} Server;

int server_init(Server *server, int port, int thread_pool_size);

void server_run(Server *server);

void *handle_client(void *client_socket);


#endif
