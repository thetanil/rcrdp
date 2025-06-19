#include "rcrdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp3/freerdp/client/cmdline.h>
#include <freerdp3/freerdp/channels/channels.h>
#include <freerdp3/freerdp/gdi/gdi.h>
#include <freerdp3/freerdp/settings.h>
#include <freerdp3/freerdp/settings_types.h>
#include <winpr3/winpr/wlog.h>

static BOOL rdp_client_pre_connect(freerdp* instance)
{
    WINPR_UNUSED(instance);
    return TRUE;
}

static BOOL rdp_client_begin_paint(rdpContext* context)
{
    WINPR_UNUSED(context);
    return TRUE;
}

static BOOL rdp_client_end_paint(rdpContext* context)
{
    RDPContext* ctx = (RDPContext*)context;
    RDPClient* client = ctx->client;
    
    // Mark that we've received at least one frame
    if (client) {
        client->first_frame_received = TRUE;
        
        // If a screenshot was requested, take it now
        if (client->screenshot_requested) {
            ScreenshotResult result = execute_screenshot(client, client->screenshot_filename);
            
            if (result == SCREENSHOT_BLACK && client->screenshot_retry_count < MAX_SCREENSHOT_RETRIES - 1) {
                // Screenshot is black and we have retries left, try again
                client->screenshot_retry_count++;
                
                // Send additional desktop wake-up triggers for retries
                rdpInput* input = client->context->context.input;
                if (input) {
                    // Move mouse to different position to encourage screen updates
                    freerdp_input_send_mouse_event(input, PTR_FLAGS_MOVE, 
                                                 50 + (client->screenshot_retry_count * 20), 
                                                 50 + (client->screenshot_retry_count * 20));
                    usleep(100000); // 100ms
                    freerdp_input_send_mouse_event(input, PTR_FLAGS_MOVE, 0, 0);
                }
                
                // Don't clear screenshot_requested, let it try again on next EndPaint
                return TRUE;
            }
            
            // Either success, error, or max retries reached - finish the request
            client->screenshot_requested = FALSE;
            client->screenshot_retry_count = 0;
            
            // Clean up filename
            if (client->screenshot_filename) {
                free(client->screenshot_filename);
                client->screenshot_filename = NULL;
            }
        }
    }
    
    return TRUE;
}

static BOOL rdp_client_post_connect(freerdp* instance)
{
    if (!gdi_init(instance, PIXEL_FORMAT_RGBX32))
        return FALSE;
    
    rdpUpdate* update = instance->context->update;
    if (update) {
        update->BeginPaint = rdp_client_begin_paint;
        update->EndPaint = rdp_client_end_paint;
    }
        
    return TRUE;
}

static void rdp_client_post_disconnect(freerdp* instance)
{
    if (instance && instance->context)
    {
        rdpContext* context = instance->context;
        if (context->gdi)
        {
            gdi_free(instance);
        }
    }
}

static BOOL rdp_client_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
    WINPR_UNUSED(instance);
    WINPR_UNUSED(username);
    WINPR_UNUSED(password);
    WINPR_UNUSED(domain);
    return TRUE;
}

static DWORD rdp_client_verify_certificate(freerdp* instance, const char* host, UINT16 port,
                                          const char* common_name, const char* subject,
                                          const char* issuer, const char* fingerprint, DWORD flags)
{
    WINPR_UNUSED(instance);
    WINPR_UNUSED(host);
    WINPR_UNUSED(port);
    WINPR_UNUSED(common_name);
    WINPR_UNUSED(subject);
    WINPR_UNUSED(issuer);
    WINPR_UNUSED(fingerprint);
    WINPR_UNUSED(flags);
    
    // Accept all certificates to avoid certificate prompts
    return 2; // Accept certificate for this session only
}

