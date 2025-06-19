#ifndef RCRDP_H
#define RCRDP_H

#include <freerdp3/freerdp/freerdp.h>
#include <freerdp3/freerdp/gdi/gdi.h>
#include <freerdp3/freerdp/client/rdpei.h>
#include <freerdp3/freerdp/client/rdpgfx.h>
#include <freerdp3/freerdp/codec/bitmap.h>
#include <pthread.h>
#include <winpr3/winpr/synch.h>

#define MAX_SCREENSHOT_RETRIES 20

// Forward declaration
typedef struct _RDPClient RDPClient;

// Context extension to hold reference to RDPClient
typedef struct {
    rdpContext context;
    RDPClient* client;
} RDPContext;

typedef struct _RDPClient {
    freerdp* instance;
    RDPContext* context;
    BOOL connected;
    BOOL first_frame_received;
    BOOL screenshot_requested;
    char* screenshot_filename;
    int screenshot_retry_count;
    char* hostname;
    int port;
    char* username;
    char* password;
    char* domain;
    
    // Event processing thread
    pthread_t event_thread;
    BOOL thread_running;
    BOOL stop_requested;
    
    // Latest frame data for screenshots
    BYTE* latest_frame_buffer;
    UINT32 latest_frame_width;
    UINT32 latest_frame_height;
    UINT32 latest_frame_stride;
    pthread_mutex_t frame_mutex;
    BOOL frame_updated;
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
typedef enum {
    SCREENSHOT_SUCCESS = 0,
    SCREENSHOT_ERROR = 1,
    SCREENSHOT_BLACK = 2
} ScreenshotResult;

ScreenshotResult execute_screenshot(RDPClient* client, const char* output_file);
BOOL request_screenshot(RDPClient* client, const char* output_file);
BOOL execute_sendkey(RDPClient* client, DWORD flags, DWORD code);
BOOL execute_sendmouse(RDPClient* client, DWORD flags, UINT16 x, UINT16 y);
BOOL execute_movemouse(RDPClient* client, UINT16 x, UINT16 y);

// Event processing thread functions
BOOL rdp_client_start_event_thread(RDPClient* client);
void rdp_client_stop_event_thread(RDPClient* client);
void* rdp_event_thread_proc(void* arg);

// Non-blocking screenshot functions
BOOL get_latest_frame(RDPClient* client, BYTE** buffer, UINT32* width, UINT32* height, UINT32* stride);
BOOL copy_frame_buffer(RDPClient* client, BYTE* src_buffer, UINT32 width, UINT32 height, UINT32 stride);

// Utility functions
CommandType parse_command(const char* cmd_str);
void print_usage(void);

#endif // RCRDP_H