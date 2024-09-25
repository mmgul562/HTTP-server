#ifndef HTTP_SERVER_HANDLERS_H
#define HTTP_SERVER_HANDLERS_H

#include "route.h"


void handle_http_request(HttpRequest *req, Task *context);

bool needs_db_conn(HttpRequest *req);


#endif
