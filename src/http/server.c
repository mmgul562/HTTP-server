#include "server.h"
#include "routing/handlers.h"
#include "util/db_cleanup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>

#define MAX_RETRIES 10
#define INITIAL_RETRY_DELAY_MS 100
#define MAX_RETRY_DELAY_MS 10000
#define MAX_QUEUE_SIZE 100
#define DB_CONN_WAIT_TIMEOUT_MS 10000


volatile sig_atomic_t keep_running = 1;
pthread_t threads[THREAD_POOL_SIZE];
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

Task task_queue[MAX_QUEUE_SIZE];
int queue_size = 0;
int queue_front = 0;
int queue_rear = -1;

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

    PGconn *conn = NULL;
    int retry_count = 0;
    int retry_delay_ms = INITIAL_RETRY_DELAY_MS;

    while (retry_count < MAX_RETRIES) {
        conn = PQconnectdb(conninfo);

        if (PQstatus(conn) == CONNECTION_OK) {
            printf("Successfully connected to the database after %d retries\n", retry_count);
            return conn;
        }

        fprintf(stderr, "Connection attempt %d failed: %s", retry_count + 1, PQerrorMessage(conn));
        PQfinish(conn);

        if (retry_count < MAX_RETRIES - 1) {
            printf("Retrying in %d ms...\n", retry_delay_ms);
            struct timespec ts;
            ts.tv_sec = retry_delay_ms / 1000;
            ts.tv_nsec = (retry_delay_ms % 1000) * 1000000;
            nanosleep(&ts, NULL);

            retry_delay_ms *= 2;
            if (retry_delay_ms > MAX_RETRY_DELAY_MS) {
                retry_delay_ms = MAX_RETRY_DELAY_MS;
            }
        }

        retry_count++;
    }

    fprintf(stderr, "Failed to connect to the database after %d attempts\n", MAX_RETRIES);
    return NULL;
}


bool init_connection_pool(ConnectionPool *pool) {
    pthread_mutex_init(&pool->mutex, NULL);

    for (int i = 0; i < CONN_POOL_SIZE; ++i) {
        pool->connections[i].conn = connect_to_db();
        if (!pool->connections[i].conn) {
            for (int j = i - 1; j >= 0; --j) {
                PQfinish(pool->connections[j].conn);
            }
            return false;
        }
        pool->connections[i].in_use = 0;
    }
    return true;
}


PGconn *get_connection(ConnectionPool *pool) {
    pthread_mutex_lock(&pool->mutex);
    for (int i = 0; i < CONN_POOL_SIZE; ++i) {
        if (!pool->connections[i].in_use) {
            pool->connections[i].in_use = true;
            pthread_mutex_unlock(&pool->mutex);
            return pool->connections[i].conn;
        }
    }
    pthread_mutex_unlock(&pool->mutex);
    return NULL;
}


void release_connection(ConnectionPool *pool, PGconn *conn) {
    pthread_mutex_lock(&pool->mutex);
    for (int i = 0; i < CONN_POOL_SIZE; ++i) {
        if (pool->connections[i].conn == conn) {
            pool->connections[i].in_use = false;
            break;
        }
    }
    pthread_mutex_unlock(&pool->mutex);
}


void cleanup_connection_pool(ConnectionPool *pool) {
    for (int i = 0; i < CONN_POOL_SIZE; ++i) {
        if (pool->connections[i].conn) {
            PQfinish(pool->connections[i].conn);
        }
    }
    pthread_mutex_destroy(&pool->mutex);
}


static void enqueue_task(Task task) {
    pthread_mutex_lock(&queue_mutex);
    if (queue_size < MAX_QUEUE_SIZE) {
        queue_rear = (queue_rear + 1) % MAX_QUEUE_SIZE;
        task_queue[queue_rear] = task;
        queue_size++;
        pthread_cond_signal(&queue_cond);
    }
    pthread_mutex_unlock(&queue_mutex);
}


