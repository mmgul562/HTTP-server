#ifndef HTTP_SERVER_ROUTE_H
#define HTTP_SERVER_ROUTE_H

#include "../response.h"
#include "../util/task.h"


typedef struct {
    const char *url;
    Method method;
    void (*handler)(HttpRequest *, Task *);
} Route;


#endif
