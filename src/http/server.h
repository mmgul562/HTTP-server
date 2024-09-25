#ifndef HTTP_SERVER_SERVER_H
#define HTTP_SERVER_SERVER_H

#include <arpa/inet.h>
#include "util/task.h"

#define CONN_POOL_SIZE 10
#define THREAD_POOL_SIZE 10


typedef struct {
    PGconn *conn;
    bool in_use;
} PooledConnection;

typedef struct {
    PooledConnection connections[CONN_POOL_SIZE];
    pthread_mutex_t mutex;
} ConnectionPool;

typedef struct {
    int server_socket;
    struct sockaddr_in server_addr;
    int port;
    ConnectionPool conns;
} Server;

bool server_init(Server *server, int port);

void server_run(Server *server);

void cleanup_connection_pool(ConnectionPool *pool);


#endif
