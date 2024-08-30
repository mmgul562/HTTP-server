#include "response.h"
#include "../todos/todos.h"
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_PATH_LENGTH 304
#define DOCUMENT_ROOT "../src/http/www"
#define MAX_TEMPLATE_SIZE 4096
#define MAX_TODOS_HTML_SIZE 10240

static void get_home(HttpRequest *req, ThreadContext *context);

static void get_about(HttpRequest *req, ThreadContext *context);

static void get_todos(HttpRequest *req, ThreadContext *context);

static void create_todo(HttpRequest *req, ThreadContext *context);


const Route ROUTES[] = {{"/",      GET,  get_todos},
                        {"/",      POST, create_todo},
                        {"/about", GET,  get_about},
};

const int ROUTES_COUNT = sizeof(ROUTES) / sizeof(Route);


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


static char *read_template(const char *filename, const char *placeholder, char **remainder) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening template file");
        return NULL;
    }

    char *buffer = malloc(MAX_TEMPLATE_SIZE);
    size_t bytesRead = fread(buffer, 1, MAX_TEMPLATE_SIZE - 1, file);
    buffer[bytesRead] = '\0';

    fclose(file);

    char *placeholder_pos = strstr(buffer, placeholder);
    if (placeholder_pos) {
        *placeholder_pos = '\0';
        *remainder = placeholder_pos + strlen(placeholder);
    } else {
        *remainder = buffer + strlen(buffer);
    }

    return buffer;
}


