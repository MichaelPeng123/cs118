#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/**
 * Project 1 starter code
 * All parts needed to be changed/added are marked with TODO
 */

#define BUFFER_SIZE 1024
#define DEFAULT_SERVER_PORT 8081
#define DEFAULT_REMOTE_HOST "131.179.176.34"
#define DEFAULT_REMOTE_PORT 5001

struct server_app {
    // Parameters of the server
    // Local port of HTTP server
    uint16_t server_port;

    // Remote host and port of remote proxy
    char *remote_host;
    uint16_t remote_port;
};

// The following function is implemented for you and doesn't need
// to be change
void parse_args(int argc, char *argv[], struct server_app *app);

// The following functions need to be updated
void handle_request(struct server_app *app, int client_socket);
void serve_local_file(int client_socket, const char *path);
void proxy_remote_file(struct server_app *app, int client_socket, const char *path);

// The main function is provided and no change is needed
int main(int argc, char *argv[])
{
    struct server_app app;
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int ret;

    parse_args(argc, argv, &app);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(app.server_port);

    // The following allows the program to immediately bind to the port in case
    // previous run exits recently
    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", app.server_port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("accept failed");
            continue;
        }
        
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        handle_request(&app, client_socket);
        close(client_socket);
    }

    close(server_socket);
    return 0;
}

void parse_args(int argc, char *argv[], struct server_app *app)
{
    int opt;

    app->server_port = DEFAULT_SERVER_PORT;
    app->remote_host = NULL;
    app->remote_port = DEFAULT_REMOTE_PORT;

    while ((opt = getopt(argc, argv, "b:r:p:")) != -1) {
        switch (opt) {
        case 'b':
            app->server_port = atoi(optarg);
            break;
        case 'r':
            app->remote_host = strdup(optarg);
            break;
        case 'p':
            app->remote_port = atoi(optarg);
            break;
        default: /* Unrecognized parameter or "-?" */
            fprintf(stderr, "Usage: server [-b local_port] [-r remote_host] [-p remote_port]\n");
            exit(-1);
            break;
        }
    }

    if (app->remote_host == NULL) {
        app->remote_host = strdup(DEFAULT_REMOTE_HOST);
    }
}

bool endMatches(const char *path, const char *end) {
    if (strlen(path) < strlen(end)){
        return 0;
    }

    if (strcmp(path + strlen(path) - strlen(end), end) == 0) {
        return 1;
    }
    else {
        return 0;
    }
}

void handle_request(struct server_app *app, int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Read the request from HTTP client
    // Note: This code is not ideal in the real world because it
    // assumes that the request header is small enough and can be read
    // once as a whole.
    // However, the current version suffices for our testing.
    bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        return;  // Connection closed or error
    }

    buffer[bytes_read] = '\0';
    // copy buffer to a new string
    char *request = malloc(strlen(buffer) + 1);
    strcpy(request, buffer);
    char* copy_req = malloc(strlen(request)+1); 
    strcpy(copy_req, request); 

    printf("%s", request);

    // TODO: Parse the header and extract essential fields, e.g. file name
    // Hint: if the requested path is "/" (root), default to index.html
    char *split_file = strtok(request, " ");
    split_file = strtok(NULL, " ");
    char *file_ptr = split_file;

    while(*file_ptr != '\0') {
        if (strncmp(file_ptr, "%20", 3) == 0) {
            *file_ptr++ = ' ';
            memmove(file_ptr, file_ptr + 2, strlen(file_ptr + 2) + 1);
        }
        else if (strncmp(file_ptr, "%25", 3) == 0) {
            *file_ptr++ = '%';
            memmove(file_ptr, file_ptr + 2, strlen(file_ptr + 2) + 1);
        }
        else {
            file_ptr++;
        }
    }

    if (split_file[0] == '/'){
        split_file += 1;
    }
    if (split_file[0] == '\0') {
        split_file = "index.html";
    }
    
    printf("%s\n\n", split_file);
        
    // char *split_file = "index.html";

    // TODO: Implement proxy and call the function under condition
    // specified in the spec
    if (endMatches(split_file, ".ts")) {
        proxy_remote_file(app, client_socket, copy_req);
    } else {
        serve_local_file(client_socket, split_file);
    }
}


