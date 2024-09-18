#include "users.h"
#include "sessions.h"
#include "util/generate_token.h"
#include <string.h>
#include <libpq-fe.h>
#include <argon2.h>
#include <openssl/rand.h>

#define HASH_LEN 32
#define SALT_LEN 16
#define ENCODED_LEN 128
#define VERIFICATION_EXPIRY_HRS 24


QueryResult db_get_verification_token(PGconn *conn, const char *email, char *verification_token) {
    const char *query = "SELECT is_verified, verification_token FROM users WHERE email = $1 AND token_expires_at > NOW()";
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

    if (strcmp(PQgetvalue(res, 0, 0), "t") == 0) {
        PQclear(res);
        return QRESULT_USER_ERROR;
    }
    strcpy(verification_token, PQgetvalue(res, 0, 1));

    PQclear(res);
    return QRESULT_OK;
}


bool db_verify_email(PGconn *conn, const char *email) {
    const char *query = "UPDATE users SET is_verified = true, verification_token = NULL, token_expires_at = NULL WHERE email = $1";
    const char *params[1] = {email};
    int param_lengths[1] = {strlen(email)};
    int param_formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Email verification failed: %s", PQerrorMessage(conn));
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


char *db_get_user_email(PGconn *conn, int id) {
    char query[64];
    sprintf(query, "SELECT email FROM users WHERE id = %d", id);

    PGresult *res = PQexec(conn, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "User email retrieval failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return NULL;
    }
    if (PQntuples(res) == 0) {
        fprintf(stderr, "No user with provided ID was found\n");
        PQclear(res);
        return NULL;
    }
    char *email = strdup(PQgetvalue(res, 0, 0));

    PQclear(res);
    return email;
}


QueryResult db_signup_user(PGconn *conn, User *user) {
    uint8_t salt[SALT_LEN];
    char encoded[ENCODED_LEN];

    if (RAND_bytes(salt, SALT_LEN) != 1) {
        fprintf(stderr, "Error generating random salt\n");
        return QRESULT_INTERNAL_ERROR;
    }
    uint32_t t_cost = 2;
    uint32_t m_cost = (1 << 16);
    uint32_t parallelism = 1;

    int result = argon2id_hash_encoded(t_cost, m_cost, parallelism,
                                       user->password, strlen(user->password),
                                       salt, SALT_LEN,
                                       HASH_LEN, encoded, ENCODED_LEN);

    if (result != ARGON2_OK) {
        fprintf(stderr, "Error hashing password: %s\n", argon2_error_message(result));
        return QRESULT_INTERNAL_ERROR;
    }

    char verification_token[SESSION_TOKEN_LENGTH * 2 + 1];
    if (!generate_token(verification_token)) {
        fprintf(stderr, "Error generating verification token\n");
        return QRESULT_INTERNAL_ERROR;
    }
    time_t expiry_time = time(NULL) + (VERIFICATION_EXPIRY_HRS * 3600);
    char expiry_str[21];
    snprintf(expiry_str, sizeof(expiry_str), "%ld", expiry_time);

    const char *query = "INSERT INTO users (email, password, verification_token, token_expires_at) VALUES ($1, $2, $3, to_timestamp($4))";
    const char *params[4] = {user->email, encoded, verification_token, expiry_str};
    int param_lengths[4] = {strlen(user->email), strlen(encoded), strlen(verification_token), strlen(expiry_str)};
    int param_formats[4] = {0, 0, 0, 0};

    PGresult *res = PQexecParams(conn, query, 4, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (strcmp(PQresultErrorField(res, PG_DIAG_SQLSTATE), "23505") == 0) {
            PQclear(res);
            return QRESULT_UNIQUE_CONSTRAINT_ERROR;
        }
        fprintf(stderr, "User registration failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return QRESULT_INTERNAL_ERROR;
    }
    PQclear(res);
    return QRESULT_OK;
}


// for cleaning up old sessions
static bool delete_user_sessions(PGconn *conn, int user_id) {
    char query[64];
    sprintf(query, "DELETE FROM sessions WHERE user_id = %d", user_id);

    PGresult *res = PQexec(conn, query);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "User session deletion failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}


QueryResult db_login_user(PGconn *conn, User *user, char *session_token) {
    PGresult *res = PQexec(conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Failed to begin transaction: %s", PQerrorMessage(conn));
        PQclear(res);
        return QRESULT_INTERNAL_ERROR;
    }
    PQclear(res);

    const char *query = "SELECT id, password, is_verified FROM users WHERE email = $1";
    const char *params[1] = {user->email};
    int param_lengths[1] = {strlen(user->email)};
    int param_formats[1] = {0};

    res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "User login failed: %s", PQerrorMessage(conn));
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return QRESULT_INTERNAL_ERROR;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return QRESULT_NONE_AFFECTED;
    }

    int user_id = atoi(PQgetvalue(res, 0, 0));
    char *stored_hash = PQgetvalue(res, 0, 1);
    bool is_verified = strcmp(PQgetvalue(res, 0, 2), "t") == 0;

    user->is_verified = is_verified;
    if (!is_verified) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return QRESULT_USER_ERROR;
    }

    char csrf_token[SESSION_TOKEN_LENGTH * 2 + 1];
    int verify_result = argon2id_verify(stored_hash, user->password, strlen(user->password));
    PQclear(res);

    if (verify_result == ARGON2_OK) {
        if (!delete_user_sessions(conn, user_id)) {
            PQexec(conn, "ROLLBACK");
            return QRESULT_INTERNAL_ERROR;
        } else if (!db_create_session(conn, user_id, session_token, csrf_token)) {
            fprintf(stderr, "Failed to create session\n");
            PQexec(conn, "ROLLBACK");
            return QRESULT_INTERNAL_ERROR;
        }
    } else {
        PQexec(conn, "ROLLBACK");
        return QRESULT_USER_ERROR;
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


QueryResult db_update_user_email(PGconn *conn, int id, const char *email) {
    char id_str[10];
    snprintf(id_str, sizeof(id_str), "%d", id);
    const char *params[2] = {email, id_str};
    const char *query = "UPDATE users SET email = $1 WHERE id = $2";
    int param_lengths[2] = {strlen(email), strlen(id_str)};
    int param_formats[2] = {0, 0};

    PGresult *res = PQexecParams(conn, query, 2, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (strcmp(PQresultErrorField(res, PG_DIAG_SQLSTATE), "23505") == 0) {
            PQclear(res);
            return QRESULT_UNIQUE_CONSTRAINT_ERROR;
        }
        fprintf(stderr, "User email updating failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return QRESULT_INTERNAL_ERROR;
    }
    PQclear(res);
    return QRESULT_OK;
}


bool db_update_user_password(PGconn *conn, int id, const char *password) {
    uint8_t salt[SALT_LEN];
    char encoded[ENCODED_LEN];

    if (RAND_bytes(salt, SALT_LEN) != 1) {
        fprintf(stderr, "Error generating random salt\n");
        return false;
    }
    uint32_t t_cost = 2;
    uint32_t m_cost = (1 << 16);
    uint32_t parallelism = 1;

    int result = argon2id_hash_encoded(t_cost, m_cost, parallelism,
                                       password, strlen(password),
                                       salt, SALT_LEN,
                                       HASH_LEN, encoded, ENCODED_LEN);

    if (result != ARGON2_OK) {
        fprintf(stderr, "Error hashing password: %s\n", argon2_error_message(result));
        return false;
    }

    char id_str[10];
    snprintf(id_str, sizeof(id_str), "%d", id);

    const char *params[2] = {encoded, id_str};
    const char *query = "UPDATE users SET password = $1 WHERE id = $2";
    int param_lengths[2] = {strlen(encoded), strlen(id_str)};
    int param_formats[2] = {0, 0};

    PGresult *res = PQexecParams(conn, query, 2, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "User password updating failed: %s", PQerrorMessage(conn));
        return false;
    }
    PQclear(res);
    return true;
}


bool db_delete_user(PGconn *conn, int id) {
    char query[64];
    sprintf(query, "DELETE FROM users WHERE id = %d", id);

    PGresult *res = PQexec(conn, query);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "User deletion failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}