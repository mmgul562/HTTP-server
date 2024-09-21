#ifndef HTTP_SERVER_DB_CLEANUP_H
#define HTTP_SERVER_DB_CLEANUP_H

#include <libpq-fe.h>


void cleanup_database(PGconn *conn);


#endif
