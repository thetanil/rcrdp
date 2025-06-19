#ifndef RCRDP_H
#define RCRDP_H

#include <freerdp3/freerdp/freerdp.h>
#include <freerdp3/freerdp/gdi/gdi.h>
#include <freerdp3/freerdp/client/rdpei.h>
#include <freerdp3/freerdp/client/rdpgfx.h>
#include <freerdp3/freerdp/codec/bitmap.h>

typedef struct {
    freerdp* instance;
    rdpContext* context;
    BOOL connected;
    char* hostname;
    int port;
    char* username;
    char* password;
    char* domain;
} RDPClient;

typedef enum {
    CMD_SCREENSHOT,
    CMD_SENDKEY,
    CMD_SENDMOUSE, 
    CMD_MOVEMOUSE,
    CMD_CONNECT,
    CMD_DISCONNECT,
    CMD_INVALID
} CommandType;

typedef struct {
    CommandType type;
    union {
        struct {
            char* output_file;
        } screenshot;
        struct {
            DWORD flags;
            DWORD code;
        } sendkey;
        struct {
            DWORD flags;
            UINT16 x;
            UINT16 y;
        } mouse;
    } params;
} Command;

// RDP Client functions
RDPClient* rdp_client_new(void);
void rdp_client_free(RDPClient* client);
BOOL rdp_client_connect(RDPClient* client, const char* hostname, int port, 
                       const char* username, const char* password, const char* domain);
void rdp_client_disconnect(RDPClient* client);

// Command functions
BOOL execute_screenshot(RDPClient* client, const char* output_file);
BOOL execute_sendkey(RDPClient* client, DWORD flags, DWORD code);
BOOL execute_sendmouse(RDPClient* client, DWORD flags, UINT16 x, UINT16 y);
BOOL execute_movemouse(RDPClient* client, UINT16 x, UINT16 y);

// Utility functions
CommandType parse_command(const char* cmd_str);
void print_usage(void);

#endif // RCRDP_H