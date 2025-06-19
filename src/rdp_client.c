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
    
    if (!client)
        return TRUE;
    
    // Mark that we've received at least one frame
    client->first_frame_received = TRUE;
    
    // Copy current frame to latest frame buffer for non-blocking screenshots
    rdpGdi* gdi = context->gdi;
    if (gdi && gdi->primary_buffer) {
        copy_frame_buffer(client, gdi->primary_buffer, gdi->width, gdi->height, gdi->stride);
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
    
    // Initialize using the proper FreeRDP client entry points pattern
    // This follows the same approach as TestConnect.c for better compatibility
    client->instance = freerdp_new();
    if (!client->instance)
    {
        fprintf(stderr, "Failed to create FreeRDP instance\n");
        free(client);
        return NULL;
    }
    
    // Set the context size to our extended context
    client->instance->ContextSize = sizeof(RDPContext);
    
    // Create the context with proper error handling
    if (!freerdp_context_new(client->instance))
    {
        fprintf(stderr, "Failed to create FreeRDP context\n");
        freerdp_free(client->instance);
        free(client);
        return NULL;
    }
    
    // Cast to our extended context and set up the back-reference
    client->context = (RDPContext*)client->instance->context;
    if (!client->context) {
        fprintf(stderr, "Failed to get FreeRDP context\n");
        freerdp_context_free(client->instance);
        freerdp_free(client->instance);
        free(client);
        return NULL;
    }
    client->context->client = client;
    
    // Set up callback functions
    client->instance->PreConnect = rdp_client_pre_connect;
    client->instance->PostConnect = rdp_client_post_connect;
    client->instance->PostDisconnect = rdp_client_post_disconnect;
    client->instance->Authenticate = rdp_client_authenticate;
    client->instance->VerifyCertificateEx = rdp_client_verify_certificate;
    
    // Initialize client state
    client->connected = FALSE;
    client->first_frame_received = FALSE;
    client->screenshot_requested = FALSE;
    client->screenshot_filename = NULL;
    client->screenshot_retry_count = 0;
    client->port = 3389;
    
    // Initialize threading components
    client->thread_running = FALSE;
    client->stop_requested = FALSE;
    client->latest_frame_buffer = NULL;
    client->latest_frame_width = 0;
    client->latest_frame_height = 0;
    client->latest_frame_stride = 0;
    client->frame_updated = FALSE;
    
    if (pthread_mutex_init(&client->frame_mutex, NULL) != 0) {
        fprintf(stderr, "Failed to initialize frame mutex\n");
        freerdp_context_free(client->instance);
        freerdp_free(client->instance);
        free(client);
        return NULL;
    }
    
    printf("DEBUG: RDP client initialized successfully\n");
    return client;
}

void rdp_client_free(RDPClient* client)
{
    if (!client)
        return;
    
    // Stop event processing thread first
    rdp_client_stop_event_thread(client);
        
    if (client->connected)
        rdp_client_disconnect(client);
    
    // Clean up frame buffer
    pthread_mutex_lock(&client->frame_mutex);
    if (client->latest_frame_buffer) {
        free(client->latest_frame_buffer);
        client->latest_frame_buffer = NULL;
    }
    pthread_mutex_unlock(&client->frame_mutex);
    pthread_mutex_destroy(&client->frame_mutex);
        
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
    
    // Connection timeout settings (similar to TestConnect.c)
    freerdp_settings_set_uint32(settings, FreeRDP_TcpConnectTimeout, 5000); // 5 seconds
    freerdp_settings_set_uint32(settings, FreeRDP_TcpAckTimeout, 9000);     // 9 seconds
    
    // Mouse cursor settings - disable cursor effects that might hide cursor in screenshots
    freerdp_settings_set_bool(settings, FreeRDP_DisableCursorShadow, TRUE);
    freerdp_settings_set_bool(settings, FreeRDP_DisableCursorBlinking, TRUE);
    
    if (!freerdp_connect(client->instance))
    {
        fprintf(stderr, "Failed to connect to %s:%d\n", hostname, port);
        return FALSE;
    }
    
    client->connected = TRUE;
    printf("Connected to %s:%d\n", hostname, port);
    
    // Start the event processing thread
    if (!rdp_client_start_event_thread(client)) {
        fprintf(stderr, "Failed to start event processing thread\n");
        freerdp_disconnect(client->instance);
        client->connected = FALSE;
        return FALSE;
    }
    
    return TRUE;
}

