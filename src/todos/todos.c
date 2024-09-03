#include "todos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int db_get_total_todos_count(PGconn *conn) {
    const char *query = "SELECT COUNT(*) FROM todos";
    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "COUNT failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}


Todo *db_get_all_todos(PGconn *conn, int *count, int page, int page_size) {
    char query[128];
    sprintf(query, "SELECT id, creation_time, summary, task, due_time FROM todos "
                   "ORDER BY -id LIMIT %d OFFSET %d",
                   page_size, (page - 1) * page_size);
    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return NULL;
    }
    *count = PQntuples(res);
    Todo *todos = malloc(*count * sizeof(Todo));
    for (int i = 0; i < *count; ++i) {
        todos[i].id = atoi(PQgetvalue(res, i, 0));
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


static bool db_update_todo_no_duetime(PGconn *conn, Todo *todo) {
    char id_str[10];
    snprintf(id_str, sizeof(id_str), "%d", todo->id);
    const char *params[3] = {todo->summary, todo->task, id_str};
    const char *query = "UPDATE todos SET summary = $1, task = $2 WHERE id = $3";
    int param_lengths[3] = {strlen(todo->summary), strlen(todo->task), strlen(id_str)};
    int param_formats[3] = {0, 0, 0};

    PGresult *res = PQexecParams(conn, query, 3, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE failed: %s", PQerrorMessage(conn));
        return false;
    }

    PQclear(res);
    return true;
}


bool db_update_todo(PGconn *conn, Todo *todo) {
    if (!todo->due_time) {
        return db_update_todo_no_duetime(conn, todo);
    }
    char id_str[10];
    snprintf(id_str, sizeof(id_str), "%d", todo->id);
    const char *params[4] = {todo->summary, todo->task, todo->due_time, id_str};
    const char *query = "UPDATE todos SET summary = $1, task = $2, due_time = $3 WHERE id = $4";
    int param_lengths[4] = {strlen(todo->summary), strlen(todo->task), strlen(todo->due_time), strlen(id_str)};
    int param_formats[4] = {0, 0, 0, 0};

    PGresult *res = PQexecParams(conn, query, 4, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE failed: %s", PQerrorMessage(conn));
        return false;
    }

    int affected_rows = atoi(PQcmdTuples(res));
    PQclear(res);

    return affected_rows > 0;
}


bool db_delete_todo(PGconn *conn, int id) {
    const char *query = "DELETE FROM todos WHERE id = $1";
    char id_str[10];
    snprintf(id_str, sizeof(id_str), "%d", id);
    const char *param_values[1] = {id_str};

    PGresult *res = PQexecParams(conn, query, 1, NULL, param_values, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "DELETE failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }

    int affected_rows = atoi(PQcmdTuples(res));
    PQclear(res);

    return affected_rows > 0;
}