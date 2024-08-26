#include "response.h"
#include "router.h"
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_PATH_LENGTH 304
#define DOCUMENT_ROOT "../src/http/www"


static const char *get_route_file(const char *url) {
    for (int i = 0; i < ROUTES_COUNT; ++i) {
        if (strcmp(url, ROUTES[i].url) == 0) {
            return ROUTES[i].file;
        }
    }
    return NULL;
}


static const char *get_content_type(const char *path) {
    const char *extension = strrchr(path, '.');
    if (extension == NULL) return "application/octet-stream";
    if (strcmp(extension, ".html") == 0 || strcmp(extension, ".htm") == 0) return "text/html";
    if (strcmp(extension, ".css") == 0) return "text/css";
    if (strcmp(extension, ".js") == 0) return "application/javascript";
    if (strcmp(extension, ".jpg") == 0 || strcmp(extension, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(extension, ".png") == 0) return "image/png";
    if (strcmp(extension, ".gif") == 0) return "image/gif";
    return "application/octet-stream";
}


static int is_path_safe(const char *path) {
    char resolved_path[MAX_PATH_LENGTH];
    char resolved_root[MAX_PATH_LENGTH];

    if (realpath(path, resolved_path) == NULL) {
        return 0;
    }
    if (realpath(DOCUMENT_ROOT, resolved_root) == NULL) {
        perror("Invalid DOCUMENT_ROOT");
        return 0;
    }

    return (strncmp(resolved_path, resolved_root, strlen(resolved_root)) == 0);
}


static void send_headers(int client_socket, int status_code, const char *content_type, const char *other) {
    char response_header[1024];
    if (!other) other = "";

    const char *status_text;
    switch (status_code) {
        case 400:
            status_text = "Bad Request";
            break;
        case 403:
            status_text = "Forbidden";
            break;
        case 404:
            status_text = "Not Found";
            break;
        case 500:
            status_text = "Internal Server Error";
            break;
        default:
            status_text = "Internal Server Error";
    }

    sprintf(response_header, "HTTP/1.1 %d %s\r\n"
                             "Content-Type: %s\r\n"
                             "%s"
                             "\r\n",
            status_code, status_text, content_type, other);

    send(client_socket, response_header, strlen(response_header), 0);
}


static void send_file(int client_socket, int fd) {
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }
}


static void try_sending_error(int client_socket, int status_code) {
    char err_path[MAX_PATH_LENGTH];
    snprintf(err_path, sizeof(err_path), DOCUMENT_ROOT"/errors/%d.html", status_code);
    int fd = open(err_path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening error file");
        if (status_code == 500) {
            send_headers(client_socket, 500, "text/html", NULL);
            send(client_socket, "<h1>Internal Server Error</h1>\r\n"
                                "\t<p>Sorry, something went wrong on our side. Please try again later.</p>\r\n",
                 30, 0);
        } else {
            try_sending_error(client_socket, 500);
        }
        return;
    }
    send_headers(client_socket, status_code, "text/html", NULL);
    send_file(client_socket, fd);
    close(fd);
}


static void try_sending_file(int client_socket, const char *file_path) {
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        try_sending_error(client_socket, 500);
        return;
    }
    struct stat file_stat;
    if (stat(file_path, &file_stat) != 0) {
        perror("Error getting file stats");
        try_sending_error(client_socket, 500);
        return;
    }

    off_t file_size = file_stat.st_size;
    const char *content_type = get_content_type(file_path);
    char content_length[64];
    snprintf(content_length, sizeof(content_length), "Content-Length: %ld\r\n", file_size);

    send_headers(client_socket, 200, content_type, content_length);
    send_file(client_socket, fd);
    close(fd);
}


void send_http_response(HttpRequest *request, int client_socket) {
    char file_path[MAX_PATH_LENGTH];
    const char *route_file = get_route_file(request->path);
    if (route_file) {
        snprintf(file_path, sizeof(file_path), "%s/%s", DOCUMENT_ROOT, route_file);
    } else {
        snprintf(file_path, sizeof(file_path), "%s/static%s", DOCUMENT_ROOT, request->path);
        if (!is_path_safe(file_path)) {
            try_sending_error(client_socket, 404);
            return;
        }
    }

    try_sending_file(client_socket, file_path);
}


void send_failure_response(RequestParsingStatus status, int client_socket) {
    if (status == REQ_PARSE_INVALID_FORMAT) {
        try_sending_error(client_socket, 400);
    } else {
        try_sending_error(client_socket, 500);
    }
}