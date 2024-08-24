#include "response.h"
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_PATH_LENGTH 304
#define DOCUMENT_ROOT "../src/http/www"


const char *get_content_type(const char *path) {
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


int is_path_safe(const char *path) {
    char resolved_path[MAX_PATH_LENGTH];
    char resolved_root[MAX_PATH_LENGTH];

    if (realpath(path, resolved_path) == NULL) {
        if (errno == ENOENT) {
            return 1;   // 404 will be sent shortly after
        }
        return 0;
    }
    if (realpath(DOCUMENT_ROOT, resolved_root) == NULL) {
        return 0;
    }

    return (strncmp(resolved_path, resolved_root, strlen(resolved_root)) == 0);
}


void send_headers(int client_socket, int status_code, const char *status_text, const char *content_type, const char *other) {
    char response_header[1024];
    if (!other) other = "";
    sprintf(response_header, "HTTP/1.1 %d %s\r\n"
                             "Content-Type: %s\r\n"
                             "%s"
                             "\r\n",
            status_code, status_text, content_type, other);

    send(client_socket, response_header, strlen(response_header), 0);
}


void send_html(int client_socket, const char *file_path) {
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return;
    }
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }
    close(fd);
}


void send_http_response(HttpRequest *request, int client_socket) {
    char file_path[MAX_PATH_LENGTH];
    snprintf(file_path, sizeof(file_path), "%s%s", DOCUMENT_ROOT, request->path);
    if (strcmp(request->path, "/") == 0) {
        strcat(file_path, "index.html");
    }

    if (!is_path_safe(file_path)) {
        send_headers(client_socket, 403, "Forbidden", "text/html", NULL);
        snprintf(file_path, sizeof(file_path), "%s/errors/403.html", DOCUMENT_ROOT);
        send_html(client_socket, file_path);
        return;
    }

    if (access(file_path, F_OK) != 0) {
        send_headers(client_socket, 404, "Not Found", "text/html", NULL);
        snprintf(file_path, sizeof(file_path), "%s/errors/404.html", DOCUMENT_ROOT);
        send_html(client_socket, file_path);
        return;
    }

    struct stat file_stat;
    if (stat(file_path, &file_stat) != 0) {
        perror("Error getting file stats");
        send_headers(client_socket, 500, "Internal Server Error", "text/html", NULL);
        snprintf(file_path, sizeof(file_path), "%s/errors/500.html", DOCUMENT_ROOT);
        send_html(client_socket, file_path);
        return;
    }

    off_t file_size = file_stat.st_size;
    const char *content_type = get_content_type(file_path);
    char content_length[64];
    snprintf(content_length, sizeof(content_length), "Content-Length: %ld\r\n", file_size);

    send_headers(client_socket, 200, "OK", content_type, content_length);
    send_html(client_socket, file_path);
}


void send_failure_response(RequestParsingStatus status, int client_socket) {
    char path[MAX_PATH_LENGTH];
    if (status == REQ_PARSE_INVALID_FORMAT) {
        snprintf(path, sizeof(path), "%s/errors/400.html", DOCUMENT_ROOT);
        send_headers(client_socket, 400, "Bad Request", "text/html", NULL);
    } else {
        snprintf(path, sizeof(path), "%s/errors/500.html", DOCUMENT_ROOT);
        send_headers(client_socket, 500, "Internal Server Error", "text/html", NULL);
    }
    send_html(client_socket, path);
}