#include "rcrdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <freerdp3/freerdp/input.h>
#include <freerdp3/freerdp/gdi/gdi.h>
#include <png.h>


static BOOL write_png_file(const char* filename, BYTE* buffer, UINT32 width, UINT32 height, UINT32 stride)
{
    FILE* fp = fopen(filename, "wb");
    if (!fp)
    {
        fprintf(stderr, "Failed to open file %s for writing\n", filename);
        return FALSE;
    }
    
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        return FALSE;
    }
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return FALSE;
    }
    
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return FALSE;
    }
    
    png_init_io(png_ptr, fp);
    
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    
    png_write_info(png_ptr, info_ptr);
    
    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for (UINT32 y = 0; y < height; y++)
    {
        row_pointers[y] = (png_byte*)malloc(width * 3);
        BYTE* src_line = buffer + y * stride;
        
        for (UINT32 x = 0; x < width; x++)
        {
            UINT32 pixel = ((UINT32*)src_line)[x];
            // For PIXEL_FORMAT_RGBX32, the format is typically BGRX in memory
            row_pointers[y][x * 3 + 0] = (pixel >> 16) & 0xFF; // R
            row_pointers[y][x * 3 + 1] = (pixel >> 8) & 0xFF;  // G  
            row_pointers[y][x * 3 + 2] = pixel & 0xFF;         // B
        }
    }
    
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);
    
    for (UINT32 y = 0; y < height; y++)
        free(row_pointers[y]);
    free(row_pointers);
    
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    
    return TRUE;
}

