#ifndef HTTP_SERVER_RESPONSE_H
#define HTTP_SERVER_RESPONSE_H

#include "request.h"


void send_headers(int client_socket, int status_code, const char *content_type, const char *other);

void try_sending_error_file(int client_socket, int status_code);

void send_error_message(int client_socket, int status_code, const char *message);

void try_sending_file(int client_socket, const char *file_path);

void handle_invalid_http_request(RequestParsingStatus status, int client_socket);


#endif
