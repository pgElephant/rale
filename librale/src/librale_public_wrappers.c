/*-------------------------------------------------------------------------
 *
 * librale_public_wrappers.c
 *    Public API wrapper functions for librale library.
 *    These functions convert between internal types and public opaque types.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/librale_public_wrappers.c
 *
 *-------------------------------------------------------------------------
 */

#include "librale.h"
#include "librale_internal.h"
#include "config.h"
#include "cluster.h"
#include "node.h"
#include "rale_error.h"
#include <stdarg.h>

/* Legacy logging compatibility constants */
#define RALE_ERROR      0
#define RALE_WARNING    1
#define RALE_INFO       2
#define RALE_DEBUG      3

librale_config_t *
librale_config_create(void)
{
	config_t *internal_config = config_create();
	return (librale_config_t *)internal_config;
}

void
librale_config_destroy(librale_config_t *config)
{
	if (config != NULL) {
		config_destroy((config_t *)config);
	}
}

librale_status_t
librale_config_set_node_id(librale_config_t *config, int32_t node_id)
{
	if (config == NULL) {
		return RALE_ERROR_GENERAL;
	}
	((config_t *)config)->node.id = node_id;
	return RALE_SUCCESS;
}

librale_status_t
librale_config_set_dstore_port(librale_config_t *config, uint16_t port)
{
	if (config == NULL) {
		return RALE_ERROR_GENERAL;
	}
	((config_t *)config)->node.dstore_port = port;
	return RALE_SUCCESS;
}

librale_status_t
librale_config_set_db_path(librale_config_t *config, const char *path)
{
	if (config == NULL || path == NULL) {
		return RALE_ERROR_GENERAL;
	}
	strncpy(((config_t *)config)->db.path, path, sizeof(((config_t *)config)->db.path) - 1);
	((config_t *)config)->db.path[sizeof(((config_t *)config)->db.path) - 1] = '\0';
	return RALE_SUCCESS;
}

int32_t
librale_config_get_node_id(const librale_config_t *config)
{
	if (config == NULL) {
		return -1;
	}
	return ((config_t *)config)->node.id;
}

uint16_t
librale_config_get_dstore_port(const librale_config_t *config)
{
	if (config == NULL) {
		return 0;
	}
	return ((config_t *)config)->node.dstore_port;
}

librale_status_t
librale_dstore_init(uint16_t dstore_port, const librale_config_t *config)
{
	if (config == NULL) {
		return RALE_ERROR_GENERAL;
	}
	return dstore_init(dstore_port, (config_t *)config);
}

librale_status_t
librale_dstore_finit(char *errbuf, size_t errbuflen)
{
	return dstore_finit(errbuf, errbuflen);
}

void
librale_dstore_server_loop(char *errbuf, size_t errbuflen)
{
	dstore_server_loop(errbuf, errbuflen);
}

librale_status_t
librale_dstore_client_loop(char *errbuf, size_t errbuflen)
{
	return dstore_client_loop(errbuf, errbuflen);
}

void
librale_dstore_put_from_command(const char *command, char *errbuf, size_t errbuflen)
{
	dstore_put_from_command(command, errbuf, errbuflen);
}

void
librale_dstore_replicate_to_followers(const char *key, const char *value, char *errbuf, size_t errbuflen)
{
	dstore_replicate_to_followers(key, value, errbuf, errbuflen);
}

librale_status_t
librale_db_get(const char *key, char *value, size_t value_size, char *errbuf, size_t errbuflen)
{
	return db_get(key, value, value_size, errbuf, errbuflen);
}

uint32_t
librale_cluster_get_node_count(void)
{
	return cluster_get_node_count();
}

librale_status_t
librale_cluster_get_node(int32_t node_id, librale_node_t *node)
{
	if (node == NULL) {
		return RALE_ERROR_GENERAL;
	}
	return cluster_get_node(node_id, (node_t *)node);
}

int32_t
librale_cluster_get_self_id(void)
{
	return cluster_get_self_id();
}

librale_status_t
librale_cluster_set_state_file(const char *path)
{
	return cluster_set_state_file(path);
}

librale_status_t
librale_rale_init(const librale_config_t *config)
{
	if (config == NULL) {
		return RALE_ERROR_GENERAL;
	}
	return rale_init((config_t *)config);
}

librale_status_t
librale_rale_finit(void)
{
	return rale_finit();
}

librale_status_t
librale_rale_quram_process(void)
{
	return rale_quram_process();
}

int32_t
librale_get_current_role(void)
{
	return librale_get_current_role();
}

int
librale_unix_socket_server_init(void)
{
	return unix_socket_server_init();
}

int
librale_unix_socket_server_loop(void)
{
	return unix_socket_server_loop();
}

int
librale_unix_socket_server_finit(void)
{
	return unix_socket_server_finit();
}

void
librale_log_set_config(const librale_config_t *cfg)
{
	if (cfg != NULL) {
		log_set_config((config_t *)cfg);
	}
}

void
librale_log(librale_log_level_t level, const char *message)
{
	/* 
	 * Logging removed from librale - library now uses error codes.
	 * Daemon (raled) is responsible for all logging based on error codes.
	 * This function is retained for API compatibility but does nothing.
	 */
	(void)level;    /* suppress unused parameter warning */
	(void)message;  /* suppress unused parameter warning */
}

void
librale_log_with_context(librale_log_level_t level, const char *context, const char *message)
{
	/* 
	 * Logging removed from librale - library now uses error codes.
	 * Daemon (raled) is responsible for all logging based on error codes.
	 * This function is retained for API compatibility but does nothing.
	 */
	(void)level;    /* suppress unused parameter warning */
	(void)context;  /* suppress unused parameter warning */
	(void)message;  /* suppress unused parameter warning */
}

const char *
librale_get_version(void)
{
	return "1.0.0";
}

const char *
librale_get_build_info(void)
{
	return "librale 1.0.0 - RALE Consensus and Distributed Store Library";
}

/*
 * Stub functions for legacy logging system - DO NOT USE
 * These are here only for backwards compatibility during transition
 */
void rale_log(int level, const char *module, const char *fmt, ...)
{
	(void)level;
	(void)module;
	(void)fmt;
	/* Legacy logging is disabled - library uses error codes only */
}

void rale_report(int level, const char *context, const char *message, const char *hint, const char *fmt, ...)
{
	(void)level;
	(void)context;
	(void)message;
	(void)hint;
	(void)fmt;
	/* Legacy logging is disabled - library uses error codes only */
} 