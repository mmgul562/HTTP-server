#ifndef HTTP_SERVER_TODOS_H
#define HTTP_SERVER_TODOS_H

#include <libpq-fe.h>
#include <stdbool.h>


typedef struct {
    char *summary;
    char *task;
    char *due_time;
    bool completed;
} Todo;

bool db_create_todo(PGconn *conn, Todo *todo);


#endif