#include "handlers.h"
#include "helpers.h"
#include "../../db/todos.h"
#include "../../db/users.h"
#include "../../db/email_change_requests.h"
#include "../../db/sessions.h"
#include "../../db/verifications.h"
#include "../../middlewares/session_middleware.h"
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define SERVER_DOMAIN "http://localhost:8080"
#define PAGE_SIZE 8
#define MAX_TODOS_HTML_SIZE 20240
#define MAX_COOKIE_SIZE 256


static void get_home(HttpRequest *req, Task *context);

static void get_about(HttpRequest *req, Task *context);

static void get_todo_page(HttpRequest *req, Task *context, int user_id, const char *csrf_token);

static void create_todo(HttpRequest *req, Task *context);

static void update_todo(HttpRequest *req, Task *context);

static void delete_todo(HttpRequest *req, Task *context);

static void get_user_page(HttpRequest *req, Task *context);

static void verify_email(HttpRequest *req, Task *context);

static void get_verification_page(HttpRequest *req, Task *context);

static void verify_new_email(HttpRequest *req, Task *context);

static void forgot_password(HttpRequest *req, Task *context);

static void get_reset_password_page(HttpRequest *req, Task *context);

static void reset_password(HttpRequest *req, Task *context);

static void signup_user(HttpRequest *req, Task *context);

static void login_user(HttpRequest *req, Task *context);

static void logout_user(HttpRequest *req, Task *context);

static void update_user(HttpRequest *req, Task *context);

static void delete_user(HttpRequest *req, Task *context);

static const Route ROUTES[] = {
        {"/",                     GET,    get_home},
        {"/about",                GET,    get_about},
        {"/todo",                 POST,   create_todo},
        {"/todo/",                PATCH,  update_todo},
        {"/todo/",                DELETE, delete_todo},
        {"/user/verify",          POST,   verify_email},
        {"/user/verify",          GET,    get_verification_page},
        {"/user/verify-new",      POST,   verify_new_email},
        {"/user/forgot-password", POST,   forgot_password},
        {"/user/reset-password",  GET,    get_reset_password_page},
        {"/user/reset-password",  POST,   reset_password},
        {"/user",                 GET,    get_user_page},
        {"/user/signup",          POST,   signup_user},
        {"/user/login",           POST,   login_user},
        {"/user/logout",          POST,   logout_user},
        {"/user",                 PATCH,  update_user},
        {"/user",                 DELETE, delete_user}
};

static const int ROUTES_COUNT = sizeof(ROUTES) / sizeof(Route);


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


static void get_authentication_page(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *authentication_path = DOCUMENT_ROOT"/authentication.html";
    try_sending_file(client_socket, authentication_path);
}


static void get_home(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    char csrf_token[MAX_TOKEN_LENGTH + 1];
    int user_id;

    QueryResult qres = check_session(req->headers, context->db_conn, &user_id, csrf_token);
    if (qres == QRESULT_NONE_AFFECTED) {
        const char *location = "Location: /user\r\n";
        send_headers(client_socket, 303, NULL, location);
    } else if (qres == QRESULT_INTERNAL_ERROR) {
        try_sending_error_file(client_socket, 500);
    } else {
        get_todo_page(req, context, user_id, csrf_token);
    }
}


static void get_about(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *about_path = DOCUMENT_ROOT"/about.html";
    try_sending_file(client_socket, about_path);
}


