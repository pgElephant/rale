/*-------------------------------------------------------------------------
 *
 * response.c
 *    Response handling for RALED daemon.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/src/response.c
 *
 *-------------------------------------------------------------------------
 */

/** System headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

/** Local headers */
#include "raled_inc.h"
#include "librale.h"
#include "cluster.h"
#include "node.h"

/** Function declarations */
void init_response(response_t *response, int status_code, const char *message);
char *response_to_json(const response_t *response);
static char *construct_list_response(const char *nodes_json) __attribute__((unused));

int
handle_list_command(void)
{
	node_t node;
	uint32_t node_count;
	int i;
	
	/* Get cluster node count */
	node_count = cluster_get_node_count();
	if (node_count == 0)
	{
		printf("{\"nodes\": [], \"count\": 0}\n");
		return 0;
	}
	
	printf("{\"nodes\": [");
	for (i = 0; i < (int)node_count; i++)
	{
		if (cluster_get_node_by_index((uint32_t)i, &node) == RALE_SUCCESS)
		{
			if (i > 0) printf(",");
			printf("{\"id\": %d, \"host\": \"%s\", \"port\": %d, \"state\": \"%s\"}", 
			       node.id, node.name, node.rale_port,
			       (node.state == NODE_STATE_LEADER) ? "leader" :
			       (node.state == NODE_STATE_CANDIDATE) ? "candidate" : "offline");
		}
	}
	printf("], \"count\": %d}\n", node_count);
	
	return 0;
}

static char *
construct_list_response(const char *nodes_json)
{
	response_t response;
	char    *response_json_str = NULL;
	char     message_buffer[MAX_RESPONSE_LENGTH];

	if (nodes_json == NULL)
	{
		/** Handle error case: nodes_json is NULL */
		/** log_error("nodes_json is NULL in construct_list_response"); */
		init_response(&response, 500, "Error: Failed to retrieve node data.");
	}
	else
	{
		/** For now, we'll assume nodes_json is a simple string to be wrapped.
		 * In a real scenario, you would parse nodes_json and construct a more
		 * complex response. This is a simplified version.
		 */
		snprintf(message_buffer, MAX_RESPONSE_LENGTH, "{\"nodes\": %s}", nodes_json);
		/** Ensure null termination if snprintf truncates */
		message_buffer[MAX_RESPONSE_LENGTH - 1] = '\0';

		init_response(&response, 200, message_buffer);
	}

	response_json_str = response_to_json(&response);
	if (response_json_str == NULL)
	{
		/** log_error("Failed to convert response to JSON string"); */
		/** If response_to_json can return NULL, handle it.
		 * This might involve returning a static error JSON or handling upstream.
		 * For now, returning NULL as per original logic.
		 */
		return NULL;
	}

	return response_json_str; /** Caller is responsible for freeing this string */
}

void
init_response(response_t *response, int status_code, const char *message)
{
	if (response == NULL)
	{
		/** log_error("Response pointer is NULL in init_response"); */
		return;
	}
	response->status_code = status_code;
	strncpy(response->message, message, MAX_RESPONSE_LENGTH - 1);
	response->message[MAX_RESPONSE_LENGTH - 1] = '\0'; /** Ensure null-termination */
}

/**
 * Stub for response_to_json if not fully defined elsewhere yet
 * This function should allocate memory for the JSON string.
 * The caller is responsible for freeing this memory.
 */
char *
response_to_json(const response_t *response)
{
	size_t buffer_size;
	char  *json_str;

	if (response == NULL)
	{
		/** log_error("Response pointer is NULL in response_to_json"); */
		return NULL;
	}

	/**
	 * Estimate buffer size: {"status_code":<int_max_digits>,"message":"<message_content>"}
	 * int_max_digits is roughly 10-11 for a 32-bit integer.
	 * Add some padding for keys, quotes, colons, commas, braces.
	 */
	buffer_size = strlen(response->message) + 50; /** Simplified estimation */
	json_str = (char *) malloc(buffer_size);

	if (json_str == NULL)
	{
		/** log_error("Memory allocation failed in response_to_json"); */
		return NULL;
	}

	snprintf(json_str, buffer_size, "{\"status_code\":%d,\"message\":\"%s\"}",
			 response->status_code, response->message);
	/** Ensure null termination if snprintf truncates, though buffer_size should be adequate */
	json_str[buffer_size - 1] = '\0';

	return json_str;
}
