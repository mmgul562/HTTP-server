#ifndef HTTP_SERVER_SERVER_H
#define HTTP_SERVER_SERVER_H

#include <arpa/inet.h>


typedef struct {
    int server_socket;
    struct sockaddr_in server_addr;
    int port;
} Server;


int server_init(Server *server, int port);

void server_run(Server *server);

void handle_client(int client_socket);

#endif
