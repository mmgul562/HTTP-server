#include "server.h"
#include "request.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>


volatile sig_atomic_t keep_running = 1;

void signal_handler(int signum) {
    keep_running = 0;
}


int server_init(Server *server, int port) {
    server->port = port;
    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket == -1) {
        perror("Socket creation failed");
        return -1;
    }

    server->server_addr.sin_family = AF_INET;
    server->server_addr.sin_addr.s_addr = INADDR_ANY;
    server->server_addr.sin_port = htons(port);

    if (bind(server->server_socket, (struct sockaddr *) &server->server_addr, sizeof(server->server_addr)) < 0) {
        perror("Bind failed");
        return -1;
    }

    if (listen(server->server_socket, 10) < 0) {
        perror("Listen failed");
        return -1;
    }

    return 0;
}


void server_run(Server *server) {
    printf("Server listening on port %d\n", server->port);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    while (keep_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server->server_socket, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(server->server_socket + 1, &read_fds, NULL, NULL, &timeout);

        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("Select failed");
            break;
        } else if (ready == 0) {
            continue;
        }

        int client_socket = accept(server->server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("Accept failed");
            continue;
        }

        printf("New connection accepted\n");
        handle_client(client_socket);
    }

    printf("Server shutting down...\n");
}


void handle_client(int client_socket) {
    char *buffer = NULL;
    size_t buffer_size = 0;
    size_t total_bytes = 0;
    ssize_t bytes_received;

    while (1) {
        buffer_size += 1024;
        char *new_buffer = realloc(buffer, buffer_size);
        if (!new_buffer) {
            perror("Failed to allocate memory");
            free(buffer);
            close(client_socket);
            return;
        }
        buffer = new_buffer;

        bytes_received = recv(client_socket, buffer + total_bytes, buffer_size - total_bytes - 1, 0);

        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                perror("recv failed");
                free(buffer);
                close(client_socket);
                return;
            }
        } else if (bytes_received == 0) {
            break;
        }
        total_bytes += bytes_received;

        if (strstr(buffer, "\r\n\r\n")) {
            break;
        }
        if (total_bytes >= buffer_size - 1) {
            continue;
        }
    }

    if (total_bytes > 0) {
        buffer[total_bytes] = '\0';
        HttpRequest request;
        memset(&request, 0, sizeof(HttpRequest));

        if (parse_http_request(buffer, &request) != 0) {
            char error_msg[] = "No HTTP request";
            send(client_socket, error_msg, sizeof(error_msg) - 1, 0);
        } else {
            printf("Received request:\n");
            printf("Method: %s\n", request.method);
            printf("Path: %s\n", request.path);
            printf("Protocol: %s\n", request.protocol);
            printf("Headers:\n%s\n", request.headers ? request.headers : "");
            printf("Body: %s\n\n", request.body ? request.body : "");

            send_http_response(&request, client_socket);
            free_http_request(&request);
        }
    }

    free(buffer);
    close(client_socket);
}