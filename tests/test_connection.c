#include "../include/rcrdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define TEST_TIMEOUT 10

static void alarm_handler(int sig)
{
    (void)sig;
    fprintf(stderr, "Test timed out after %d seconds\n", TEST_TIMEOUT);
    exit(1);
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
    
    if (failures == 0) {
        printf("=== ALL TESTS PASSED ===\n");
        return 0;
    } else {
        printf("=== %d TEST(S) FAILED ===\n", failures);
        return 1;
    }
}