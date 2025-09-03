/*-------------------------------------------------------------------------
 *
 * cluster.c
 *		Cluster management implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include "librale_internal.h"
#include "rale_error.h"

/* Compatibility with legacy logging calls - will be removed */
#define MAX_REASONABLE_NODE_ID 1000
#define MAX_REASONABLE_PORT 65535
#define MAX_REASONABLE_NAME_LEN 256
#define MAX_REASONABLE_IP_LEN 46
#define MODULE "RALE"

static char cluster_state_file[512] = "";
static pthread_mutex_t cluster_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool cluster_initialized = false;

cluster_t cluster;

static librale_status_t cluster_save_state(void);
static librale_status_t cluster_load_state(void);

librale_status_t
cluster_init(void)
{
	pthread_mutex_lock(&cluster_mutex);
	if (cluster_initialized)
	{
		pthread_mutex_unlock(&cluster_mutex);
		rale_debug_log("Cluster already initialized, skipping");
		return RALE_SUCCESS;
	}

	memset(&cluster, 0, sizeof(cluster_t));
	cluster.node_count = 0;
	cluster.self_id = -1;

	cluster_initialized = true;
	pthread_mutex_unlock(&cluster_mutex);

	cluster_load_state();

	rale_debug_log("Cluster initialized successfully");
	return RALE_SUCCESS;
}

librale_status_t
cluster_finit(void)
{
	librale_status_t result = RALE_SUCCESS;

	pthread_mutex_lock(&cluster_mutex);
	if (cluster_initialized)
	{
		memset(&cluster, 0, sizeof(cluster_t));
		cluster_initialized = false;
		rale_debug_log("Cluster cleaned up successfully");
	}
	else
	{
		rale_debug_log("Cluster was not initialized, nothing to cleanup");
	}
	pthread_mutex_unlock(&cluster_mutex);

	return result;
}

librale_status_t
cluster_add_node(int32_t node_id, const char *name, const char *ip, uint16_t rale_port, uint16_t dstore_port)
{
	uint32_t i;
	node_t *node;

	if (!cluster_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "cluster_add_node",
			"cluster_init() must be called before using cluster functions.",
			"Cluster not initialized.",
			"Call cluster_init() at startup.");
		return RALE_ERROR_GENERAL;
	}

	if (node_id <= 0 || node_id > MAX_REASONABLE_NODE_ID)
	{
		rale_set_error(RALE_ERROR_INVALID_NODE_ID, "cluster_add_node",
			"Invalid node_id",
			"Invalid parameter",
			"Node ID must be between 1 and 1000");
		return RALE_ERROR_GENERAL;
	}

	if (name == NULL || strlen(name) == 0 || strlen(name) > MAX_REASONABLE_NAME_LEN)
	{
		rale_set_error(RALE_ERROR_INVALID_CONFIG, "cluster_add_node",
			"Invalid node name",
			"Invalid parameter",
			"Node name must be non-empty and less than 256 characters");
		return RALE_ERROR_GENERAL;
	}

	if (ip == NULL || strlen(ip) == 0 || strlen(ip) > MAX_REASONABLE_IP_LEN)
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_CONFIG, "cluster_add_node",
			"Invalid node IP: must be non-empty and less than %d characters", MAX_REASONABLE_IP_LEN);
		return RALE_ERROR_GENERAL;
	}

	if (rale_port <= 0 || rale_port > MAX_REASONABLE_PORT)
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_CONFIG, "cluster_add_node",
			"Invalid RALE port: %d, must be between 1 and %d", rale_port, MAX_REASONABLE_PORT);
		return RALE_ERROR_GENERAL;
	}

	if (dstore_port <= 0 || dstore_port > MAX_REASONABLE_PORT)
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_CONFIG, "cluster_add_node",
			"Invalid DStore port: %d, must be between 1 and %d", dstore_port, MAX_REASONABLE_PORT);
		return RALE_ERROR_GENERAL;
	}

	pthread_mutex_lock(&cluster_mutex);

	for (i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id == node_id)
		{
			pthread_mutex_unlock(&cluster_mutex);
			rale_set_error_fmt(RALE_ERROR_INVALID_CONFIG, "cluster_add_node",
				"Node with ID %d already exists", node_id);
			return RALE_ERROR_GENERAL;
		}
	}

	if (cluster.node_count >= MAX_NODES)
	{
		pthread_mutex_unlock(&cluster_mutex);
		rale_set_error_fmt(RALE_ERROR_INVALID_CONFIG, "cluster_add_node",
			"Maximum number of nodes (%d) reached", MAX_NODES);
		return RALE_ERROR_GENERAL;
	}

	node = &cluster.nodes[cluster.node_count];
	node->id = node_id;
	strlcpy(node->name, name, sizeof(node->name));
	strlcpy(node->ip, ip, sizeof(node->ip));
	node->rale_port = rale_port;
	node->dstore_port = dstore_port;
	node->status = NODE_STATUS_ACTIVE;
	node->last_heartbeat = time(NULL);

	cluster.node_count++;

	cluster_save_state();

	pthread_mutex_unlock(&cluster_mutex);

	rale_debug_log("Added node %d (%s) at %s:%d/%d",
		node_id, name, ip, rale_port, dstore_port);

	return RALE_SUCCESS;
}

