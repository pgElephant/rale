/*-------------------------------------------------------------------------
 *
 * ralectrl.c
 *		Rale control command-line interface
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <limits.h>
#include <cjson/cJSON.h>

#include "ralectrl.h"
#include "ralectrl_http_client.h"

#define RESPONSE_BUF_SIZE			1024
#define DEFAULT_HTTP_PORT			8080

static volatile int running = 1;
static ralectrl_http_config_t g_http_config;

static void handle_signal(int signal) __attribute__((unused));
static void print_help(const char *progname);
static void print_add_help(const char *progname);
static void print_remove_help(const char *progname);
static void print_list_help(const char *progname);
static void print_start_help(const char *progname);
static void print_stop_help(const char *progname);
static int wait_for_raled_exit(const char *config_path, int timeout_ms);
static int handle_add_command(const char *socket_path, int argc, char *argv[]);
static int handle_remove_command(const char *socket_path, int argc, char *argv[]);
static int handle_list_command(const char *socket_path, int argc, char *argv[]);
static int handle_start_command(int argc, char *argv[]);
static int handle_stop_command(int argc, char *argv[]);
static int resolve_raled_path(char *buf, size_t buflen, const char *argv0 __attribute__((unused)));
static void print_status_help(const char *progname);
static int handle_status_command(int argc, char *argv[]);
static int find_raled_pid_for_config(const char *config_path);

static int
wait_for_tcp_port_closed(int port, int timeout_ms)
{
	int			elapsed = 0;
	const int	step_ms = 100;

	while (elapsed < timeout_ms)
	{
		int					fd = socket(AF_INET, SOCK_STREAM, 0);
		struct sockaddr_in	addr;

		if (fd < 0)
			return 1;

		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons((uint16_t) port);
		inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

		int rc = connect(fd, (struct sockaddr *) &addr, sizeof(addr));
		close(fd);

		if (rc != 0)
			return 1;

		usleep(step_ms * 1000);
		elapsed += step_ms;
	}
	return 0;
}

static int
wait_for_raled_exit(const char *config_path, int timeout_ms)
{
	int			elapsed = 0;
	const int	step_ms = 100;

	while (elapsed < timeout_ms)
	{
		FILE   *fp = popen("ps ax -o pid= -o command=", "r");
		char	line[1024];
		int		found = 0;

		if (!fp)
			return 0;

		while (fgets(line, sizeof(line), fp))
		{
			if (strstr(line, "bin/raled") && strstr(line, "--config") && strstr(line, config_path))
			{
				found = 1;
				break;
			}
		}
		pclose(fp);

		if (!found)
			return 1;

		usleep(step_ms * 1000);
		elapsed += step_ms;
	}
	return 0;
}

static void force_kill_raled(const char *config_path)
{
	FILE *fp = popen("ps ax -o pid= -o command=", "r");
	if (!fp)
		return;
	char line[1024];
	while (fgets(line, sizeof(line), fp))
	{
		int pid = 0;
		char *ptr = line;
		while (*ptr == ' ' || *ptr == '\t') ptr++;
		pid = (int)strtol(ptr, &ptr, 10);
		if (pid <= 0)
			continue;
		if (strstr(line, "bin/raled") && strstr(line, "--config") && strstr(line, config_path))
		{
			kill(pid, SIGTERM);
			usleep(200000);
			kill(pid, SIGKILL);
		}
	}
	pclose(fp);
}




static void print_nodes_table(const char *json_response);
static char *extract_json_value(const char *json, const char *key) __attribute__((unused));

/* HTTP-based communication - old Unix socket send_command function removed */

