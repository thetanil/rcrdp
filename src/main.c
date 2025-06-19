#include "rcrdp.h"
#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>

#ifdef _WIN32
#define strdup _strdup
#endif

static HttpServer* g_server = NULL;
static RDPClient* g_client = NULL;

typedef struct {
    char* hostname;
    int rdp_port;
    char* username;
    char* password;
    char* domain;
    int http_port;
} ServerConfig;

static void config_init(ServerConfig* config)
{
    memset(config, 0, sizeof(ServerConfig));
    config->rdp_port = 3389;
    config->http_port = DEFAULT_PORT;
}

static void config_free(ServerConfig* config)
{
    if (config->hostname) free(config->hostname);
    if (config->username) free(config->username);
    if (config->password) free(config->password);
    if (config->domain) free(config->domain);
}

static void signal_handler(int signum)
{
    printf("\nReceived signal %d, shutting down...\n", signum);
    
    if (g_server) {
        http_server_stop(g_server);
    }
    
    if (g_client) {
        rdp_client_disconnect(g_client);
    }
}

static void print_server_usage(void)
{
    printf("Usage: rcrdp [options]\n\n");
    printf("Connection options:\n");
    printf("  -h, --host <hostname>     RDP server hostname (required)\n");
    printf("  -r, --rdp-port <port>     RDP server port (default: 3389)\n");
    printf("  -u, --username <user>     Username for authentication\n");
    printf("  -P, --password <pass>     Password for authentication\n");
    printf("  -d, --domain <domain>     Domain for authentication\n\n");
    printf("Server options:\n");
    printf("  -p, --port <port>         HTTP server port (default: 8080)\n");
    printf("  --help                    Show this help message\n\n");
    printf("HTTP API Endpoints:\n");
    printf("  GET  /screen              Get current screenshot (PNG)\n");
    printf("  GET  /status              Get connection status (JSON)\n");
    printf("  POST /sendkey             Send keyboard event (JSON: {\"flags\": 1, \"code\": 65})\n");
    printf("  POST /sendmouse           Send mouse event (JSON: {\"flags\": 4096, \"x\": 100, \"y\": 200})\n");
    printf("  POST /movemouse           Move mouse (JSON: {\"x\": 100, \"y\": 200})\n\n");
    printf("Examples:\n");
    printf("  rcrdp -h 192.168.1.100 -u admin -P password\n");
    printf("  curl http://localhost:8080/screen > screenshot.png\n");
    printf("  curl -X POST -d '{\"flags\":1,\"code\":65}' http://localhost:8080/sendkey\n");
    printf("  curl -X POST -d '{\"x\":100,\"y\":200}' http://localhost:8080/movemouse\n");
}

static int parse_arguments(int argc, char** argv, ServerConfig* config)
{
    int opt;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"host", required_argument, 0, 'h'},
        {"rdp-port", required_argument, 0, 'r'},
        {"port", required_argument, 0, 'p'},
        {"username", required_argument, 0, 'u'},
        {"password", required_argument, 0, 'P'},
        {"domain", required_argument, 0, 'd'},
        {"help", no_argument, 0, '?'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "h:r:p:u:P:d:?", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
            case 'h':
                config->hostname = strdup(optarg);
                break;
            case 'r':
                config->rdp_port = atoi(optarg);
                break;
            case 'p':
                config->http_port = atoi(optarg);
                break;
            case 'u':
                config->username = strdup(optarg);
                break;
            case 'P':
                config->password = strdup(optarg);
                break;
            case 'd':
                config->domain = strdup(optarg);
                break;
            case '?':
            default:
                print_server_usage();
                return -1;
        }
    }
    
    if (!config->hostname) {
        fprintf(stderr, "Error: hostname is required\n");
        print_server_usage();
        return -1;
    }
    
    return 0;
}

int main(int argc, char** argv)
{
    ServerConfig config;
    int ret = 0;
    
    config_init(&config);
    
    if (parse_arguments(argc, argv, &config) != 0) {
        ret = 1;
        goto cleanup;
    }
    
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create RDP client
    g_client = rdp_client_new();
    if (!g_client) {
        fprintf(stderr, "Error: Failed to create RDP client\n");
        ret = 1;
        goto cleanup;
    }
    
    // Connect to RDP server
    printf("Connecting to RDP server %s:%d...\n", config.hostname, config.rdp_port);
    if (!rdp_client_connect(g_client, config.hostname, config.rdp_port,
                           config.username, config.password, config.domain)) {
        fprintf(stderr, "Error: Failed to connect to RDP server\n");
        ret = 1;
        goto cleanup;
    }
    
    // Create HTTP server
    g_server = http_server_new(config.http_port);
    if (!g_server) {
        fprintf(stderr, "Error: Failed to create HTTP server\n");
        ret = 1;
        goto cleanup;
    }
    
    // Start HTTP server
    if (http_server_start(g_server, g_client) != 0) {
        fprintf(stderr, "Error: Failed to start HTTP server\n");
        ret = 1;
        goto cleanup;
    }
    
    // Run server loop
    printf("RDP-HTTP bridge running. Press Ctrl+C to stop.\n");
    http_server_run(g_server);
    
cleanup:
    if (g_client) {
        rdp_client_disconnect(g_client);
        rdp_client_free(g_client);
        g_client = NULL;
    }
    
    if (g_server) {
        http_server_stop(g_server);
        http_server_free(g_server);
        g_server = NULL;
    }
    
    config_free(&config);
    printf("Server shutdown complete.\n");
    return ret;
}