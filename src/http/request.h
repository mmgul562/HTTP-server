#ifndef HTTP_SERVER_REQUEST_H
#define HTTP_SERVER_REQUEST_H


typedef struct {
    char method[8];
    char *query_string;
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

void free_http_request(HttpRequest *request);

char* extract_form_value(const char* body, const char* key);

char* url_decode(const char* src);


#endif
