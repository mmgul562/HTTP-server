#ifndef HTTP_SERVER_SERVER_H
#define HTTP_SERVER_SERVER_H

#include <arpa/inet.h>
#include <libpq-fe.h>


typedef struct {
    int client_socket;
    PGconn *db_conn;
} Task;

typedef struct {
    int server_socket;
    struct sockaddr_in server_addr;
    int port;
    int thread_pool_size;
    PGconn *db_conn;
} Server;

int server_init(Server *server, int port, int thread_pool_size);

void server_run(Server *server);


#endif
