#ifndef HTTP_SERVER_RESPONSE_H
#define HTTP_SERVER_RESPONSE_H

#include "request.h"
#include "server.h"
#include <libpq-fe.h>


typedef enum {
    GET,
    POST
    // will add other later
} Method;

typedef struct {
    const char *url;
    Method method;
    void (*handler)(HttpRequest *, Task *);
} Route;

void send_http_response(HttpRequest *request, Task *context);

void send_failure_response(RequestParsingStatus status, int client_socket);


#endif
