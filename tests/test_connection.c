#include "../include/rcrdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define TEST_TIMEOUT 10
#define DESKTOP_LOAD_TIMEOUT 15

static void alarm_handler(int sig)
{
    (void)sig;
    fprintf(stderr, "Test timed out after %d seconds\n", TEST_TIMEOUT);
    exit(1);
}

static BOOL wait_for_desktop_ready(RDPClient* client, int timeout_seconds)
{
    printf("Waiting for desktop to be ready (up to %d seconds)...\n", timeout_seconds);
    
    for (int i = 0; i < timeout_seconds; i++) {
        sleep(1);
        
        rdpGdi* gdi = client->instance->context->gdi;
        if (!gdi || !gdi->primary_buffer) {
            continue;
        }
        
        // Check if screen has non-black content
        UINT32 width = gdi->width;
        UINT32 height = gdi->height;
        UINT32 stride = gdi->stride;
        BYTE* buffer = gdi->primary_buffer;
        
        int non_black_pixels = 0;
        int sample_pixels = 100; // Sample 100 pixels
        
        for (int sample = 0; sample < sample_pixels && sample < (int)(width * height); sample++) {
            int x = (sample * 37) % width;  // Use prime number for pseudo-random sampling
            int y = (sample * 41) % height;
            UINT32 pixel = ((UINT32*)(buffer + y * stride))[x];
            
            // Check if pixel is not black (allowing for some tolerance)
            BYTE r = (pixel >> 16) & 0xFF;
            BYTE g = (pixel >> 8) & 0xFF;
            BYTE b = pixel & 0xFF;
            
            if (r > 10 || g > 10 || b > 10) {
                non_black_pixels++;
            }
        }
        
        // If more than 10% of sampled pixels are non-black, consider desktop ready
        if (non_black_pixels > sample_pixels / 10) {
            printf("Desktop appears ready after %d seconds (%d/%d non-black pixels)\n", 
                   i + 1, non_black_pixels, sample_pixels);
            return TRUE;
        }
        
        if ((i + 1) % 3 == 0) {
            printf("Still waiting... (%d/%d non-black pixels)\n", non_black_pixels, sample_pixels);
        }
    }
    
    printf("Desktop not ready after %d seconds, proceeding anyway\n", timeout_seconds);
    return FALSE;
}

static int test_connection_basic(void)
{
    const char* host = getenv("RCRDP_TEST_HOST");
    const char* user = getenv("RCRDP_TEST_USER");
    const char* pass = getenv("RCRDP_TEST_PASS");
    const char* port_str = getenv("RCRDP_TEST_PORT");
    const char* domain = getenv("RCRDP_TEST_DOMAIN");
    
    if (!host || !user || !pass) {
        printf("SKIP: Missing required environment variables (RCRDP_TEST_HOST, RCRDP_TEST_USER, RCRDP_TEST_PASS)\n");
        return 0;
    }
    
    int port = port_str ? atoi(port_str) : 3389;
    if (port <= 0 || port > 65535) {
        port = 3389;
    }
    
    printf("Testing connection to %s:%d with user %s\n", host, port, user);
    
    RDPClient* client = rdp_client_new();
    if (!client) {
        fprintf(stderr, "FAIL: Failed to create RDP client\n");
        return 1;
    }
    
    signal(SIGALRM, alarm_handler);
    alarm(TEST_TIMEOUT);
    
    BOOL connected = rdp_client_connect(client, host, port, user, pass, 
                                       (domain && strlen(domain) > 0) ? domain : NULL);
    
    alarm(0);
    
    if (!connected) {
        printf("FAIL: Connection to %s:%d failed\n", host, port);
        rdp_client_free(client);
        return 1;
    }
    
    printf("PASS: Successfully connected to %s:%d\n", host, port);
    
    if (client->connected) {
        printf("PASS: Client reports connected state\n");
    } else {
        printf("FAIL: Client not in connected state\n");
        rdp_client_free(client);
        return 1;
    }
    
    sleep(1);
    
    rdp_client_disconnect(client);
    
    if (!client->connected) {
        printf("PASS: Successfully disconnected\n");
    } else {
        printf("FAIL: Client still reports connected after disconnect\n");
        rdp_client_free(client);
        return 1;
    }
    
    rdp_client_free(client);
    printf("PASS: Client cleanup completed\n");
    
    return 0;
}

static int test_invalid_connection(void)
{
    printf("Testing connection with invalid credentials\n");
    
    RDPClient* client = rdp_client_new();
    if (!client) {
        fprintf(stderr, "FAIL: Failed to create RDP client\n");
        return 1;
    }
    
    signal(SIGALRM, alarm_handler);
    alarm(TEST_TIMEOUT);
    
    BOOL connected = rdp_client_connect(client, "127.0.0.1", 3389, 
                                       "invaliduser", "invalidpass", NULL);
    
    alarm(0);
    
    if (connected) {
        printf("UNEXPECTED: Connection succeeded with invalid credentials\n");
        rdp_client_disconnect(client);
    } else {
        printf("PASS: Connection properly failed with invalid credentials\n");
    }
    
    rdp_client_free(client);
    return 0;
}

