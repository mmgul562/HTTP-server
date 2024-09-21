#include "email_change_requests.h"
#include "util/generate_token.h"
#include "users.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define VERIFICATION_EXPIRY_HRS 24


static bool rollback_transaction(PGconn *conn, PGresult *res) {
    res = PQexec(conn, "ROLLBACK");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Failed to rollback transaction: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}


static bool check_email_taken(PGconn *conn, const char *email, bool *taken) {
    const char *query = "SELECT EXISTS (SELECT 1 FROM users WHERE email = $1 AND is_verified = true LIMIT 1)";
    const char *params[1] = {email};
    int param_lengths[1] = {strlen(email)};
    int param_formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Checking email existence failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }
    if (PQcmdTuples(res) == 0) {
        PQclear(res);
        return false;
    }
    *taken = strcmp(PQgetvalue(res, 0, 0), "t") == 0;
    PQclear(res);
    return true;
}


QueryResult db_create_email_change_request(PGconn *conn, int user_id, const char *email, char *token) {
    bool is_taken;
    if (!check_email_taken(conn, email, &is_taken)) {
        return QRESULT_INTERNAL_ERROR;
    } else if (is_taken) {
        return QRESULT_UNIQUE_CONSTRAINT_ERROR;
    }

    char verification_token[SESSION_TOKEN_LENGTH * 2 + 1];
    if (!generate_token(verification_token)) {
        fprintf(stderr, "Error generating verification token\n");
        return QRESULT_INTERNAL_ERROR;
    }
    time_t expiry_time = time(NULL) + (VERIFICATION_EXPIRY_HRS * 3600);
    char expiry_str[21];
    snprintf(expiry_str, sizeof(expiry_str), "%ld", expiry_time);
    char user_id_str[10];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);

    const char *query = "INSERT INTO email_change_requests(user_id, new_email, verification_token, token_expires_at) "
                        "VALUES ($1, $2, $3, to_timestamp($4))";
    const char *params[4] = {user_id_str, email, verification_token, expiry_str};
    int param_lengths[4] = {strlen(user_id_str), strlen(email), strlen(verification_token), strlen(expiry_str)};
    int param_formats[4] = {0, 0, 0, 0};

    PGresult *res = PQexecParams(conn, query, 4, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (strcmp(PQresultErrorField(res, PG_DIAG_SQLSTATE), "23505") == 0) {
            PQclear(res);
            return QRESULT_UNIQUE_CONSTRAINT_ERROR;
        }
        fprintf(stderr, "Email change request creation failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return QRESULT_INTERNAL_ERROR;
    }
    if (PQcmdTuples(res) == 0) {
        PQclear(res);
        return QRESULT_NONE_AFFECTED;
    }
    strcpy(token, verification_token);
    PQclear(res);
    return QRESULT_OK;
}


QueryResult db_get_new_verification_token(PGconn *conn, const char *email, int *user_id, char *token) {
    const char *query = "SELECT user_id, verification_token FROM email_change_requests "
                        "WHERE new_email = $1 AND token_expires_at > NOW() "
                        "ORDER BY id DESC LIMIT 1";
    const char *params[1] = {email};
    int param_lengths[1] = {strlen(email)};
    int param_formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Verification token retrieval failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return QRESULT_INTERNAL_ERROR;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        return QRESULT_NONE_AFFECTED;
    }
    *user_id = atoi(PQgetvalue(res, 0, 0));
    strcpy(token, PQgetvalue(res, 0, 1));

    PQclear(res);
    return QRESULT_OK;
}


static bool delete_email_change_request(PGconn *conn, const char *token) {
    const char *query = "DELETE FROM email_change_requests WHERE verification_token = $1";
    const char *params[1] = {token};
    int param_lengths[1] = {strlen(token)};
    int param_formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Email change request deletion failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }
    if (PQcmdTuples(res) == 0) {
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}


QueryResult db_verify_new_email(PGconn *conn, int user_id, const char *email, const char *token) {
    PGresult *res = PQexec(conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Failed to begin transaction: %s", PQerrorMessage(conn));
        PQclear(res);
        return QRESULT_INTERNAL_ERROR;
    }
    PQclear(res);

    QueryResult qres = db_update_user_email(conn, user_id, email);
    if (qres != QRESULT_OK) {
        if (!rollback_transaction(conn, res)) {
            return QRESULT_INTERNAL_ERROR;
        }
        return qres;
    }
    if (!delete_email_change_request(conn, token)) {
        rollback_transaction(conn, res);
        return QRESULT_INTERNAL_ERROR;
    }

    res = PQexec(conn, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Failed to commit transaction: %s", PQerrorMessage(conn));
        PQclear(res);
        return QRESULT_INTERNAL_ERROR;
    }
    PQclear(res);
    return QRESULT_OK;
}