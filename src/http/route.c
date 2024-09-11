#include "route.h"
#include "../db/todos.h"
#include "../db/users.h"
#include "../db/sessions.h"
#include "../middlewares/session_middleware.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>

#define MAX_PATH_LENGTH 304
#define DOCUMENT_ROOT "../src/http/www"
#define MAX_TEMPLATE_SIZE 8192
#define PAGE_SIZE 8
#define MAX_TODOS_HTML_SIZE 20240
#define MAX_TOKEN_LENGTH 65
#define MAX_COOKIE_SIZE 256


static void get_home(HttpRequest *req, Task *context);

static void get_about(HttpRequest *req, Task *context);

static void get_todo_page(HttpRequest *req, Task *context, int user_id);

static void create_todo(HttpRequest *req, Task *context);

static void update_todo(HttpRequest *req, Task *context);

static void delete_todo(HttpRequest *req, Task *context);

static void get_user_page(HttpRequest *req, Task *context);

static void signup_user(HttpRequest *req, Task *context);

static void login_user(HttpRequest *req, Task *context);

static void logout_user(HttpRequest *req, Task *context);

static void update_user(HttpRequest *req, Task *context);

static void delete_user(HttpRequest *req, Task *context);

static const Route ROUTES[] = {
        {"/",            GET,    get_home},
        {"/about",       GET,    get_about},
        {"/todo",        POST,   create_todo},
        {"/todo/",       PATCH,  update_todo},
        {"/todo/",       DELETE, delete_todo},
        {"/user",        GET,    get_user_page},
        {"/user/signup", POST,   signup_user},
        {"/user/login",  POST,   login_user},
        {"/user/logout", POST,   logout_user},
        {"/user",        PATCH,  update_user},
        {"/user",        DELETE, delete_user}
};

static const int ROUTES_COUNT = sizeof(ROUTES) / sizeof(Route);

// HELPERS

