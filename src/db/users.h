#ifndef HTTP_SERVER_USERS_H
#define HTTP_SERVER_USERS_H

#include <libpq-fe.h>
#include <stdbool.h>


typedef struct {
    int id;
    char *email;
    char *password;
} User;

bool db_signup_user(PGconn *conn, User *user);

bool db_login_user(PGconn *conn, User *user, char *session_token);


#endif