#ifndef HTTP_SERVER_SESSIONS_H
#define HTTP_SERVER_SESSIONS_H

#include <libpq-fe.h>


bool db_create_session(PGconn *conn, int user_id, char *token);

int db_validate_session(PGconn *conn, const char *token);

bool db_delete_session(PGconn *conn, const char *token);


#endif
