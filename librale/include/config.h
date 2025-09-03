/*-------------------------------------------------------------------------
 *
 * config.h
 *		Configuration structures for librale.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_STRING_LENGTH 255
#define MAX_LONG_STRING_LENGTH 1024
#define MAX_NODES 10
#define MAX_LINE_LENGTH 512

typedef enum protocol
{
	PROTOCOL_UNIX
} protocol_t;

typedef enum log_destination
{
	LOG_DEST_STDERR,
	LOG_DEST_STDOUT,
	LOG_DEST_FILE,
	LOG_DEST_SYSLOG
} log_destination_t;

typedef struct node_config
{
	char				name[MAX_STRING_LENGTH];
	int32_t				id;
	char				socket[MAX_STRING_LENGTH];
	char				ip[MAX_STRING_LENGTH];
	int32_t				priority;
	uint16_t			rale_port;
	uint16_t			dstore_port;
	char				db_path[MAX_STRING_LENGTH];
	char				log_directory[MAX_STRING_LENGTH];
} node_config_t;
typedef struct log_config
{
	log_destination_t	destination;
	char				file[MAX_STRING_LENGTH];
	int					level;
	uint32_t			rotation_size;
	uint32_t			rotation_age;
} log_config_t;

typedef struct communication_config
{
	protocol_t			protocol;	/* Communication protocol (UNIX socket) */
	uint32_t			timeout;	/* Communication timeout in seconds */
	uint32_t			max_retries; /* Maximum number of retry attempts */
	char				socket[MAX_STRING_LENGTH]; /* Unix socket path */
	log_config_t		log;		/* Logging configuration */
} communication_config_t;

typedef struct database_config
{
	char				path[MAX_STRING_LENGTH];
	uint32_t			max_size;
	uint32_t			max_connections;
} database_config_t;

typedef struct dstore_config
{
	uint32_t			keep_alive_interval;
	uint32_t			keep_alive_timeout;
} dstore_config_t;

typedef struct config_t
{
	database_config_t	db;
	node_config_t		node;
	log_config_t		raled_log;
	log_config_t		dstore_log;
	dstore_config_t		dstore;
	communication_config_t communication;
	char				log_directory[MAX_LONG_STRING_LENGTH];
} config_t;

#endif							/* CONFIG_H */
