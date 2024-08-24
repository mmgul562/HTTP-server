#include "request.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


RequestParsingStatus parse_http_request(char *buffer, HttpRequest *request) {
    char *req_line_end = strstr(buffer, "\r\n");
    if (!req_line_end) {
        return REQ_PARSE_INVALID_FORMAT;
    }
    *req_line_end = '\0';

    char *method = strtok(buffer, " ");
    char *path = strtok(NULL, " ");
    char *protocol = strtok(NULL, " ");
    if (!method || !path || !protocol) {
        return REQ_PARSE_INVALID_FORMAT;
    }

    if (strlen(method) >= sizeof(request->method) ||
        strlen(path) >= sizeof(request->path) ||
        path[0] != '/' ||
        strlen(protocol) >= sizeof(request->protocol) ||
        strncmp(protocol, "HTTP/", 5) != 0) {
        return REQ_PARSE_INVALID_FORMAT;
    }
    strcpy(request->method, method);
    strcpy(request->path, path);
    strcpy(request->protocol, protocol);

    *req_line_end = '\r';
    req_line_end += 2;
    char *headers_end = strstr(req_line_end, "\r\n\r\n");
    if (headers_end) {
        *headers_end = '\0';

        size_t headers_length = headers_end - req_line_end;
        request->headers = malloc(headers_length + 1);
        if (!request->headers) {
            perror("Failed to allocate memory for request headers");
            return REQ_PARSE_MEMORY_FAILURE;
        }
        strcpy(request->headers, req_line_end);

        headers_end += 4;
        size_t body_length = strlen(headers_end);
        if (body_length == 0) {
            request->body = NULL;
        } else {
            request->body = malloc(body_length + 1);
            if (!request->body) {
                perror("Failed to allocate memory for request body");
                return REQ_PARSE_MEMORY_FAILURE;
            }
            strcpy(request->body, headers_end);
        }
    } else {
        if (!strstr(req_line_end, "\r\n")) {
            return REQ_PARSE_INVALID_FORMAT;
        }
        request->headers = NULL;
    }
    return REQ_PARSE_SUCCESS;
}


void free_http_request(HttpRequest *request) {
    if (request->headers) {
        free(request->headers);
    }
    if (request->body) {
        free(request->body);
    }
}