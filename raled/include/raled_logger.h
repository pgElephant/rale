/*-------------------------------------------------------------------------
 *
 * raled_logger.h
 *    Professional Unix daemon logging interface for RALED.
 *    Follows Linux daemon standards with clean, crisp messages.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/include/raled_logger.h
 *
 *------------------------------------------------------------------------- */

#ifndef RALED_LOGGER_H
#define RALED_LOGGER_H

/** System headers */
#include <stdarg.h>
#include <stdio.h>

/** Log levels for raled */
typedef enum raled_log_level
{
	RALED_LOG_ERROR,   /** Error level logging */
	RALED_LOG_WARNING, /** Warning level logging */
	RALED_LOG_INFO,    /** Info level logging */
	RALED_LOG_DEBUG    /** Debug level logging */
} raled_log_level_t;

/** Log modules for raled */
typedef enum raled_log_module
{
	RALED_MODULE_RALED,    /** RALED module */
	RALED_MODULE_RALE,     /** RALE module */
	RALED_MODULE_RALECTRL, /** RALECTRL module */
	RALED_MODULE_DSTORE,   /** DSTORE module */
	RALED_MODULE_CONFIG    /** CONFIG module */
} raled_log_module_t;

/** Function declarations */

/* Core logging functions */
extern void raled_logger_init(int is_daemon_mode, const char *log_path, raled_log_level_t level);
extern void raled_logger_cleanup(void);
extern void raled_logger_set_level(raled_log_level_t level);
extern raled_log_level_t raled_logger_get_level(void);

/* Professional error reporting */
extern void raled_ereport(raled_log_level_t level, const char *module, const char *message,
						  const char *detail, const char *hint);

/* Simple logging functions */
extern void raled_log_error(const char *format, ...);
extern void raled_log_warning(const char *format, ...);
extern void raled_log_info(const char *format, ...);
extern void raled_log_debug(const char *format, ...);

/* Module-specific logging */
extern void raled_log_module(raled_log_level_t level, raled_log_module_t module, const char *format, ...);

/* Specialized logging functions */
extern void raled_log_startup(void);
extern void raled_log_shutdown(void);
extern void raled_log_config_change(const char *parameter, const char *old_value, const char *new_value);
extern void raled_log_network_event(const char *event, const char *address, int port);
extern void raled_log_cluster_event(const char *event, const char *node_id, const char *details);

#endif							/* RALED_LOGGER_H */
