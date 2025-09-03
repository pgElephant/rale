/*-------------------------------------------------------------------------
 *
 * validation.c
 *		Input validation functions.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

/** Local headers */
#include "validation.h"
#include "constants.h"
#include "macros.h"

/** System headers */
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>

/** Third-party headers */
#include <cjson/cJSON.h>

/**
 * Constants for validation limits
 */
#define MAX_HOSTNAME_LEN        253
#define MIN_VALID_PORT          1
#define MAX_VALID_PORT          65535
#define MAX_PATH_COMPONENT_LEN  255
#define MIN_NODE_NAME_LEN       1
#define MAX_NODE_NAME_LEN       64
#define MAX_JSON_DEPTH          10

/**
 * Validate a pointer is not NULL.
 *
 * @param ptr The pointer to validate
 * @param name Parameter name for error reporting (can be NULL)
 * @return LIBRALE_SUCCESS if valid, LIBRALE_ERROR if NULL
 */
int
validate_pointer(const void *ptr, const char *name __attribute__((unused)))
{
	if (IS_NULL(ptr))
	{
		return LIBRALE_ERROR;
	}
	return LIBRALE_SUCCESS;
}

/**
 * Validate a JSON string has proper basic structure using cJSON.
 *
 * @param str The JSON string to validate
 * @param name Parameter name for error reporting (can be NULL)
 * @return LIBRALE_SUCCESS if valid, LIBRALE_ERROR if invalid
 */
int
validate_json_string(const char *str, const char *name __attribute__((unused)))
{
	cJSON *json;

	if (IS_EMPTY_STRING(str))
	{
		return LIBRALE_ERROR;
	}

	if (strlen(str) > MAX_REASONABLE_JSON_LEN)
	{
		return LIBRALE_ERROR;
	}

	/** Parse JSON using cJSON */
	json = cJSON_Parse(str);
	if (json == NULL)
	{
		return LIBRALE_ERROR;
	}

	/** Clean up */
	cJSON_Delete(json);
	return LIBRALE_SUCCESS;
}

/**
 * Validate a node ID is within acceptable range.
 *
 * @param id The node ID to validate
 * @param name Parameter name for error reporting (can be NULL)
 * @return LIBRALE_SUCCESS if valid, LIBRALE_ERROR if invalid
 */
int
validate_node_id(int id, const char *name __attribute__((unused)))
{
	if (id < 0 || id > MAX_NODES)
	{
		return LIBRALE_ERROR;
	}
	return LIBRALE_SUCCESS;
}

/**
 * Validate an IP address (IPv4 or IPv6) or hostname.
 *
 * @param ip The IP address or hostname to validate
 * @param name Parameter name for error reporting (can be NULL)
 * @return LIBRALE_SUCCESS if valid, LIBRALE_ERROR if invalid
 */
int
validate_ip_address(const char *ip, const char *name __attribute__((unused)))
{
	struct sockaddr_in  sa4;
	struct sockaddr_in6 sa6;
	size_t             len;
	size_t             i;
	int                dot_count = 0;
	int                has_alpha = 0;

	if (IS_EMPTY_STRING(ip))
	{
		return LIBRALE_ERROR;
	}

	len = strlen(ip);
	if (len > MAX_HOSTNAME_LEN)
	{
		return LIBRALE_ERROR;
	}

	/** Try IPv4 first */
	if (inet_pton(AF_INET, ip, &(sa4.sin_addr)) == 1)
	{
		return LIBRALE_SUCCESS;
	}

	/** Try IPv6 */
	if (inet_pton(AF_INET6, ip, &(sa6.sin6_addr)) == 1)
	{
		return LIBRALE_SUCCESS;
	}

	/** Validate as hostname - basic rules */
	for (i = 0; i < len; i++)
	{
		if (ip[i] == '.')
		{
			dot_count++;
		}
		else if (isalpha((unsigned char)ip[i]))
		{
			has_alpha = 1;
		}
		else if (!isdigit((unsigned char)ip[i]) && ip[i] != '-' && ip[i] != '_')
		{
			return LIBRALE_ERROR; /** Invalid hostname character */
		}
	}

	/** If it looks like an IP but failed inet_pton, reject it */
	if (!has_alpha && dot_count == 3)
	{
		return LIBRALE_ERROR;
	}

	/** Basic hostname validation passed */
	return LIBRALE_SUCCESS;
}