BOOL request_screenshot(RDPClient* client, const char* output_file)
{
    if (!client || !client->connected)
        return FALSE;
    
    // Use the new non-blocking approach - get latest frame immediately
    BYTE* buffer = NULL;
    UINT32 width, height, stride;
    
    // Get the latest frame from the buffer updated by EndPaint callback
    if (!get_latest_frame(client, &buffer, &width, &height, &stride)) {
        printf("No frame data available yet - connection may be initializing\n");
        return FALSE;
    }
    
    // Create filename if not provided
    char filename[512];
    if (output_file) {
        strncpy(filename, output_file, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
    } else {
        // Create png directory if it doesn't exist
        struct stat st = {0};
        if (stat("png", &st) == -1) {
            mkdir("png", 0755);
        }
        
        // Generate ISO timestamp filename
        time_t now = time(NULL);
        struct tm* tm_info = gmtime(&now);
        snprintf(filename, sizeof(filename), "png/screenshot_%04d-%02d-%02dT%02d:%02d:%02dZ.png",
                tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    }
    
    // Write PNG file
    BOOL success = write_png_file(filename, buffer, width, height, stride);
    
    if (success) {
        printf("Screenshot saved to %s (%ux%u)\n", filename, width, height);
    } else {
        fprintf(stderr, "Failed to write PNG file: %s\n", filename);
    }
    
    // Clean up the frame buffer copy
    free(buffer);
    
    return success;
}

ScreenshotResult execute_screenshot(RDPClient* client, const char* output_file)
{
    // This function is now a wrapper around request_screenshot for compatibility
    return request_screenshot(client, output_file) ? SCREENSHOT_SUCCESS : SCREENSHOT_ERROR;
}

BOOL execute_sendkey(RDPClient* client, DWORD flags, DWORD code)
{
    if (!client || !client->connected)
        return FALSE;
        
    rdpInput* input = client->context->context.input;
    if (!input)
        return FALSE;
        
    if (!freerdp_input_send_keyboard_event(input, flags, code))
    {
        fprintf(stderr, "Failed to send keyboard event\n");
        return FALSE;
    }
    
    printf("Sent key event: flags=0x%08X, code=0x%08X\n", flags, code);
    return TRUE;
}

BOOL execute_sendmouse(RDPClient* client, DWORD flags, UINT16 x, UINT16 y)
{
    if (!client || !client->connected) {
        printf("DEBUG: Mouse event failed - client not connected\n");
        return FALSE;
    }
        
    rdpInput* input = client->context->context.input;
    if (!input) {
        printf("DEBUG: Mouse event failed - no input interface\n");
        return FALSE;
    }
    
    // Debug coordinate bounds check
    printf("DEBUG: Desktop resolution: 1024x768, mouse coordinates: %u,%u\n", x, y);
    if (x >= 1024 || y >= 768) {
        printf("WARNING: Mouse coordinates (%u,%u) are outside desktop bounds (1024x768)\n", x, y);
    }
    
    // Decode mouse button flags for debugging
    printf("DEBUG: Analyzing mouse flags 0x%08X:\n", flags);
    if (flags & PTR_FLAGS_DOWN) printf("  - PTR_FLAGS_DOWN (0x8000)\n");
    if (flags & PTR_FLAGS_BUTTON1) printf("  - PTR_FLAGS_BUTTON1 (0x1000) - Left\n");
    if (flags & PTR_FLAGS_BUTTON2) printf("  - PTR_FLAGS_BUTTON2 (0x2000) - Right\n");
    if (flags & PTR_FLAGS_BUTTON3) printf("  - PTR_FLAGS_BUTTON3 (0x4000) - Middle\n");
    if (flags & PTR_FLAGS_MOVE) printf("  - PTR_FLAGS_MOVE (0x0800)\n");
    if (flags & PTR_FLAGS_WHEEL) printf("  - PTR_FLAGS_WHEEL (0x0200)\n");
    if (flags & PTR_FLAGS_HWHEEL) printf("  - PTR_FLAGS_HWHEEL (0x0400)\n");
    
    const char* button_desc = "unknown";
    if (flags & PTR_FLAGS_BUTTON1) button_desc = "left_button";
    else if (flags & PTR_FLAGS_BUTTON2) button_desc = "right_button";
    else if (flags & PTR_FLAGS_BUTTON3) button_desc = "middle_button";
    else if (flags & PTR_FLAGS_MOVE) button_desc = "move";
    
    printf("DEBUG: Sending mouse event - %s at coordinates (%u,%u)\n", button_desc, x, y);
        
    if (!freerdp_input_send_mouse_event(input, flags, x, y))
    {
        fprintf(stderr, "ERROR: FreeRDP failed to send mouse event\n");
        return FALSE;
    }
    
    printf("SUCCESS: Mouse event sent to RDP session\n");
    
    // No need for manual message processing - event thread handles this
    return TRUE;
}

BOOL execute_movemouse(RDPClient* client, UINT16 x, UINT16 y)
{
    if (!client || !client->connected) {
        printf("DEBUG: Mouse move failed - client not connected\n");
        return FALSE;
    }
        
    rdpInput* input = client->context->context.input;
    if (!input) {
        printf("DEBUG: Mouse move failed - no input interface\n");
        return FALSE;
    }
    
    // Debug coordinate bounds check
    printf("DEBUG: Desktop resolution: 1024x768, moving mouse to: %u,%u\n", x, y);
    if (x >= 1024 || y >= 768) {
        printf("WARNING: Mouse coordinates (%u,%u) are outside desktop bounds (1024x768)\n", x, y);
    }
        
    if (!freerdp_input_send_mouse_event(input, PTR_FLAGS_MOVE, x, y))
    {
        fprintf(stderr, "ERROR: FreeRDP failed to move mouse\n");
        return FALSE;
    }
    
    printf("SUCCESS: Mouse moved to coordinates (%u,%u)\n", x, y);
    
    // No need for manual message processing - event thread handles this
    return TRUE;
}

CommandType parse_command(const char* cmd_str)
{
    if (!cmd_str)
        return CMD_INVALID;
        
    if (strcmp(cmd_str, "screenshot") == 0)
        return CMD_SCREENSHOT;
    else if (strcmp(cmd_str, "sendkey") == 0)
        return CMD_SENDKEY;
    else if (strcmp(cmd_str, "sendmouse") == 0)
        return CMD_SENDMOUSE;
    else if (strcmp(cmd_str, "movemouse") == 0)
        return CMD_MOVEMOUSE;
    else if (strcmp(cmd_str, "connect") == 0)
        return CMD_CONNECT;
    else if (strcmp(cmd_str, "disconnect") == 0)
        return CMD_DISCONNECT;
    else
        return CMD_INVALID;
}

void print_usage(void)
{
    printf("Usage: rcrdp [options] <command> [command_args]\n\n");
    printf("Connection options:\n");
    printf("  -h, --host <hostname>     RDP server hostname\n");
    printf("  -p, --port <port>         RDP server port (default: 3389)\n");
    printf("  -u, --username <user>     Username for authentication\n");
    printf("  -P, --password <pass>     Password for authentication\n");
    printf("  -d, --domain <domain>     Domain for authentication\n\n");
    printf("Commands:\n");
    printf("  connect                   Connect to RDP server\n");
    printf("  disconnect                Disconnect from RDP server\n");
    printf("  screenshot [file.png]     Take screenshot and save as PNG file (auto-generated filename if not provided)\n");
    printf("  sendkey <flags> <code>    Send keyboard event\n");
    printf("                            flags: 1=down, 2=release\n");
    printf("                            code: virtual key code\n");
    printf("  sendmouse <flags> <x> <y> Send mouse event\n");
    printf("                            flags: mouse button/action flags\n");
    printf("  movemouse <x> <y>         Move mouse to coordinates\n\n");
    printf("Examples:\n");
    printf("  rcrdp -h 192.168.1.100 -u admin -P password connect\n");
    printf("  rcrdp screenshot desktop.png\n");
    printf("  rcrdp sendkey 1 65      # Press 'A' key\n");
    printf("  rcrdp sendkey 2 65      # Release 'A' key\n");
    printf("  rcrdp movemouse 100 200\n");
    printf("  rcrdp sendmouse 0x1000 100 200  # Left click at (100,200)\n");
}