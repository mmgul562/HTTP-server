#ifndef HTTP_SERVER_SESSION_MIDDLEWARE_H
#define HTTP_SERVER_SESSION_MIDDLEWARE_H

#include "../http/request.h"
#include "../http/util/task.h"


int check_session(HttpRequest *req, Task *context);

bool extract_session_token(const char *cookie_header, char *session_token, size_t max_length);


#endif