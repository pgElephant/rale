/*-------------------------------------------------------------------------
 *
 * raled_logger.c
 *    Professional Unix daemon logging implementation for RALED.
 *    Follows Linux daemon standards with clean, crisp messages.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/src/raled_logger.c
 *
 *------------------------------------------------------------------------- */

/** System headers */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

/** Local headers */
#include "raled_logger.h"
#include "raled_inc.h"

/** Constants */
#define MAX_LOG_MESSAGE 1024
#define MAX_TIMESTAMP 32
#define LOG_IDENT "raled"

/** Global state */
static int syslog_initialized = 0;
static FILE *log_file = NULL;
static char log_file_path[512] = "";
static raled_log_level_t current_level = RALED_LOG_INFO;

/** Private function declarations */
static void write_to_log_file(raled_log_level_t level, const char *module __attribute__((unused)), 
                             const char *message, const char *detail __attribute__((unused)), const char *hint __attribute__((unused)));
static const char *level_to_string(raled_log_level_t level)
{
	switch (level)
	{
		case RALED_LOG_ERROR:   return "ERROR";
		case RALED_LOG_WARNING: return "WARN";
		case RALED_LOG_INFO:    return "INFO";
		case RALED_LOG_DEBUG:   return "DEBUG";
		default:                return "UNKNOWN";
	}
}
static int level_to_syslog_priority(raled_log_level_t level);
static void format_timestamp(char *buffer, size_t size);

/* Initialize daemon logging system */
void
raled_logger_init(int is_daemon_mode, const char *log_path, raled_log_level_t level)
{
	daemon_mode = is_daemon_mode;
	current_level = level;
	
	if (daemon_mode)
	{
		/* Initialize syslog for daemon mode */
		openlog(LOG_IDENT, LOG_PID | LOG_CONS, LOG_DAEMON);
		syslog_initialized = 1;
		
		/* Open log file if specified */
		if (log_path != NULL && strlen(log_path) > 0)
		{
			strncpy(log_file_path, log_path, sizeof(log_file_path) - 1);
			log_file_path[sizeof(log_file_path) - 1] = '\0';
			
			log_file = fopen(log_path, "a");
			if (log_file == NULL)
			{
				syslog(LOG_WARNING, "failed to open log file %s: %s", 
				       log_path, strerror(errno));
			}
			else
			{
				/* Set file permissions to 0640 (owner rw, group r, others none) */
				fchmod(fileno(log_file), S_IRUSR | S_IWUSR | S_IRGRP);
			}
		}
		
		raled_log_info("Daemon logging system initialized successfully.");
	}
	else
	{
		/* Interactive mode: use stderr with timestamps */
		raled_log_info("Interactive logging initialized.");
	}
}

/* Cleanup logging resources */
void
raled_logger_cleanup(void)
{
	if (log_file != NULL)
	{
		fclose(log_file);
		log_file = NULL;
	}
	
	if (syslog_initialized)
	{
		closelog();
		syslog_initialized = 0;
	}
}

/* Professional error reporting with clean formatting */
void
raled_ereport(raled_log_level_t level, const char *module, const char *message,
              const char *detail, const char *hint)
{
	/* Skip if below current log level */
	if (level > current_level)
		return;
	
	const char *module_name = module ? module : "raled";
	const char *level_str = level_to_string(level);
	
	if (daemon_mode)
	{
		/* Daemon mode: use syslog with clean formatting */
		if (syslog_initialized)
		{
			int priority = level_to_syslog_priority(level);
			
			/* Clean, informative message format */
			syslog(priority, "[%s] %s: %s", 
			       level_str, module_name, message);
		}
		
		/* Also write to log file if available */
		if (log_file != NULL)
		{
			write_to_log_file(level, module_name, message, detail, hint);
		}
	}
	else
	{
		/* Interactive mode: use stderr with professional formatting */
		char timestamp[MAX_TIMESTAMP];
		format_timestamp(timestamp, sizeof(timestamp));
		
		/* Color-coded output with visual indicators for better readability */
		const char *color_code = "";
		const char *reset_code = "";
		const char *indicator = "";
		
		switch (level)
		{
			case RALED_LOG_ERROR:   
				color_code = "\033[1;31m"; 
				indicator = "✗"; /* red-cross-tick */
				break;
			case RALED_LOG_WARNING: 
				color_code = "\033[1;33m"; 
				indicator = "⚠"; /* yellow-warning-symbol */
				break;
			case RALED_LOG_INFO:    
				color_code = "\033[1;32m"; 
				indicator = "✓"; /* green-info-tick */
				break;
			case RALED_LOG_DEBUG:   
				color_code = "\033[1;36m"; 
				indicator = "⚡"; /* debug symbol */
				break;
			default:                
				color_code = ""; 
				indicator = "?";
				break;
		}
		reset_code = "\033[0m";
		
		fprintf(stderr, "%s%s - %-6d %-8s %s %s: %s%s", 
		        color_code, indicator, getpid(), getenv("USER") ? getenv("USER") : "unknown", timestamp, LOG_IDENT, message, reset_code);
		
		fprintf(stderr, "\n");
	}
}

