/*-------------------------------------------------------------------------
 *
 * cluster.h
 *		Cluster management interface.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALE_CLUSTER_H
#define RALE_CLUSTER_H

#include <stdint.h>
#include "librale.h"
#include "config.h"
#include "node.h"

typedef struct cluster_t
{
	node_t				nodes[MAX_NODES];
	uint32_t			node_count;
	int32_t				self_id;
} cluster_t;

extern librale_status_t cluster_init(void);
extern librale_status_t cluster_finit(void);
extern librale_status_t cluster_add_node(int32_t node_id, const char *name, const char *ip, uint16_t rale_port, uint16_t dstore_port);
extern librale_status_t cluster_remove_node(int32_t node_id);
extern uint32_t cluster_get_node_count(void);
extern librale_status_t cluster_get_node(int32_t node_id, node_t *node);
extern librale_status_t cluster_get_node_by_index(uint32_t index, node_t *node);
extern librale_status_t cluster_set_state_file(const char *path);
extern int32_t cluster_get_self_id(void);
/* Set the local node id (self) and persist in cluster.state if configured */
extern librale_status_t cluster_set_self_id(int32_t self_id);

#endif							/* RALE_CLUSTER_H */