void rdp_client_disconnect(RDPClient* client)
{
    if (!client || !client->connected)
        return;
    
    // Stop event processing thread first
    rdp_client_stop_event_thread(client);
        
    freerdp_disconnect(client->instance);
    client->connected = FALSE;
    printf("Disconnected from %s:%d\n", client->hostname, client->port);
}

// Event processing thread functions
BOOL rdp_client_start_event_thread(RDPClient* client)
{
    if (!client || client->thread_running)
        return FALSE;
    
    client->stop_requested = FALSE;
    
    if (pthread_create(&client->event_thread, NULL, rdp_event_thread_proc, client) != 0) {
        fprintf(stderr, "Failed to create event processing thread\n");
        return FALSE;
    }
    
    client->thread_running = TRUE;
    printf("DEBUG: Event processing thread started\n");
    return TRUE;
}

void rdp_client_stop_event_thread(RDPClient* client)
{
    if (!client || !client->thread_running)
        return;
    
    printf("DEBUG: Stopping event processing thread...\n");
    client->stop_requested = TRUE;
    
    // Wait for thread to finish
    pthread_join(client->event_thread, NULL);
    client->thread_running = FALSE;
    printf("DEBUG: Event processing thread stopped\n");
}

void* rdp_event_thread_proc(void* arg)
{
    RDPClient* client = (RDPClient*)arg;
    
    if (!client || !client->instance) {
        fprintf(stderr, "Invalid client in event thread\n");
        return NULL;
    }
    
    printf("DEBUG: Event processing thread running\n");
    
    while (!client->stop_requested && client->connected) {
        // Get event handles for the RDP connection
        HANDLE handles[32];
        DWORD count = freerdp_get_event_handles(&client->context->context, handles, 32);
        
        if (count == 0) {
            fprintf(stderr, "No event handles available\n");
            break;
        }
        
        // Wait for events with a short timeout to allow checking stop_requested
        DWORD status = WaitForMultipleObjects(count, handles, FALSE, 100); // 100ms timeout
        
        if (status == WAIT_FAILED) {
            fprintf(stderr, "WaitForMultipleObjects failed\n");
            break;
        }
        
        if (status != WAIT_TIMEOUT) {
            // Process the event
            if (!freerdp_check_event_handles(&client->context->context)) {
                fprintf(stderr, "freerdp_check_event_handles failed\n");
                break;
            }
        }
    }
    
    printf("DEBUG: Event processing thread exiting\n");
    return NULL;
}

// Frame buffer management functions
BOOL copy_frame_buffer(RDPClient* client, BYTE* src_buffer, UINT32 width, UINT32 height, UINT32 stride)
{
    if (!client || !src_buffer)
        return FALSE;
    
    pthread_mutex_lock(&client->frame_mutex);
    
    // Calculate required buffer size
    size_t buffer_size = (size_t)height * stride;
    
    // Reallocate buffer if size changed
    if (client->latest_frame_width != width || 
        client->latest_frame_height != height || 
        client->latest_frame_stride != stride) {
        
        if (client->latest_frame_buffer) {
            free(client->latest_frame_buffer);
        }
        
        client->latest_frame_buffer = malloc(buffer_size);
        if (!client->latest_frame_buffer) {
            pthread_mutex_unlock(&client->frame_mutex);
            return FALSE;
        }
        
        client->latest_frame_width = width;
        client->latest_frame_height = height;
        client->latest_frame_stride = stride;
    }
    
    // Copy the frame data
    memcpy(client->latest_frame_buffer, src_buffer, buffer_size);
    client->frame_updated = TRUE;
    
    pthread_mutex_unlock(&client->frame_mutex);
    return TRUE;
}

BOOL get_latest_frame(RDPClient* client, BYTE** buffer, UINT32* width, UINT32* height, UINT32* stride)
{
    if (!client || !buffer || !width || !height || !stride)
        return FALSE;
    
    pthread_mutex_lock(&client->frame_mutex);
    
    if (!client->latest_frame_buffer || !client->frame_updated) {
        pthread_mutex_unlock(&client->frame_mutex);
        return FALSE;
    }
    
    // Calculate buffer size and make a copy
    size_t buffer_size = (size_t)client->latest_frame_height * client->latest_frame_stride;
    *buffer = malloc(buffer_size);
    if (!*buffer) {
        pthread_mutex_unlock(&client->frame_mutex);
        return FALSE;
    }
    
    memcpy(*buffer, client->latest_frame_buffer, buffer_size);
    *width = client->latest_frame_width;
    *height = client->latest_frame_height;
    *stride = client->latest_frame_stride;
    
    pthread_mutex_unlock(&client->frame_mutex);
    return TRUE;
}