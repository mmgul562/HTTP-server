#ifndef HTTP_SERVER_RESPONSE_H
#define HTTP_SERVER_RESPONSE_H

#include "request.h"


void send_http_response(HttpRequest *request, int client_socket);

void send_failure_response(RequestParsingStatus status, int client_socket);


#endif