RDPClient* rdp_client_new(void)
{
    RDPClient* client = (RDPClient*)calloc(1, sizeof(RDPClient));
    if (!client)
        return NULL;
    
    // Set quieter logging to reduce noise from FreeRDP warnings
    WLog_SetLogLevel(WLog_GetRoot(), WLOG_FATAL);
        
    client->instance = freerdp_new();
    if (!client->instance)
    {
        free(client);
        return NULL;
    }
    
    // Set the context size to our extended context
    client->instance->ContextSize = sizeof(RDPContext);
    
    if (!freerdp_context_new(client->instance))
    {
        freerdp_free(client->instance);
        free(client);
        return NULL;
    }
    
    client->context = (RDPContext*)client->instance->context;
    client->context->client = client;
    
    client->instance->PreConnect = rdp_client_pre_connect;
    client->instance->PostConnect = rdp_client_post_connect;
    client->instance->PostDisconnect = rdp_client_post_disconnect;
    client->instance->Authenticate = rdp_client_authenticate;
    client->instance->VerifyCertificateEx = rdp_client_verify_certificate;
    
    client->connected = FALSE;
    client->first_frame_received = FALSE;
    client->screenshot_requested = FALSE;
    client->screenshot_filename = NULL;
    client->screenshot_retry_count = 0;
    client->port = 3389;
    
    return client;
}

void rdp_client_free(RDPClient* client)
{
    if (!client)
        return;
        
    if (client->connected)
        rdp_client_disconnect(client);
        
    if (client->hostname)
        free(client->hostname);
    if (client->username)
        free(client->username);
    if (client->password)
        free(client->password);
    if (client->domain)
        free(client->domain);
    if (client->screenshot_filename)
        free(client->screenshot_filename);
        
    if (client->instance)
    {
        freerdp_context_free(client->instance);
        freerdp_free(client->instance);
    }
    
    free(client);
}

BOOL rdp_client_connect(RDPClient* client, const char* hostname, int port,
                       const char* username, const char* password, const char* domain)
{
    if (!client || !hostname)
        return FALSE;
        
    rdpSettings* settings = client->context->context.settings;
    
    client->hostname = _strdup(hostname);
    client->port = port;
    if (username)
        client->username = _strdup(username);
    if (password)
        client->password = _strdup(password);
    if (domain)
        client->domain = _strdup(domain);
    
    freerdp_settings_set_string(settings, FreeRDP_ServerHostname, hostname);
    freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, port);
    if (username)
        freerdp_settings_set_string(settings, FreeRDP_Username, username);
    if (password)
        freerdp_settings_set_string(settings, FreeRDP_Password, password);
    if (domain)
        freerdp_settings_set_string(settings, FreeRDP_Domain, domain);
    
    freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 1024);
    freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 768);
    freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32);
    freerdp_settings_set_bool(settings, FreeRDP_SoftwareGdi, TRUE);
    freerdp_settings_set_bool(settings, FreeRDP_IgnoreCertificate, TRUE);
    
    // License and security settings to reduce warnings
    freerdp_settings_set_bool(settings, FreeRDP_ServerLicenseRequired, FALSE);
    freerdp_settings_set_uint32(settings, FreeRDP_EncryptionMethods, ENCRYPTION_METHOD_NONE);
    freerdp_settings_set_uint32(settings, FreeRDP_ExtEncryptionMethods, 0);
    
    // Network settings for better compatibility
    freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheEnabled, TRUE);
    freerdp_settings_set_uint32(settings, FreeRDP_OffscreenSupportLevel, 1);
    freerdp_settings_set_bool(settings, FreeRDP_CompressionEnabled, TRUE);
    
    if (!freerdp_connect(client->instance))
    {
        fprintf(stderr, "Failed to connect to %s:%d\n", hostname, port);
        return FALSE;
    }
    
    client->connected = TRUE;
    printf("Connected to %s:%d\n", hostname, port);
    return TRUE;
}

void rdp_client_disconnect(RDPClient* client)
{
    if (!client || !client->connected)
        return;
        
    freerdp_disconnect(client->instance);
    client->connected = FALSE;
    printf("Disconnected from %s:%d\n", client->hostname, client->port);
}