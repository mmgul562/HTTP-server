#ifndef HTTP_SERVER_SESSION_MIDDLEWARE_H
#define HTTP_SERVER_SESSION_MIDDLEWARE_H

#include "../http/response.h"


bool check_session(HttpRequest *req, Task *context);

bool extract_session_token(const char *cookie_header, char *session_token, size_t max_length);


#endif