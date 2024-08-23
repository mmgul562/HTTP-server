#include "request.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>


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

// will add a proper router later
void send_http_response(HttpRequest *request, int client_socket) {
    char response[1024];
    if (strcmp(request->method, "GET") == 0) {
        if (strcmp(request->path, "/") == 0) {
            sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>Hello, World!</h1>\n");
        } else {
            sprintf(response, "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>Page not found!</h1>\n");
        }
    } else {
        sprintf(response, "HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/plain\r\n\r\nMethod %s not allowed\n",
                request->method);
    }
    send(client_socket, response, strlen(response), 0);
}


void send_failure_response(RequestParsingStatus status, int client_socket) {
    const char *error_msg;
    switch (status) {
        case REQ_PARSE_INVALID_FORMAT:
            error_msg = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid request format\n";
            break;
        case REQ_PARSE_MEMORY_FAILURE:
            error_msg = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nServer encountered an error\n";
            break;
        default:
            error_msg = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nUnknown error occurred\n";
    }
    send(client_socket, error_msg, strlen(error_msg), 0);
}


void free_http_request(HttpRequest *request) {
    if (request->headers) {
        free(request->headers);
    }
    if (request->body) {
        free(request->body);
    }
}