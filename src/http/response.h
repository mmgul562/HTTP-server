#ifndef HTTP_SERVER_RESPONSE_H
#define HTTP_SERVER_RESPONSE_H

#include "request.h"


void send_http_response(HttpRequest *request, int client_socket);

void send_html(int client_socket, const char *file_path);

void send_headers(int client_socket, int status_code, const char *status_text, const char *content_type, const char *other);

void send_failure_response(RequestParsingStatus status, int client_socket);

#endif
