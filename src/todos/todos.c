#include "todos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


Todo *db_get_all_todos(PGconn *conn, int *count) {
    const char *query = "SELECT id, creation_time, summary, task, due_time FROM todos ORDER BY id";
    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return NULL;
    }
    *count = PQntuples(res);
    Todo *todos = malloc(*count * sizeof(Todo));
    for (int i = 0; i < *count; ++i) {
        todos[i].creation_time = strdup(PQgetvalue(res, i, 1));
        todos[i].summary = strdup(PQgetvalue(res, i, 2));
        todos[i].task = strdup(PQgetvalue(res, i, 3));
        todos[i].due_time = PQgetisnull(res, i, 4) ? NULL : strdup(PQgetvalue(res, i, 4));
    }
    PQclear(res);
    return todos;
}


void free_todos(Todo *todos, int count) {
    for (int i = 0; i < count; ++i) {
        free(todos[i].creation_time);
        free(todos[i].summary);
        free(todos[i].task);
        free(todos[i].due_time);
    }
    free(todos);
}


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