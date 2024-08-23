#ifndef HTTP_SERVER_REQUEST_H
#define HTTP_SERVER_REQUEST_H


typedef struct {
    char method[8];
    char path[256];
    char protocol[16];
    char *headers;
    char *body;
} HttpRequest;


typedef enum {
    REQ_PARSE_SUCCESS,
    REQ_PARSE_MEMORY_FAILURE,
    REQ_PARSE_INVALID_FORMAT,
} RequestParsingStatus;


RequestParsingStatus parse_http_request(char *buffer, HttpRequest *request);

void send_http_response(HttpRequest *request, int client_socket);

void send_failure_response(RequestParsingStatus status, int client_socket);

void free_http_request(HttpRequest *request);

#endif
