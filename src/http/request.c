#include "request.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


static int convert_method_str(const char *method) {
    if (strcmp(method, "GET") == 0) {
        return GET;
    } else if (strcmp(method, "POST") == 0) {
        return POST;
    } else if (strcmp(method, "DELETE") == 0) {
        return DELETE;
    } else if (strcmp(method, "PATCH") == 0) {
        return PATCH;
    }
    return -1;
}


RequestParsingStatus parse_http_request(char *buffer, HttpRequest *request) {
    char *req_line_end = strstr(buffer, "\r\n");
    if (!req_line_end) {
        return REQ_PARSE_INVALID_FORMAT;
    }
    *req_line_end = '\0';

    char *method_str = strtok(buffer, " ");
    char *path = strtok(NULL, " ");
    char *protocol = strtok(NULL, " ");
    char *query_string_path = strtok(path, "?");
    char *query_string = NULL;
    if (query_string_path) {
        path = query_string_path;
        query_string = strtok(NULL, "\r");
    }
    if (!method_str || !path || !protocol) {
        return REQ_PARSE_INVALID_FORMAT;
    }

    if (strlen(method_str) >= 8 ||
        strlen(path) >= sizeof(request->path) ||
        path[0] != '/' ||
        strlen(protocol) >= sizeof(request->protocol) ||
        strncmp(protocol, "HTTP/", 5) != 0) {
        return REQ_PARSE_INVALID_FORMAT;
    }
    Method method = convert_method_str(method_str);
    request->method = method;
    strcpy(request->path, path);
    if (query_string) request->query_string = strdup(query_string);
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
            if (request->query_string) free(request->query_string);
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
                if (request->query_string) free(request->query_string);
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
    if (request->query_string) {
        free(request->query_string);
    }
    if (request->headers) {
        free(request->headers);
    }
    if (request->body) {
        free(request->body);
    }
}