/**
 * Validate a network port number.
 *
 * @param port The port number to validate
 * @param name Parameter name for error reporting (can be NULL)
 * @return LIBRALE_SUCCESS if valid, LIBRALE_ERROR if invalid
 */
int
validate_port(int port, const char *name __attribute__((unused)))
{
	if (port < MIN_VALID_PORT || port > MAX_VALID_PORT)
	{
		return LIBRALE_ERROR;
	}
	return LIBRALE_SUCCESS;
}

/**
 * Validate a file path for security and correctness.
 *
 * @param path The file path to validate
 * @param name Parameter name for error reporting (can be NULL)
 * @return LIBRALE_SUCCESS if valid, LIBRALE_ERROR if invalid
 */
int
validate_file_path(const char *path, const char *name __attribute__((unused)))
{
	size_t len;
	size_t i;
	int    component_len = 0;

	if (IS_EMPTY_STRING(path))
	{
		return LIBRALE_ERROR;
	}

	len = strlen(path);
	if (len >= PATH_MAX)
	{
		return LIBRALE_ERROR;
	}

	/** Security checks */
	if (strstr(path, "../") != NULL || strstr(path, "/..") != NULL)
	{
		return LIBRALE_ERROR; /** Path traversal attempt */
	}

	if (strstr(path, "//") != NULL)
	{
		return LIBRALE_ERROR; /** Double slashes */
	}

	/** Check path components */
	for (i = 0; i < len; i++)
	{
		if (path[i] == '/')
		{
			if (component_len > MAX_PATH_COMPONENT_LEN)
			{
				return LIBRALE_ERROR;
			}
			component_len = 0;
		}
		else
		{
			component_len++;
			/** Check for invalid characters */
			if (path[i] == '\0' || iscntrl((unsigned char)path[i]))
			{
				return LIBRALE_ERROR;
			}
		}
	}

	/** Check final component */
	if (component_len > MAX_PATH_COMPONENT_LEN)
	{
		return LIBRALE_ERROR;
	}

	return LIBRALE_SUCCESS;
}

/**
 * Validate a node name for cluster operations.
 *
 * @param name The node name to validate
 * @param param_name Parameter name for error reporting (can be NULL)
 * @return LIBRALE_SUCCESS if valid, LIBRALE_ERROR if invalid
 */
int
validate_node_name(const char *name, const char *param_name __attribute__((unused)))
{
	size_t len;
	size_t i;

	if (IS_EMPTY_STRING(name))
	{
		return LIBRALE_ERROR;
	}

	len = strlen(name);
	if (len < MIN_NODE_NAME_LEN || len > MAX_NODE_NAME_LEN)
	{
		return LIBRALE_ERROR;
	}

	/** Node names should start with alphanumeric */
	if (!isalnum((unsigned char)name[0]))
	{
		return LIBRALE_ERROR;
	}

	/** Check all characters */
	for (i = 0; i < len; i++)
	{
		if (!isalnum((unsigned char)name[i]) && name[i] != '_' && name[i] != '-')
		{
			return LIBRALE_ERROR;
		}
	}

	return LIBRALE_SUCCESS;
}

/**
 * Validate a buffer size is reasonable.
 *
 * @param size The buffer size to validate
 * @param min_size Minimum acceptable size
 * @param max_size Maximum acceptable size
 * @return LIBRALE_SUCCESS if valid, LIBRALE_ERROR if invalid
 */
int
validate_buffer_size(size_t size, size_t min_size, size_t max_size)
{
	if (size < min_size || size > max_size)
	{
		return LIBRALE_ERROR;
	}
	return LIBRALE_SUCCESS;
}

/**
 * Validate a timeout value is reasonable.
 *
 * @param timeout_ms Timeout in milliseconds
 * @return LIBRALE_SUCCESS if valid, LIBRALE_ERROR if invalid
 */
int
validate_timeout(int timeout_ms)
{
	if (timeout_ms < 0 || timeout_ms > MAX_REASONABLE_TIMEOUT_MS)
	{
		return LIBRALE_ERROR;
	}
	return LIBRALE_SUCCESS;
} 
