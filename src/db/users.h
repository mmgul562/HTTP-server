#ifndef HTTP_SERVER_USERS_H
#define HTTP_SERVER_USERS_H

#include "util/query_result.h"
#include <libpq-fe.h>
#include <time.h>

#define DB_EMAIL_LEN 128
#define DB_PASSWORD_LEN 128


typedef struct {
    int id;
    char *email;
    char *password;
    bool is_verified;
} User;

bool db_check_reset_password_verification_token(PGconn *conn, const char *token, bool *exists);

QueryResult db_get_verification_token(PGconn *conn, const char *email, char *token, bool *is_verified);

QueryResult db_set_verification_token(PGconn *conn, const char *email);

bool db_verify_email(PGconn *conn, const char *email);

bool db_get_user_email(PGconn *conn, int id, char *email);

QueryResult db_signup_user(PGconn *conn, User *user);

QueryResult db_login_user(PGconn *conn, User *user, char *session_token);

QueryResult db_update_user_email(PGconn *conn, int id, const char *email);

bool db_reset_user_password(PGconn *conn, const char *vtoken, const char *password);

bool db_update_user_password(PGconn *conn, int id, const char *password);

bool db_delete_unverified_user(PGconn *conn, const char *email);

bool db_delete_user(PGconn *conn, int id);


#endif