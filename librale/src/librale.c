/*-------------------------------------------------------------------------
 *
 * librale.c
 *		Public API implementation for librale library.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "librale.h"
#include "librale_internal.h"
#include "shutdown.h"
#include "rale_error.h"

volatile int system_exit = 0;

librale_config_t *
librale_config_create(void)
{
	config_t *internal_config;

	rale_debug_log("Creating librale configuration");

	internal_config = malloc(sizeof(config_t));
	if (internal_config == NULL)
	{
		rale_set_error(RALE_ERROR_OUT_OF_MEMORY, "librale_config_create",
					   "Failed to allocate memory for configuration",
					   "Unable to allocate memory for config_t structure", 
					   "Check system memory availability");
		return NULL;
	}

	memset(internal_config, 0, sizeof(config_t));
	rale_debug_log("Configuration created successfully");
	return (librale_config_t *)internal_config;
}

void
librale_config_destroy(librale_config_t *config)
{
	if (config != NULL)
	{
		free((config_t *)config);
	}
}

librale_status_t
librale_config_set_node_id(librale_config_t *config, int32_t node_id)
{
	if (config == NULL)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, "librale_config_set_node_id",
					   "Configuration pointer is NULL", 
					   "config parameter must not be NULL",
					   "Ensure configuration is created before setting node ID");
		return RALE_ERROR_GENERAL;
	}

	if (node_id < 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "librale_config_set_node_id",
					   "Node ID must be non-negative",
					   "Provided node_id is negative",
					   "Use a positive integer for node ID");
		return RALE_ERROR_GENERAL;
	}

	rale_debug_log("Setting node ID to %d", node_id);
	((config_t *)config)->node.id = node_id;
	return RALE_SUCCESS;
}

librale_status_t
librale_config_set_node_name(librale_config_t *config, const char *name)
{
	if (config == NULL || name == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	strlcpy(((config_t *)config)->node.name, name, 
		sizeof(((config_t *)config)->node.name));
	return RALE_SUCCESS;
}

librale_status_t
librale_config_set_node_ip(librale_config_t *config, const char *ip)
{
	if (config == NULL || ip == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	strlcpy(((config_t *)config)->node.ip, ip, 
		sizeof(((config_t *)config)->node.ip));
	return RALE_SUCCESS;
}

librale_status_t
librale_config_set_dstore_port(librale_config_t *config, uint16_t port)
{
	if (config == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	((config_t *)config)->node.dstore_port = port;
	return RALE_SUCCESS;
}

librale_status_t
librale_config_set_rale_port(librale_config_t *config, uint16_t port)
{
	if (config == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	((config_t *)config)->node.rale_port = port;
	return RALE_SUCCESS;
}

librale_status_t
librale_config_set_db_path(librale_config_t *config, const char *path)
{
	if (config == NULL || path == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	strlcpy(((config_t *)config)->node.db_path, path, 
		sizeof(((config_t *)config)->node.db_path));
	return RALE_SUCCESS;
}

librale_status_t
librale_config_set_log_directory(librale_config_t *config, const char *path)
{
	if (config == NULL || path == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	strlcpy(((config_t *)config)->node.log_directory, path, 
		sizeof(((config_t *)config)->node.log_directory));
	return RALE_SUCCESS;
}

librale_status_t
librale_config_set_config(librale_config_t *dest, const void *src)
{
	if (dest == NULL || src == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	memcpy((config_t *)dest, src, sizeof(config_t));
	return RALE_SUCCESS;
}

int32_t
librale_config_get_node_id(const librale_config_t *config)
{
	if (config == NULL)
	{
		return -1;
	}

	return ((config_t *)config)->node.id;
}

const char *
librale_config_get_node_name(const librale_config_t *config)
{
	if (config == NULL)
	{
		return NULL;
	}

	return ((config_t *)config)->node.name;
}

const char *
librale_config_get_node_ip(const librale_config_t *config)
{
	if (config == NULL)
	{
		return NULL;
	}

	return ((config_t *)config)->node.ip;
}

uint16_t
librale_config_get_dstore_port(const librale_config_t *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return ((config_t *)config)->node.dstore_port;
}

uint16_t
librale_config_get_rale_port(const librale_config_t *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return ((config_t *)config)->node.rale_port;
}

const char *
librale_config_get_db_path(const librale_config_t *config)
{
	if (config == NULL)
	{
		return NULL;
	}

	return ((config_t *)config)->node.db_path;
}

const char *
librale_config_get_log_directory(const librale_config_t *config)
{
	if (config == NULL)
	{
		return NULL;
	}

	return ((config_t *)config)->node.log_directory;
}

librale_status_t
librale_config_set_dstore_keep_alive_interval(librale_config_t *config, uint32_t interval_seconds)
{
	if (config == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	((config_t *)config)->dstore.keep_alive_interval = interval_seconds;
	return RALE_SUCCESS;
}

librale_status_t
librale_config_set_dstore_keep_alive_timeout(librale_config_t *config, uint32_t timeout_seconds)
{
	if (config == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	((config_t *)config)->dstore.keep_alive_timeout = timeout_seconds;
	return RALE_SUCCESS;
}

librale_status_t
librale_dstore_init(uint16_t dstore_port, const librale_config_t *config)
{
	int result;

	if (config == NULL)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, "librale_dstore_init",
					   "Configuration pointer is NULL",
					   "config parameter must not be NULL", 
					   "Ensure configuration is created and populated before initializing dstore");
		return RALE_ERROR_GENERAL;
	}

	if (dstore_port == 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "librale_dstore_init",
					   "Dstore port cannot be zero",
					   "Provided dstore_port is 0",
					   "Use a valid port number (1-65535)");
		return RALE_ERROR_GENERAL;
	}

	rale_debug_log("Initializing dstore on port %u", dstore_port);
	
	result = dstore_init(dstore_port, (config_t *)config);
	if (result != RALE_SUCCESS)
	{
		rale_set_error(RALE_ERROR_NETWORK_INIT, "librale_dstore_init",
					   "Failed to initialize dstore",
					   "dstore_init() returned error",
					   "Check network configuration and port availability");
		return result;
	}

	rale_debug_log("Dstore initialized successfully");
	return RALE_SUCCESS;
}

librale_status_t
librale_dstore_finit(char *errbuf, size_t errbuflen)
{
	return dstore_finit(errbuf, errbuflen);
}

/* New tick functions for daemon-driven processing */
librale_status_t
librale_dstore_server_tick(void)
{
	int result = dstore_server_tick();
	return (result < 0) ? RALE_ERROR_GENERAL : RALE_SUCCESS;
}

librale_status_t
librale_dstore_client_tick(void)
{
	int result = dstore_client_tick();
	return (result < 0) ? RALE_ERROR_GENERAL : RALE_SUCCESS;
}

librale_status_t
librale_rale_tick(void)
{
	return rale_quram_process();
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
	if (key == NULL || value == NULL || errbuf == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

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
	if (node == NULL)
	{
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
	if (path == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	return cluster_set_state_file(path);
}

librale_status_t
librale_rale_init(const librale_config_t *config)
{
	if (config == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	return rale_init((config_t *)config);
}

librale_status_t
librale_rale_finit(void)
{
	return rale_finit();
}

int32_t
librale_get_current_role(void)
{
	extern rale_state_t current_rale_state;
	return (int32_t)current_rale_state.role;
}