static void get_todo_page(HttpRequest *req, Task *context, int user_id, const char *csrf_token) {
    int client_socket = context->client_socket;
    int page = 1;

    if (req->query_string) {
        const char *expected_keys[] = {"page"};
        bool found_keys[1] = {0};
        if (!parse_url_data(req->query_string, expected_keys, 1, found_keys)) {
            try_sending_error_file(client_socket, 400);
            return;
        }
        char page_str[12];
        if (!extract_url_param(req->query_string, "page", page_str, sizeof(page_str) - 1)) {
            try_sending_error_file(client_socket, 404);
            return;
        }
        page = atoi(page_str);
        if (page < 1) page = 1;
    }

    int count;
    Todo *todos = db_get_all_todos(context->db_conn, user_id, &count, page, PAGE_SIZE);

    if (!todos) {
        try_sending_error_file(client_socket, 500);
        return;
    }

    int total_count = db_get_total_todos_count(context->db_conn);
    int total_pages = 1;
    if (total_count > 0) {
        total_pages = (total_count + PAGE_SIZE - 1) / PAGE_SIZE;
    }

    char *csrf_remainder;
    const char *template_path = DOCUMENT_ROOT"/templates/todos_page.html";
    char *template = read_template(template_path, "<!-- CSRF_TOKEN -->", &csrf_remainder);

    if (!template) {
        try_sending_error_file(client_socket, 500);
        free_todos(todos, count);
        return;
    }

    char csrf_html[164];
    sprintf(csrf_html, "<meta name=\"csrf-token\" content=\"%s\">", csrf_token);

    char *todos_remainder;
    skip_placeholder(csrf_remainder, "<!-- TODO_ITEMS -->", &todos_remainder);
    if (*todos_remainder == '\0') {
        try_sending_error_file(client_socket, 500);
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
                     "<button type=\"button\" class=\"edit-btn\">✏️</button>"
                     "<button type=\"button\" class=\"complete-btn\">✅</button>"
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

    size_t template_len = strlen(template);
    size_t csrf_len = strlen(csrf_html);
    size_t csrf_rem_len = strlen(csrf_remainder);
    size_t todos_len = count > 0 ? strlen(todos_html) : 0;
    size_t todos_rem_len = strlen(todos_remainder);
    size_t total_len = template_len + csrf_len + csrf_rem_len + todos_len + todos_rem_len;
    char content_length[64];
    snprintf(content_length, sizeof(content_length), "Content-Length: %ld\r\n", total_len);

    send_headers(client_socket, 200, "text/html", content_length);
    send(client_socket, template, template_len, 0);
    send(client_socket, csrf_html, csrf_len, 0);
    send(client_socket, csrf_remainder, csrf_rem_len, 0);
    if (count != 0) {
        send(client_socket, todos_html, todos_len, 0);
    }
    send(client_socket, todos_remainder, todos_rem_len, 0);

    free(template);
    free(todos_html);
    free_todos(todos, count);
}


static void create_todo(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    char csrf_token[MAX_TOKEN_LENGTH + 1];
    int user_id;

    QueryResult qres = check_session(req->headers, context->db_conn, &user_id, csrf_token);
    if (qres == QRESULT_NONE_AFFECTED) {
        send_error_message(client_socket, 401, "Authentication required.");
        return;
    } else if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't retrieve session information.");
        return;
    }
    const char *body = req->body;

    if (strstr(req->headers, "\r\nContent-Type: application/x-www-form-urlencoded\r\n") == NULL) {
        send_error_message(client_socket, 415, "Content-Type should be set to application/x-www-form-urlencoded.");
        return;
    } else if (!check_csrf_token(req, csrf_token)) {
        send_error_message(client_socket, 403, "No CSRF token found.");
        return;
    }

    const char *expected_keys[] = {"summary", "task", "duetime"};
    bool found_keys[3];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_url_data(body, expected_keys, 3, found_keys)) {
        send_error_message(client_socket, 400, "Unexpected key. Only 'summary', 'task' and 'duetime' accepted.");
        return;
    } else if (!found_keys[0]) {
        send_error_message(client_socket, 400, "Summary must be provided.");
        return;
    } else if (!found_keys[1]) {
        send_error_message(client_socket, 400, "Task must be provided.");
        return;
    }

    char summary[DB_SUMMARY_LEN + 1];
    char *task = malloc(DB_TASK_LEN + 1);
    char due_time[65];

    if (!extract_url_param(body, "summary", summary, DB_SUMMARY_LEN)) {
        char msg[64];
        sprintf(msg, "Summary cannot be longer than %d characters.", DB_SUMMARY_LEN);
        send_error_message(client_socket, 400, msg);
    } else if (!extract_url_param(body, "task", task, DB_TASK_LEN)) {
        char msg[64];
        sprintf(msg, "Task cannot be longer than %d characters.", DB_TASK_LEN);
        send_error_message(client_socket, 400, msg);
    } else {
        Todo todo = {.user_id = user_id, .summary = summary, .task = task};
        if (extract_url_param(body, "duetime", due_time, sizeof(due_time) - 1)) {
            todo.due_time = due_time;
        } else {
            todo.due_time = NULL;
        }

        if (!db_create_todo(context->db_conn, &todo)) {
            send_error_message(client_socket, 500, "Couldn't create To-Do.");
        } else {
            const char *location = "Location: /\r\n";
            send_headers(client_socket, 201, NULL, location);
        }
    }
    free(task);
}


