#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

// Simple JSON parsing helper for POST requests
static int parse_json_int(const char* json, const char* key)
{
    if (!json || !key)
        return 0;
    
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char* key_pos = strstr(json, search_key);
    if (!key_pos)
        return 0;
    
    key_pos += strlen(search_key);
    
    // Skip whitespace
    while (*key_pos == ' ' || *key_pos == '\t')
        key_pos++;
    
    return atoi(key_pos);
}

HttpResponse* handle_get_screen(RDPClient* client)
{
    if (!client || !client->connected) {
        return create_http_response(500, "text/plain", "RDP not connected", 17, 0);
    }
    
    // Generate temporary filename
    char temp_filename[256];
    snprintf(temp_filename, sizeof(temp_filename), "/tmp/rcrdp_screen_%ld.png", time(NULL));
    
    // Take screenshot
    if (!request_screenshot(client, temp_filename)) {
        return create_http_response(500, "text/plain", "Screenshot failed", 17, 0);
    }
    
    // Read PNG file
    FILE* fp = fopen(temp_filename, "rb");
    if (!fp) {
        return create_http_response(500, "text/plain", "Failed to read screenshot", 25, 0);
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(fp);
        unlink(temp_filename);
        return create_http_response(500, "text/plain", "Empty screenshot", 16, 0);
    }
    
    // Read file content
    char* png_data = malloc(file_size);
    if (!png_data) {
        fclose(fp);
        unlink(temp_filename);
        return create_http_response(500, "text/plain", "Memory allocation failed", 24, 0);
    }
    
    size_t bytes_read = fread(png_data, 1, file_size, fp);
    fclose(fp);
    unlink(temp_filename); // Clean up temp file
    
    if (bytes_read != (size_t)file_size) {
        free(png_data);
        return create_http_response(500, "text/plain", "Failed to read PNG data", 23, 0);
    }
    
    // Create response with PNG data
    HttpResponse* response = create_http_response(200, "image/png", png_data, file_size, 1);
    free(png_data); // create_http_response makes its own copy
    
    return response;
}

HttpResponse* handle_post_sendkey(RDPClient* client, HttpRequest* request)
{
    if (!client || !client->connected) {
        return create_http_response(500, "text/plain", "RDP not connected", 17, 0);
    }
    
    if (!request->body) {
        return create_http_response(400, "text/plain", "Missing request body", 20, 0);
    }
    
    // Parse JSON: {"flags": 1, "code": 65}
    int flags = parse_json_int(request->body, "flags");
    int code = parse_json_int(request->body, "code");
    
    if (flags == 0 && code == 0) {
        return create_http_response(400, "text/plain", "Invalid flags or code", 21, 0);
    }
    
    if (!execute_sendkey(client, (DWORD)flags, (DWORD)code)) {
        return create_http_response(500, "text/plain", "Failed to send key", 18, 0);
    }
    
    return create_http_response(200, "text/plain", "OK", 2, 0);
}

HttpResponse* handle_post_sendmouse(RDPClient* client, HttpRequest* request)
{
    if (!client || !client->connected) {
        return create_http_response(500, "text/plain", "RDP not connected", 17, 0);
    }
    
    if (!request->body) {
        return create_http_response(400, "text/plain", "Missing request body", 20, 0);
    }
    
    // Parse JSON: {"flags": 4096, "x": 100, "y": 200}
    int flags = parse_json_int(request->body, "flags");
    int x = parse_json_int(request->body, "x");
    int y = parse_json_int(request->body, "y");
    
    if (!execute_sendmouse(client, (DWORD)flags, (UINT16)x, (UINT16)y)) {
        return create_http_response(500, "text/plain", "Failed to send mouse event", 27, 0);
    }
    
    return create_http_response(200, "text/plain", "OK", 2, 0);
}

HttpResponse* handle_post_movemouse(RDPClient* client, HttpRequest* request)
{
    if (!client || !client->connected) {
        return create_http_response(500, "text/plain", "RDP not connected", 17, 0);
    }
    
    if (!request->body) {
        return create_http_response(400, "text/plain", "Missing request body", 20, 0);
    }
    
    // Parse JSON: {"x": 100, "y": 200}
    int x = parse_json_int(request->body, "x");
    int y = parse_json_int(request->body, "y");
    
    if (!execute_movemouse(client, (UINT16)x, (UINT16)y)) {
        return create_http_response(500, "text/plain", "Failed to move mouse", 20, 0);
    }
    
    return create_http_response(200, "text/plain", "OK", 2, 0);
}

HttpResponse* handle_get_status(RDPClient* client)
{
    if (!client) {
        return create_http_response(500, "text/plain", "No RDP client", 13, 0);
    }
    
    char status_json[512];
    snprintf(status_json, sizeof(status_json),
        "{"
        "\"connected\": %s,"
        "\"hostname\": \"%s\","
        "\"port\": %d,"
        "\"username\": \"%s\""
        "}",
        client->connected ? "true" : "false",
        client->hostname ? client->hostname : "",
        client->port,
        client->username ? client->username : "");
    
    return create_http_response(200, "application/json", status_json, strlen(status_json), 0);
}