static const Route *check_route(const char *url, Method method) {
    if (method == DELETE || method == PATCH) {
        const char *route_url;
        for (int i = 0; i < ROUTES_COUNT; ++i) {
            route_url = ROUTES[i].url;
            if (strncmp(url, route_url, strlen(route_url)) == 0 && method == ROUTES[i].method) {
                return &ROUTES[i];
            }
        }
    } else {
        for (int i = 0; i < ROUTES_COUNT; ++i) {
            if (strcmp(url, ROUTES[i].url) == 0 && method == ROUTES[i].method) {
                return &ROUTES[i];
            }
        }
    }
    return NULL;
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


static bool parse_url_data(const char *body, const char **expected_keys, int n_expected_keys, bool *found_keys) {
    if (!body) return false;

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


static bool validate_url_id(const char *url_id, int *id) {
    size_t url_len = strlen(url_id);
    for (int i = 0; i < url_len; ++i) {
        if (!isdigit(*(url_id + i))) {
            return false;
        }
    }

    *id = atoi(url_id);
    if (*id < 0 || (*id == 0 && strcmp(url_id, "0") != 0)) {
        return false;
    }
    return true;
}


static bool is_valid_email(const char *email) {
    int atSymbolIndex = -1;
    int dotSymbolIndex = -1;
    size_t length = strlen(email);

    for (int i = 0; i < length; i++) {
        if (email[i] == '@') {
            if (atSymbolIndex != -1) {
                return false;
            }
            atSymbolIndex = i;
        } else if (email[i] == '.' && atSymbolIndex != -1) {
            dotSymbolIndex = i;
        }
    }
    if (atSymbolIndex > 0 && dotSymbolIndex > atSymbolIndex + 1 && dotSymbolIndex < length - 1) {
        return true;
    }
    return false;
}


static bool is_valid_password(const char *password) {
    size_t length = strlen(password);
    bool hasChar = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    if (length < 8 || length > 128) {
        return false;
    }

    for (int i = 0; i < length; i++) {
        if (isalpha(password[i])) {
            hasChar = true;
        } else if (isdigit(password[i])) {
            hasDigit = true;
        } else if (ispunct(password[i])) {
            hasSpecial = true;
        }
    }
    if (hasChar && (hasDigit || hasSpecial)) {
        return true;
    }
    return false;
}

// HANDLERS

static void get_authentication_page(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *authentication_path = DOCUMENT_ROOT"/authentication.html";
    try_sending_file(client_socket, authentication_path);
}


static void get_home(HttpRequest *req, Task *context) {
    int user_id = check_session(req, context);
    if (user_id < 0) {
        const char *location = "Location: /user\r\n";
        send_headers(context->client_socket, 303, NULL, location);
    } else {
        get_todo_page(req, context, user_id);
    }
}


static void get_about(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *about_path = DOCUMENT_ROOT"/about.html";
    try_sending_file(client_socket, about_path);
}


static void get_todo_page(HttpRequest *req, Task *context, int user_id) {
    int client_socket = context->client_socket;
    int page = 1;

    if (req->query_string) {
        const char *expected_keys[] = {"page"};
        bool found_keys[1] = {0};
        if (!parse_url_data(req->query_string, expected_keys, 1, found_keys)) {
            try_sending_error(client_socket, 400);
            return;
        }
        page = atoi(extract_url_param(req->query_string, "page"));
        if (page < 1) page = 1;
    }

    int count;
    Todo *todos = db_get_all_todos(context->db_conn, user_id, &count, page, PAGE_SIZE);

    if (!todos) {
        try_sending_error(client_socket, 500);
        return;
    }

    int total_count = db_get_total_todos_count(context->db_conn);
    int total_pages = 1;
    if (total_count > 0) {
        total_pages = (total_count + PAGE_SIZE - 1) / PAGE_SIZE;
    }

    char *remainder;
    const char *template_path = DOCUMENT_ROOT"/templates/todos_page.html";
    char *template = read_template(template_path, "<!-- TODO_ITEMS -->", &remainder);
    if (!template) {
        try_sending_error(client_socket, 500);
        free_todos(todos, count);
        return;
    }

    char *todos_html = malloc(MAX_TODOS_HTML_SIZE);
    char *p = todos_html;
    for (int i = 0; i < count; ++i) {
        p += sprintf(p,
                     "<div class=\"todo-item\" data-todo-id=\"%d\">"
                     "<div class=\"todo-details\" onclick=\"toggleExpand(this)\">"
                     "<header class=\"todo-header\">"
                     "<p><time datetime=\"%s\" class=\"creation-time\"></time></p>", todos[i].id,
                     todos[i].creation_time);
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

    p += sprintf(p,
                 "<div class=\"pagination-info\">"
                 "<p>Page %d of %d</p>"
                 "<p>Showing %d-%d of %d To-Dos</p>"
                 "</div>"
                 "<div class=\"pagination-controls\">",
                 page, total_pages,
                 (page - 1) * PAGE_SIZE + 1,
                 (page - 1) * PAGE_SIZE + count,
                 total_count
    );

    if (page > 1) {
        p += sprintf(p, "<button><a href=\"/?page=%d\">Previous</a></button>", page - 1);
    }
    if (page < total_pages) {
        p += sprintf(p, "<button><a href=\"/?page=%d\">Next</a></button>", page + 1);
    }
    sprintf(p, "</div>");

    send_headers(client_socket, 200, "text/html", NULL);
    send(client_socket, template, strlen(template), 0);
    if (count != 0) {
        send(client_socket, todos_html, strlen(todos_html), 0);
    }
    send(client_socket, remainder, strlen(remainder), 0);

    free(template);
    free(todos_html);
    free_todos(todos, count);
}


static void create_todo(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    int user_id = check_session(req, context);
    if (user_id < 0) {
        try_sending_error(client_socket, 401);
        return;
    }
    const char *headers = req->headers;
    const char *body = req->body;

    if (strstr(headers, "Content-Type: application/x-www-form-urlencoded") == NULL) {
        try_sending_error(client_socket, 415);
        return;
    }

    const char *expected_keys[] = {"summary", "task", "duetime"};
    bool found_keys[3];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_url_data(body, expected_keys, 3, found_keys) || !found_keys[0] || !found_keys[1]) {
        try_sending_error(client_socket, 400);
        return;
    }

    char *summary = extract_url_param(body, "summary");
    char *task = extract_url_param(body, "task");
    char *due_time = extract_url_param(body, "duetime");

    if (strlen(summary) > 128 || strlen(task) > 2048) {
        free(summary);
        free(task);
        if (due_time) free(due_time);
        try_sending_error(client_socket, 400);
        return;
    }

    Todo todo = {.user_id = user_id, .summary = summary, .task = task, .due_time = due_time};

    if (!db_create_todo(context->db_conn, &todo)) {
        free(summary);
        free(task);
        if (due_time) free(due_time);
        try_sending_error(client_socket, 500);
        return;
    }

    const char *location = "Location: /\r\n";
    send_headers(client_socket, 201, NULL, location);
    free(summary);
    free(task);
    if (due_time) free(due_time);
}


static void update_todo(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    int user_id = check_session(req, context);
    if (user_id < 0) {
        try_sending_error(client_socket, 401);
        return;
    }
    const char *headers = req->headers;
    const char *body = req->body;

    if (strstr(headers, "\r\nContent-Type: application/x-www-form-urlencoded\r\n") == NULL) {
        try_sending_error(client_socket, 415);
        return;
    }

    if (strlen(req->path) == 6) {                           // no id after /todo/
        try_sending_error(client_socket, 400);
        return;
    }

    int id;
    if (!validate_url_id(req->path + 6, &id)) {
        try_sending_error(client_socket, 400);
        return;
    }

    const char *expected_keys[] = {"summary", "task", "duetime"};
    bool found_keys[3];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_url_data(body, expected_keys, 3, found_keys) || !found_keys[0] || !found_keys[1]) {
        try_sending_error(client_socket, 400);
        return;
    }

    char *summary = extract_url_param(body, "summary");
    char *task = extract_url_param(body, "task");
    char *due_time = extract_url_param(body, "duetime");

    if (strlen(summary) > 128 || strlen(task) > 2048) {
        free(summary);
        free(task);
        if (due_time) free(due_time);
        try_sending_error(client_socket, 400);
        return;
    }

    Todo todo = {.id = id, .user_id = user_id, .summary = summary, .task = task, .due_time = due_time};

    if (!db_update_todo(context->db_conn, &todo)) {
        free(summary);
        free(task);
        if (due_time) free(due_time);
        try_sending_error(client_socket, 404);
        return;
    }

    send_headers(client_socket, 204, NULL, NULL);
    free(summary);
    free(task);
    if (due_time) free(due_time);
}


static void delete_todo(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    int user_id = check_session(req, context);
    if (user_id < 0) {
        try_sending_error(client_socket, 401);
        return;
    }

    if (strlen(req->path) == 6) {
        try_sending_error(client_socket, 400);
        return;
    }

    int id;
    if (!validate_url_id(req->path + 6, &id)) {
        try_sending_error(client_socket, 400);
        return;
    }

    if (!db_delete_todo(context->db_conn, id)) {
        try_sending_error(client_socket, 404);
        return;
    }
    send_headers(client_socket, 204, NULL, NULL);
}


static void get_user_page(HttpRequest *req, Task *context) {
    int user_id = check_session(req, context);
    if (user_id < 0) {
        get_authentication_page(req, context);
        return;
    }

    int client_socket = context->client_socket;
    char *email = db_get_user_email(context->db_conn, user_id);
    if (!email) {
        try_sending_error(client_socket, 500);
        return;
    }

    char *remainder;
    const char *template_path = DOCUMENT_ROOT"/templates/user_page.html";
    char *template = read_template(template_path, "<!-- USER_EMAIL -->", &remainder);
    if (!template) {
        try_sending_error(client_socket, 500);
        return;
    }

    char *user_html = malloc(MAX_TEMPLATE_SIZE + 128);
    sprintf(user_html, "%s", email);

    send_headers(client_socket, 200, "text/html", NULL);
    send(client_socket, template, strlen(template), 0);
    send(client_socket, user_html, strlen(user_html), 0);
    send(client_socket, remainder, strlen(remainder), 0);

    free(template);
    free(user_html);
    free(email);
}


static void signup_user(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *headers = req->headers;
    const char *body = req->body;

    if (strstr(headers, "Content-Type: application/x-www-form-urlencoded") == NULL) {
        try_sending_error(client_socket, 415);
        return;
    }

    const char *expected_keys[] = {"email", "password"};
    bool found_keys[2];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_url_data(body, expected_keys, 2, found_keys) || !found_keys[0] || !found_keys[1]) {
        try_sending_error(client_socket, 400);
        return;
    }

    char *email = extract_url_param(body, "email");
    char *password = extract_url_param(body, "password");

    if (!is_valid_email(email) || !is_valid_password(password)) {
        free(email);
        free(password);
        try_sending_error(client_socket, 400);
        return;
    }

    User user = {.email = email, .password = password};

    int signup_result = db_signup_user(context->db_conn, &user);
    if (signup_result == 1) {
        free(email);
        free(password);
        try_sending_error(client_socket, 500);
        return;
    } else if (signup_result == 23505) {
        free(email);
        free(password);
        try_sending_error(client_socket, 400);
        return;
    }

    const char *location = "Location: /user\r\n";
    send_headers(client_socket, 201, NULL, location);
    free(email);
    free(password);
}


