#ifndef HTTP_SERVER_USERS_H
#define HTTP_SERVER_USERS_H

#include "util/query_result.h"
#include <libpq-fe.h>
#include <time.h>


typedef struct {
    int id;
    char *email;
    char *password;
    bool is_verified;
} User;

QueryResult db_get_verification_token(PGconn *conn, const char *email, char *verification_token);

bool db_verify_email(PGconn *conn, const char *email);

char *db_get_user_email(PGconn *conn, int id);

QueryResult db_signup_user(PGconn *conn, User *user);

QueryResult db_login_user(PGconn *conn, User *user, char *session_token);

QueryResult db_update_user_email(PGconn *conn, int id, const char *email);

bool db_update_user_password(PGconn *conn, int id, const char *password);

bool db_delete_user(PGconn *conn, int id);


#endif