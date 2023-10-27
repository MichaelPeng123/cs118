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
#define ENOUGH 1000000

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

bool need_proxy(const char *file_name)
{
    int n = strlen(file_name); 
   return file_name[n-1] == 's' && file_name[n-2] == 't' && file_name[n-3] == '.'; 
}

void handle_request(struct server_app *app, int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Read the request from HTTP client
    // Note: This code is not ideal in the real world because it
    // assumes that the request header is small enough and can be read
    // once as a whole.
    // However, the curßrent version suffices for our testing.
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
 

    // TODO: Parse the header and extract essential fields, e.g. file name
    // Hint: if the requested path is "/" (root), default to index.html
    char file_name[ENOUGH]; 
    memset(file_name,0,strlen(file_name)); 
    int ptr = 5;
    while(request[ptr] !=  ' '){
        strncat(file_name, &request[ptr], 1); 
        ptr++;
    }

   char* modified = (!strlen(file_name)) ? "index.html\0" : file_name; 
   printf("FILE_NAMES IS %s\n", modified); 

   
   int i =0;
   int j =0; 
   int N = strlen(modified); 
   char modified2[N];
   printf("N is %d\n", N); 
   while(i < N){
    printf("%d\n", i); 
    if(i+1 < N && i+2 < N){
        
        if(modified[i] == '%' && modified[i+1] == '2' && modified[i+2] == '0'){
            modified2[j] = ' '; 
            i+=3;
        }
        else if(modified[i] == '%' && modified[i+1] == '2' && modified[i+2] == '5'){
            modified2[j] = '%'; 
            i+=3;
        }
        else{
            printf("%d\n", i);
            modified2[j] = modified[i];
            i++;
        }

    }
    else{ modified2[j] = modified[i];
            i++;}
            j++; 
   }
   modified2[j] = '\0'; 
   printf("FILE_NAME IS %s\n", modified2);
    // TODO: Impßlement proxy and call the function under condition
     // specified in the spec
     if (need_proxy(modified2)) {
        printf("NEED PROXY\n"); 
        proxy_remote_file(app, client_socket, copy_req);
     } else {
        serve_local_file(client_socket, modified2);
    }
    
}

void concat(char* s, char* s1, int length1, int length2){

    int i;
    for (i = 0; i < length2; i++) {
        s[i + length1] = s1[i];
    }
}

void serve_local_file(int client_socket, const char *path) {
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
    printf("PATH IS %s\n", path); 
    int p= 0 ;
    int path_sz = strlen(path); 

    while(p < path_sz + 1 && path[p]!='.'){
        p++;
    }
    char* file_ext = path + p + 1; 
    //printf("%s\n", file_ext); 
    
    FILE* ptr;
    char ch;
    // Opening file in reading mode
    char * buffer = 0;
    long length;
    FILE * f = fopen (path, "rb");
    if(!f){
        char g[] = "HTTP/1.0 404 NOT FOUND\r\n";
        send(client_socket, g, strlen(g), 0);
        return;
    }
    
    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = malloc (length);
    if (buffer)
    {
        fread (buffer, 1, length, f);
    }
    fclose (f);

    char * content_type; 
    if(!strcmp(file_ext, "html\0")){
        content_type = "Content-Type: text/html; charset=UTF-8\r\n"; 
    }
    else if(!strcmp(file_ext, "txt\0")){
         content_type = "Content-Type: text/plain; charset=UTF-8\r\n"; 
    }
    else if(!strcmp(file_ext, "jpg\0")){
        content_type = "Content-Type: image/jpeg\r\n"; 
    }
    else{
        content_type = "Content-Type: application/octet-stream\r\n"; 
        
    }
    char content_sz[ENOUGH]; 
    sprintf(content_sz, "%d\r\n", length); 
    char content_length[ENOUGH] ="Content-Length: "; 
    strcat(content_length, content_sz);    
    char response[ENOUGH];
    memset(response,0,strlen(response)); 
    strcat(response, "HTTP/1.0 200 OK\r\n"); 
    strcat(response, content_type); 
    strcat(response, content_length); 
    strcat(response, "\r\n"); 
    int header_len = strlen(response); 
    concat(response, buffer, header_len, length); 
    
    char g[] = "HTTP/1.0 200 OK\r\n"
                      "Content-Type: text/plain; charset=UTF-8\r\n"
                      "Content-Length: 15\r\n"
                      "\r\n"
                      "Sample response";

    send(client_socket, response, header_len + length, 0);
}

void proxy_remote_file(struct server_app *app, int client_socket, const char *message) {
    // TODO: Implement proxy request and replace the following code
    // What's needed:
    // * Connect to remote server (app->remote_server/app->remote_port)
    // * Forward the original request to the remote server
    // * Pass the response from remote server back
    // Bonus:
    // * When connection to the remote server fail, properly generate
    // HTTP 502 "Bad Gateway" response
     struct sockaddr_in remote_addr; 
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (sockfd == -1) {
       
        char r[] = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
        send(client_socket, r, strlen(r), 0); 
        
    }

    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(app->remote_port);
    if(inet_pton(AF_INET, app->remote_host, &remote_addr.sin_addr)<=0){
            printf("Invalid Address\n");
            close(sockfd); 
            char r[] = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
            send(client_socket, r, strlen(r), 0); 
    }
    printf("CONNECTING\n"); 
     if (connect(sockfd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr_in)) < 0){
        char r[] = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
        send(client_socket, r, strlen(r), 0); 
     }
    
    
    //send the request 
    printf("SENDING REQUEST\n%s\n", message); 
    send(sockfd, message, strlen(message), 0); 
    
    // receive the response 
    printf("RECEIVING RESPONSE\n"); 
    char strData[1024]; 
    ssize_t bytesRead;
    while((bytesRead = recv(sockfd, strData, sizeof(strData), 0)) > 0){
        send(client_socket, strData,bytesRead, 0); 
    }
    close(sockfd); 
}