static void update_todo(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    char csrf_token[MAX_TOKEN_LENGTH + 1];
    int user_id;

    QueryResult qres = check_session(req->headers, context->db_conn, &user_id, csrf_token);
    if (qres == QRESULT_NONE_AFFECTED) {
        send_error_message(client_socket, 401, "Authentication required.");
        return;
    } else if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't retrieve session information.");
        return;
    }
    const char *body = req->body;

    if (strstr(req->headers, "\r\nContent-Type: application/x-www-form-urlencoded\r\n") == NULL) {
        send_error_message(client_socket, 415, "Content-Type should be set to application/x-www-form-urlencoded.");
        return;
    } else if (!check_csrf_token(req, csrf_token)) {
        send_error_message(client_socket, 403, "No CSRF token found.");
        return;
    } else if (strlen(req->path) == 6) {
        send_error_message(client_socket, 400, "Expected to-do ID in the URL.");
        return;
    }
    int id;
    if (!validate_url_id(req->path + 6, &id)) {
        send_error_message(client_socket, 400, "Invalid to-do ID.");
        return;
    }

    const char *expected_keys[] = {"summary", "task", "duetime"};
    bool found_keys[3];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_url_data(body, expected_keys, 3, found_keys)) {
        send_error_message(client_socket, 400, "Unexpected key. Only 'summary', 'task' and 'duetime' accepted.");
        return;
    } else if (!found_keys[0]) {
        send_error_message(client_socket, 400, "Summary must be provided.");
        return;
    } else if (!found_keys[1]) {
        send_error_message(client_socket, 400, "Task must be provided.");
        return;
    }

    char summary[DB_SUMMARY_LEN + 1];
    char *task = malloc(DB_TASK_LEN + 1);
    char due_time[65];

    if (!extract_url_param(body, "summary", summary, DB_SUMMARY_LEN)) {
        char msg[64];
        sprintf(msg, "Summary cannot be longer than %d characters.", DB_SUMMARY_LEN);
        send_error_message(client_socket, 400, msg);
    } else if (!extract_url_param(body, "task", task, DB_TASK_LEN)) {
        char msg[64];
        sprintf(msg, "Task cannot be longer than %d characters.", DB_TASK_LEN);
        send_error_message(client_socket, 400, msg);
    } else {
        Todo todo = {.id = id, .user_id = user_id, .summary = summary, .task = task};
        if (extract_url_param(body, "duetime", due_time, sizeof(due_time) - 1)) {
            todo.due_time = due_time;
        } else {
            todo.due_time = NULL;
        }

        qres = db_update_todo(context->db_conn, &todo);
        if (qres == QRESULT_INTERNAL_ERROR) {
            send_error_message(client_socket, 500, "Couldn't update the to-do.");
        } else if (qres == QRESULT_NONE_AFFECTED) {
            send_error_message(client_socket, 404, "Couldn't find the requested to-do.");
        } else {
            send_headers(client_socket, 204, NULL, NULL);
        }
    }
    free(task);
}


static void delete_todo(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    char csrf_token[MAX_TOKEN_LENGTH + 1];
    int user_id;

    QueryResult qres = check_session(req->headers, context->db_conn, &user_id, csrf_token);
    if (qres == QRESULT_NONE_AFFECTED) {
        send_error_message(client_socket, 401, "Authentication required.");
        return;
    } else if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't retrieve session information.");
        return;
    }

    if (!check_csrf_token(req, csrf_token)) {
        send_error_message(client_socket, 403, "No CSRF token found.");
        return;
    } else if (strlen(req->path) == 6) {
        send_error_message(client_socket, 400, "Expected to-do ID in the URL.");
        return;
    }
    int id;
    if (!validate_url_id(req->path + 6, &id)) {
        send_error_message(client_socket, 400, "Invalid to-do ID.");
        return;
    }

    qres = db_delete_todo(context->db_conn, id, user_id);
    if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't delete the to-do.");
    } else if (qres == QRESULT_NONE_AFFECTED) {
        send_error_message(client_socket, 404, "Couldn't find the requested to-do.");
    } else {
        send_headers(client_socket, 204, NULL, NULL);
    }
}


static void get_user_page(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    char csrf_token[MAX_TOKEN_LENGTH + 1];
    int user_id;

    QueryResult qres = check_session(req->headers, context->db_conn, &user_id, csrf_token);
    if (qres == QRESULT_NONE_AFFECTED) {
        get_authentication_page(req, context);
        return;
    } else if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't retrieve session information.");
        return;
    }

    char email[129];
    if (!db_get_user_email(context->db_conn, user_id, email)) {
        try_sending_error_file(client_socket, 500);
        return;
    }

    char *csrf_remainder;
    const char *template_path = DOCUMENT_ROOT"/templates/user_page.html";
    char *template = read_template(template_path, "<!-- CSRF_TOKEN -->", &csrf_remainder);

    if (!template) {
        try_sending_error_file(client_socket, 500);
        return;
    }

    char csrf_html[164];
    sprintf(csrf_html, "<meta name=\"csrf-token\" content=\"%s\">", csrf_token);

    char *email_remainder;
    skip_placeholder(csrf_remainder, "<!-- USER_EMAIL -->", &email_remainder);
    if (*email_remainder == '\0') {
        try_sending_error_file(client_socket, 500);
        return;
    }

    char *email_html = malloc(MAX_TEMPLATE_SIZE + 128);
    sprintf(email_html, "%s", email);

    size_t template_len = strlen(template);
    size_t csrf_len = strlen(csrf_html);
    size_t csrf_rem_len = strlen(csrf_remainder);
    size_t email_len = strlen(email_html);
    size_t email_rem_len = strlen(email_remainder);
    size_t total_len = template_len + csrf_len + csrf_rem_len + email_len + email_rem_len;
    char content_length[64];
    snprintf(content_length, sizeof(content_length), "Content-Length: %ld\r\n", total_len);


    send_headers(client_socket, 200, "text/html", content_length);
    send(client_socket, template, template_len, 0);
    send(client_socket, csrf_html, csrf_len, 0);
    send(client_socket, csrf_remainder, csrf_rem_len, 0);
    send(client_socket, email_html, email_len, 0);
    send(client_socket, email_remainder, email_rem_len, 0);

    free(template);
    free(email_html);
}


