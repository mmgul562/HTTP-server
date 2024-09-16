#ifndef HTTP_SERVER_SESSIONS_H
#define HTTP_SERVER_SESSIONS_H

#include "util/query_result.h"
#include <libpq-fe.h>

#define OFFSET 5


bool db_create_session(PGconn *conn, int user_id, char *token, char *csrf_token);

int db_validate_and_retrieve_session_info(PGconn *conn, const char *token, char *csrf_token);

bool db_delete_session(PGconn *conn, const char *token);


#endif
