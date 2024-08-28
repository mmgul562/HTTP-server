#include "todos.h"
#include <stdio.h>
#include <string.h>


static bool db_create_todo_no_duetime(PGconn *conn, Todo *todo) {
    const char *params[2] = {todo->summary, todo->task};
    const char *query = "INSERT INTO todos (summary, task) VALUES ($1, $2)";
    int param_lengths[2] = {strlen(todo->summary), strlen(todo->task)};
    int param_formats[2] = {0, 0};

    PGresult *res = PQexecParams(conn, query, 2, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "INSERT failed: %s", PQerrorMessage(conn));
        return false;
    }

    PQclear(res);
    return true;
}


bool db_create_todo(PGconn *conn, Todo *todo) {
    if (!todo->due_time) {
        return db_create_todo_no_duetime(conn, todo);
    }
    const char *params[3] = {todo->summary, todo->task, todo->due_time};
    const char *query = "INSERT INTO todos (summary, task, due_time) VALUES ($1, $2, $3)";
    int param_lengths[3] = {strlen(todo->summary), strlen(todo->task), strlen(todo->due_time)};
    int param_formats[3] = {0, 0, 0};

    PGresult *res = PQexecParams(conn, query, 3, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "INSERT failed: %s", PQerrorMessage(conn));
        return false;
    }

    PQclear(res);
    return true;
}