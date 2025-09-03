/*-------------------------------------------------------------------------
 *
 * raled_command.c
 *    Command processing implementation for RALED daemon.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/src/raled_command.c
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <cjson/cJSON.h>
#include "raled_command.h"
#include "raled_logger.h"
#include "librale.h"

#define MAX_COMMAND_LENGTH 1024
#define MAX_RESPONSE_LENGTH 2048
#define MAX_KEY_LENGTH 256
#define MAX_VALUE_LENGTH 1024

static librale_status_t process_get_command(const char *key, char *response, size_t response_size);
static librale_status_t process_put_command(const char *key, const char *value, char *response, size_t response_size);
static librale_status_t process_list_command(char *response, size_t response_size);
static librale_status_t process_status_command(char *response, size_t response_size);
static librale_status_t process_stop_command(char *response, size_t response_size);
static librale_status_t process_add_command(int node_id, const char *name, const char *ip, int rale_port, int dstore_port, char *response, size_t response_size);
static librale_status_t process_remove_command(int node_id, char *response, size_t response_size);

librale_status_t
raled_process_command(const char *command_text, char *response, size_t response_size)
{
	if (!command_text || !response || response_size == 0) {
		raled_log_error("Invalid parameters to raled_process_command: missing required arguments.");
		return RALE_ERROR_GENERAL;
	}

	/* Initialize response buffer */
	response[0] = '\0';

	/* Handle empty commands */
	if (strlen(command_text) == 0) {
		snprintf(response, response_size, "ERROR: Empty command");
		return RALE_ERROR_GENERAL;
	}

	raled_log_debug("Processing command: \"%s\".", command_text);

	/* Try to parse as JSON first */
	cJSON *json = cJSON_Parse(command_text);
	if (json) {
		cJSON *cmd_obj = cJSON_GetObjectItemCaseSensitive(json, "command");
		if (cJSON_IsString(cmd_obj)) {
			const char *cmd = cmd_obj->valuestring;
			
			if (strcmp(cmd, "GET") == 0) {
				cJSON *key_obj = cJSON_GetObjectItemCaseSensitive(json, "key");
				if (cJSON_IsString(key_obj)) {
					librale_status_t result = process_get_command(key_obj->valuestring, response, response_size);
					cJSON_Delete(json);
					return result;
				}
			} else if (strcmp(cmd, "PUT") == 0) {
				cJSON *key_obj = cJSON_GetObjectItemCaseSensitive(json, "key");
				cJSON *value_obj = cJSON_GetObjectItemCaseSensitive(json, "value");
				if (cJSON_IsString(key_obj) && cJSON_IsString(value_obj)) {
					librale_status_t result = process_put_command(key_obj->valuestring, value_obj->valuestring, response, response_size);
					cJSON_Delete(json);
					return result;
				}
			}
		}
		cJSON_Delete(json);
	}

	/* Parse text commands */
	char command_copy[MAX_COMMAND_LENGTH];
	strncpy(command_copy, command_text, sizeof(command_copy) - 1);
	command_copy[sizeof(command_copy) - 1] = '\0';

	char *token = strtok(command_copy, " \t\n");
	if (!token) {
		snprintf(response, response_size, "ERROR: Invalid command format");
		return RALE_ERROR_GENERAL;
	}

	/* Convert command to uppercase for case-insensitive matching */
	for (char *p = token; *p; p++) {
		*p = (char)toupper(*p);
	}

	if (strcmp(token, "GET") == 0) {
		char *key = strtok(NULL, " \t\n");
		if (!key) {
			snprintf(response, response_size, "ERROR: GET requires a key");
			return RALE_ERROR_GENERAL;
		}
		return process_get_command(key, response, response_size);
	} else if (strcmp(token, "PUT") == 0) {
		char *key = strtok(NULL, " \t\n");
		char *value = strtok(NULL, "");  /* Get rest of line as value */
		if (!key || !value) {
			snprintf(response, response_size, "ERROR: PUT requires key and value");
			return RALE_ERROR_GENERAL;
		}
		/* Trim leading whitespace from value */
		while (*value && (*value == ' ' || *value == '\t')) {
			value++;
		}
		return process_put_command(key, value, response, response_size);
	} else if (strcmp(token, "LIST") == 0) {
		return process_list_command(response, response_size);
	} else if (strcmp(token, "STATUS") == 0) {
		return process_status_command(response, response_size);
	} else if (strcmp(token, "STOP") == 0) {
		return process_stop_command(response, response_size);
	} else if (strcmp(token, "ADD") == 0) {
		char *node_id_str = strtok(NULL, " \t\n");
		char *name = strtok(NULL, " \t\n");
		char *ip = strtok(NULL, " \t\n");
		char *rale_port_str = strtok(NULL, " \t\n");
		char *dstore_port_str = strtok(NULL, " \t\n");
		
		if (!node_id_str || !name || !ip || !rale_port_str || !dstore_port_str) {
			snprintf(response, response_size, "ERROR: ADD requires node_id name ip rale_port dstore_port");
			return RALE_ERROR_GENERAL;
		}
		
		int node_id = atoi(node_id_str);
		int rale_port = atoi(rale_port_str);
		int dstore_port = atoi(dstore_port_str);
		
		return process_add_command(node_id, name, ip, rale_port, dstore_port, response, response_size);
	} else if (strcmp(token, "REMOVE") == 0) {
		char *node_id_str = strtok(NULL, " \t\n");
		if (!node_id_str) {
			snprintf(response, response_size, "ERROR: REMOVE requires node_id");
			return RALE_ERROR_GENERAL;
		}
		
		int node_id = atoi(node_id_str);
		return process_remove_command(node_id, response, response_size);
	} else {
		snprintf(response, response_size, "ERROR: Unknown command '%s'", token);
		return RALE_ERROR_GENERAL;
	}
}

