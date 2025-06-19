#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <arpa/inet.h>

HttpServer* http_server_new(int port)
{
    HttpServer* server = (HttpServer*)calloc(1, sizeof(HttpServer));
    if (!server)
        return NULL;
    
    server->port = port > 0 ? port : DEFAULT_PORT;
    server->server_fd = -1;
    server->rdp_client = NULL;
    server->running = 0;
    
    return server;
}

void http_server_free(HttpServer* server)
{
    if (!server)
        return;
        
    if (server->server_fd >= 0)
        close(server->server_fd);
        
    free(server);
}

int http_server_start(HttpServer* server, RDPClient* rdp_client)
{
    if (!server || !rdp_client)
        return -1;
        
    server->rdp_client = rdp_client;
    
    // Create socket
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        perror("socket failed");
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(server->server_fd);
        return -1;
    }
    
    // Bind to address
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(server->port);
    
    if (bind(server->server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server->server_fd);
        return -1;
    }
    
    // Listen for connections
    if (listen(server->server_fd, 10) < 0) {
        perror("listen failed");
        close(server->server_fd);
        return -1;
    }
    
    server->running = 1;
    printf("HTTP server listening on port %d\n", server->port);
    return 0;
}

void http_server_stop(HttpServer* server)
{
    if (!server)
        return;
        
    server->running = 0;
    if (server->server_fd >= 0) {
        close(server->server_fd);
        server->server_fd = -1;
    }
}

HttpRequest* parse_http_request(const char* request_data)
{
    if (!request_data)
        return NULL;
        
    HttpRequest* request = (HttpRequest*)calloc(1, sizeof(HttpRequest));
    if (!request)
        return NULL;
    
    // Parse first line: METHOD PATH HTTP/1.1
    const char* line_end = strstr(request_data, "\r\n");
    if (!line_end) {
        free(request);
        return NULL;
    }
    
    char first_line[512];
    size_t line_len = (size_t)(line_end - request_data);
    if (line_len >= sizeof(first_line)) {
        free(request);
        return NULL;
    }
    
    strncpy(first_line, request_data, line_len);
    first_line[line_len] = '\0';
    
    // Parse method
    if (strncmp(first_line, "GET ", 4) == 0) {
        request->method = HTTP_GET;
        sscanf(first_line + 4, "%255s", request->path);
    } else if (strncmp(first_line, "POST ", 5) == 0) {
        request->method = HTTP_POST;
        sscanf(first_line + 5, "%255s", request->path);
    } else {
        request->method = HTTP_INVALID;
        free(request);
        return NULL;
    }
    
    // Find headers end and body start
    const char* headers_end = strstr(request_data, "\r\n\r\n");
    if (headers_end) {
        // Copy headers
        size_t headers_len = (size_t)(headers_end - request_data);
        if (headers_len < sizeof(request->headers)) {
            strncpy(request->headers, request_data, headers_len);
            request->headers[headers_len] = '\0';
        }
        
        // Parse body for POST requests
        if (request->method == HTTP_POST) {
            const char* body_start = headers_end + 4;
            request->body_length = strlen(body_start);
            if (request->body_length > 0) {
                request->body = malloc(request->body_length + 1);
                if (request->body) {
                    strcpy(request->body, body_start);
                }
            }
        }
    }
    
    return request;
}

void free_http_request(HttpRequest* request)
{
    if (!request)
        return;
        
    if (request->body)
        free(request->body);
    free(request);
}

HttpResponse* create_http_response(int status_code, const char* content_type, 
                                 const char* body, size_t body_length, int is_binary)
{
    HttpResponse* response = (HttpResponse*)calloc(1, sizeof(HttpResponse));
    if (!response)
        return NULL;
    
    response->status_code = status_code;
    response->content_type = content_type ? strdup(content_type) : strdup("text/plain");
    response->is_binary = is_binary;
    
    if (body && body_length > 0) {
        response->body = malloc(body_length);
        if (response->body) {
            memcpy(response->body, body, body_length);
            response->body_length = body_length;
        }
    }
    
    return response;
}

void free_http_response(HttpResponse* response)
{
    if (!response)
        return;
        
    if (response->content_type)
        free(response->content_type);
    if (response->body)
        free(response->body);
    free(response);
}

int send_http_response(int client_fd, HttpResponse* response)
{
    if (!response)
        return -1;
    
    // Determine status text
    const char* status_text;
    switch (response->status_code) {
        case 200: status_text = "OK"; break;
        case 400: status_text = "Bad Request"; break;
        case 404: status_text = "Not Found"; break;
        case 500: status_text = "Internal Server Error"; break;
        default: status_text = "Unknown"; break;
    }
    
    // Send headers
    char headers[1024];
    snprintf(headers, sizeof(headers),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        response->status_code, status_text,
        response->content_type,
        response->body_length);
    
    if (send(client_fd, headers, strlen(headers), 0) < 0)
        return -1;
    
    // Send body if present
    if (response->body && response->body_length > 0) {
        if (send(client_fd, response->body, response->body_length, 0) < 0)
            return -1;
    }
    
    return 0;
}

static HttpResponse* route_request(HttpServer* server, HttpRequest* request)
{
    if (!server || !request || !server->rdp_client)
        return create_http_response(500, "text/plain", "Server error", 12, 0);
    
    if (request->method == HTTP_GET) {
        if (strncmp(request->path, "/screen", 7) == 0) {
            return handle_get_screen(server->rdp_client);
        } else if (strcmp(request->path, "/status") == 0) {
            return handle_get_status(server->rdp_client);
        } else {
            return create_http_response(404, "text/plain", "Not Found", 9, 0);
        }
    } else if (request->method == HTTP_POST) {
        if (strcmp(request->path, "/sendkey") == 0) {
            return handle_post_sendkey(server->rdp_client, request);
        } else if (strcmp(request->path, "/sendmouse") == 0) {
            return handle_post_sendmouse(server->rdp_client, request);
        } else if (strcmp(request->path, "/movemouse") == 0) {
            return handle_post_movemouse(server->rdp_client, request);
        } else {
            return create_http_response(404, "text/plain", "Not Found", 9, 0);
        }
    }
    
    return create_http_response(400, "text/plain", "Bad Request", 11, 0);
}

int http_server_run(HttpServer* server)
{
    if (!server || !server->running)
        return -1;
    
    printf("Server ready. Available endpoints:\n");
    printf("  GET  /screen     - Get current screenshot (PNG)\n");
    printf("  GET  /status     - Get connection status\n");
    printf("  POST /sendkey    - Send keyboard event\n");
    printf("  POST /sendmouse  - Send mouse button event\n");
    printf("  POST /movemouse  - Move mouse cursor\n");
    
    while (server->running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server->server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (server->running) {
                perror("accept failed");
            }
            continue;
        }
        
        // Read request
        char buffer[MAX_REQUEST_SIZE];
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            
            // Parse and route request
            HttpRequest* request = parse_http_request(buffer);
            if (request) {
                HttpResponse* response = route_request(server, request);
                send_http_response(client_fd, response);
                free_http_response(response);
                free_http_request(request);
            }
        }
        
        close(client_fd);
    }
    
    return 0;
}