static bool parse_form_data(const char *body, const char **expected_keys, int n_expected_keys, bool *found_keys) {
    char *body_copy = strdup(body);
    char *token, *rest = body_copy;
    while ((token = strtok_r(rest, "&", &rest))) {
        char *key = strtok(token, "=");
        char *value = strtok(NULL, "=");

        if (key && value) {
            bool is_expected = false;
            for (int i = 0; i < n_expected_keys; ++i) {
                if (strcmp(key, expected_keys[i]) == 0) {
                    found_keys[i] = true;
                    is_expected = true;
                    break;
                }
            }
            if (!is_expected) {
                free(body_copy);
                return false;
            }
        }
    }
    free(body_copy);
    return true;
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
    char content_type_h[64];
    if (!content_type) {
        strcat(content_type_h, "");
    } else {
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
        case 400:
            status_text = "Bad Request";
            break;
        case 403:
            status_text = "Forbidden";
            break;
        case 404:
            status_text = "Not Found";
            break;
        case 405:
            status_text = "Method Not Allowed";
            break;
        case 415:
            status_text = "Unsupported Media Type";
            break;
        case 500:
            status_text = "Internal Server Error";
            break;
        default:
            status_text = "Internal Server Error";
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


static void try_sending_error(int client_socket, int status_code) {
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

// ROUTING

static void get_about(HttpRequest *req, ThreadContext *context) {
    const char *about_path = DOCUMENT_ROOT"/about.html";
    try_sending_file(context->client_socket, about_path);
}


static void get_todos(HttpRequest *req, ThreadContext *context) {
    int count;
    Todo *todos = db_get_all_todos(context->db_conn, &count);

    if (!todos) {
        try_sending_error(context->client_socket, 500);
        return;
    }

    char *remainder;
    const char *template_path = DOCUMENT_ROOT"/templates/todos.html";
    char *template = read_template(template_path, "<!-- TODO_ITEMS -->", &remainder);
    if (!template) {
        try_sending_error(context->client_socket, 500);
        free_todos(todos, count);
        return;
    }

    char *todos_html = malloc(MAX_TODOS_HTML_SIZE);
    char *p = todos_html;
    for (int i = 0; i < count; i++) {
        p += sprintf(p,
                     "<div class=\"todo-item\">"
                     "<div class=\"todo-details\" onclick=\"toggleExpand(this)\">"
                     "<header class=\"todo-header\">"
                     "<p><time datetime=\"%s\" class=\"creation-time\"></time></p>", todos[i].creation_time);
        if (todos[i].due_time) {
            p += sprintf(p, "<p>Due: <time datetime=\"%s\" class=\"due-time\"></time></p>", todos[i].due_time);
        }
        p += sprintf(p,
                     "</header>"
                     "<div class=\"todo-summary\">"
                     "<p>%s</p>"
                     "</div><div class=\"todo-task\">"
                     "<p>%s</p>"
                     "</div></div>"
                     "<div class=\"todo-buttons\">"
                     "<button class=\"edit-btn\">✏️</button>"
                     "<button class=\"complete-btn\">✅</button>"
                     "</div></div>",
                     todos[i].summary,
                     todos[i].task);
    }

    send_headers(context->client_socket, 200, "text/html", NULL);
    send(context->client_socket, template, strlen(template), 0);
    if (count != 0) {
        send(context->client_socket, todos_html, strlen(todos_html), 0);
    }
    send(context->client_socket, remainder, strlen(remainder), 0);

    free(template);
    free(todos_html);
    free_todos(todos, count);
}


static void create_todo(HttpRequest *req, ThreadContext *context) {
    int client_socket = context->client_socket;
    const char *headers = req->headers;
    if (strstr(headers, "Content-Type: application/x-www-form-urlencoded") == NULL) {
        try_sending_error(client_socket, 415);
        return;
    }
    const char *body = req->body;
    if (!body) {
        try_sending_error(client_socket, 400);
        return;
    }

    const char *expected_keys[] = {"summary", "task", "duetime"};
    int num_expected_keys = sizeof(expected_keys) / sizeof(expected_keys[0]);

    bool found_keys[num_expected_keys];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_form_data(body, expected_keys, num_expected_keys, found_keys)) {
        try_sending_error(client_socket, 400);
        return;
    }

    if (!found_keys[0] || !found_keys[1]) {
        try_sending_error(client_socket, 400);
        return;
    }

    char *summary = extract_form_value(body, "summary");
    char *task = extract_form_value(body, "task");
    char *due_time = extract_form_value(body, "duetime");

    Todo todo = {.summary = summary, .task = task, .due_time = due_time};

    if (!db_create_todo(context->db_conn, &todo)) {
        free(summary);
        free(task);
        if (due_time) free(due_time);
        try_sending_error(client_socket, 500);
        return;
    }

    send_headers(client_socket, 201, NULL, NULL);
    free(summary);
    free(task);
    if (due_time) free(due_time);
}


static const Route *check_route(const char *url, Method method) {
    for (int i = 0; i < ROUTES_COUNT; ++i) {
        if (strcmp(url, ROUTES[i].url) == 0 && method == ROUTES[i].method) {
            return &ROUTES[i];
        }
    }
    return NULL;
}


static int parse_request_method(const char *method) {
    if (strcmp(method, "GET") == 0) {
        return GET;
    } else if (strcmp(method, "POST") == 0) {
        return POST;
    }
    return -1;
}


void send_http_response(HttpRequest *request, ThreadContext *context) {
    int client_socket = context->client_socket;
    int req_method = parse_request_method(request->method);
    if (req_method == -1) {
        try_sending_error(client_socket, 400);
        return;
    }

    const Route *route = check_route(request->path, req_method);
    if (route) {                                                                // routed files
        route->handler(request, context);
        return;
    } else {                                                                    // static files
        if (req_method != GET) {
            try_sending_error(client_socket, 405);
            return;
        }
        char file_path[MAX_PATH_LENGTH];
        snprintf(file_path, sizeof(file_path), "%s/static%s", DOCUMENT_ROOT, request->path);
        if (!is_path_safe(file_path)) {
            try_sending_error(client_socket, 404);
            return;
        }
        try_sending_file(client_socket, file_path);
    }
}


void send_failure_response(RequestParsingStatus status, int client_socket) {
    if (status == REQ_PARSE_INVALID_FORMAT) {
        try_sending_error(client_socket, 400);
    } else {
        try_sending_error(client_socket, 500);
    }
}