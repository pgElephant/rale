/*-------------------------------------------------------------------------
 *
 * args.c
 *    Command-line argument parsing for RALED daemon.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/src/args.c
 *
 *------------------------------------------------------------------------- */

/** System headers */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/** Local headers */
#include "raled_inc.h"

/** Constants */
#define DEFAULT_CONFIG_FILE "raled.conf"
#define MODULE             "RALED"

/** External variables */
extern int verbose;
extern config_t config;

int verbose = 0;
int daemon_mode = 0;
char pid_file[256] = "/tmp/raled_%d.pid";

char config_file[MAX_LINE_LENGTH] = {0};

/** Function declarations */
void handle_config_file(void);
void handle_verbose(void);
void print_help(const char *progname);

static int run_config_checks(void);

void
parse_arguments(int argc, char *argv[])
{
	int                    opt;
	int                    check_only = 0;
	static struct option  long_options[] = {
		{"config", required_argument, 0, 'c'},
		{"verbose", no_argument, 0, 'v'},
		{"check", no_argument, 0, 'C'},
		{"daemon", no_argument, 0, 'd'},
		{"foreground", no_argument, 0, 'f'},
		{"pid-file", required_argument, 0, 'p'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "c:vhCdfp:", long_options, NULL)) != -1)
	{
		switch (opt)
		{
			case 'c':
				snprintf(config_file, sizeof(config_file), "%s", optarg);
				break;
			case 'v':
				/* Map -v to DEBUG log level */
				guc_set("raled_log_level", "debug");
				handle_verbose();
				break;
			case 'C':
				check_only = 1;
				break;
			case 'd':
				daemon_mode = 1;
				break;
			case 'f':
				daemon_mode = 0;
				break;
			case 'p':
				snprintf(pid_file, sizeof(pid_file), "%s", optarg);
				break;
			case 'h':
				print_help(argv[0]);
				exit(0);
		}
	}
	if (strlen(config_file) == 0)
	{
		char msg[128];

		snprintf(msg, sizeof(msg),
				 "No configuration file specified, using default: %s",
				 DEFAULT_CONFIG_FILE);
		raled_ereport(RALED_LOG_INFO, MODULE, msg, NULL, NULL);
		snprintf(config_file, sizeof(config_file), "%s", DEFAULT_CONFIG_FILE);
	}
	handle_config_file();

	if (check_only)
	{
		int rc = run_config_checks();
		exit(rc == 0 ? 0 : 1);
	}
}

void
handle_config_file(void)
{
	char msg[128];

    snprintf(msg, sizeof(msg), "Loading configuration file %s", config_file);
    raled_ereport(RALED_LOG_INFO, MODULE, msg, NULL, NULL);

	if (read_config(config_file, &config) != 0)
	{
		/** Error already reported by read_config; exit without duplicate log */
		exit(1);
	}

	/** Configuration loaded successfully */
	/* Suppress extra INFO to reduce noise */
}

static int
run_config_checks(void)
{
	int rc = 0;
	char detail[256];

	/* Validate directories: log_directory and db.path */
	if (config.log_directory[0] == '\0')
	{
		strlcpy(config.log_directory, "./log", sizeof(config.log_directory));
	}
	if (mkdir(config.log_directory, 0755) != 0 && errno != EEXIST)
	{
		snprintf(detail, sizeof(detail), "Cannot create log directory '%s'", config.log_directory);
		raled_ereport(RALED_LOG_ERROR, MODULE, "Log directory check failed: cannot create directory.", detail, NULL);
		rc = -1;
	}
	if (config.db.path[0] == '\0')
	{
		strlcpy(config.db.path, "./db1", sizeof(config.db.path));
	}
	if (mkdir(config.db.path, 0755) != 0 && errno != EEXIST)
	{
		snprintf(detail, sizeof(detail), "Cannot create db path '%s'", config.db.path);
		raled_ereport(RALED_LOG_ERROR, MODULE, "Database path check failed: directory does not exist.", detail, NULL);
		rc = -1;
	}

	/* Derive socket path if empty */
	if (config.communication.socket[0] == '\0')
	{
		snprintf(config.communication.socket, sizeof(config.communication.socket),
			 "/tmp/rale_%d.sock", config.node.rale_port);
	}

	/* Check resources (ports and socket) */
	if (rc == 0)
	{
		if (check_unix_socket_availability(config.communication.socket) != 0)
			rc = -1;
		if (check_port_availability(config.node.rale_port, "RALE") != 0)
			rc = -1;
		if (check_port_availability(config.node.dstore_port, "DStore") != 0)
			rc = -1;
	}

	if (rc == 0)
	{
		raled_ereport(RALED_LOG_INFO, MODULE, "Configuration check passed.", NULL, NULL);
	}
	return rc;
}

int
check_port_availability(int port, const char *service_name)
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		raled_ereport(RALED_LOG_ERROR, MODULE, "Failed to create socket for port availability check.", NULL, NULL);
		return -1;
	}

	int opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		close(sock);
		raled_ereport(RALED_LOG_ERROR, MODULE, "Failed to set socket options for port availability check.", NULL, NULL);
		return -1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		close(sock);
		char detail[256];
		snprintf(detail, sizeof(detail), "Port %d is not available for %s service", port, service_name);
		raled_ereport(RALED_LOG_ERROR, MODULE, "Port availability check failed: port is already in use.", detail, NULL);
		return -1;
	}

	close(sock);
	return 0;
}

int
check_unix_socket_availability(const char *socket_path)
{
	if (socket_path == NULL || socket_path[0] == '\0')
	{
		return 0; /* Empty path is valid */
	}

	/* Check if socket file exists and is accessible */
	struct stat st;
	if (stat(socket_path, &st) == 0)
	{
		/* Socket file exists, check if it's actually a socket */
		if (!S_ISSOCK(st.st_mode))
		{
			char detail[256];
			snprintf(detail, sizeof(detail), "Path '%s' exists but is not a socket", socket_path);
			raled_ereport(RALED_LOG_ERROR, MODULE, "Socket availability check failed: socket path is not accessible.", detail, NULL);
			return -1;
		}
	}

	return 0;
}

void
handle_verbose(void)
{
	verbose = 1;
	raled_ereport(RALED_LOG_DEBUG, MODULE, "Verbose mode enabled: detailed logging will be displayed.", NULL, NULL);
}

/**
 * Print help message.
 */
void
print_help(const char *progname)
{
	printf("Usage: %s --config <config_file> [--check] [--verbose] [--daemon|--foreground] [--pid-file <file>] [--help]\n", progname);
	printf("Options:\n");
	printf("  -c, --config <config_file>  Specify the configuration file\n");
	printf("  -C, --check                 Validate config, ports, sockets, and directories then exit\n");
	printf("  -v, --verbose               Enable verbose mode\n");
	printf("  -d, --daemon                Run in daemon mode (default)\n");
	printf("  -f, --foreground            Run in foreground mode\n");
	printf("  -p, --pid-file <file>       Specify PID file path (default: /tmp/raled_<port>.pid)\n");
	printf("  -h, --help                  Show this help message\n");
}
