#include "rcrdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#ifdef _WIN32
#define strdup _strdup
#endif

typedef struct {
    char* hostname;
    int port;
    char* username;
    char* password;
    char* domain;
    Command command;
} Config;

static void config_init(Config* config)
{
    memset(config, 0, sizeof(Config));
    config->port = 3389;
    config->command.type = CMD_INVALID;
}

static void config_free(Config* config)
{
    if (config->hostname) free(config->hostname);
    if (config->username) free(config->username);
    if (config->password) free(config->password);
    if (config->domain) free(config->domain);
    if (config->command.type == CMD_SCREENSHOT && config->command.params.screenshot.output_file)
        free(config->command.params.screenshot.output_file);
}

static int parse_arguments(int argc, char** argv, Config* config)
{
    int opt;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"username", required_argument, 0, 'u'},
        {"password", required_argument, 0, 'P'},
        {"domain", required_argument, 0, 'd'},
        {"help", no_argument, 0, '?'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "h:p:u:P:d:?", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
            case 'h':
                config->hostname = strdup(optarg);
                break;
            case 'p':
                config->port = atoi(optarg);
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
                print_usage();
                return -1;
        }
    }
    
    if (optind >= argc)
    {
        fprintf(stderr, "Error: No command specified\n");
        print_usage();
        return -1;
    }
    
    const char* cmd_str = argv[optind];
    config->command.type = parse_command(cmd_str);
    
    switch (config->command.type)
    {
        case CMD_CONNECT:
        case CMD_DISCONNECT:
            break;
            
        case CMD_SCREENSHOT:
            if (optind + 1 < argc && argv[optind + 1][0] != '-')
            {
                // Optional filename provided
                config->command.params.screenshot.output_file = strdup(argv[optind + 1]);
            }
            else
            {
                // No filename provided, will auto-generate
                config->command.params.screenshot.output_file = NULL;
            }
            break;
            
        case CMD_SENDKEY:
            if (optind + 2 >= argc)
            {
                fprintf(stderr, "Error: sendkey command requires flags and code\n");
                return -1;
            }
            config->command.params.sendkey.flags = (DWORD)strtoul(argv[optind + 1], NULL, 0);
            config->command.params.sendkey.code = (DWORD)strtoul(argv[optind + 2], NULL, 0);
            break;
            
        case CMD_SENDMOUSE:
            if (optind + 3 >= argc)
            {
                fprintf(stderr, "Error: sendmouse command requires flags, x, and y\n");
                return -1;
            }
            config->command.params.mouse.flags = (DWORD)strtoul(argv[optind + 1], NULL, 0);
            config->command.params.mouse.x = (UINT16)atoi(argv[optind + 2]);
            config->command.params.mouse.y = (UINT16)atoi(argv[optind + 3]);
            break;
            
        case CMD_MOVEMOUSE:
            if (optind + 2 >= argc)
            {
                fprintf(stderr, "Error: movemouse command requires x and y coordinates\n");
                return -1;
            }
            config->command.params.mouse.x = (UINT16)atoi(argv[optind + 1]);
            config->command.params.mouse.y = (UINT16)atoi(argv[optind + 2]);
            break;
            
        case CMD_INVALID:
        default:
            fprintf(stderr, "Error: Invalid command '%s'\n", cmd_str);
            print_usage();
            return -1;
    }
    
    return 0;
}

static int execute_command(RDPClient* client, const Config* config)
{
    switch (config->command.type)
    {
        case CMD_CONNECT:
            if (!config->hostname)
            {
                fprintf(stderr, "Error: hostname required for connect command\n");
                return -1;
            }
            if (!rdp_client_connect(client, config->hostname, config->port,
                                  config->username, config->password, config->domain))
            {
                return -1;
            }
            break;
            
        case CMD_DISCONNECT:
            rdp_client_disconnect(client);
            break;
            
        case CMD_SCREENSHOT:
            if (!request_screenshot(client, config->command.params.screenshot.output_file))
            {
                return -1;
            }
            break;
            
        case CMD_SENDKEY:
            if (!execute_sendkey(client, config->command.params.sendkey.flags,
                               config->command.params.sendkey.code))
            {
                return -1;
            }
            break;
            
        case CMD_SENDMOUSE:
            if (!execute_sendmouse(client, config->command.params.mouse.flags,
                                 config->command.params.mouse.x,
                                 config->command.params.mouse.y))
            {
                return -1;
            }
            break;
            
        case CMD_MOVEMOUSE:
            if (!execute_movemouse(client, config->command.params.mouse.x,
                                 config->command.params.mouse.y))
            {
                return -1;
            }
            break;
            
        default:
            fprintf(stderr, "Error: Invalid command\n");
            return -1;
    }
    
    return 0;
}

int main(int argc, char** argv)
{
    Config config;
    RDPClient* client = NULL;
    int ret = 0;
    
    config_init(&config);
    
    if (parse_arguments(argc, argv, &config) != 0)
    {
        ret = 1;
        goto cleanup;
    }
    
    client = rdp_client_new();
    if (!client)
    {
        fprintf(stderr, "Error: Failed to create RDP client\n");
        ret = 1;
        goto cleanup;
    }
    
    if (config.command.type == CMD_CONNECT)
    {
        ret = execute_command(client, &config);
        if (ret == 0)
        {
            printf("Connected successfully. Use other commands to interact with the session.\n");
        }
    }
    else
    {
        if (config.hostname)
        {
            if (!rdp_client_connect(client, config.hostname, config.port,
                                  config.username, config.password, config.domain))
            {
                ret = 1;
                goto cleanup;
            }
            

            
            ret = execute_command(client, &config);
            
            rdp_client_disconnect(client);
        }
        else
        {
            fprintf(stderr, "Error: hostname required\n");
            ret = 1;
        }
    }
    
cleanup:
    if (client)
        rdp_client_free(client);
    config_free(&config);
    return ret;
}