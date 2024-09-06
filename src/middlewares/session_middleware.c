#include "session_middleware.h"
#include "../db/sessions.h"
#include <string.h>

#define MAX_TOKEN_LENGTH 65


bool check_session(HttpRequest *req, Task *context) {
    const char *cookie_header = strstr(req->headers, "\r\nCookie: ");
    if (!cookie_header) {
        fprintf(stderr, "No Cookie Header found\n");
        return false;
    }

    char session_token[MAX_TOKEN_LENGTH] = {0};
    if (!extract_session_token(cookie_header, session_token, sizeof(session_token))) {
        return false;
    }

    return db_validate_session(context->db_conn, session_token);
}


bool extract_session_token(const char *cookie_header, char *session_token, size_t max_length) {
    const char *token_start = strstr(cookie_header, "session=");
    if (!token_start) {
        fprintf(stderr, "No session token found in cookies\n");
        return false;
    }

    token_start += 8;
    const char *token_end = strpbrk(token_start, ";\r\n");
    size_t token_length = token_end ? (size_t)(token_end - token_start) : strlen(token_start);

    if (token_length >= max_length) {
        fprintf(stderr, "Invalid session token length");
        return false;
    }

    strncpy(session_token, token_start, token_length);
    session_token[token_length] = '\0';
    return true;
}

