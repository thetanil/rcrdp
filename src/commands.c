#include "rcrdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp3/freerdp/input.h>
#include <freerdp3/freerdp/gdi/gdi.h>

BOOL execute_screenshot(RDPClient* client, const char* output_file)
{
    if (!client || !client->connected || !output_file)
        return FALSE;
        
    rdpGdi* gdi = client->instance->context->gdi;
    if (!gdi || !gdi->primary_buffer)
    {
        fprintf(stderr, "No graphics buffer available for screenshot\n");
        return FALSE;
    }
    
    FILE* file = fopen(output_file, "wb");
    if (!file)
    {
        fprintf(stderr, "Failed to open output file: %s\n", output_file);
        return FALSE;
    }
    
    UINT32 width = gdi->width;
    UINT32 height = gdi->height;
    UINT32 stride = gdi->stride;
    BYTE* buffer = gdi->primary_buffer;
    
    fprintf(file, "P6\n%u %u\n255\n", width, height);
    
    for (UINT32 y = 0; y < height; y++)
    {
        BYTE* line = buffer + y * stride;
        for (UINT32 x = 0; x < width; x++)
        {
            UINT32 pixel = ((UINT32*)line)[x];
            BYTE r = (pixel >> 16) & 0xFF;
            BYTE g = (pixel >> 8) & 0xFF;
            BYTE b = pixel & 0xFF;
            fwrite(&r, 1, 1, file);
            fwrite(&g, 1, 1, file);
            fwrite(&b, 1, 1, file);
        }
    }
    
    fclose(file);
    printf("Screenshot saved to %s (%ux%u)\n", output_file, width, height);
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
    printf("  screenshot <file.ppm>     Take screenshot and save as PPM file\n");
    printf("  sendkey <flags> <code>    Send keyboard event\n");
    printf("                            flags: 1=down, 2=release\n");
    printf("                            code: virtual key code\n");
    printf("  sendmouse <flags> <x> <y> Send mouse event\n");
    printf("                            flags: mouse button/action flags\n");
    printf("  movemouse <x> <y>         Move mouse to coordinates\n\n");
    printf("Examples:\n");
    printf("  rcrdp -h 192.168.1.100 -u admin -P password connect\n");
    printf("  rcrdp screenshot desktop.ppm\n");
    printf("  rcrdp sendkey 1 65      # Press 'A' key\n");
    printf("  rcrdp sendkey 2 65      # Release 'A' key\n");
    printf("  rcrdp movemouse 100 200\n");
    printf("  rcrdp sendmouse 0x1000 100 200  # Left click at (100,200)\n");
}