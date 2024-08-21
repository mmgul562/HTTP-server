#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024


typedef struct {
    char method[16];
    char path[256];
    char protocol[16];
    char headers[BUFFER_SIZE];
    char body[BUFFER_SIZE];
} HttpRequest;


void parse_http_request(char *buffer, HttpRequest *request) {
    if (sscanf(buffer, "%15s %255s %15s", request->method, request->path, request->protocol) != 3) {
        strcpy(request->method, "");
        strcpy(request->path, "");
        strcpy(request->protocol, "");
        return;
    }

    char *body = strstr(buffer, "\r\n\r\n");
    if (body) {
        *body = '\0';
        body += 4;
        strcpy(request->body, body);

        char *header = buffer;
        while ((body = strstr(header, "\r\n")) != NULL) {
            *body = '\0';
            strcat(request->headers, header);
            strcat(request->headers, "\n");
            header = body + 2;
        }
    } else {
        strcpy(request->headers, "");
        strcpy(request->body, "");
    }
}


void handle_http_request(HttpRequest *request, int client_socket) {
    char response[BUFFER_SIZE];
    if (strcmp(request->method, "GET") == 0) {
        if (strcmp(request->path, "/") == 0) {
            sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello, World!\n");
        } else if (strcmp(request->path, "/about") == 0) {
            sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>Base for an HTTP server</h1>");
        } else {
            sprintf(response, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nPage not found.\n");
        }
    } else {
        sprintf(response, "HTTP/1.1 405 Method Not Allowed\r\nContent-Type:text/plain\r\n\r\nInvalid method.\n");
    }

    send(client_socket, response, strlen(response), 0);
    close(client_socket);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';

        HttpRequest request;
        memset(&request, 0, sizeof(HttpRequest));
        parse_http_request(buffer, &request);

        printf("Received request:\n");
        printf("Method: %s\n", request.method);
        printf("Path: %s\n", request.path);
        printf("Protocol: %s\n", request.protocol);
        printf("Headers:\n%s\n", request.headers);
        printf("Body: %s\n", request.body);

        handle_http_request(&request, client_socket);
    }
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        printf("New connection accepted\n");
        handle_client(client_socket);
    }

    close(server_socket);
    return 0;
}