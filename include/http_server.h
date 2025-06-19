#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "rcrdp.h"
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_REQUEST_SIZE 8192
#define MAX_RESPONSE_SIZE 65536
#define DEFAULT_PORT 8080

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_INVALID
} HttpMethod;

typedef struct {
    HttpMethod method;
    char path[256];
    char* body;
    size_t body_length;
    char headers[1024];
} HttpRequest;

typedef struct {
    int status_code;
    char* content_type;
    char* body;
    size_t body_length;
    int is_binary;
} HttpResponse;

typedef struct {
    int server_fd;
    int port;
    RDPClient* rdp_client;
    int running;
} HttpServer;

// HTTP Server functions
HttpServer* http_server_new(int port);
void http_server_free(HttpServer* server);
int http_server_start(HttpServer* server, RDPClient* rdp_client);
void http_server_stop(HttpServer* server);
int http_server_run(HttpServer* server);

// HTTP handling functions
HttpRequest* parse_http_request(const char* request_data);
void free_http_request(HttpRequest* request);
HttpResponse* create_http_response(int status_code, const char* content_type, 
                                 const char* body, size_t body_length, int is_binary);
void free_http_response(HttpResponse* response);
int send_http_response(int client_fd, HttpResponse* response);

// Route handlers
HttpResponse* handle_get_screen(RDPClient* client);
HttpResponse* handle_post_sendkey(RDPClient* client, HttpRequest* request);
HttpResponse* handle_post_sendmouse(RDPClient* client, HttpRequest* request);
HttpResponse* handle_post_movemouse(RDPClient* client, HttpRequest* request);
HttpResponse* handle_get_status(RDPClient* client);

#endif // HTTP_SERVER_H