/*-------------------------------------------------------------------------
 *
 * node.h
 *		Node structure and types for cluster management.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALE_NODE_H
#define RALE_NODE_H

#include <stdint.h>
#include <time.h>

#define NAME_MAX 255
#define IP_ADDR_MAX 64

typedef enum node_state
{
	NODE_STATE_LEADER,
	NODE_STATE_CANDIDATE,
	NODE_STATE_OFFLINE
} node_state_t;

typedef enum node_status
{
	NODE_STATUS_ACTIVE,
	NODE_STATUS_INACTIVE,
	NODE_STATUS_FAILED
} node_status_t;

typedef struct node_t
{
	int32_t				id;
	char				name[NAME_MAX];
	char				ip[IP_ADDR_MAX];
	uint16_t			rale_port;
	uint16_t			dstore_port;
	int32_t				priority;
	node_state_t		state;
	node_status_t		status;
	uint32_t			term;
	uint64_t			last_log_index;
	uint32_t			last_log_term;
	time_t				last_heartbeat;
	bool				is_voting_member;
} node_t;

#endif							/* RALE_NODE_H */
