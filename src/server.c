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


int server_init(Server *server, int port, int thread_pool_size) {
    server->port = port;
    server->thread_pool_size = thread_pool_size;
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

    pthread_t threads[server->thread_pool_size];
    int thread_idx = 0;

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

        int *client_socket = malloc(sizeof(int));
        if (client_socket == NULL) {
            perror("Failed to allocate memory for client socket");
            continue;
        }
        *client_socket = accept(server->server_socket, (struct sockaddr *) &client_addr, &client_len);
        if (*client_socket < 0) {
            if (errno == EINTR) {
                free(client_socket);
                continue;
            }
            perror("Accept failed");
            free(client_socket);
            continue;
        }

        printf("New connection accepted\n");

        if (pthread_create(&threads[thread_idx], NULL, handle_client, (void *) client_socket) != 0) {
            perror("Failed to create client thread");
            close(*client_socket);
            free(client_socket);
        } else {
            pthread_detach(threads[thread_idx]);
            thread_idx = (thread_idx + 1) % server->thread_pool_size;
        }
    }

    printf("Server shutting down...\n");
}


void *handle_client(void *client_socket_ptr) {
    int client_socket = *(int *) client_socket_ptr;
    free(client_socket_ptr);

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
            return NULL;
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
                return NULL;
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

        RequestParsingStatus status = parse_http_request(buffer, &request);
        if (status == REQ_PARSE_SUCCESS) {
            printf("Received request:\n");
            printf("Method: %s\n", request.method);
            printf("Path: %s\n", request.path);
            printf("Protocol: %s\n", request.protocol);
            printf("Headers:\n%s\n", request.headers ? request.headers : "");
            printf("Body: %s\n\n", request.body ? request.body : "");

            send_http_response(&request, client_socket);
            free_http_request(&request);
        } else {
            send_failure_response(status, client_socket);
        }

        free(buffer);
        close(client_socket);
        return NULL;
    }
}