/* Write formatted log entry to file */
static void
write_to_log_file(raled_log_level_t level, const char *module __attribute__((unused)), 
                  const char *message, const char *detail __attribute__((unused)), const char *hint __attribute__((unused)))
{
	char timestamp[MAX_TIMESTAMP];
	format_timestamp(timestamp, sizeof(timestamp));
	const char *level_str = level_to_string(level);
	
	fprintf(log_file, "%s - %-6d %-8s %s %s: %s\n", 
	        level_str, getpid(), getenv("USER") ? getenv("USER") : "unknown", timestamp, LOG_IDENT, message);
	
	fflush(log_file);
}

/* Format current timestamp */
static void
format_timestamp(char *buffer, size_t size)
{
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);
	strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}



/* Convert log level to syslog priority */
static int
level_to_syslog_priority(raled_log_level_t level)
{
	switch (level)
	{
		case RALED_LOG_ERROR:   return LOG_ERR;
		case RALED_LOG_WARNING: return LOG_WARNING;
		case RALED_LOG_INFO:    return LOG_INFO;
		case RALED_LOG_DEBUG:   return LOG_DEBUG;
		default:                return LOG_INFO;
	}
}

/* Set log level */
void
raled_logger_set_level(raled_log_level_t level)
{
	current_level = level;
	if (syslog_initialized)
	{
		setlogmask(LOG_UPTO(level_to_syslog_priority(level)));
	}
}

/* Get current log level */
raled_log_level_t
raled_logger_get_level(void)
{
	return current_level;
}

/* Simple logging functions with professional formatting */
void
raled_log_error(const char *format, ...)
{
	va_list args;
	char message[MAX_LOG_MESSAGE];
	
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	
	raled_ereport(RALED_LOG_ERROR, NULL, message, NULL, NULL);
}

void
raled_log_warning(const char *format, ...)
{
	va_list args;
	char message[MAX_LOG_MESSAGE];
	
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	
	raled_ereport(RALED_LOG_WARNING, NULL, message, NULL, NULL);
}

void
raled_log_info(const char *format, ...)
{
	va_list args;
	char message[MAX_LOG_MESSAGE];
	
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	
	raled_ereport(RALED_LOG_INFO, NULL, message, NULL, NULL);
}

void
raled_log_debug(const char *format, ...)
{
	va_list args;
	char message[MAX_LOG_MESSAGE];
	
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	
	raled_ereport(RALED_LOG_DEBUG, NULL, message, NULL, NULL);
}

/* Module-specific logging functions */
void
raled_log_module(raled_log_level_t level, raled_log_module_t module, const char *format, ...)
{
	va_list args;
	char message[MAX_LOG_MESSAGE];
	const char *module_name = NULL;
	
	/* Convert module enum to string */
	switch (module)
	{
		case RALED_MODULE_RALED:    module_name = "raled"; break;
		case RALED_MODULE_RALE:     module_name = "rale"; break;
		case RALED_MODULE_RALECTRL: module_name = "ralectrl"; break;
		case RALED_MODULE_DSTORE:   module_name = "dstore"; break;
		case RALED_MODULE_CONFIG:   module_name = "config"; break;
		default:                    module_name = "unknown"; break;
	}
	
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	
	raled_ereport(level, module_name, message, NULL, NULL);
}

/* Log startup message */
void
raled_log_startup(void)
{
	raled_log_info("RALED daemon starting up.");
	raled_log_info("Version: \"1.0\".");
	raled_log_info("pid: %d", getpid());
	raled_log_info("User: \"%s\".", getenv("USER") ? getenv("USER") : "unknown");
}

/* Log shutdown message */
void
raled_log_shutdown(void)
{
	raled_log_info("RALED daemon shutting down.");
	raled_log_info("pid: %d", getpid());
}

/* Log configuration changes */
void
raled_log_config_change(const char *parameter, const char *old_value, const char *new_value)
{
	if (old_value && new_value)
	{
		raled_log_info("Configuration changed: \"%s\" = \"%s\" (was: \"%s\").", 
		               parameter, new_value, old_value);
	}
	else if (new_value)
	{
		raled_log_info("configuration set: %s = %s", parameter, new_value);
	}
	else
	{
		raled_log_info("configuration unset: %s", parameter);
	}
}

/* Log network events */
void
raled_log_network_event(const char *event, const char *address, int port)
{
	if (address && port > 0)
	{
		raled_log_info("network: %s %s:%d", event, address, port);
	}
	else if (address)
	{
		raled_log_info("network: %s %s", event, address);
	}
	else
	{
		raled_log_info("network: %s", event);
	}
}

/* Log cluster events */
void
raled_log_cluster_event(const char *event, const char *node_id, const char *details)
{
	if (node_id && details)
	{
		raled_log_info("cluster: %s node=%s %s", event, node_id, details);
	}
	else if (node_id)
	{
		raled_log_info("cluster: %s node=%s", event, node_id);
	}
	else
	{
		raled_log_info("cluster: %s", event);
	}
}

