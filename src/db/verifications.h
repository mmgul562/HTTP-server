#ifndef HTTP_SERVER_VERIFICATIONS_H
#define HTTP_SERVER_VERIFICATIONS_H

#include "util/query_result.h"
#include <libpq-fe.h>
#include <time.h>


typedef struct {
    char *token;
    time_t expires_at;
    char *message;
    bool success;
} VerificationResult;

bool db_create_verification_result(PGconn *conn, VerificationResult *result);

QueryResult db_get_verification_result(PGconn *conn, VerificationResult *result);


#endif
