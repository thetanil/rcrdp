#include "rcrdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
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

BOOL execute_screenshot(RDPClient* client, const char* output_file)
{
    if (!client || !client->connected)
        return FALSE;
    
    // Wait for desktop to be ready and process messages (500ms total)
    for (int i = 0; i < 20; i++) { // 500ms total (5 x 100ms)
        // Process pending messages
        if (!freerdp_check_event_handles(client->instance->context)) {
            break;
        }
        usleep(100000); // Sleep 100ms
    }
    
    printf("Capturing screenshot...\n");
    
    rdpGdi* gdi = client->instance->context->gdi;
    if (!gdi || !gdi->primary_buffer)
    {
        fprintf(stderr, "No graphics buffer available for screenshot\n");
        return FALSE;
    }
    
    UINT32 width = gdi->width;
    UINT32 height = gdi->height;
    UINT32 stride = gdi->stride;
    BYTE* buffer = gdi->primary_buffer;
    
    char filename[512];
    if (output_file)
    {
        // User provided filename
        strncpy(filename, output_file, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
    }
    else
    {
        // Create png directory if it doesn't exist
        struct stat st = {0};
        if (stat("png", &st) == -1)
        {
            mkdir("png", 0755);
        }
        
        // Generate ISO timestamp filename
        time_t now = time(NULL);
        struct tm* tm_info = gmtime(&now);
        snprintf(filename, sizeof(filename), "png/screenshot_%04d-%02d-%02dT%02d:%02d:%02dZ.png",
                tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    }
    
    if (!write_png_file(filename, buffer, width, height, stride))
    {
        fprintf(stderr, "Failed to write PNG file: %s\n", filename);
        return FALSE;
    }
    
    printf("Screenshot saved to %s (%ux%u)\n", filename, width, height);
    return TRUE;
}

BOOL execute_sendkey(RDPClient* client, DWORD flags, DWORD code)
{
    if (!client || !client->connected)
        return FALSE;
        
    rdpInput* input = client->instance->context->input;
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
    if (!client || !client->connected)
        return FALSE;
        
    rdpInput* input = client->instance->context->input;
    if (!input)
        return FALSE;
        
    if (!freerdp_input_send_mouse_event(input, flags, x, y))
    {
        fprintf(stderr, "Failed to send mouse event\n");
        return FALSE;
    }
    
    printf("Sent mouse event: flags=0x%08X, x=%u, y=%u\n", flags, x, y);
    return TRUE;
}

BOOL execute_movemouse(RDPClient* client, UINT16 x, UINT16 y)
{
    if (!client || !client->connected)
        return FALSE;
        
    rdpInput* input = client->instance->context->input;
    if (!input)
        return FALSE;
        
    if (!freerdp_input_send_mouse_event(input, PTR_FLAGS_MOVE, x, y))
    {
        fprintf(stderr, "Failed to move mouse\n");
        return FALSE;
    }
    
    printf("Moved mouse to: x=%u, y=%u\n", x, y);
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