static void login_user(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *headers = req->headers;
    const char *body = req->body;

    if (strstr(headers, "Content-Type: application/x-www-form-urlencoded") == NULL) {
        try_sending_error(client_socket, 415);
        return;
    }

    const char *expected_keys[] = {"email", "password"};
    bool found_keys[2];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_url_data(body, expected_keys, 2, found_keys) || !found_keys[0] || !found_keys[1]) {
        try_sending_error(client_socket, 400);
        return;
    }

    char *email = extract_url_param(body, "email");
    char *password = extract_url_param(body, "password");

    if (strlen(email) > 128 || strlen(password) > 128) {
        free(email);
        free(password);
        try_sending_error(client_socket, 400);
        return;
    }

    User user = {.email = email, .password = password};
    char session_token[MAX_TOKEN_LENGTH];

    if (db_login_user(context->db_conn, &user, session_token)) {
        char cookie[MAX_COOKIE_SIZE];
        snprintf(cookie, sizeof(cookie), "Set-Cookie: session=%s; Path=/; HttpOnly; SameSite=Strict\r\n",
                 session_token);
        const char *location = "Location: /\r\n";
        char header[strlen(cookie) + strlen(location) + 1];
        header[0] = '\0';
        strcat(header, location);
        strcat(header, cookie);
        send_headers(client_socket, 303, NULL, header);
    } else {
        try_sending_error(client_socket, 500);
    }
    free(email);
    free(password);
}


