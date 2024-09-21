#ifndef HTTP_SERVER_HANDLERS_H
#define HTTP_SERVER_HANDLERS_H

#include "route.h"


void handle_http_request(HttpRequest *req, Task *context);


#endif
