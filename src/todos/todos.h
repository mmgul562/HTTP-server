#ifndef HTTP_SERVER_TODOS_H
#define HTTP_SERVER_TODOS_H

#include <libpq-fe.h>
#include <stdbool.h>


typedef struct {
    int id;
    char *creation_time;
    char *summary;
    char *task;
    char *due_time;
} Todo;

bool db_create_todo(PGconn *conn, Todo *todo);

Todo *db_get_all_todos(PGconn *conn, int *count);

bool db_update_todo(PGconn *conn, Todo *todo);

bool db_delete_todo(PGconn *conn, int id);

void free_todos(Todo *todos, int count);


#endif