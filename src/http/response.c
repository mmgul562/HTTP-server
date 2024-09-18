#include "response.h"
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_PATH_LENGTH 256
#define MAX_ERROR_JSON_LENGTH 512
#define DOCUMENT_ROOT "../src/http/www"


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


void send_headers(int client_socket, int status_code, const char *content_type, const char *other) {
    char response_header[1024];
    char content_type_h[64] = "";
    if (content_type) {
        snprintf(content_type_h, sizeof(content_type_h), "Content-Type: %s\r\n", content_type);
    }
    if (!other) other = "";

    const char *status_text;
    switch (status_code) {
        case 200:
            status_text = "OK";
            break;
        case 201:
            status_text = "Created";
            break;
        case 204:
            status_text = "No Content";
            break;
        case 303:
            status_text = "See Other";
            break;
        case 400:
            status_text = "Bad Request";
            break;
        case 401:
            status_text = "Unauthorized";
            break;
        case 404:
            status_text = "Not Found";
            break;
        case 405:
            status_text = "Method Not Allowed";
            break;
        case 409:
            status_text = "Conflict";
            break;
        case 415:
            status_text = "Unsupported Media Type";
            break;
        default:
            status_text = "Internal Server Error";
            status_code = 500;
    }

    sprintf(response_header, "HTTP/1.1 %d %s\r\n"
                             "%s"
                             "%s"
                             "\r\n",
            status_code, status_text, content_type_h, other);

    send(client_socket, response_header, strlen(response_header), 0);
}


static void send_file(int client_socket, int fd) {
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }
}


void send_error_message(int client_socket, int status_code, const char *message) {
    char err_message[MAX_ERROR_JSON_LENGTH];
    sprintf(err_message, "{\"error\": {\"message\": \"%s\"}}\n", message);
    char content_length[64];
    sprintf(content_length, "Content-Length: %ld\r\n", strlen(err_message));

    send_headers(client_socket, status_code, "application/json", content_length);
    send(client_socket, err_message, sizeof(err_message), 0);
}


void try_sending_error_file(int client_socket, int status_code) {
    char err_path[MAX_PATH_LENGTH];
    snprintf(err_path, sizeof(err_path), DOCUMENT_ROOT"/errors/%d.html", status_code);

    int fd = open(err_path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening error file");
        if (status_code == 500) {
            send_headers(client_socket, 500, "text/html", NULL);
            const char err_msg[] = "<h1>Internal Server Error</h1>\r\n"
                                   "\t<p>Sorry, something went wrong on our side. Please try again later.</p>\r\n";
            send(client_socket, err_msg, sizeof(err_msg), 0);
        } else {
            try_sending_error_file(client_socket, 500);
        }
        return;
    }
    struct stat file_stat;
    if (stat(err_path, &file_stat) != 0) {
        perror("Error getting file stats");
        try_sending_error_file(client_socket, 500);
        return;
    }

    off_t file_size = file_stat.st_size;
    char content_length[64];
    snprintf(content_length, sizeof(content_length), "Content-Length: %ld\r\n", file_size);

    send_headers(client_socket, status_code, "text/html", content_length);
    send_file(client_socket, fd);
    close(fd);
}


void try_sending_file(int client_socket, const char *file_path) {
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        try_sending_error_file(client_socket, 500);
        return;
    }
    struct stat file_stat;
    if (stat(file_path, &file_stat) != 0) {
        perror("Error getting file stats");
        try_sending_error_file(client_socket, 500);
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


void handle_invalid_http_request(RequestParsingStatus status, int client_socket) {
    if (status == REQ_PARSE_INVALID_FORMAT) {
        send_error_message(client_socket, 400, "Invalid HTTP request");
    } else {
        try_sending_error_file(client_socket, 500);
    }
}