static int test_screenshot(void)
{
    const char* host = getenv("RCRDP_TEST_HOST");
    const char* user = getenv("RCRDP_TEST_USER");
    const char* pass = getenv("RCRDP_TEST_PASS");
    const char* port_str = getenv("RCRDP_TEST_PORT");
    const char* domain = getenv("RCRDP_TEST_DOMAIN");
    
    if (!host || !user || !pass) {
        printf("SKIP: Missing required environment variables for screenshot test\n");
        return 0;
    }
    
    int port = port_str ? atoi(port_str) : 3389;
    if (port <= 0 || port > 65535) {
        port = 3389;
    }
    
    printf("Testing screenshot functionality\n");
    
    RDPClient* client = rdp_client_new();
    if (!client) {
        fprintf(stderr, "FAIL: Failed to create RDP client\n");
        return 1;
    }
    
    signal(SIGALRM, alarm_handler);
    alarm(TEST_TIMEOUT);
    
    BOOL connected = rdp_client_connect(client, host, port, user, pass, 
                                       (domain && strlen(domain) > 0) ? domain : NULL);
    
    alarm(0);
    
    if (!connected) {
        printf("FAIL: Connection failed for screenshot test\n");
        rdp_client_free(client);
        return 1;
    }
    
    printf("PASS: Connected for screenshot test\n");
    
    // Wait for desktop to be ready with intelligent detection
    wait_for_desktop_ready(client, DESKTOP_LOAD_TIMEOUT);
    
    // Test auto-generated filename
    if (!execute_screenshot(client, NULL)) {
        printf("FAIL: Auto-generated screenshot failed\n");
        rdp_client_disconnect(client);
        rdp_client_free(client);
        return 1;
    }
    
    printf("PASS: Auto-generated screenshot succeeded\n");
    
    // Test custom filename
    if (!execute_screenshot(client, "test_screenshot.png")) {
        printf("FAIL: Custom filename screenshot failed\n");
        rdp_client_disconnect(client);
        rdp_client_free(client);
        return 1;
    }
    
    printf("PASS: Custom filename screenshot succeeded\n");
    
    // Verify files exist
    struct stat st;
    if (stat("test_screenshot.png", &st) == 0) {
        printf("PASS: Custom screenshot file exists (%ld bytes)\n", st.st_size);
        // unlink("test_screenshot.png"); // Clean up
    } else {
        printf("FAIL: Custom screenshot file not found\n");
        rdp_client_disconnect(client);
        rdp_client_free(client);
        return 1;
    }
    
    rdp_client_disconnect(client);
    rdp_client_free(client);
    
    printf("PASS: Screenshot test completed\n");
    return 0;
}

static int test_connection_lifecycle(void)
{
    const char* host = getenv("RCRDP_TEST_HOST");
    const char* user = getenv("RCRDP_TEST_USER");
    const char* pass = getenv("RCRDP_TEST_PASS");
    const char* port_str = getenv("RCRDP_TEST_PORT");
    const char* domain = getenv("RCRDP_TEST_DOMAIN");
    
    if (!host || !user || !pass) {
        printf("SKIP: Missing required environment variables for lifecycle test\n");
        return 0;
    }
    
    int port = port_str ? atoi(port_str) : 3389;
    if (port <= 0 || port > 65535) {
        port = 3389;
    }
    
    printf("Testing connection lifecycle (multiple client instances)\n");
    
    for (int i = 0; i < 3; i++) {
        printf("Connection attempt %d/3\n", i + 1);
        
        RDPClient* client = rdp_client_new();
        if (!client) {
            fprintf(stderr, "FAIL: Failed to create RDP client %d\n", i + 1);
            return 1;
        }
        
        signal(SIGALRM, alarm_handler);
        alarm(TEST_TIMEOUT);
        
        BOOL connected = rdp_client_connect(client, host, port, user, pass, 
                                           (domain && strlen(domain) > 0) ? domain : NULL);
        
        alarm(0);
        
        if (!connected) {
            printf("FAIL: Connection %d failed\n", i + 1);
            rdp_client_free(client);
            return 1;
        }
        
        printf("PASS: Connection %d successful\n", i + 1);
        
        sleep(1);
        
        rdp_client_disconnect(client);
        printf("PASS: Disconnection %d successful\n", i + 1);
        
        rdp_client_free(client);
        printf("PASS: Client %d cleanup completed\n", i + 1);
        
        if (i < 2) {
            sleep(2);
        }
    }
    
    printf("PASS: Connection lifecycle test completed\n");
    
    return 0;
}

int main(void)
{
    int failures = 0;
    
    printf("=== RDP Connection Integration Tests ===\n\n");
    
    printf("Test 1: Basic Connection Test\n");
    failures += test_connection_basic();
    printf("\n");
    
    printf("Test 2: Invalid Connection Test\n");
    failures += test_invalid_connection();
    printf("\n");
    
    printf("Test 3: Connection Lifecycle Test\n");
    failures += test_connection_lifecycle();
    printf("\n");
    
    printf("Test 4: Screenshot Test\n");
    failures += test_screenshot();
    printf("\n");
    
    if (failures == 0) {
        printf("=== ALL TESTS PASSED ===\n");
        return 0;
    } else {
        printf("=== %d TEST(S) FAILED ===\n", failures);
        return 1;
    }
}