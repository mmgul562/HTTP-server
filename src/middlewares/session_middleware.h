#ifndef HTTP_SERVER_SESSION_MIDDLEWARE_H
#define HTTP_SERVER_SESSION_MIDDLEWARE_H

#include "../http/request.h"
#include "../http/util/task.h"
#include "../db/util/query_result.h"

#define MAX_TOKEN_LENGTH 64


QueryResult check_session(const char *headers, PGconn *conn, int *user_id, char *csrf_token);

QueryResult check_and_retrieve_session(const char *headers, PGconn *conn, int *user_id, char *csrf_token, char *session_token, size_t max_length);

bool check_csrf_token(HttpRequest *req, const char *expected_csrf_token);


#endif