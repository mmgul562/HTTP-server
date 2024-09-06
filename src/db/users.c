#include "users.h"
#include "sessions.h"
#include <string.h>
#include <libpq-fe.h>
#include <argon2.h>
#include <openssl/rand.h>

#define HASH_LEN 32
#define SALT_LEN 16
#define ENCODED_LEN 128


bool db_signup_user(PGconn *conn, User *user) {
    uint8_t salt[SALT_LEN];
    char encoded[ENCODED_LEN];

    if (RAND_bytes(salt, SALT_LEN) != 1) {
        fprintf(stderr, "Error generating random salt\n");
        return false;
    }

    uint32_t t_cost = 2;
    uint32_t m_cost = (1<<16);
    uint32_t parallelism = 1;

    int result = argon2id_hash_encoded(t_cost, m_cost, parallelism,
                                       user->password, strlen(user->password),
                                       salt, SALT_LEN,
                                       HASH_LEN, encoded, ENCODED_LEN);

    if (result != ARGON2_OK) {
        fprintf(stderr, "Error hashing password: %s\n", argon2_error_message(result));
        return false;
    }

    const char *query = "INSERT INTO users (email, hashed_password) VALUES ($1, $2)";
    const char *params[2] = {user->email, encoded};
    int param_lengths[2] = {strlen(user->email), strlen(encoded)};
    int param_formats[2] = {0, 0};

    PGresult *res = PQexecParams(conn, query, 2, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "User registration failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}


bool db_login_user(PGconn *conn, User *user, char *session_token) {
    const char *query = "SELECT id, hashed_password FROM users WHERE email = $1";
    const char *params[1] = {user->email};
    int param_lengths[1] = {strlen(user->email)};
    int param_formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, params, param_lengths, param_formats, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "User login failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        return false;
    }
    int user_id = atoi(PQgetvalue(res, 0, 0));
    char *stored_hash = PQgetvalue(res, 0, 1);

    int verify_result = argon2id_verify(stored_hash, user->password, strlen(user->password));
    if (verify_result == ARGON2_OK) {
        if (!db_create_session(conn, user_id, session_token)) {
            fprintf(stderr, "Failed to create session\n");
            PQclear(res);
            return false;
        }
    }

    PQclear(res);
    return (verify_result == ARGON2_OK);
}