static void verify_email(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *body = req->body;

    if (strstr(req->headers, "Content-Type: application/x-www-form-urlencoded") == NULL) {
        send_error_message(client_socket, 415, "Content-Type should be set to application/x-www-form-urlencoded.");
        return;
    }

    const char *expected_keys[2] = {"email", "vtoken"};
    bool found_keys[2];
    memset(found_keys, 0, sizeof(found_keys));
    VerificationResult result = {0};

    if (!parse_url_data(body, expected_keys, 2, found_keys) || !found_keys[0] || !found_keys[1]) {
        try_sending_error_file(client_socket, 405);
        return;
    }

    char email[DB_EMAIL_LEN + 1];
    char provided_token[MAX_TOKEN_LENGTH + 1];
    extract_url_param(body, "email", email, DB_EMAIL_LEN);
    extract_url_param(body, "vtoken", provided_token, MAX_TOKEN_LENGTH);

    bool is_verified;
    char expected_token[MAX_TOKEN_LENGTH + 1];
    QueryResult qres = db_get_verification_token(context->db_conn, email, expected_token, &is_verified);
    if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't retrieve verification information.");
        return;
    } else if (qres == QRESULT_NONE_AFFECTED || is_verified) {
        send_error_message(client_socket, 404, "Invalid or expired verification link.");
        return;
    }

    result.token = provided_token;
    if (strcmp(provided_token, expected_token) != 0) {
        result.message = "Invalid or expired verification link.";
        result.success = false;
    } else if (!db_verify_email(context->db_conn, email)) {
        result.message = "Couldn't verify the e-mail.";
        result.success = false;
    } else {
        result.message = "You can now sign in.";
        result.success = true;
    }

    if (!db_create_verification_result(context->db_conn, &result)) {
        send_error_message(client_socket, 500, "Couldn't create verification result.");
        return;
    }
    char location[128];
    sprintf(location, "Location: /user/verify?v=%s\r\n", provided_token);
    send_headers(client_socket, 303, NULL, location);
}


static void get_verification_page(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *expected_keys[] = {"v"};
    bool found_keys[1];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_url_data(req->query_string, expected_keys, 1, found_keys) || !found_keys[0]) {
        try_sending_error_file(client_socket, 404);
        return;
    }

    char token[MAX_TOKEN_LENGTH + 1];
    extract_url_param(req->query_string, "v", token, MAX_TOKEN_LENGTH);
    VerificationResult result = {.token = token};

    char *remainder;
    const char *template_path = DOCUMENT_ROOT"/templates/verification_page.html";
    char *template = read_template(template_path, "<!-- RESULT_BODY -->", &remainder);

    if (!template) {
        try_sending_error_file(client_socket, 500);
        return;
    }

    QueryResult qres = db_get_verification_result(context->db_conn, &result);
    if (qres == QRESULT_INTERNAL_ERROR) {
        try_sending_error_file(client_socket, 500);
        free(template);
        return;
    } else if (qres == QRESULT_NONE_AFFECTED) {
        try_sending_error_file(client_socket, 404);
        free(template);
        return;
    }

    if (result.expires_at < time(NULL)) {
        try_sending_error_file(client_socket, 404);
        free(template);
        return;
    }
    char *result_html = malloc(MAX_TEMPLATE_SIZE);
    sprintf(result_html, "<h2>Verification %s</h2>"
                         "<p>%s</p>",
            result.success ? "successful!" : "failed...",
            result.message);

    size_t template_len = strlen(template);
    size_t result_len = strlen(result_html);
    size_t remainder_len = strlen(remainder);
    size_t total_len = template_len + result_len + remainder_len;
    char content_length[64];
    snprintf(content_length, sizeof(content_length), "Content-Length: %ld\r\n", total_len);

    send_headers(client_socket, 200, "text/html", content_length);
    send(client_socket, template, template_len, 0);
    send(client_socket, result_html, result_len, 0);
    send(client_socket, remainder, remainder_len, 0);

    free(result_html);
    free(template);
}