void serve_local_file(int client_socket, const char *path) {
    printf("%s hi", path);
    // TODO: Properly implement serving of local files
    // The following code returns a dummy response for all requests
    // but it should give you a rough idea about what a proper response looks like
    // What you need to do 
    // (when the requested file exists):
    // * Open the requested file
    // * Build proper response headers (see details in the spec), and send them
    // * Also send file content
    // (When the requested file does not exist):
    // * Generate a correct response

    char* content_type;
    if (access(path, F_OK) == 0) {
        // file exists
    } else {
        char dne_response[100];

        sprintf(
            dne_response,
            "HTTP/1.0 404 Not Found\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "404 No "
        );
        send(client_socket, dne_response, strlen(dne_response), 0);
        return;
    }

    if (endMatches(path, ".html")) {
        content_type = "text/html; charset=UTF-8";
    }
    else if (endMatches(path, ".txt")) {
        content_type = "text/plain; charset=UTF-8";
    }
    else if (endMatches(path, ".jpg")){
        content_type = "image/jpeg";
    }
    else {
        content_type = "application/octet-stream";
    }

    FILE * file = fopen (path, "rb");

    fseek(file, 0, SEEK_END);
    long content_length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = (char *)malloc(content_length + 1);
    fread(buffer, 1, content_length, file);

    char response[100];
    
    sprintf(
        response,
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "\r\n", content_type, content_length
    );

    send(client_socket, response, strlen(response), 0);
    send(client_socket, buffer, content_length, 0);
}

void proxy_remote_file(struct server_app *app, int client_socket, const char *message) {
    printf("hello!");
    // TODO: Implement proxy request and replace the following code
    // What's needed:
    // * Connect to remote server (app->remote_server/app->remote_port)
    // * Forward the original request to the remote server
    // * Pass the response from remote server back
    // Bonus:
    // * When connection to the remote server fail, properly generate
    // HTTP 502 "Bad Gateway" response
    char error_response[] = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
    

    int remote_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in remote_addr; 
    int connected = 1;
    
    setsockopt(remote_socket, SOL_SOCKET, SO_REUSEADDR, &connected, sizeof(connected));
    if (remote_socket == -1) {
        send(client_socket, error_response, strlen(error_response), 0); 
        // exit(EXIT_FAILURE);
        close(remote_socket); 
    }

    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(app->remote_port);
    if (inet_pton(AF_INET, app->remote_host, &remote_addr.sin_addr)<=0) {
        // exit(EXIT_FAILURE);
        send(client_socket, error_response, strlen(error_response), 0); 
        close(remote_socket); 
    }

    // remote_addr.sin_family = AF_INET;
    // remote_addr.sin_addr.s_addr = inet_addr(app->remote_host);
    // remote_addr.sin_port = htons(app->remote_port);

    // if (connect(remote_socket, (struct sockaddr*)&remote_addr, sizeof(remote_addr)) == -1) {
    //     perror("connect failed");
    //     close(remote_socket);
    //     exit(EXIT_FAILURE);
    // }

    printf("Attempting to connect...\n"); 
    if (connect(remote_socket,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr_in)) < 0) {
        // exit(EXIT_FAILURE);
        send(client_socket, error_response, strlen(error_response), 0); 
        close(remote_socket); 
     }
    
    send(remote_socket, message, strlen(message), 0); 
    
    printf("Connection Successful!\n"); 
    char buffer[1024]; 
    // buffer[bytes_read] = '\0';
    while(true){
        ssize_t currRead = recv(remote_socket, buffer, sizeof(buffer), 0);
        if (currRead > 0) {
            send(client_socket, buffer, currRead, 0); 
        }
        else break;
    }
    close(remote_socket); 
}
