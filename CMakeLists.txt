cmake_minimum_required(VERSION 3.22.1)
project(HTTP_server C)

set(CMAKE_C_STANDARD 23)

add_executable(HTTP_server src/main.c
        src/http/request.c
        src/http/request.h
        src/http/server.c
        src/http/server.h
        src/http/response.c
        src/http/response.h
)