static void verify_new_email(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *body = req->body;

    if (strstr(req->headers, "Content-Type: application/x-www-form-urlencoded") == NULL) {
        send_error_message(client_socket, 415, "Content-Type should be set to application/x-www-form-urlencoded.");
        return;
    }

    const char *expected_keys[2] = {"email", "vtoken"};
    bool found_keys[2];
    memset(found_keys, 0, sizeof(found_keys));
    VerificationResult result = {0};

    if (!parse_url_data(body, expected_keys, 2, found_keys) || !found_keys[0] || !found_keys[1]) {
        try_sending_error_file(client_socket, 405);
        return;
    }

    char email[DB_EMAIL_LEN + 1];
    char provided_token[MAX_TOKEN_LENGTH + 1];
    extract_url_param(body, "email", email, DB_EMAIL_LEN);
    extract_url_param(body, "vtoken", provided_token, MAX_TOKEN_LENGTH);

    int user_id;
    char expected_token[MAX_TOKEN_LENGTH + 1];
    QueryResult qres = db_get_new_verification_token(context->db_conn, email, &user_id, expected_token);
    if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't retrieve verification information.");
        return;
    } else if (qres == QRESULT_NONE_AFFECTED) {
        send_error_message(client_socket, 404, "Invalid or expired verification link.");
        return;
    }

    result.token = provided_token;
    if (strcmp(provided_token, expected_token) != 0) {
        result.message = "Invalid or expired verification link.";
        result.success = false;

        if (!db_create_verification_result(context->db_conn, &result)) {
            send_error_message(client_socket, 500, "Couldn't create verification result.");
            return;
        }
        char location[128];
        sprintf(location, "Location: /user/verify?v=%s\r\n", provided_token);
        send_headers(client_socket, 303, NULL, location);
        return;
    }

    Verifying:
    qres = db_verify_new_email(context->db_conn, user_id, email, provided_token);
    if (qres == QRESULT_INTERNAL_ERROR || qres == QRESULT_NONE_AFFECTED) {
        result.message = "Couldn't verify the e-mail.";
        result.success = false;
    } else if (qres == QRESULT_UNIQUE_CONSTRAINT_ERROR) {
        if (!db_delete_unverified_user(context->db_conn, email)) {
            result.message = "Couldn't verify the e-mail.";
            result.success = false;
        } else {
            goto Verifying;
        }
    } else {
        result.message = "E-Mail has been updated.";
        result.success = true;
    }
    if (!db_create_verification_result(context->db_conn, &result)) {
        send_error_message(client_socket, 500, "Couldn't create verification result.");
        return;
    }
    char location[128];
    sprintf(location, "Location: /user/verify?v=%s\r\n", provided_token);
    send_headers(client_socket, 303, NULL, location);
}


static void forgot_password(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;

    if (strstr(req->headers, "Content-Type: application/x-www-form-urlencoded") == NULL) {
        send_error_message(client_socket, 415, "Content-Type should be set to application/x-www-form-urlencoded.");
        return;
    }

    const char *expected_keys[] = {"email"};
    bool found_keys[1];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_url_data(req->body, expected_keys, 1, found_keys) || !found_keys[0]) {
        try_sending_error_file(client_socket, 405);
        return;
    }

    char email[DB_EMAIL_LEN + 1];
    if (!extract_url_param(req->body, "email", email, DB_EMAIL_LEN)) {
        send_error_message(client_socket, 401, "Invalid e-mail.");
        return;
    }

    char token[MAX_TOKEN_LENGTH + 1];
    QueryResult qres = db_set_verification_token(context->db_conn, email, token);
    if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't create password-reset link.");
        return;
    } else if (qres == QRESULT_NONE_AFFECTED) {
        send_error_message(client_socket, 404, "Invalid e-mail.");
        return;
    }

    const char *reset_filepath = DOCUMENT_ROOT"/mails/password_reset.html";
    char reset_link[256 + MAX_TOKEN_LENGTH];
    snprintf(reset_link, sizeof(reset_link),
             "<a href=\"%s/user/reset-password?v=%s\">Click Here</a>",
             SERVER_DOMAIN, token);

    if (!send_email(email, "Reset Your password", reset_filepath, "<!-- RESET_LINK -->", reset_link)) {
        send_error_message(client_socket, 500, "Couldn't send an e-mail for resetting password.");
        return;
    }

    send_headers(client_socket, 204, NULL, NULL);
}


