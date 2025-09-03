/*-------------------------------------------------------------------------
 *
 * main.c
 *		Main entry point for the RALED daemon
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include "librale.h"
#include "raled.h"
#include "raled_args.h"
#include "raled_configfile.h"
#include "raled_logger.h"
#include "raled_comm.h"
#include "raled_signal.h"
#include "raled_response.h"
#include "shutdown.h"
#include "cluster.h"
#include "rale_error.h"

#define MAX_CLIENTS 10
#define SOCKET_BACKLOG 5

config_t config;
librale_config_t *librale_cfg = NULL;

static void cleanup_resources(void);
static librale_status_t initialize_librale(void);
static void *raled_main_loop_thread(void *arg);

static pthread_t dstore_server_thread;
static int dstore_threads_started = 0;

int
main(int argc, char *argv[])
{
	librale_status_t result;

	parse_arguments(argc, argv);

	/* Check if PID file already exists and another instance is running */
	if (daemon_mode) {
		char actual_pid_file[256];
		snprintf(actual_pid_file, sizeof(actual_pid_file), pid_file, config.node.rale_port);
		
		FILE *existing_pid = fopen(actual_pid_file, "r");
		if (existing_pid != NULL) {
			int existing_pid_num;
			if (fscanf(existing_pid, "%d", &existing_pid_num) == 1) {
				/* Check if process is actually running */
				if (kill(existing_pid_num, 0) == 0) {
					fprintf(stderr, "ERROR: Another raled instance is already running with PID %d\n", existing_pid_num);
					fprintf(stderr, "PID file: %s\n", actual_pid_file);
					fprintf(stderr, "Use 'kill %d' to stop the existing instance, or remove the PID file manually\n", existing_pid_num);
					fclose(existing_pid);
					exit(1);
				} else {
					/* Process not running, remove stale PID file */
					fclose(existing_pid);
					unlink(actual_pid_file);
					printf("Removed stale PID file: %s\n", actual_pid_file);
				}
			} else {
				fclose(existing_pid);
			}
		}
	}

	/* Initialize coordinated shutdown system */
	(void) librale_shutdown_init();

	int signal_result;
	
	signal_result = setup_signal_handlers();
	if (signal_result != 0)
	{
		char error_msg[256];
		snprintf(error_msg, sizeof(error_msg), "Failed to setup signal handlers\n");
		fputs(error_msg, stderr);
		return 1;
	}
	
	comm_init();
	
	/* Initialize professional daemon logging */
	raled_logger_init(daemon_mode, config.raled_log.file, (raled_log_level_t)config.raled_log.level);
	raled_log_startup();
	
	/* Log node configuration */
	raled_log_info("Node configuration: id=\"%d\", name=\"%s\", rale_port=\"%d\", dstore_port=\"%d\".",
	               config.node.id, config.node.name, config.node.rale_port, config.node.dstore_port);

			raled_log_debug("Initializing librale subsystem.");
	result = initialize_librale();
	if (result != RALE_SUCCESS)
	{
		const rale_error_info_t *error_info = rale_get_last_error();
		if (error_info && error_info->error_message) {
			if (error_info->system_errno != 0) {
				raled_log_error("Librale initialization failed: %s (errno: %s)", 
				               error_info->error_message, strerror(error_info->system_errno));
			} else {
				raled_log_error("Librale initialization failed: %s", error_info->error_message);
			}
		} else {
			const char *error_desc = rale_error_code_to_string(result);
			raled_log_error("Librale initialization failed: %s (code %d)", 
			               error_desc ? error_desc : "Unknown error", result);
		}
		return 1;
	}
			raled_log_debug("Librale subsystem initialized successfully.");

	/* Initialize RALED with UDP support */
			raled_log_debug("Initializing RALED daemon.");
	if (raled_init(&config) != 0)
	{
		raled_log_error("RALED daemon initialization failed: system resources unavailable.");
		return 1;
	}
			raled_log_debug("RALED daemon initialized successfully.");

	/* Daemonize if requested */
	if (daemon_mode) {
		printf("Daemonizing process...\n");
		if (daemon(0, 0) != 0) {
			perror("Failed to daemonize");
			return -1;
		}
		printf("Process daemonized successfully\n");
		
		/* Create PID file after daemonization */
		char actual_pid_file[256];
		snprintf(actual_pid_file, sizeof(actual_pid_file), pid_file, config.node.rale_port);
		FILE *pid_fp = fopen(actual_pid_file, "w");
		if (pid_fp) {
			fprintf(pid_fp, "%d\n", getpid());
			fclose(pid_fp);
			printf("PID file created: %s\n", actual_pid_file);
		}
	}

	raled_log_info("RALED started successfully.");
	if (!daemon_mode) {
		printf("RALED started successfully in foreground mode\n");
		printf("Press Ctrl+C to stop\n");
	} else {
		printf("RALED started successfully in daemon mode\n");
	}

	while (!librale_is_shutdown_requested(SHUTDOWN_SUBSYSTEM_RALE))
	{
		/* Main processing loop handled by separate thread */

		/* Handle SIGHUP reload request */
		if (is_reload_requested())
		{
			clear_reload_request();
			/* Best-effort: re-read config file; dynamic reconfig is limited */
			extern char config_file[]; /* from raled_args.c */
			(void) read_config(config_file, &config);
			raled_ereport(RALED_LOG_INFO, "RALED", "Configuration reload applied where supported.", NULL, NULL);
		}

		/* Handle SIGUSR1 status request */
		if (is_status_requested())
		{
			clear_status_request();
			int32_t role = librale_get_current_role();
			const char *role_str = (role == 0 ? "follower" : (role == 1 ? "candidate" : (role == 2 ? "leader" : "unknown")));
			char msg[256];
			snprintf(msg, sizeof(msg), "Status: node_id=%d role=%s", config.node.id, role_str);
			raled_ereport(RALED_LOG_INFO, "RALED", msg, NULL, NULL);
		}

		usleep(100000);
	}
	
	printf("Shutdown requested, cleaning up...\n");
			raled_ereport(RALED_LOG_INFO, "RALED", "RALED shutting down - initiating shutdown sequence.", NULL, NULL);
	cleanup_resources();
	
	/* Always remove PID file if it exists (safety cleanup) */
	char actual_pid_file[256];
	snprintf(actual_pid_file, sizeof(actual_pid_file), pid_file, config.node.rale_port);
	if (access(actual_pid_file, F_OK) == 0) {
		unlink(actual_pid_file);
		printf("PID file removed: %s\n", actual_pid_file);
	}
	
			raled_ereport(RALED_LOG_INFO, "RALED", "RALED shutdown complete - all resources cleaned up.", NULL, NULL);
	printf("RALED shutdown complete\n");

	return 0;
}

