cmake_minimum_required(VERSION 3.22.1)
project(HTTP_server C)

set(CMAKE_C_STANDARD 23)

find_package(PostgreSQL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

include_directories(${PostgreSQL_INCLUDE_DIRS})
include_directories(${CURL_INCLUDE_DIRS})

add_executable(HTTP_server src/main.c
        src/http/request.c
        src/http/request.h
        src/http/server.c
        src/http/server.h
        src/http/response.c
        src/http/response.h
        src/db/todos.c
        src/db/todos.h
        src/db/users.c
        src/db/users.h
        src/db/sessions.c
        src/db/sessions.h
        src/middlewares/session_middleware.c
        src/middlewares/session_middleware.h
        src/http/util/task.h
        src/http/route.c
        src/http/route.h
        src/db/util/query_result.h
        src/db/util/generate_token.c
        src/db/util/generate_token.h
        src/db/verifications.c
        src/db/verifications.h
        src/db/email_change_requests.c
        src/db/email_change_requests.h
)

target_link_libraries(HTTP_server ${PostgreSQL_LIBRARIES} argon2 OpenSSL::Crypto ${CURL_LIBRARIES})