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

int db_get_total_todos_count(PGconn *conn);

Todo *db_get_all_todos(PGconn *conn, int *count, int page, int page_size);

bool db_create_todo(PGconn *conn, Todo *todo);

bool db_update_todo(PGconn *conn, Todo *todo);

bool db_delete_todo(PGconn *conn, int id);

void free_todos(Todo *todos, int count);


#endif