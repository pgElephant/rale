/*-------------------------------------------------------------------------
 *
 * node.c
 *    Implements node management for RALE.
 *    This file handles node initialization, addition, removal,
 *    and listing within the RALE cluster.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/node.c
 *
 *-------------------------------------------------------------------------
 */

/** System headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/** Local headers */
#include "librale_internal.h"
#include "rale_error.h"

/** Constants */
#define MAX_NODE_NAME_LEN 256
#define MAX_NODE_IP_LEN 46

/** External variables */
extern cluster_t cluster;
extern bool cluster_initialized;

/**
 * Initialize the node system.
 */
librale_status_t
node_init(void)
{
	if (cluster_initialized)
	{
		rale_debug_log("Node system already initialized");
		return RALE_SUCCESS;
	}

	memset(cluster.nodes, 0, sizeof(cluster.nodes));
	cluster.node_count = 0;
	cluster_initialized = true;

	rale_debug_log("Node system initialized successfully");
	return RALE_SUCCESS;
}

/**
 * Add a node to the cluster.
 */
librale_status_t
node_add(int32_t node_id, const char *name, const char *ip, uint16_t rale_port, uint16_t dstore_port)
{
	if (!cluster_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "node_add",
			"node_init() must be called before using node functions",
			"Node system not initialized",
			"Call node_init() at startup");
		return RALE_ERROR_GENERAL;
	}

	if (cluster.node_count >= MAX_NODES)
	{
		rale_set_error(RALE_ERROR_INVALID_CONFIG, "node_add",
			"Maximum number of nodes reached",
			"Node limit exceeded",
			"Remove some nodes before adding new ones");
		return RALE_ERROR_GENERAL;
	}

	if (node_id <= 0)
	{
		rale_set_error(RALE_ERROR_INVALID_NODE_ID, "node_add",
			"Invalid node ID",
			"Node ID must be positive",
			"Use a positive integer for node ID");
		return RALE_ERROR_GENERAL;
	}

	// Check if node already exists
	for (uint32_t i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id == node_id)
		{
			rale_set_error(RALE_ERROR_INVALID_CONFIG, "node_add",
				"Node with ID already exists",
				"Duplicate node ID",
				"Use a unique node ID");
			return RALE_ERROR_GENERAL;
		}
	}

	// Add new node
	cluster.nodes[cluster.node_count].id = node_id;
	strlcpy(cluster.nodes[cluster.node_count].name, name, sizeof(cluster.nodes[cluster.node_count].name));
	strlcpy(cluster.nodes[cluster.node_count].ip, ip, sizeof(cluster.nodes[cluster.node_count].ip));
	cluster.nodes[cluster.node_count].rale_port = rale_port;
	cluster.nodes[cluster.node_count].dstore_port = dstore_port;
	cluster.nodes[cluster.node_count].state = NODE_STATE_OFFLINE; // Assuming NODE_STATE_OFFLINE is the default
	cluster.nodes[cluster.node_count].is_voting_member = 1; // Assuming all added nodes are voting members
	cluster.nodes[cluster.node_count].last_heartbeat = time(NULL);

	cluster.node_count++;

	rale_debug_log("Added node (%d) to cluster", node_id);
	return RALE_SUCCESS;
}

/**
 * Remove a node from the cluster.
 */
librale_status_t
node_remove(int32_t node_id)
{
	if (!cluster_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "node_remove",
			"node_init() must be called before using node functions",
			"Node system not initialized",
			"Call node_init() at startup");
		return RALE_ERROR_GENERAL;
	}

	for (uint32_t i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id == node_id)
		{
			// Remove node by shifting remaining nodes
			for (uint32_t j = i; j < cluster.node_count - 1; j++)
			{
				cluster.nodes[j] = cluster.nodes[j + 1];
			}
			cluster.node_count--;
			rale_debug_log("Removed node (%d) from cluster", node_id);
			return RALE_SUCCESS;
		}
	}

	rale_set_error(RALE_ERROR_INVALID_NODE_ID, "node_remove",
		"Node not found in cluster",
		"Node ID does not exist",
		"Check the node ID and try again");
	return RALE_ERROR_GENERAL;
}

/**
 * Get node information by ID.
 */
librale_status_t
node_get(int32_t id, node_t *node)
{
	if (!cluster_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "node_get",
			"node_init() must be called before using node functions",
			"Node system not initialized",
			"Call node_init() at startup");
		return RALE_ERROR_GENERAL;
	}

	if (node == NULL)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "node_get",
			"Node parameter is NULL",
			"Invalid parameter",
			"Provide a valid node pointer");
		return RALE_ERROR_GENERAL;
	}

	for (uint32_t i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id == id)
		{
			*node = cluster.nodes[i];
			return RALE_SUCCESS;
		}
	}

	rale_set_error(RALE_ERROR_INVALID_NODE_ID, "node_get",
		"Node not found in cluster",
		"Node ID does not exist",
		"Check the node ID and try again");
	return RALE_ERROR_GENERAL;
}

/**
 * List nodes in the cluster.
 */
librale_status_t
node_list(node_t *node_list, uint32_t *count)
{
	if (!cluster_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "node_list",
			"node_init() must be called before using node functions",
			"Node system not initialized",
			"Call node_init() at startup");
		return RALE_ERROR_GENERAL;
	}

	if (node_list == NULL || count == NULL)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "node_list",
			"Invalid node list buffer",
			"Parameters are NULL",
			"Provide valid pointers for node list and count");
		return RALE_ERROR_GENERAL;
	}

	*count = cluster.node_count;
	memcpy(node_list, cluster.nodes, sizeof(cluster.nodes[0]) * cluster.node_count);

	return RALE_SUCCESS;
}

/**
 * Cleanup the node system.
 */
librale_status_t
node_cleanup(void)
{
	if (!cluster_initialized)
	{
		rale_debug_log("Node system not initialized, nothing to cleanup");
		return RALE_SUCCESS;
	}

	memset(cluster.nodes, 0, sizeof(cluster.nodes));
	cluster.node_count = 0;
	cluster_initialized = false;

	rale_debug_log("Node system cleaned up successfully");
	return RALE_SUCCESS;
}
