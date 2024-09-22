#ifndef HTTP_SERVER_TODOS_H
#define HTTP_SERVER_TODOS_H

#include "util/query_result.h"
#include <libpq-fe.h>

#define DB_SUMMARY_LEN 128
#define DB_TASK_LEN 2048


typedef struct {
    int id;
    int user_id;
    char *creation_time;
    char *summary;
    char *task;
    char *due_time;
} Todo;

int db_get_total_todos_count(PGconn *conn, int user_id);

Todo *db_get_all_todos(PGconn *conn, int user_id, int *count, int page, int page_size);

bool db_create_todo(PGconn *conn, Todo *todo);

QueryResult db_update_todo(PGconn *conn, Todo *todo);

QueryResult db_delete_todo(PGconn *conn, int id, int user_id);

void free_todos(Todo *todos, int count);


#endif