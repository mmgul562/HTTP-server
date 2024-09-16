#ifndef HTTP_SERVER_SESSION_MIDDLEWARE_H
#define HTTP_SERVER_SESSION_MIDDLEWARE_H

#include "../http/request.h"
#include "../http/util/task.h"

#define MAX_TOKEN_LENGTH 64


int check_session(HttpRequest *req, Task *context, char *csrf_token);

int check_and_retrieve_session(HttpRequest *req, Task *context, char *csrf_token, char *session_token, size_t max_length);

bool check_csrf_token(HttpRequest *req, const char *expected_csrf_token);


#endif