cmake_minimum_required(VERSION 3.22.1)
project(HTTP_server C)

set(CMAKE_C_STANDARD 23)

add_executable(HTTP_server src/main.c
        src/request.c
        src/request.h
        src/server.c
        src/server.h
)