static void get_reset_password_page(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *expected_keys[] = {"v"};
    bool found_keys[1];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_url_data(req->query_string, expected_keys, 1, found_keys) || !found_keys[0]) {
        try_sending_error_file(client_socket, 404);
        return;
    }

    char token[MAX_TOKEN_LENGTH + 1];
    extract_url_param(req->query_string, "v", token, MAX_TOKEN_LENGTH);

    bool exists;
    if (!db_check_reset_password_verification_token(context->db_conn, token, &exists)) {
        try_sending_error_file(client_socket, 500);
        return;
    } else if (!exists) {
        try_sending_error_file(client_socket, 404);
        return;
    }

    const char *reset_pswd_path = DOCUMENT_ROOT"/reset_password_page.html";
    char *remainder;
    char *reset_html = read_template(reset_pswd_path, "<!-- V_TOKEN -->", &remainder);

    char token_html[256 + MAX_TOKEN_LENGTH];
    snprintf(token_html, sizeof(token_html), "<input type=\"hidden\" name=\"vtoken\" value=\"%s\">", token);

    size_t reset_len = strlen(reset_html);
    size_t token_len = strlen(token_html);
    size_t remainder_len = strlen(remainder);
    size_t total_len = reset_len + token_len + remainder_len;
    char content_length[total_len + 1];
    snprintf(content_length, sizeof(content_length), "Content-Length: %ld\r\n", total_len);

    send_headers(client_socket, 200, "text/html", content_length);
    send(client_socket, reset_html, reset_len, 0);
    send(client_socket, token_html, token_len, 0);
    send(client_socket, remainder, remainder_len, 0);

    free(reset_html);
}


static void reset_password(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *body = req->body;

    if (strstr(req->headers, "Content-Type: application/x-www-form-urlencoded") == NULL) {
        send_error_message(client_socket, 415, "Content-Type should be set to application/x-www-form-urlencoded.");
        return;
    }

    const char *expected_keys[] = {"password", "vtoken"};
    bool found_keys[2];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_url_data(body, expected_keys, 2, found_keys) || !found_keys[0] || !found_keys[1]) {
        try_sending_error_file(client_socket, 405);
        return;
    }

    char msg[64];
    char password[DB_PASSWORD_LEN + 1];
    char token[MAX_TOKEN_LENGTH + 1];
    if (!extract_url_param(body, "password", password, DB_PASSWORD_LEN)) {
        sprintf(msg, "Password cannot be longer than %d characters.", DB_PASSWORD_LEN);
        send_error_message(client_socket, 400, msg);
        return;
    } else if (!is_valid_password(password, msg)) {
        send_error_message(client_socket, 400, msg);
        return;
    } else if (!extract_url_param(body, "vtoken", token, MAX_TOKEN_LENGTH)) {
        send_error_message(client_socket, 400, "Invalid or expired verification link.");
        return;
    }

    bool exists;
    if (!db_check_reset_password_verification_token(context->db_conn, token, &exists)) {
        send_error_message(client_socket, 500, "Couldn't check password verification information.");
        return;
    } else if (!exists) {
        send_error_message(client_socket, 400, "Invalid or expired link.");
        return;
    }

    if (!db_reset_user_password(context->db_conn, token, password)) {
        send_error_message(client_socket, 500, "Couldn't reset the password.");
        return;
    }

    const char *location = "Location: /user\r\n";
    send_headers(client_socket, 303, NULL, location);
}


static void signup_user(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *body = req->body;

    if (strstr(req->headers, "Content-Type: application/x-www-form-urlencoded") == NULL) {
        send_error_message(client_socket, 415, "Content-Type should be set to application/x-www-form-urlencoded.");
        return;
    }

    const char *expected_keys[] = {"email", "password"};
    bool found_keys[2];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_url_data(body, expected_keys, 2, found_keys)) {
        send_error_message(client_socket, 400, "Unexpected key. Only 'email' and 'password' accepted.");
        return;
    } else if (!found_keys[0]) {
        send_error_message(client_socket, 400, "E-Mail must be provided.");
        return;
    } else if (!found_keys[1]) {
        send_error_message(client_socket, 400, "Password must be provided.");
        return;
    }

    char email[DB_EMAIL_LEN + 1];
    char password[DB_PASSWORD_LEN + 1];
    extract_url_param(body, "email", email, DB_EMAIL_LEN);
    extract_url_param(body, "password", password, DB_PASSWORD_LEN);

    char msg[64];
    if (!is_valid_password(password, msg)) {
        send_error_message(client_socket, 400, msg);
        return;
    } else if (!is_valid_email(email)) {
        send_error_message(client_socket, 400, "Invalid e-mail.");
        return;
    }

    User user = {.email = email, .password = password};
    char verification_token[MAX_TOKEN_LENGTH + 1];

    QueryResult qres = db_signup_user(context->db_conn, &user, verification_token);
    if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't sign up the user.");
    } else if (qres == QRESULT_UNIQUE_CONSTRAINT_ERROR) {
        send_error_message(client_socket, 409, "E-Mail already taken.");
    } else {
        const char *verify_filepath = DOCUMENT_ROOT"/mails/email_verification.html";
        char verification_form[512 + MAX_TOKEN_LENGTH];
        snprintf(verification_form, sizeof(verification_form),
                 "<form action=\"%s/user/verify\" method=\"POST\" enctype=\"application/x-www-form-urlencoded\">"
                 "<input type=\"hidden\" name=\"email\" value=\"%s\">"
                 "<input type=\"hidden\" name=\"vtoken\" value=\"%s\">"
                 "<button type=\"submit\">Click Here</button>"
                 "</form>",
                 SERVER_DOMAIN, email, verification_token);

        if (!send_email(email, "Verify Your To-Do account", verify_filepath, "<!-- VER_FORM -->", verification_form)) {
            send_error_message(client_socket, 500, "Couldn't send a verification e-mail.");
            return;
        }
        const char *location = "Location: /user\r\n";
        send_headers(client_socket, 201, NULL, location);
    }
}


