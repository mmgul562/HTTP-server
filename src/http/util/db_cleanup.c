#include "db_cleanup.h"
#include <stdio.h>


void cleanup_database(PGconn *conn) {
    PGresult *res;

    res = PQexec(conn, "SELECT * FROM cleanup_all()");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Cleanup failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return;
    }

    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        const char *table_name = PQgetvalue(res, i, 0);
        const char *deleted_count = PQgetvalue(res, i, 1);
        printf("Cleaned up %s deleted records from %s\n", deleted_count, table_name);
    }

    PQclear(res);
}