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
#define PASSWORD_RESET_EXPIRY_HRS 1


bool db_check_reset_password_verification_token(PGconn *conn, const char *token, bool *exists) {
    const char *query = "SELECT EXISTS (SELECT 1 FROM users WHERE is_verified = true AND verification_token = $1 LIMIT 1)";
    const char *params[1] = {token};
    int param_lengths[1] = {strlen(token)};
    int param_formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Token verification checking failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        return false;
    }
    *exists = strcmp(PQgetvalue(res, 0, 0), "t") == 0;

    PQclear(res);
    return true;
}


QueryResult db_get_verification_token(PGconn *conn, const char *email, char *token, bool *is_verified) {
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
    if (is_verified) {
        *is_verified = strcmp(PQgetvalue(res, 0, 0), "t") == 0;
    }
    strcpy(token, PQgetvalue(res, 0, 1));

    PQclear(res);
    return QRESULT_OK;
}


QueryResult db_set_verification_token(PGconn *conn, const char *email, char *token) {
    char verification_token[SESSION_TOKEN_LENGTH * 2 + 1];
    if (!generate_token(verification_token)) {
        fprintf(stderr, "Error generating verification token\n");
        return QRESULT_INTERNAL_ERROR;
    }
    time_t expiry_time = time(NULL) + (PASSWORD_RESET_EXPIRY_HRS * 3600);
    char expiry_str[21];
    snprintf(expiry_str, sizeof(expiry_str), "%ld", expiry_time);

    const char *query = "UPDATE users SET verification_token = $1, token_expires_at = to_timestamp($2) WHERE email = $3";
    const char *params[3] = {verification_token, expiry_str, email};
    int param_lengths[3] = {strlen(verification_token), strlen(expiry_str), strlen(email)};
    int param_formats[3] = {0, 0, 0};

    PGresult *res = PQexecParams(conn, query, 3, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Verification info update failed: %s", PQerrorMessage(conn));
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


bool db_get_user_email(PGconn *conn, int id, char *email) {
    char query[64];
    sprintf(query, "SELECT email FROM users WHERE id = %d", id);

    PGresult *res = PQexec(conn, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "User email retrieval failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }
    if (PQntuples(res) == 0) {
        fprintf(stderr, "No user with provided ID was found\n");
        PQclear(res);
        return false;
    }
    strcpy(email, PQgetvalue(res, 0, 0));

    PQclear(res);
    return true;
}


static bool check_user_verified(PGconn *conn, const char *email, bool *is_verified) {
    const char *query = "SELECT is_verified FROM users WHERE email = $1";
    const char *params[1] = {email};
    int param_lengths[1] = {strlen(email)};
    int param_formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "User verification info retrieval failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        return false;
    }
    *is_verified = strcmp(PQgetvalue(res, 0, 0), "t") == 0;
    PQclear(res);
    return true;
}


static bool update_unverified_user(PGconn *conn, const char *email, const char *password, const char *token, const char *expiry) {
    const char *query = "UPDATE users SET password = $1, verification_token = $2, token_expires_at = to_timestamp($3) WHERE email = $4";
    const char *params[4] = {password, token, expiry, email};
    int param_lengths[4] = {strlen(password), strlen(token), strlen(expiry), strlen(email)};
    int param_formats[4] = {0, 0, 0, 0};

    PGresult *res = PQexecParams(conn, query, 4, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Unverified user update failed: %s", PQerrorMessage(conn));
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


QueryResult db_signup_user(PGconn *conn, User *user, char *token) {
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
            bool is_verified;
            if (!check_user_verified(conn, user->email, &is_verified)) {
                return QRESULT_INTERNAL_ERROR;
            }
            if (is_verified) {
                return QRESULT_UNIQUE_CONSTRAINT_ERROR;
            } else {
                if (!update_unverified_user(conn, user->email, encoded, verification_token, expiry_str)) {
                    return QRESULT_INTERNAL_ERROR;
                }
                strcpy(token, verification_token);
                return QRESULT_OK;
            }
        }
        fprintf(stderr, "User registration failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return QRESULT_INTERNAL_ERROR;
    }
    strcpy(token, verification_token);
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
    if (PQcmdTuples(res) == 0) {
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}


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
        rollback_transaction(conn, res);
        return QRESULT_INTERNAL_ERROR;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        if (!rollback_transaction(conn, res)) {
            return QRESULT_INTERNAL_ERROR;
        }
        return QRESULT_NONE_AFFECTED;
    }

    int user_id = atoi(PQgetvalue(res, 0, 0));
    char *stored_hash = PQgetvalue(res, 0, 1);
    bool is_verified = strcmp(PQgetvalue(res, 0, 2), "t") == 0;

    user->is_verified = is_verified;
    if (!is_verified) {
        PQclear(res);
        if (!rollback_transaction(conn, res)) {
            return QRESULT_INTERNAL_ERROR;
        }
        return QRESULT_USER_ERROR;
    }

    char csrf_token[SESSION_TOKEN_LENGTH * 2 + 1];
    int verify_result = argon2id_verify(stored_hash, user->password, strlen(user->password));
    PQclear(res);

    if (verify_result == ARGON2_OK) {
        if (!delete_user_sessions(conn, user_id)) {
            rollback_transaction(conn, res);
            return QRESULT_INTERNAL_ERROR;
        } else if (!db_create_session(conn, user_id, session_token, csrf_token)) {
            fprintf(stderr, "Failed to create session\n");
            rollback_transaction(conn, res);
            return QRESULT_INTERNAL_ERROR;
        }
    } else {
        if (!rollback_transaction(conn, res)) {
            return QRESULT_INTERNAL_ERROR;
        }
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
    if (PQcmdTuples(res) == 0) {
        PQclear(res);
        return QRESULT_NONE_AFFECTED;
    }
    PQclear(res);
    return QRESULT_OK;
}


bool db_reset_user_password(PGconn *conn, const char *vtoken, const char *password) {
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

    const char *query = "UPDATE users SET password = $1, verification_token = NULL, token_expires_at = NULL WHERE verification_token = $2";
    const char *params[2] = {encoded, vtoken};
    int param_lengths[2] = {strlen(encoded), strlen(vtoken)};
    int param_formats[2] = {0, 0};

    PGresult *res = PQexecParams(conn, query, 2, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "User password reset failed: %s", PQerrorMessage(conn));
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
        fprintf(stderr, "User password update failed: %s", PQerrorMessage(conn));
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


bool db_delete_unverified_user(PGconn *conn, const char *email) {
    const char *query = "DELETE FROM users WHERE email = $1 AND is_verified = false";
    const char *params[1] = {email};
    int param_lengths[1] = {strlen(email)};
    int param_formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "User deletion failed: %s", PQerrorMessage(conn));
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


bool db_delete_user(PGconn *conn, int id) {
    char query[64];
    sprintf(query, "DELETE FROM users WHERE id = %d", id);

    PGresult *res = PQexec(conn, query);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "User deletion failed: %s", PQerrorMessage(conn));
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