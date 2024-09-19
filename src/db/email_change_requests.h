#ifndef HTTP_SERVER_EMAIL_CHANGE_REQUESTS_H
#define HTTP_SERVER_EMAIL_CHANGE_REQUESTS_H

#include "util/query_result.h"
#include <libpq-fe.h>


QueryResult db_create_email_change_request(PGconn *conn, int user_id, const char *email);

QueryResult db_get_new_verification_token(PGconn *conn, const char *email, int *user_id, char *token);

QueryResult db_verify_new_email(PGconn *conn, int user_id, const char *email, const char *token);


#endif
