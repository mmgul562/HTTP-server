#include "sessions.h"
#include "./util/generate_token.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>

#define SESSION_EXPIRY_DAYS 30


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


QueryResult db_validate_and_retrieve_session_info(PGconn *conn, const char *token, char *csrf_token, int *user_id) {
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
    *user_id = atoi(PQgetvalue(res, 0, 0));
    if (csrf_token) {
        strcpy(csrf_token, PQgetvalue(res, 0, 1));
    }

    PQclear(res);
    return QRESULT_OK;
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