/* Simple send_command implementation using HTTP client */
static int
send_command(const char *command, char *response, size_t response_size, const char *socket_path)
{
	ralectrl_http_response_t http_response;
	char json_body[512];
	int result;
	
	/* Convert command to JSON format */
	snprintf(json_body, sizeof(json_body), "{\"command\": \"%s\"}", command);
	
	/* Make HTTP POST request to the API */
	result = ralectrl_http_post_json(&g_http_config, "/api/command", json_body, &http_response);
	
	if (result != 0) {
		snprintf(response, response_size, "ERROR: Failed to send command via HTTP");
		return -1;
	}
	
	if (ralectrl_http_is_success(&http_response)) {
		/* Copy response body */
		if (http_response.body && http_response.body_length > 0) {
			size_t copy_len = (http_response.body_length < response_size - 1) ? 
							 http_response.body_length : response_size - 1;
			strncpy(response, http_response.body, copy_len);
			response[copy_len] = '\0';
		} else {
			snprintf(response, response_size, "OK: Command sent successfully");
		}
	} else {
		snprintf(response, response_size, "ERROR: HTTP %d - %s", 
				 http_response.status_code, 
				 http_response.body ? http_response.body : "Unknown error");
	}
	
	ralectrl_http_response_cleanup(&http_response);
	return 0;
}

static void
handle_signal(int sig __attribute__((unused)))
{
	running = 0;
}