static Task dequeue_task() {
    Task task;
    pthread_mutex_lock(&queue_mutex);
    while (queue_size == 0 && keep_running) {
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    if (queue_size > 0) {
        task = task_queue[queue_front];
        queue_front = (queue_front + 1) % MAX_QUEUE_SIZE;
        queue_size--;
    } else {
        task.client_socket = -1;
    }
    pthread_mutex_unlock(&queue_mutex);
    return task;
}


static ssize_t receive_full_request(int client_socket, char **request_buffer, size_t *buffer_size) {
    ssize_t total_bytes = 0;
    ssize_t bytes_received;
    size_t content_length = 0;
    bool headers_end = false;
    char *header_end = NULL;

    while (1) {
        if (total_bytes + 1024 > *buffer_size) {
            *buffer_size += 1024;
            char *new_buffer = realloc(*request_buffer, *buffer_size);
            if (!new_buffer) {
                perror("Failed to allocate memory for the new request buffer");
                return -1;
            }
            *request_buffer = new_buffer;
        }

        bytes_received = recv(client_socket, *request_buffer + total_bytes, *buffer_size - total_bytes - 1, 0);

        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                perror("recv failed");
                return -1;
            }
        } else if (bytes_received == 0) {
            break;
        }

        total_bytes += bytes_received;
        (*request_buffer)[total_bytes] = '\0';

        if (!headers_end) {
            header_end = strstr(*request_buffer, "\r\n\r\n");
            if (header_end) {
                headers_end = true;
                char *content_length_header = strstr(*request_buffer, "Content-Length:");
                if (content_length_header) {
                    content_length_header += 15;
                    while (isspace(*content_length_header)) content_length_header++;
                    content_length = strtoul(content_length_header, NULL, 10);
                }
            }
        }
        if (headers_end && total_bytes >= (size_t)(header_end + 4 - *request_buffer + content_length)) {
            break;
        }
    }
    return total_bytes;
}


static void *worker_thread(void *connections) {
    ConnectionPool *connection_pool = (ConnectionPool *)connections;
    while (keep_running) {
        Task task = dequeue_task();
        int client_socket = task.client_socket;
        if (client_socket == -1) {
            break;
        }

        char *request_buffer = NULL;
        size_t buffer_size = 0;
        ssize_t total_bytes = receive_full_request(client_socket, &request_buffer, &buffer_size);

        if (total_bytes > 0) {
            HttpRequest request;
            memset(&request, 0, sizeof(HttpRequest));

            RequestParsingStatus status = parse_http_request(request_buffer, &request);
            if (status == REQ_PARSE_SUCCESS) {
                if (needs_db_conn(&request)) {
                    PGconn *task_conn = get_connection(connection_pool);

                    // should only happen when thread pool is bigger than connection pool
                    if (!task_conn) {
                        struct timespec start, now;
                        clock_gettime(CLOCK_MONOTONIC, &start);

                        while (!task_conn && keep_running) {
                            task_conn = get_connection(connection_pool);
                            if (!task_conn) {
                                struct timespec wait_time = {0, 100000000};
                                nanosleep(&wait_time, NULL);

                                clock_gettime(CLOCK_MONOTONIC, &now);
                                if ((now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000 > DB_CONN_WAIT_TIMEOUT_MS) {
                                    break;
                                }
                            }
                        }
                        if (!task_conn) {
                            try_sending_error_file(client_socket, 503);
                            free_http_request(&request);
                            free(request_buffer);
                            close(client_socket);
                            continue;
                        }
                    }
                    task.db_conn = task_conn;
                }
                handle_http_request(&request, &task);
                free_http_request(&request);
            } else {
                handle_invalid_http_request(status, client_socket);
            }
        }
        free(request_buffer);
        if (task.db_conn) release_connection(connection_pool, task.db_conn);
        close(client_socket);
    }
    return NULL;
}


bool server_init(Server *server, int port) {
    if (!init_connection_pool(&server->conns)) {
        return false;
    }

    cleanup_database(server->conns.connections[0].conn);

    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket == -1) {
        perror("Socket creation failed");
        return false;
    }

    server->server_addr.sin_family = AF_INET;
    server->server_addr.sin_addr.s_addr = INADDR_ANY;
    server->server_addr.sin_port = htons(port);

    if (bind(server->server_socket, (struct sockaddr *) &server->server_addr, sizeof(server->server_addr)) < 0) {
        perror("Bind failed");
        return false;
    }
    if (listen(server->server_socket, 10) < 0) {
        perror("Listen failed");
        return false;
    }

    server->port = port;

    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        if (pthread_create(&threads[i], NULL, worker_thread, &server->conns) != 0) {
            perror("Failed to create worker thread");
            return false;
        }
    }
    return true;
}


void server_run(Server *server) {
    printf("Server listening on port %d\n", server->port);

    int server_socket = server->server_socket;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    while (keep_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(server_socket + 1, &read_fds, NULL, NULL, &timeout);

        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("Select failed");
            break;
        } else if (ready == 0) {
            continue;
        }

        int client_socket = accept(server_socket, (struct sockaddr *) &client_addr, &client_len);
        if (client_socket < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("Accept failed");
            continue;
        }

        printf("New connection accepted\n");
        Task new_task = {client_socket, NULL};
        enqueue_task(new_task);
    }

    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        Task exit_task = {-1, NULL};
        enqueue_task(exit_task);
    }

    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        pthread_join(threads[i], NULL);
    }

    printf("Server shutting down...\n");
}