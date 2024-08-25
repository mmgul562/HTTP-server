#ifndef HTTP_SERVER_ROUTER_H
#define HTTP_SERVER_ROUTER_H


typedef struct {
    const char *url;
    const char *file;
} Route;

const Route ROUTES[] = {
        {"/", "index.html"},
        {"/about", "about.html"}
};

const int ROUTES_COUNT = sizeof(ROUTES) / sizeof(Route);


#endif