static void logout_user(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *cookie_header = strstr(req->headers, "\r\nCookie: ");

    if (!cookie_header) {
        try_sending_error(client_socket, 401);
        return;
    }

    char session_token[MAX_TOKEN_LENGTH] = {0};
    if (!extract_session_token(cookie_header, session_token, sizeof(session_token))) {
        try_sending_error(client_socket, 401);
        return;
    }

    if (db_delete_session(context->db_conn, session_token)) {
        const char *cookie = "Set-Cookie: session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n";
        send_headers(client_socket, 204, NULL, cookie);
    } else {
        try_sending_error(client_socket, 500);
    }
}


void update_user(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    int user_id = check_session(req, context);
    if (user_id < 0) {
        try_sending_error(client_socket, 401);
        return;
    }
    const char *headers = req->headers;
    const char *body = req->body;

    if (strstr(headers, "Content-Type: application/x-www-form-urlencoded") == NULL) {
        try_sending_error(client_socket, 415);
        return;
    }

    const char *expected_keys[] = {"email", "password"};
    bool found_keys[2];
    memset(found_keys, 0, sizeof(found_keys));

    // email and password can't be changed in a single request
    if (!parse_url_data(body, expected_keys, 2, found_keys) || (found_keys[0] && found_keys[1])) {
        try_sending_error(client_socket, 400);
        return;
    }

    char *email = NULL;
    char *password = NULL;
    if (found_keys[0]) {
        email = extract_url_param(body, "email");
        if (!is_valid_email(email)) {
            free(email);
            try_sending_error(client_socket, 400);
            return;
        }
        if (db_update_user_email(context->db_conn, user_id, email) == 1) {
            free(email);
            try_sending_error(client_socket, 500);
            return;
        } else if (db_update_user_email(context->db_conn, user_id, email) == 23505) {
            free(email);
            try_sending_error(client_socket, 400);
            return;
        }
    } else if (found_keys[1]) {
        password = extract_url_param(body, "password");
        if (!is_valid_password(password)) {
            free(password);
            try_sending_error(client_socket, 400);
            return;
        }
        if (!db_update_user_password(context->db_conn, user_id, password)) {
            free(password);
            try_sending_error(client_socket, 500);
            return;
        }
    }
    send_headers(client_socket, 204, NULL, NULL);
    if (password) free(password);
    if (email) free(email);
}


void delete_user(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    int user_id = check_session(req, context);
    if (user_id < 0) {
        try_sending_error(client_socket, 401);
        return;
    }

    if (!db_delete_user(context->db_conn, user_id)) {
        try_sending_error(client_socket, 500);
        return;
    }
    send_headers(client_socket, 204, NULL, NULL);
}


void handle_http_request(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    int req_method = req->method;
    if (req_method == -1) {
        try_sending_error(client_socket, 400);
        return;
    }

    const Route *route = check_route(req->path, req_method);
    if (route) {                                                                // routed files
        route->handler(req, context);
        return;
    } else {                                                                    // static files
        if (req_method != GET) {
            try_sending_error(client_socket, 405);
            return;
        }
        char file_path[MAX_PATH_LENGTH];
        snprintf(file_path, sizeof(file_path), "%s/static%s", DOCUMENT_ROOT, req->path);
        if (!is_path_safe(file_path)) {
            try_sending_error(client_socket, 404);
            return;
        }
        try_sending_file(client_socket, file_path);
    }
}