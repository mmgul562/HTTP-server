#include "sessions.h"
#include <openssl/rand.h>
#include <time.h>
#include <string.h>

#define SESSION_TOKEN_LENGTH 32
#define SESSION_EXPIRY_DAYS 30


static bool generate_session_token(char *token, size_t token_length) {
    unsigned char random_bytes[SESSION_TOKEN_LENGTH];
    if (RAND_bytes(random_bytes, SESSION_TOKEN_LENGTH) != 1) {
        return false;
    }
    for (int i = 0; i < SESSION_TOKEN_LENGTH; i++) {
        sprintf(token + (i * 2), "%02x", random_bytes[i]);
    }
    return true;
}


bool db_create_session(PGconn *conn, int user_id, char *token) {
    char session_token[SESSION_TOKEN_LENGTH * 2 + 1] = {0};
    if (!generate_session_token(session_token, sizeof(session_token))) {
        return false;
    }

    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    tm_info->tm_mday += SESSION_EXPIRY_DAYS;
    time_t expires = mktime(tm_info);

    const char *query = "INSERT INTO sessions (user_id, token, expires_at) VALUES ($1, $2, to_timestamp($3))";
    const char *params[3];
    int param_lengths[3];
    int param_formats[3] = {0, 0, 0};
    char user_id_str[12];
    char expires_str[21];

    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    snprintf(expires_str, sizeof(expires_str), "%ld", expires);

    params[0] = user_id_str;
    params[1] = session_token;
    params[2] = expires_str;

    param_lengths[0] = strlen(user_id_str);
    param_lengths[1] = strlen(session_token);
    param_lengths[2] = strlen(expires_str);

    PGresult *res = PQexecParams(conn, query, 3, NULL, params, param_lengths, param_formats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Session creation failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }

    PQclear(res);
    strcpy(token, session_token);
    return true;
}


bool db_validate_session(PGconn *conn, const char *token) {
    const char *query = "SELECT id FROM sessions WHERE token = $1 AND expires_at > NOW()";
    const char *params[1] = {token};
    int param_lengths[1] = {strlen(token)};
    int param_formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Session validation failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }

    if (PQntuples(res) == 0) {
        fprintf(stderr, "No session with provided token found\n");
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}


bool db_delete_session(PGconn *conn, const char *token) {
    const char *query = "DELETE FROM sessions WHERE token = $1";
    const char *params[1] = {token};
    int param_lengths[1] = {strlen(token)};
    int param_formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Session deletion failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}