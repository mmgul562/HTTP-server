#include "request.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


static char hex_to_char(char c) {
    if (c >= '0' && c <= '9') return (char)(c - '0');
    if (c >= 'a' && c <= 'f') return (char)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (char)(c - 'A' + 10);
    return -1;
}


char *url_decode(const char *src) {
    size_t src_len = strlen(src);
    char *decoded = malloc(src_len + 1);
    size_t i, j;

    for (i = 0, j = 0; i < src_len; ++i, ++j) {
        if (src[i] == '%' && i + 2 < src_len) {
            char high = hex_to_char(src[i + 1]);
            char low = hex_to_char(src[i + 2]);
            if (high >= 0 && low >= 0) {
                decoded[j] = (char)((high << 4) | low);
                i += 2;
            } else {
                decoded[j] = src[i];
            }
        } else if (src[i] == '+') {
            decoded[j] = ' ';
        } else {
            decoded[j] = src[i];
        }
    }
    decoded[j] = '\0';
    return decoded;
}


char *extract_form_value(const char *body, const char *key) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "%s=", key);

    char *start = strstr(body, search_key);
    if (!start) return NULL;
    start += strlen(search_key);

    char *end = strchr(start, '&');
    if (!end) end = start + strlen(start);

    size_t len = end - start;
    char *value = malloc(len + 1);
    strncpy(value, start, len);
    value[len] = '\0';

    char *decoded = url_decode(value);
    free(value);
    return decoded;
}


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