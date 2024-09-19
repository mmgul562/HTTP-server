#include "verifications.h"
#include <string.h>


bool db_create_verification_result(PGconn *conn, VerificationResult *result) {
    const char *query = "INSERT INTO verification_results(token, message, success) VALUES ($1, $2, $3)";
    const char *success_str = result->success ? "true" : "false";
    const char *params[3] = {result->token, result->message, success_str};
    int param_lengths[3] = {strlen(result->token), strlen(result->message), strlen(success_str)};
    int param_formats[3] = {0, 0, 0};

    PGresult *res = PQexecParams(conn, query, 3, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Verification result creation failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}


QueryResult db_get_verification_result(PGconn *conn, VerificationResult *result) {
    const char *query = "SELECT expires_at, message, success FROM verification_results WHERE token = $1 ORDER BY id DESC LIMIT 1";
    const char *params[1] = {result->token};
    int param_lengths[1] = {strlen(result->token)};
    int param_formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Verification result retrieval failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return QRESULT_INTERNAL_ERROR;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        return QRESULT_NONE_AFFECTED;
    }

    struct tm tm = (struct tm){0};
    sscanf(PQgetvalue(res, 0, 0), "%d-%d-%d %d:%d:%d",
           &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
           &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;

    result->expires_at = mktime(&tm);
    result->message = PQgetvalue(res, 0, 1);
    result->success = strcmp(PQgetvalue(res, 0, 2), "t") == 0;

    PQclear(res);
    return QRESULT_OK;
}
