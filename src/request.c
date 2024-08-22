#include "request.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>


int parse_http_request(char *buffer, HttpRequest *request) {
    if (sscanf(buffer, "%7s %255s %15s", request->method, request->path, request->protocol) != 3) {
        return -1;
    }
    char *req_line_end = strstr(buffer, "\r\n");
    if (!req_line_end) {
        return -1;
    }

    req_line_end += 2;
    char *headers_end = strstr(req_line_end, "\r\n\r\n");
    if (headers_end) {
        headers_end += 2;
        *headers_end = '\0';

        size_t headers_length = headers_end - req_line_end;
        request->headers = malloc(headers_length + 1);
        if (request->headers) {
            strcpy(request->headers, req_line_end);
        }

        headers_end += 2;
        size_t body_length = strlen(headers_end);
        if (body_length == 0) {
            request->body = NULL;
        } else {
            request->body = malloc(body_length + 1);
            if (request->body) {
                strcpy(request->body, headers_end);
            }
        }
    } else {
        if (!strstr(req_line_end, "\r\n")) {
            return -1;
        }
        request->headers = NULL;
    }
    return 0;
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
        sprintf(response, "HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/plain\r\n\r\nMethod %s not allowed\n", request->method);
    }
    send(client_socket, response, strlen(response), 0);
}


void free_http_request(HttpRequest *request) {
    if (request->headers) {
        free(request->headers);
    }
    if (request->body) {
        free(request->body);
    }
}