librale_status_t
cluster_remove_node(int32_t node_id)
{
	uint32_t i;
	bool found = false;

	if (!cluster_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "cluster_remove_node",
			"cluster_init() must be called before using cluster functions.",
			"Cluster not initialized.",
			"Call cluster_init() at startup.");
		return RALE_ERROR_GENERAL;
	}

	pthread_mutex_lock(&cluster_mutex);

	for (i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id == node_id)
		{
			found = true;
			break;
		}
	}

	if (!found)
	{
		pthread_mutex_unlock(&cluster_mutex);
		rale_set_error_fmt(RALE_ERROR_INVALID_NODE_ID, "cluster_remove_node",
			"Node with ID %d not found", node_id);
		return RALE_ERROR_GENERAL;
	}

	for (; i < cluster.node_count - 1; i++)
	{
		cluster.nodes[i] = cluster.nodes[i + 1];
	}

	cluster.node_count--;

	cluster_save_state();

	pthread_mutex_unlock(&cluster_mutex);

	rale_debug_log("Removed node %d", node_id);

	return RALE_SUCCESS;
}

uint32_t
cluster_get_node_count(void)
{
	uint32_t count;

	pthread_mutex_lock(&cluster_mutex);
	count = cluster.node_count;
	pthread_mutex_unlock(&cluster_mutex);

	return count;
}

librale_status_t
cluster_get_node(int32_t node_id, node_t *node)
{
	uint32_t i;

	if (node == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	pthread_mutex_lock(&cluster_mutex);

	for (i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id == node_id)
		{
			*node = cluster.nodes[i];
			pthread_mutex_unlock(&cluster_mutex);
			return RALE_SUCCESS;
		}
	}

	pthread_mutex_unlock(&cluster_mutex);
	return RALE_ERROR_GENERAL;
}

librale_status_t
cluster_get_node_by_index(uint32_t index, node_t *node)
{
	if (node == NULL)
		return RALE_ERROR_GENERAL;

	pthread_mutex_lock(&cluster_mutex);
	if (index >= cluster.node_count)
	{
		pthread_mutex_unlock(&cluster_mutex);
		return RALE_ERROR_GENERAL;
	}
	*node = cluster.nodes[index];
	pthread_mutex_unlock(&cluster_mutex);
	return RALE_SUCCESS;
}

librale_status_t
cluster_set_state_file(const char *path)
{
	if (path == NULL || strlen(path) == 0)
	{
		return RALE_ERROR_GENERAL;
	}

	if (strlen(path) >= sizeof(cluster_state_file))
	{
		return RALE_ERROR_GENERAL;
	}

	strlcpy(cluster_state_file, path, sizeof(cluster_state_file));

	return RALE_SUCCESS;
}

int32_t
cluster_get_self_id(void)
{
	return cluster.self_id;
}

librale_status_t
cluster_set_self_id(int32_t self_id)
{
	if (self_id <= 0 || self_id > MAX_REASONABLE_NODE_ID)
	{
		return RALE_ERROR_GENERAL;
	}
	pthread_mutex_lock(&cluster_mutex);
	cluster.self_id = self_id;
	pthread_mutex_unlock(&cluster_mutex);
	/* best-effort persist */
	(void) cluster_save_state();
	return RALE_SUCCESS;
}