static void
cleanup_resources(void)
{
	/* Stop DStore and RALE and cleanup Unix socket */
	if (dstore_threads_started > 0)
	{
		void *rv;
		/* Signal shutdown so main loop returns */
		raled_log_info("Stopping daemon threads.");
		
		/* Use a simple approach - just cancel the thread if it's stuck */
		pthread_cancel(dstore_server_thread);
		pthread_join(dstore_server_thread, &rv);
		dstore_threads_started = 0;
	}
	
	(void) librale_rale_finit();
	comm_finit();

	/* Cleanup global librale configuration */
	if (librale_cfg != NULL)
	{
		librale_config_destroy(librale_cfg);
		librale_cfg = NULL;
	}

	/* Cleanup coordinated shutdown system */
	librale_shutdown_cleanup();
}

static librale_status_t
initialize_librale(void)
{
	librale_config_t *librale_config;
	librale_status_t result;

	librale_config = librale_config_create();
	if (librale_config == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	result = librale_config_set_node_id(librale_config, config.node.id);
	if (result != RALE_SUCCESS)
	{
		librale_config_destroy(librale_config);
		return result;
	}

	result = librale_config_set_node_name(librale_config, config.node.name);
	if (result != RALE_SUCCESS)
	{
		librale_config_destroy(librale_config);
		return result;
	}

	result = librale_config_set_node_ip(librale_config, config.node.ip);
	if (result != RALE_SUCCESS)
	{
		librale_config_destroy(librale_config);
		return result;
	}

	result = librale_config_set_rale_port(librale_config, config.node.rale_port);
	if (result != RALE_SUCCESS)
	{
		librale_config_destroy(librale_config);
		return result;
	}

	result = librale_config_set_dstore_port(librale_config, config.node.dstore_port);
	if (result != RALE_SUCCESS)
	{
		librale_config_destroy(librale_config);
		return result;
	}

	/* Use the database path parsed from config.db.path (not node.db_path) */
	result = librale_config_set_db_path(librale_config, config.db.path);
	if (result != RALE_SUCCESS)
	{
		librale_config_destroy(librale_config);
		return result;
	}

	/* Use top-level log_directory parsed by config.log_directory */
	result = librale_config_set_log_directory(librale_config, config.log_directory);
	if (result != RALE_SUCCESS)
	{
		librale_config_destroy(librale_config);
		return result;
	}

	/* Store the configuration globally for raled_init to use */
	librale_cfg = librale_config;

	result = librale_rale_init(librale_config);
	if (result != RALE_SUCCESS)
	{
		librale_config_destroy(librale_config);
		return result;
	}

	/* Add all three cluster nodes for testing */
			raled_log_debug("Adding cluster nodes for testing.");
	
	/* Add Node 1 */
	int add_result;
	
	add_result = cluster_add_node(1, "test_node_1", "127.0.0.1", 5001, 6001);
	if (add_result != RALE_SUCCESS) {
		const rale_error_info_t *error_info = rale_get_last_error();
		if (error_info && error_info->error_message) {
			raled_log_error("Failed to add node 1 (test_node_1:127.0.0.1:5001) to cluster: %s", 
			               error_info->error_message);
		} else {
			const char *error_desc = rale_error_code_to_string(add_result);
			raled_log_error("Failed to add node 1 (test_node_1:127.0.0.1:5001) to cluster: %s (code %d)", 
			               error_desc ? error_desc : "Unknown error", add_result);
		}
	}
	
	/* Add Node 2 */
	add_result = cluster_add_node(2, "test_node_2", "127.0.0.1", 5002, 6002);
	if (add_result != RALE_SUCCESS) {
		const rale_error_info_t *error_info = rale_get_last_error();
		if (error_info && error_info->error_message) {
			raled_log_error("Failed to add node 2 (test_node_2:127.0.0.1:5002) to cluster: %s", 
			               error_info->error_message);
		} else {
			const char *error_desc = rale_error_code_to_string(add_result);
			raled_log_error("Failed to add node 2 (test_node_2:127.0.0.1:5002) to cluster: %s (code %d)", 
			               error_desc ? error_desc : "Unknown error", add_result);
		}
	}
	
	/* Add Node 3 */
	add_result = cluster_add_node(3, "test_node_3", "127.0.0.1", 5003, 6003);
	if (add_result != RALE_SUCCESS) {
		const rale_error_info_t *error_info = rale_get_last_error();
		if (error_info && error_info->error_message) {
			raled_log_error("Failed to add node 3 (test_node_3:127.0.0.1:5003) to cluster: %s", 
			               error_info->error_message);
		} else {
			const char *error_desc = rale_error_code_to_string(add_result);
			raled_log_error("Failed to add node 3 (test_node_3:127.0.0.1:5003) to cluster: %s (code %d)", 
			               error_desc ? error_desc : "Unknown error", add_result);
		}
	}
	
	raled_log_info("Cluster configuration completed. Added %d nodes to cluster.", cluster_get_node_count());

	/* Start single main loop thread instead of separate server/client threads */
	if (pthread_create(&dstore_server_thread, NULL, raled_main_loop_thread, NULL) == 0)
	{
		dstore_threads_started = 1;
	}

	/* Don't destroy the config yet - it's needed by raled_init */
	return RALE_SUCCESS;
}

static void *
raled_main_loop_thread(void *arg)
{
	(void)arg;
	
			raled_ereport(RALED_LOG_INFO, "RALED", "Starting main processing loop.", NULL, NULL);
	
	while (!librale_is_shutdown_requested(SHUTDOWN_SUBSYSTEM_RALE))
	{
		/* Process all librale ticks in sequence */
		(void) librale_dstore_server_tick();
		(void) librale_dstore_client_tick();
		(void) librale_rale_tick();
		
		/* Small delay to prevent busy waiting */
		usleep(50000); /* 50ms */
	}
	
			raled_ereport(RALED_LOG_INFO, "RALED", "Main processing loop shutting down.", NULL, NULL);
	return NULL;
}
