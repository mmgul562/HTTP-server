#ifndef HTTP_SERVER_TASK_H
#define HTTP_SERVER_TASK_H

#include <libpq-fe.h>


typedef struct {
    int client_socket;
    PGconn *db_conn;
} Task;


#endif
