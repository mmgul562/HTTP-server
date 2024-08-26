#ifndef HTTP_SERVER_RESPONSE_H
#define HTTP_SERVER_RESPONSE_H

#include "request.h"


typedef enum {
    GET,
    POST
    // will add other later
} Method;

typedef struct {
    const char *url;
    Method method;
    void (*handler)(int client_socket);
} Route;

void send_http_response(HttpRequest *request, int client_socket);

void send_failure_response(RequestParsingStatus status, int client_socket);


#endif