static librale_status_t
process_get_command(const char *key, char *response, size_t response_size)
{
	char value[MAX_VALUE_LENGTH];
	char errbuf[256];

	if (strlen(key) > MAX_KEY_LENGTH) {
		snprintf(response, response_size, "ERROR: Key too long");
		return RALE_ERROR_GENERAL;
	}

	if (librale_db_get(key, value, sizeof(value), errbuf, sizeof(errbuf)) == RALE_SUCCESS) {
		snprintf(response, response_size, "OK: %s", value);
		return RALE_SUCCESS;
	} else {
		snprintf(response, response_size, "ERROR: %s", errbuf);
		return RALE_ERROR_GENERAL;
	}
}

static librale_status_t
process_put_command(const char *key, const char *value, char *response, size_t response_size)
{
	char errbuf[256];

	if (strlen(key) > MAX_KEY_LENGTH) {
		snprintf(response, response_size, "ERROR: Key too long");
		return RALE_ERROR_GENERAL;
	}

	if (strlen(value) > MAX_VALUE_LENGTH) {
		snprintf(response, response_size, "ERROR: Value too long");
		return RALE_ERROR_GENERAL;
	}

	/* Use available API for PUT command */
	char full_command[512];
	snprintf(full_command, sizeof(full_command), "PUT %s %s", key, value);
	librale_dstore_put_from_command(full_command, errbuf, sizeof(errbuf));
	
	if (strlen(errbuf) > 0) {
		snprintf(response, response_size, "ERROR: %s", errbuf);
		return RALE_ERROR_GENERAL;
	} else {
		snprintf(response, response_size, "OK: %s", value);
		return RALE_SUCCESS;
	}
}

static librale_status_t
process_list_command(char *response, size_t response_size)
{
	int32_t current_role = librale_get_current_role();
	int32_t self_id = librale_cluster_get_self_id();
	uint32_t i;
	char role_buf[16];

	const char *role_str = (current_role == 0 ? "follower" : (current_role == 1 ? "candidate" : (current_role == 2 ? "leader" : "unknown")));
	strlcpy(role_buf, role_str, sizeof(role_buf));

	snprintf(response, response_size, "{\"nodes\":[");
	size_t pos = strlen(response);
	for (i = 0; i < librale_cluster_get_node_count(); i++) {
		/* Note: librale_node_t is incomplete, so we'll provide basic cluster info */
		const char *n_role = ((int32_t)i == self_id) ? role_buf : "unknown";
		int w = snprintf(response + pos, response_size - pos,
			"%s{\"id\":%d,\"name\":\"node%d\",\"ip\":\"unknown\",\"rale_port\":0,\"dstore_port\":0,\"role\":\"%s\"}",
			(pos > 10 ? "," : ""), (int)i, (int)i, n_role);
		if (w < 0 || (size_t)w >= response_size - pos)
			break;
		pos += (size_t)w;
	}
	if (pos < response_size - 2) {
		strlcat(response, "]}", response_size);
	}
	return RALE_SUCCESS;
}

static librale_status_t
process_status_command(char *response, size_t response_size)
{
	/* Get basic status using available API */
	int32_t current_role = librale_get_current_role();
	int32_t self_id = librale_cluster_get_self_id();
	uint32_t node_count = librale_cluster_get_node_count();
	
	const char *role_str = (current_role == 0 ? "follower" : 
							(current_role == 1 ? "candidate" : 
							 (current_role == 2 ? "leader" : "unknown")));
	
	snprintf(response, response_size, 
		"STATUS: node_id=%d, role=%s, cluster_size=%u", 
		self_id, role_str, node_count);
	return RALE_SUCCESS;
}

static librale_status_t
process_stop_command(char *response, size_t response_size)
{
	/* Note: No shutdown API available, would need to be implemented */
	raled_log_info("Stop command received - daemon shutdown would be handled externally.");
	snprintf(response, response_size, "OK: stop command received");
	return RALE_SUCCESS;
}

static librale_status_t
process_add_command(int node_id, const char *name, const char *ip, int rale_port, int dstore_port, char *response, size_t response_size)
{
	/* Note: No add_node API available, would need to be implemented */
	raled_log_info("ADD command received for node \"%d\" (\"%s\"@\"%s\":\"%d\"/\"%d\").", 
		node_id, name, ip, rale_port, dstore_port);
	snprintf(response, response_size, "ERROR: ADD command not implemented in current API");
	return RALE_ERROR_GENERAL;
}

static librale_status_t
process_remove_command(int node_id, char *response, size_t response_size)
{
	/* Note: No remove_node API available, would need to be implemented */
	raled_log_info("REMOVE command received for node \"%d\".", node_id);
	snprintf(response, response_size, "ERROR: REMOVE command not implemented in current API");
	return RALE_ERROR_GENERAL;
}
