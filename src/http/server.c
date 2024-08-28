#include "server.h"
#include "request.h"
#include "response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>


volatile sig_atomic_t keep_running = 1;

static void signal_handler(int signum) {
    keep_running = 0;
}


static PGconn *connect_to_db() {
    char *db_name = getenv("DB_NAME");
    char *db_user = getenv("DB_USER");
    char *db_password = getenv("DB_PASSWD");
    char *db_host = getenv("DB_HOST");
    char *db_port = getenv("DB_PORT");

    if (!db_name || !db_user || !db_password || !db_host || !db_port) {
        fprintf(stderr, "Missing environment variables for database connection\n");
        return NULL;
    }

    char conninfo[256];
    snprintf(conninfo, sizeof(conninfo),
             "dbname=%s user=%s password=%s host=%s port=%s",
             db_name, db_user, db_password, db_host, db_port);

    PGconn *conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Connection to database failed: %s", PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }

    return conn;
}


static void *handle_client(void *thread_context) {
    ThreadContext *context = (ThreadContext *)thread_context;
    int client_socket = context->client_socket;

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

            send_http_response(&request, context);
            free_http_request(&request);
        } else {
            printf("Rejected request\n");
            send_failure_response(status, client_socket);
        }

        free(buffer);
        close(client_socket);
    }
    free(context);
    return NULL;
}


int server_init(Server *server, int port, int thread_pool_size) {
    server->db_conn = connect_to_db();
    if (!server->db_conn) {
        return -1;
    }
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

    server->port = port;
    server->thread_pool_size = thread_pool_size;

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

        int client_socket;
        client_socket = accept(server->server_socket, (struct sockaddr *) &client_addr, &client_len);
        if (client_socket < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("Accept failed");
            continue;
        }

        printf("New connection accepted\n");
        ThreadContext *context = malloc(sizeof(ThreadContext));
        if (!context) {
            perror("Memory allocation failed");
            close(client_socket);
            continue;
        }
        context->client_socket = client_socket;
        context->db_conn = server->db_conn;

        if (pthread_create(&threads[thread_idx], NULL, handle_client, (void *) context) != 0) {
            perror("Failed to create client thread");
            close(client_socket);
            free(context);
        } else {
            pthread_detach(threads[thread_idx]);
            thread_idx = (thread_idx + 1) % server->thread_pool_size;
        }
    }

    printf("Server shutting down...\n");
}