int
main(int argc, char *argv[])
{
    char             server_url[256] = {0};
    const char      *server_url_str = NULL;
	int             result = 0;
	int             c;
	int             cmd_index = -1;
	const char      *command = NULL;

	/* Initialize HTTP configuration */
	ralectrl_http_config_init(&g_http_config);

	/** Parse global options up to the first non-option argument (the command) */
	static struct option global_options[] = {
		{"server", required_argument, NULL, 's'},
		{"host", required_argument, NULL, 'H'},
		{"port", required_argument, NULL, 'p'},
		{"api-key", required_argument, NULL, 'k'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	opterr = 0;
	optind = 1;
    while (optind < argc)
	{
		if (argv[optind][0] != '-')
		{
			cmd_index = optind;
			command = argv[optind];
			break;
		}
		c = getopt_long(argc, argv, "s:H:p:k:h", global_options, NULL);
		if (c == -1)
			break;
		switch (c)
		{
			case 's':
                server_url_str = optarg;
				break;
			case 'H':
				if (g_http_config.server_host) {
					free(g_http_config.server_host);
				}
				g_http_config.server_host = strdup(optarg);
				break;
			case 'p':
				g_http_config.server_port = (uint16_t)atoi(optarg);
				break;
			case 'k':
				if (g_http_config.api_key) {
					free(g_http_config.api_key);
				}
				g_http_config.api_key = strdup(optarg);
				break;
			case 'h':
				print_help(argv[0]);
				ralectrl_http_config_cleanup(&g_http_config);
				return 0;
			default:
				print_help(argv[0]);
				ralectrl_http_config_cleanup(&g_http_config);
				return 1;
		}
	}

    if (!command)
	{
        char error_msg[256];
        		snprintf(error_msg, sizeof(error_msg), "Error: No command specified. Use ADD, REMOVE, LIST, START, STOP, or STATUS.\n");
        fputs(error_msg, stderr);
		print_help(argv[0]);
		ralectrl_http_config_cleanup(&g_http_config);
		return 1;
	}

    /** Configure server URL if provided, or use defaults */
    if (server_url_str != NULL)
    {
        char *host = NULL;
        uint16_t port = 0;
        bool use_ssl = false;
        
        if (ralectrl_parse_server_url(server_url_str, &host, &port, &use_ssl) == 0) {
            if (g_http_config.server_host) {
                free(g_http_config.server_host);
            }
            g_http_config.server_host = host;
            g_http_config.server_port = port;
            g_http_config.use_ssl = use_ssl;
        }
    }
    
    /** Set defaults if not specified */
    if (g_http_config.server_port == 0) {
        const char *env = getenv("RALED_PORT");
        if (env && *env) {
            g_http_config.server_port = (uint16_t)atoi(env);
        } else {
            g_http_config.server_port = DEFAULT_HTTP_PORT;
        }
    }

    /** Shift args to only those after the command */
	int             remaining_argc = argc - cmd_index - 1;
	char          **remaining_argv = argv + cmd_index + 1;
	int             handler_argc = remaining_argc + 1;
	char          **handler_argv;
	int             i;

	/** Prepare handler argv: argv[0] = program name, argv[1..n] = options */
	handler_argv = malloc(sizeof(char *) * (size_t)handler_argc);
	if (handler_argv == NULL)
	{
		char error_msg[256];
		snprintf(error_msg, sizeof(error_msg), "Error: Memory allocation failed. System resources exhausted.\n");
		fputs(error_msg, stderr);
		return 1;
	}
	handler_argv[0] = "ralectrl";
	for (i = 0; i < remaining_argc; i++)
		handler_argv[i + 1] = remaining_argv[i];

    if (strcmp(command, "ADD") == 0)
		result = handle_add_command("/tmp/rale.sock", handler_argc, handler_argv);
	else if (strcmp(command, "REMOVE") == 0)
		result = handle_remove_command("/tmp/rale.sock", handler_argc, handler_argv);
	else if (strcmp(command, "LIST") == 0)
		result = handle_list_command("/tmp/rale.sock", handler_argc, handler_argv);
    else if (strcmp(command, "START") == 0)
		result = handle_start_command(handler_argc, handler_argv);
    else if (strcmp(command, "STOP") == 0)
		result = handle_stop_command(handler_argc, handler_argv);
	else if (strcmp(command, "STATUS") == 0)
		result = handle_status_command(handler_argc, handler_argv);
	else if (strcmp(command, "HELP") == 0 || strcmp(command, "--help") == 0 ||
			 strcmp(command, "-h") == 0)
		print_help(argv[0]);
    else
	{
        char error_msg[256];
        		snprintf(error_msg, sizeof(error_msg), "Error: Unknown command \"%s\". Use ADD, REMOVE, LIST, START, STOP, or STATUS.\n", command);
        fputs(error_msg, stderr);
		print_help(argv[0]);
		free(handler_argv);
		handler_argv = NULL;
		ralectrl_http_config_cleanup(&g_http_config);
		return 1;
	}

	free(handler_argv);
	handler_argv = NULL;
	ralectrl_http_config_cleanup(&g_http_config);
	return result;
}

static void
print_help(const char *progname)
{
	printf("Usage: %s [GLOBAL OPTIONS] <command> [COMMAND OPTIONS]\n", progname);
	printf("\nGlobal options:\n");
    printf("  -s, --server <url>         Server URL (e.g., http://localhost:8080)\n");
    printf("  -H, --host <host>          Server hostname (default: localhost)\n");
    printf("  -p, --port <port>          Server port (default: 8080, env RALED_PORT)\n");
    printf("  -k, --api-key <key>        API key for authentication\n");
	printf("  -h, --help                 Show this help message\n");
	printf("\nCommands:\n");
	printf("  ADD      Add a new node\n");
	printf("  REMOVE   Remove a node\n");
	printf("  LIST     List all nodes\n");
	printf("  START    Start raled daemon with a config\n");
	printf("  STOP     Stop raled daemon matching a config\n");
	printf("  STATUS   Show status of raled matching a config\n");
	printf("  HELP     Show this help message\n");
	printf("\nFor command-specific options, run: %s <command> --help\n\n", progname);
}
static void
print_start_help(const char *progname)
{
	printf("Usage: %s START --config <path> [--stdout <file>]\n", progname);
}

static void
print_stop_help(const char *progname)
{
	printf("Usage: %s STOP --config <path>\n", progname);
}

static void
print_status_help(const char *progname)
{
	printf("Usage: %s STATUS --config <path>\n", progname);
}

/**
 * START: spawn raled with a config path; optional stdout redirection
 */
static int
handle_start_command(int argc, char *argv[])
{
	int				c;
	char			config_path[512] = {0};
	char			stdout_path[512] = {0};
	int			have_stdout = 0;
	static struct option start_options[] = {
		{"config", required_argument, NULL, 'c'},
		{"stdout", required_argument, NULL, 'o'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

#ifdef __APPLE__
	optreset = 1;
#endif
	optind = 0;

    while ((c = getopt_long(argc, argv, "c:o:h", start_options, NULL)) != -1)
    {
        switch (c)
        {
            case 'c':
                strlcpy(config_path, optarg, sizeof(config_path));
                break;
            case 'o':
                strlcpy(stdout_path, optarg, sizeof(stdout_path));
                have_stdout = 1;
                break;
            case 'h':
                print_start_help("ralectrl");
                return 0;
            default:
            {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "Error: Invalid option for START.\n");
                fputs(error_msg, stderr);
                print_start_help("ralectrl");
                return 1;
            }
        }
    }

    if (strlen(config_path) == 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Error: --config is required for START.\n");
        fputs(error_msg, stderr);
        print_start_help("ralectrl");
        return 1;
    }

	char			cmd[1024];
	char			raled_path_buf[512] = {0};
	const char		*raled_path = NULL;

	if (resolve_raled_path(raled_path_buf, sizeof(raled_path_buf), "ralectrl")
		!= 0)
	{
		/* Fallbacks */
		if (access("./bin/raled", X_OK) == 0)
			raled_path = "./bin/raled";
		else
			raled_path = "raled";
	}
	else
	{
		raled_path = raled_path_buf;
	}
    
    if (have_stdout)
        snprintf(cmd, sizeof(cmd), "%s --config %s >%s 2>&1 &", raled_path, config_path, stdout_path);
    else
        snprintf(cmd, sizeof(cmd), "%s --config %s >/dev/null 2>&1 &", raled_path, config_path);

    if (system(cmd) != 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Error: Failed to start raled.\n");
        fputs(error_msg, stderr);
        return 1;
    }
    return 0;
}

/**
 * STOP: terminate raled matching a config path
 */
static int
handle_stop_command(int argc, char *argv[])
{
	int				c;
	char			config_path[512] = {0};

#ifdef __APPLE__
    optreset = 1;
#endif
    optind = 0;

	static struct option stop_options[] = {
        {"config", required_argument, NULL, 'c'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    while ((c = getopt_long(argc, argv, "c:h", stop_options, NULL)) != -1)
    {
        switch (c)
        {
            case 'c':
                strlcpy(config_path, optarg, sizeof(config_path));
                break;
            case 'h':
                print_stop_help("ralectrl");
                return 0;
            default:
            {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "Error: Invalid option for STOP.\n");
                fputs(error_msg, stderr);
                print_stop_help("ralectrl");
                return 1;
            }
        }
    }

    if (strlen(config_path) == 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Error: --config is required for STOP.\n");
        fputs(error_msg, stderr);
        print_stop_help("ralectrl");
        return 1;
    }

    /** Send STOP over the configured Unix socket for this config */
    char socket_path[128];
    int  rale_port = 5001;
    {
        FILE *cf = fopen(config_path, "r");
        if (cf)
        {
            char line[256];
            while (fgets(line, sizeof(line), cf))
            {
                int v;
                if (strncmp(line, "rale_port = ", 12) == 0) {
                    v = atoi(line + 12);
                    rale_port = v;
                    break;
                }
            }
            fclose(cf);
        }
    }
    
    /** Read the communication_socket from config file */
    {
        FILE *cf = fopen(config_path, "r");
        if (cf)
        {
            char line[256];
            while (fgets(line, sizeof(line), cf))
            {
                if (strncmp(line, "communication_socket = ", 22) == 0) {
                    char *value_start = line + 22;
                    char *newline = strchr(value_start, '\n');
                    if (newline) *newline = '\0';
                    strlcpy(socket_path, value_start, sizeof(socket_path));
                    break;
                }
            }
            fclose(cf);
        }
        else
        {
            /** Fallback to default socket path if config can't be read */
            snprintf(socket_path, sizeof(socket_path), "/tmp/rale_%d.sock", rale_port);
        }
    }
	char			response[RESPONSE_BUF_SIZE];
	char			stop_command[32];
    snprintf(stop_command, sizeof(stop_command), "{\"command\":\"STOP\"}");
    if (send_command(stop_command, response, sizeof(response) - 1, socket_path) != 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Error: Failed to send STOP command.\n");
        fputs(error_msg, stderr);
        return 1;
    }

    /** Wait briefly for daemon to release port 6601 and remove socket */
    /* rale_port already parsed above */
    char sockpath[128];
    snprintf(sockpath, sizeof(sockpath), "/tmp/rale_%d.sock", rale_port);
    unlink(sockpath);
    /* Determine dstore_port from config; fallback to 6000 + node_id */
    int dstore_port = -1;
    {
        FILE *cf2 = fopen(config_path, "r");
        if (cf2)
        {
            char line[256];
            while (fgets(line, sizeof(line), cf2))
            {
                int v;
                if (strncmp(line, "dstore_port = ", 14) == 0) {
                    v = atoi(line + 14);
                    dstore_port = v;
                    break;
                }
            }
            fclose(cf2);
        }
    }
    if (dstore_port <= 0)
    {
        int node_id = rale_port - 5000;
        dstore_port = 6000 + node_id;
    }
    if (!wait_for_tcp_port_closed(dstore_port, 3000) || !wait_for_raled_exit(config_path, 3000))
    {
        /** Fallback: force kill lingering raled */
        force_kill_raled(config_path);
        wait_for_tcp_port_closed(dstore_port, 2000);
    }

    return 0;
}

static int
handle_add_command(const char *socket_path, int argc, char *argv[])
{
	int				c;
	int				node_id = -1;
	char			node_name[256] = {0};
	char			node_ip[256] = {0};
	int				rale_port = -1;
	int				dstore_port = -1;
	static struct option add_options[] = {
		{"node-id", required_argument, NULL, 'i'},
		{"node-name", required_argument, NULL, 'n'},
		{"node-ip", required_argument, NULL, 'p'},
		{"rale-port", required_argument, NULL, 'r'},
		{"dstore-port", required_argument, NULL, 'd'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

#ifdef __APPLE__
	optreset = 1;
#endif
	optind = 0;

	while ((c = getopt_long(argc, argv, "i:n:p:r:d:h", add_options, NULL)) != -1)
	{
		switch (c)
		{
			case 'i':
				node_id = atoi(optarg);
				break;
			case 'n':
				strlcpy(node_name, optarg, sizeof(node_name));
				break;
			case 'p':
				strlcpy(node_ip, optarg, sizeof(node_ip));
				break;
			case 'r':
				rale_port = atoi(optarg);
				break;
			case 'd':
				dstore_port = atoi(optarg);
				break;
			case 'h':
				print_add_help("ralectrl");
				return 0;
			default:
			{
				char error_msg[256];
				snprintf(error_msg, sizeof(error_msg), "Error: Invalid option for ADD.\n");
				fputs(error_msg, stderr);
				print_add_help("ralectrl");
				return 1;
			}
		}
	}

	if (node_id == -1 || strlen(node_name) == 0 || strlen(node_ip) == 0 ||
		rale_port == -1 || dstore_port == -1)
	{
		char error_msg[256];
		snprintf(error_msg, sizeof(error_msg), "Error: Missing required options for ADD.\n");
		fputs(error_msg, stderr);
		print_add_help("ralectrl");
		return 1;
	}

	if (add_node(socket_path, node_id, node_name, node_ip, rale_port, dstore_port) != 0)
	{
		return 1;
	}
	return 0;
}

static void
print_add_help(const char *progname)
{
	printf("Usage: %s ADD --node-id <id> --node-name <name> --node-ip <ip> "
		   "--rale-port <port> --dstore-port <port>\n", progname);
}

int
add_node(const char *socket_path_arg, int node_id, const char *node_name,
         const char *node_ip, int rale_port, int dstore_port)
{
	char			command[256];
	char			response[RESPONSE_BUF_SIZE];

	snprintf(command, sizeof(command),
			 "{\"command\":\"ADD\",\"node_id\":%d,\"node_name\":\"%s\"," \
			 "\"node_ip\":\"%s\",\"rale_port\":%d,\"dstore_port\":%d}\n",
			 node_id, node_name, node_ip, rale_port, dstore_port);

	if (send_command(command, response, sizeof(response) - 1,
				   socket_path_arg) != 0)
	{
		char error_msg[256];
		snprintf(error_msg, sizeof(error_msg), "ADD_NODE: Failed to send ADD command.\n");
		fputs(error_msg, stderr);
		return -1;
	}

	return 0;
}

int
remove_node(const char *socket_path_arg, int node_id)
{
	char			command[128];
	char			response[RESPONSE_BUF_SIZE];

	snprintf(command, sizeof(command),
			 "{\"command\":\"REMOVE\",\"node_id\":%d}", node_id);

	if (send_command(command, response, sizeof(response) - 1,
				   socket_path_arg) != 0)
	{
		char error_msg[256];
		snprintf(error_msg, sizeof(error_msg), "REMOVE_NODE: Failed to send REMOVE command.\n");
		fputs(error_msg, stderr);
		return -1;
	}

	return 0;
}

static void
print_remove_help(const char *progname)
{
	printf("Usage: %s REMOVE <node_id>\n", progname);
}

static int
handle_remove_command(const char *socket_path, int argc, char *argv[])
{
	int node_id;

	if (argc != 1)
	{
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Error: Invalid arguments for REMOVE.\n");
        fputs(error_msg, stderr);
		print_remove_help("ralectrl");
		return 1;
	}

	node_id = atoi(argv[0]);

	if (remove_node(socket_path, node_id) != 0)
	{
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Error: Failed to remove node.\n");
        fputs(error_msg, stderr);
		return 1;
	}

	return 0;
}

static void
print_list_help(const char *progname)
{
	printf("Usage: %s LIST [--pretty]\n", progname);
	printf("  --pretty    Display output as formatted table\n");
}

static int
handle_list_command(const char *socket_path, int argc, char *argv[])
{
	int				pretty_output = 0;
	int				opt;
	int				option_index = 0;
	char			response[RESPONSE_BUF_SIZE];
	char			command[64];

	/** Parse LIST command options */
	static struct option list_options[] = {
		{"pretty", no_argument, NULL, 'p'},
		{NULL, 0, NULL, 0}
	};

	/** Reset optind for subcommand parsing */
#ifdef __APPLE__
	optreset = 1;
#endif
	optind = 1;

	while ((opt = getopt_long(argc, argv, "p", list_options, &option_index)) != -1)
	{
		switch (opt)
		{
			case 'p':
				pretty_output = 1;
				break;
			default:
            {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "Error: Invalid option for LIST.\n");
                fputs(error_msg, stderr);
				print_list_help("ralectrl");
				return 1;
            }
		}
	}

	/** Send LIST command */
	snprintf(command, sizeof(command), "{\"command\":\"LIST\"}");

    if (send_command(command, response, sizeof(response) - 1, socket_path) != 0)
	{
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Error: Failed to send LIST command.\n");
		return 1;
	}

	/** Display response */
	if (pretty_output)
	{
		print_nodes_table(response);
	}
	else
	{
        fputs(response, stderr);
	}

	return 0;
}

/**
 * Extract a value from a JSON string using cJSON
 */
static char *
extract_json_value(const char *json, const char *key)
{
    cJSON *json_obj;
    cJSON *item;
    char  *value = NULL;

    /** Parse JSON using cJSON */
    json_obj = cJSON_Parse(json);
    if (json_obj == NULL)
        return NULL;

    /** Extract the value */
    item = cJSON_GetObjectItem(json_obj, key);
    if (item != NULL)
    {
        if (cJSON_IsString(item))
        {
            const char *str_val = item->valuestring;
            if (str_val != NULL)
            {
                size_t len = strlen(str_val);
                if (len > 0 && len <= 255)
                {
                    value = (char *) malloc(len + 1);
                    if (value)
                        strcpy(value, str_val);
                }
            }
        }
        else if (cJSON_IsNumber(item))
        {
            /** Convert number to string */
            char num_str[32];
            snprintf(num_str, sizeof(num_str), "%d", item->valueint);
            size_t len = strlen(num_str);
            value = (char *) malloc(len + 1);
            if (value)
                strcpy(value, num_str);
        }
    }

    /** Clean up */
    cJSON_Delete(json_obj);
    return value;
}

/**
 * Parse JSON response and display nodes as a formatted table
 */
static void
print_nodes_table(const char *json_response)
{
    cJSON *json;
    cJSON *nodes_array;
    cJSON *node;
    int    node_count = 0;

    /** Print table header */
    fputs("\n┌─────────┬──────────┬──────────────┬────────────┬─────────────┬──────────┬─────────┐\n", stdout);
    fputs("│ Node ID │   Name   │   IP Address │ RALE Port  │ DStore Port │   Role   │  State  │\n", stdout);
    fputs("├─────────┼──────────┼──────────────┼────────────┼─────────────┼──────────┼─────────┤\n", stdout);
    /** Parse JSON using cJSON */
    json = cJSON_Parse(json_response);
    if (json != NULL)
    {
        /** Get nodes array */
        nodes_array = cJSON_GetObjectItem(json, "nodes");
        if (cJSON_IsArray(nodes_array))
        {
            /** Iterate through nodes */
            cJSON_ArrayForEach(node, nodes_array)
            {
                if (cJSON_IsObject(node))
                {
                    cJSON *id = cJSON_GetObjectItem(node, "id");
                    cJSON *name = cJSON_GetObjectItem(node, "name");
                    cJSON *ip = cJSON_GetObjectItem(node, "ip");
                    cJSON *r = cJSON_GetObjectItem(node, "rale_port");
                    cJSON *dstore_port = cJSON_GetObjectItem(node, "dstore_port");
                    cJSON *role = cJSON_GetObjectItem(node, "role");

                    if (id && name && ip && r && dstore_port && role)
                    {
                        char row_buffer[256];
                        snprintf(row_buffer, sizeof(row_buffer), "│ %7d │ %8s │ %12s │ %10d │ %11d │ %8s │ %7s │\n",
                                id->valueint, 
                                name->valuestring ? name->valuestring : "",
                                ip->valuestring ? ip->valuestring : "",
                                r->valueint,
                                dstore_port->valueint,
                                role->valuestring ? role->valuestring : "",
                                "online");
                        fputs(row_buffer, stdout);
                        node_count++;
                    }
                }
            }
        }

        /** Clean up */
        cJSON_Delete(json);
    }

    /** Print table footer */
    fputs("└─────────┴──────────┴──────────────┴────────────┴─────────────┴──────────┴─────────┘\n", stdout);
    char footer_buffer[64];
    snprintf(footer_buffer, sizeof(footer_buffer), "Total nodes: %d\n\n", node_count);
    fputs(footer_buffer, stdout);
}

static int resolve_raled_path(char *buf, size_t buflen, const char *argv0 __attribute__((unused)))
{
	char		cwd[PATH_MAX] = {0};
	char		self[PATH_MAX] = {0};
	char		dir[PATH_MAX] = {0};
	char		tmp[PATH_MAX] = {0};
	const char	*env_bindir = NULL;
	int			ok = 0;

	if (buf == NULL || buflen == 0)
		return -1;

	if (argv0 && argv0[0] != '\0')
	{
		if (realpath(argv0, self) != NULL)
		{
			size_t n = strlen(self);
			while (n > 0 && self[n - 1] != '/')
				n--;
			memcpy(dir, self, n);
			dir[n] = '\0';
			snprintf(tmp, sizeof(tmp), "%s/raled", dir);
			if (access(tmp, X_OK) == 0)
			{
				strlcpy(buf, tmp, buflen);
				ok = 1;
			}
		}
	}

	if (!ok)
	{
		if (getcwd(cwd, sizeof(cwd)) != NULL)
		{
			snprintf(tmp, sizeof(tmp), "%s/bin/raled", cwd);
			if (access(tmp, X_OK) == 0)
			{
				strlcpy(buf, tmp, buflen);
				ok = 1;
			}
		}
	}

	if (!ok)
	{
		env_bindir = getenv("RALE_BINDIR");
		if (env_bindir && env_bindir[0] != '\0')
		{
			snprintf(tmp, sizeof(tmp), "%s/raled", env_bindir);
			if (access(tmp, X_OK) == 0)
			{
				strlcpy(buf, tmp, buflen);
				ok = 1;
			}
		}
	}

	if (!ok)
	{
		const char *compiled_bindir = RALE_BINDIR;
		snprintf(tmp, sizeof(tmp), "%s/raled", compiled_bindir);
		if (access(tmp, X_OK) == 0)
		{
			strlcpy(buf, tmp, buflen);
			ok = 1;
		}
	}

	if (!ok)
	{
		strlcpy(buf, "raled", buflen);
	}
	return 0;
}

static int
find_raled_pid_for_config(const char *config_path)
{
	FILE			*fp;
	char			line[1024];
	int				pid = -1;

	fp = popen("ps ax -o pid= -o command=", "r");
	if (!fp)
		return -1;
	while (fgets(line, sizeof(line), fp))
	{
		char *ptr = line;
		int	 tmp = 0;
		while (*ptr == ' ' || *ptr == '\t') ptr++;
		tmp = (int) strtol(ptr, &ptr, 10);
		if (tmp <= 0)
			continue;
		if (strstr(line, "bin/raled") && strstr(line, "--config") && strstr(line, config_path))
		{
			pid = tmp;
			break;
		}
	}
	pclose(fp);
	return pid;
}

static int
handle_status_command(int argc, char *argv[])
{
	int				c;
	char			config_path[512] = {0};
	char			socket_path[128] = {0};
	int				rale_port = 5001;
	static struct option status_options[] = {
		{"config", required_argument, NULL, 'c'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

#ifdef __APPLE__
	optreset = 1;
#endif
	optind = 0;

	while ((c = getopt_long(argc, argv, "c:h", status_options, NULL)) != -1)
	{
		switch (c)
		{
			case 'c':
				strlcpy(config_path, optarg, sizeof(config_path));
				break;
			case 'h':
				print_status_help("ralectrl");
				return 0;
			default:
			{
				char error_msg[256];
				snprintf(error_msg, sizeof(error_msg), "Error: Invalid option for STATUS.\n");
				fputs(error_msg, stderr);
				print_status_help("ralectrl");
				return 1;
			}
		}
	}

	if (strlen(config_path) == 0)
	{
		char error_msg[256];
		snprintf(error_msg, sizeof(error_msg), "Error: --config is required for STATUS.\n");
		fputs(error_msg, stderr);
		print_status_help("ralectrl");
		return 1;
	}

	{
		FILE *cf = fopen(config_path, "r");
		if (cf)
		{
			char line[256];
			while (fgets(line, sizeof(line), cf))
			{
				int v;
				if (strncmp(line, "rale_port = ", 12) == 0) {
					v = atoi(line + 12);
					rale_port = v;
					break;
				}
			}
			fclose(cf);
		}
	}
	{
		FILE *cf = fopen(config_path, "r");
		if (cf)
		{
			char line[256];
			while (fgets(line, sizeof(line), cf))
			{
				if (strncmp(line, "communication_socket = ", 22) == 0) {
					char *value_start = line + 22;
					char *newline = strchr(value_start, '\n');
					if (newline) *newline = '\0';
					strlcpy(socket_path, value_start, sizeof(socket_path));
					break;
				}
			}
			fclose(cf);
		}
		else
		{
			snprintf(socket_path, sizeof(socket_path), "/tmp/rale_%d.sock", rale_port);
		}
	}

	{
		char response[RESPONSE_BUF_SIZE];
		char status_command[32];
		int  rc;
		snprintf(status_command, sizeof(status_command), "{\"command\":\"STATUS\"}");
		rc = send_command(status_command, response, sizeof(response) - 1, socket_path);
		if (rc != 0)
		{
			int pid = find_raled_pid_for_config(config_path);
			if (pid > 0)
			{
				printf("raled is running (PID: %d)\n", pid);
				return 0;
			}
			printf("raled is not running\n");
			return 1;
		}
		fputs(response, stdout);
	}
	return 0;
}
