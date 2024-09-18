#include "session_middleware.h"
#include "../db/sessions.h"
#include <string.h>


static bool extract_session_token(const char *cookie_header, char *session_token, size_t max_length) {
    const char *token_start = strstr(cookie_header, "session=");
    if (!token_start) {
        fprintf(stderr, "No session token found in cookies\n");
        return false;
    }

    token_start += 8;
    const char *token_end = strpbrk(token_start, ";\r\n");
    size_t token_length = token_end ? (size_t)(token_end - token_start) : strlen(token_start);

    if (token_length > max_length) {
        fprintf(stderr, "Invalid session token length\n");
        return false;
    }

    strncpy(session_token, token_start, token_length);
    session_token[token_length] = '\0';
    return true;
}


QueryResult check_session(const char *headers, PGconn *conn, int *user_id, char *csrf_token) {
    const char *cookie_header = strstr(headers, "\r\nCookie: ");
    if (!cookie_header) {
        fprintf(stderr, "No Cookie Header found\n");
        return QRESULT_NONE_AFFECTED;
    }

    char session_token[MAX_TOKEN_LENGTH + 1];
    if (!extract_session_token(cookie_header, session_token, MAX_TOKEN_LENGTH)) {
        return QRESULT_NONE_AFFECTED;
    }

    return db_validate_and_retrieve_session_info(conn, session_token, csrf_token, user_id);
}


QueryResult check_and_retrieve_session(const char *headers, PGconn *conn, int *user_id, char *csrf_token, char *session_token, size_t max_length) {
    const char *cookie_header = strstr(headers, "\r\nCookie: ");
    if (!cookie_header) {
        fprintf(stderr, "No Cookie Header found\n");
        return QRESULT_NONE_AFFECTED;
    }
    if (session_token) {
        if (!extract_session_token(cookie_header, session_token, max_length)) {
            return QRESULT_NONE_AFFECTED;
        }
    }
    return db_validate_and_retrieve_session_info(conn, session_token, csrf_token, user_id);
}


bool check_csrf_token(HttpRequest *req, const char *expected_csrf_token) {
    char *token_start = strstr(req->headers, "\r\nX-CSRF-Token: ");
    if (!token_start) {
        fprintf(stderr, "No CSRF token header found\n");
        return false;
    }
    token_start += 16;
    const char *token_end = strstr(token_start, "\r\n");
    size_t token_length = token_end ? (size_t)(token_end - token_start) : strlen(token_start);

    if (token_length > MAX_TOKEN_LENGTH) {
        fprintf(stderr, "Invalid CSRF token length");
        return false;
    }
    char provided_csrf_token[MAX_TOKEN_LENGTH + 1];
    strncpy(provided_csrf_token, token_start, token_length);
    provided_csrf_token[token_length] = '\0';

    return (strcmp(expected_csrf_token, provided_csrf_token) == 0);
}