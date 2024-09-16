#include "sessions.h"
#include <openssl/rand.h>
#include <time.h>
#include <string.h>

#define SESSION_TOKEN_LENGTH 32
#define SESSION_EXPIRY_DAYS 30


static bool generate_token(char *token) {
    unsigned char random_bytes[SESSION_TOKEN_LENGTH];
    if (RAND_bytes(random_bytes, SESSION_TOKEN_LENGTH) != 1) {
        return false;
    }
    for (int i = 0; i < SESSION_TOKEN_LENGTH; ++i) {
        sprintf(token + (i * 2), "%02x", random_bytes[i]);
    }
    return true;
}


bool db_create_session(PGconn *conn, int user_id, char *session_token, char *csrf_token) {
    if (!generate_token(session_token) || !generate_token(csrf_token)) {
        return false;
    }

    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    tm_info->tm_mday += SESSION_EXPIRY_DAYS;
    time_t expires = mktime(tm_info);

    char user_id_str[12];
    char expires_str[21];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    snprintf(expires_str, sizeof(expires_str), "%ld", expires);

    const char *query = "INSERT INTO sessions (user_id, token, csrf_token, expires_at) VALUES ($1, $2, $3, to_timestamp($4))";
    const char *params[4] = {user_id_str, session_token, csrf_token, expires_str};
    int param_lengths[4] = {strlen(user_id_str), strlen(session_token), strlen(csrf_token), strlen(expires_str)};
    int param_formats[4] = {0, 0, 0, 0};

    PGresult *res = PQexecParams(conn, query, 4, NULL, params, param_lengths, param_formats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Session creation failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}


int db_validate_and_retrieve_session_info(PGconn *conn, const char *token, char *csrf_token) {
    const char *query = "SELECT user_id, csrf_token FROM sessions WHERE token = $1 AND expires_at > NOW()";
    const char *params[1] = {token};
    int param_lengths[1] = {strlen(token)};
    int param_formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Session information retrieval failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return QRESULT_INTERNAL_ERROR;
    }

    if (PQntuples(res) == 0) {
        fprintf(stderr, "No session information with provided token found\n");
        PQclear(res);
        return QRESULT_NONE_AFFECTED;
    }
    int user_id = atoi(PQgetvalue(res, 0, 0));
    if (csrf_token) {
        strcpy(csrf_token, PQgetvalue(res, 0, 1));
    }

    PQclear(res);
    // QueryResult has 5 elements represented as int from 0-4,
    // so we add 5 when we return user index
    // to avoid collisions between user indexes and enum values
    return user_id + OFFSET;
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