static void login_user(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    const char *body = req->body;

    if (strstr(req->headers, "Content-Type: application/x-www-form-urlencoded") == NULL) {
        send_error_message(client_socket, 415, "Content-Type should be set to application/x-www-form-urlencoded.");
        return;
    }

    const char *expected_keys[] = {"email", "password"};
    bool found_keys[2];
    memset(found_keys, 0, sizeof(found_keys));

    if (!parse_url_data(body, expected_keys, 2, found_keys)) {
        send_error_message(client_socket, 400, "Unexpected key. Only 'email' and 'password' accepted.");
        return;
    } else if (!found_keys[0]) {
        send_error_message(client_socket, 400, "E-Mail must be provided.");
        return;
    } else if (!found_keys[1]) {
        send_error_message(client_socket, 400, "Password must be provided.");
        return;
    }

    char email[DB_EMAIL_LEN + 1];
    char password[DB_PASSWORD_LEN + 1];

    if (!extract_url_param(body, "email", email, DB_EMAIL_LEN)) {
        send_error_message(client_socket, 401, "Invalid e-mail.");
        return;
    } else if (!extract_url_param(body, "password", password, DB_PASSWORD_LEN)) {
        send_error_message(client_socket, 401, "Invalid password.");
        return;
    }

    User user = {.email = email, .password = password};
    char session_token[MAX_TOKEN_LENGTH + 1];

    QueryResult qres = db_login_user(context->db_conn, &user, session_token);
    if (qres == QRESULT_OK) {
        char cookie[MAX_COOKIE_SIZE];
        snprintf(cookie, sizeof(cookie), "Set-Cookie: session=%s; Path=/; HttpOnly; SameSite=Strict\r\n",
                 session_token);
        const char *location = "Location: /\r\n";
        char header[strlen(cookie) + strlen(location) + 1];
        header[0] = '\0';
        strcat(header, location);
        strcat(header, cookie);

        send_headers(client_socket, 303, NULL, header);
    } else if (qres == QRESULT_USER_ERROR) {
        if (user.is_verified) {
            send_error_message(client_socket, 401, "Invalid password.");
        } else {
            send_error_message(client_socket, 401, "E-Mail not verified.");
        }
    } else if (qres == QRESULT_NONE_AFFECTED) {
        send_error_message(client_socket, 401, "Invalid e-mail.");
    } else if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't sign in the user.");
    }
}


static void logout_user(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    char csrf_token[MAX_TOKEN_LENGTH + 1];
    char session_token[MAX_TOKEN_LENGTH + 1];
    int user_id;

    QueryResult qres = check_and_retrieve_session(req->headers, context->db_conn, &user_id, csrf_token, session_token,
                                                  MAX_TOKEN_LENGTH);
    if (qres == QRESULT_NONE_AFFECTED) {
        send_error_message(client_socket, 401, "Authentication required.");
        return;
    } else if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't retrieve session information.");
        return;
    }

    if (!check_csrf_token(req, csrf_token)) {
        send_error_message(client_socket, 403, "No CSRF token found.");
        return;
    }

    if (db_delete_session(context->db_conn, session_token)) {
        const char *cookie = "Set-Cookie: session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n";
        send_headers(client_socket, 204, NULL, cookie);
    } else {
        send_error_message(client_socket, 500, "Couldn't sign out the user.");
    }
}