static librale_status_t
cluster_save_state(void)
{
	FILE *file;
	uint32_t i;

	if (strlen(cluster_state_file) == 0)
	{
		return RALE_SUCCESS; /* No state file configured */
	}

	file = fopen(cluster_state_file, "w");
	if (file == NULL)
	{
		rale_set_error_fmt(RALE_ERROR_FILE_ACCESS, "cluster_save_state",
		                   "Failed to open cluster state file for writing: %s", cluster_state_file);
		return RALE_ERROR_GENERAL;
	}

	/* Write cluster state */
	char line[256];
	snprintf(line, sizeof(line), "self_id=%d\n", cluster.self_id);
	fputs(line, file);
	snprintf(line, sizeof(line), "node_count=%d\n", cluster.node_count);
	fputs(line, file);

	/* Write node information */
	for (i = 0; i < cluster.node_count; i++)
	{
		snprintf(line, sizeof(line), "node[%d].id=%d\n", i, cluster.nodes[i].id);
		fputs(line, file);
		snprintf(line, sizeof(line), "node[%d].name=%s\n", i, cluster.nodes[i].name);
		fputs(line, file);
		snprintf(line, sizeof(line), "node[%d].ip=%s\n", i, cluster.nodes[i].ip);
		fputs(line, file);
		snprintf(line, sizeof(line), "node[%d].rale_port=%d\n", i, cluster.nodes[i].rale_port);
		fputs(line, file);
		snprintf(line, sizeof(line), "node[%d].dstore_port=%d\n", i, cluster.nodes[i].dstore_port);
		fputs(line, file);
	}

	fclose(file);
	rale_debug_log("Cluster state saved to %s", cluster_state_file);
	return RALE_SUCCESS;
}

static librale_status_t
cluster_load_state(void)
{
	FILE *file;
	char line[512];
	char key[64], value[256];
	uint32_t node_index = 0;
	int32_t node_id = 0;
	char node_name[256] = {0};
	char node_ip[256] = {0};
	uint16_t rale_port = 0;
	uint16_t dstore_port = 0;

	if (strlen(cluster_state_file) == 0)
	{
		return RALE_SUCCESS; /* No state file configured */
	}

	file = fopen(cluster_state_file, "r");
	if (file == NULL)
	{
		rale_debug_log("No existing cluster state file found: %s", cluster_state_file);
		return RALE_SUCCESS; /* Not an error if file doesn't exist */
	}

	/* Reset cluster state */
	memset(&cluster, 0, sizeof(cluster_t));
	cluster.node_count = 0;
	cluster.self_id = -1;

	while (fgets(line, sizeof(line), file))
	{
		line[strcspn(line, "\n")] = 0; /* Remove newline */
		
		/** Manual parsing for safety */
		char *equals = strchr(line, '=');
		if (equals == NULL)
		{
			continue; /* Skip malformed lines */
		}
		
		/** Extract key and value */
		size_t key_len = (size_t)(equals - line);
		if (key_len >= sizeof(key))
		{
			continue; /* Key too long */
		}
		
		strlcpy(key, line, key_len + 1);
		
		const char *value_str = equals + 1;
		if (strlen(value_str) >= sizeof(value))
		{
			continue;
		}
		
		strlcpy(value, value_str, sizeof(value));

		if (strcmp(key, "self_id") == 0)
		{
			cluster.self_id = atoi(value);
		}
		else if (strcmp(key, "node_count") == 0)
		{
			cluster.node_count = (uint32_t)atoi(value);
		}
		else if (strncmp(key, "node[", 5) == 0 && strstr(key, "].id"))
		{
			node_id = atoi(value);
		}
		else if (strncmp(key, "node[", 5) == 0 && strstr(key, "].name"))
		{
			strlcpy(node_name, value, sizeof(node_name));
		}
		else if (strncmp(key, "node[", 5) == 0 && strstr(key, "].ip"))
		{
			strlcpy(node_ip, value, sizeof(node_ip));
		}
		else if (strncmp(key, "node[", 5) == 0 && strstr(key, "].rale_port"))
		{
			rale_port = (uint16_t)atoi(value);
		}
		else if (strncmp(key, "node[", 5) == 0 && strstr(key, "].dstore_port"))
		{
			dstore_port = (uint16_t)atoi(value);
			
			/* Complete node data, add to cluster */
			if (node_index < MAX_NODES)
			{
				cluster.nodes[node_index].id = node_id;
				strlcpy(cluster.nodes[node_index].name, node_name, sizeof(cluster.nodes[node_index].name));
				strlcpy(cluster.nodes[node_index].ip, node_ip, sizeof(cluster.nodes[node_index].ip));
				cluster.nodes[node_index].rale_port = rale_port;
				cluster.nodes[node_index].dstore_port = dstore_port;
				node_index++;
			}
		}
	}

	fclose(file);
	rale_debug_log("Cluster state loaded from %s, %d nodes", cluster_state_file, cluster.node_count);
	return RALE_SUCCESS;
}