void update_user(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    char csrf_token[MAX_TOKEN_LENGTH + 1];
    int user_id;

    QueryResult qres = check_session(req->headers, context->db_conn, &user_id, csrf_token);
    if (qres == QRESULT_NONE_AFFECTED) {
        send_error_message(client_socket, 401, "Authentication required.");
        return;
    } else if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't retrieve session information.");
        return;
    }
    const char *body = req->body;

    if (strstr(req->headers, "Content-Type: application/x-www-form-urlencoded") == NULL) {
        send_error_message(client_socket, 415, "Content-Type should be set to application/x-www-form-urlencoded.");
        return;
    } else if (!check_csrf_token(req, csrf_token)) {
        send_error_message(client_socket, 403, "No CSRF token found.");
        return;
    }

    const char *expected_keys[] = {"email", "password"};
    bool found_keys[2];
    memset(found_keys, 0, sizeof(found_keys));

    // email and password can't be changed in a single request
    if (!parse_url_data(body, expected_keys, 2, found_keys)) {
        send_error_message(client_socket, 400, "Unexpected key. Only 'email' or 'password' accepted.");
        return;
    } else if (found_keys[0] && found_keys[1]) {
        send_error_message(client_socket, 400, "E-Mail and password cannot be updated in the same request.");
        return;
    } else if (!found_keys[0] && !found_keys[1]) {
        send_error_message(client_socket, 400, "Either e-mail or password must be provided.");
        return;
    }

    char email[DB_EMAIL_LEN + 1];
    char password[DB_PASSWORD_LEN + 1];
    if (found_keys[0]) {
        extract_url_param(body, "email", email, DB_EMAIL_LEN);
        if (!is_valid_email(email)) {
            send_error_message(client_socket, 400, "Invalid e-mail.");
            return;
        }
        char verification_token[MAX_TOKEN_LENGTH + 1];
        qres = db_create_email_change_request(context->db_conn, user_id, email, verification_token);
        if (qres == QRESULT_INTERNAL_ERROR || qres == QRESULT_NONE_AFFECTED) {
            send_error_message(client_socket, 500, "Couldn't update the e-mail.");
            return;
        } else if (qres == QRESULT_UNIQUE_CONSTRAINT_ERROR) {
            send_error_message(client_socket, 409, "E-Mail already taken.");
            return;
        }
        const char *verify_filepath = DOCUMENT_ROOT"/mails/email_verification.html";
        char verification_form[512 + MAX_TOKEN_LENGTH];
        snprintf(verification_form, sizeof(verification_form),
                 "<form action=\"%s/user/verify-new\" method=\"POST\" enctype=\"application/x-www-form-urlencoded\">"
                 "<input type=\"hidden\" name=\"email\" value=\"%s\">"
                 "<input type=\"hidden\" name=\"vtoken\" value=\"%s\">"
                 "<button type=\"submit\">Click Here</button>"
                 "</form>",
                 SERVER_DOMAIN, email, verification_token);

        if (!send_email(email, "Verify Your new To-Do e-mail", verify_filepath, "<!-- VER_FORM -->",
                        verification_form)) {
            send_error_message(client_socket, 500, "Couldn't send a verification e-mail.");
            return;
        }
    } else if (found_keys[1]) {
        extract_url_param(body, "password", password, DB_PASSWORD_LEN);
        char msg[64];
        if (!is_valid_password(password, msg)) {
            send_error_message(client_socket, 400, msg);
            return;
        } else if (!db_update_user_password(context->db_conn, user_id, password)) {
            send_error_message(client_socket, 500, "Couldn't update the password.");
            return;
        }
    }
    send_headers(client_socket, 204, NULL, NULL);
}


void delete_user(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    char csrf_token[MAX_TOKEN_LENGTH + 1];
    int user_id;

    QueryResult qres = check_session(req->headers, context->db_conn, &user_id, csrf_token);
    if (qres == QRESULT_NONE_AFFECTED) {
        send_error_message(client_socket, 401, "Authentication required.");
        return;
    } else if (qres == QRESULT_INTERNAL_ERROR) {
        send_error_message(client_socket, 500, "Couldn't retrieve session information.");
        return;
    }

    if (!check_csrf_token(req, csrf_token)) {
        send_error_message(client_socket, 403, "No CSRF token found.");
        return;
    }

    if (!db_delete_user(context->db_conn, user_id)) {
        send_error_message(client_socket, 500, "Couldn't delete the user.");
    } else {
        send_headers(client_socket, 204, NULL, NULL);
    }
}


void handle_http_request(HttpRequest *req, Task *context) {
    int client_socket = context->client_socket;
    int req_method = req->method;
    if (req_method == -1) {
        try_sending_error_file(client_socket, 400);
        return;
    }

    const Route *route = check_route(req->path, req_method);
    if (route) {                                                                // routed files
        route->handler(req, context);
        return;
    } else {                                                                    // static files
        if (req_method != GET) {
            try_sending_error_file(client_socket, 405);
            return;
        }
        char file_path[MAX_PATH_LENGTH];
        snprintf(file_path, sizeof(file_path), "%s/static%s", DOCUMENT_ROOT, req->path);
        if (!is_path_safe(file_path)) {
            try_sending_error_file(client_socket, 404);
            return;
        }
        try_sending_file(client_socket, file_path);
    }
}