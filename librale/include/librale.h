/*-------------------------------------------------------------------------
 *
 * librale.h
 *		Public API for librale library.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef LIBRALE_H
#define LIBRALE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum librale_status
{
	RALE_SUCCESS = 0,
	RALE_ERROR_GENERAL = -1
} librale_status_t;

typedef struct librale_config_t librale_config_t;
typedef struct librale_node_t librale_node_t;

extern librale_config_t *librale_config_create(void);
extern void librale_config_destroy(librale_config_t *config);
extern librale_status_t librale_config_set_node_id(librale_config_t *config, int32_t node_id);
extern librale_status_t librale_config_set_node_name(librale_config_t *config, const char *name);
extern librale_status_t librale_config_set_node_ip(librale_config_t *config, const char *ip);
extern librale_status_t librale_config_set_dstore_port(librale_config_t *config, uint16_t port);
extern librale_status_t librale_config_set_rale_port(librale_config_t *config, uint16_t port);
extern librale_status_t librale_config_set_db_path(librale_config_t *config, const char *path);
extern librale_status_t librale_config_set_log_directory(librale_config_t *config, const char *path);
extern librale_status_t librale_config_set_config(librale_config_t *dest, const void *src);

extern int32_t librale_config_get_node_id(const librale_config_t *config);
extern const char *librale_config_get_node_name(const librale_config_t *config);
extern const char *librale_config_get_node_ip(const librale_config_t *config);
extern uint16_t librale_config_get_dstore_port(const librale_config_t *config);
extern uint16_t librale_config_get_rale_port(const librale_config_t *config);
extern const char *librale_config_get_db_path(const librale_config_t *config);
extern const char *librale_config_get_log_directory(const librale_config_t *config);

extern librale_status_t librale_config_set_dstore_keep_alive_interval(librale_config_t *config, uint32_t interval_seconds);
extern librale_status_t librale_config_set_dstore_keep_alive_timeout(librale_config_t *config, uint32_t timeout_seconds);

extern librale_status_t librale_dstore_init(uint16_t dstore_port, const librale_config_t *config);
extern librale_status_t librale_dstore_finit(char *errbuf, size_t errbuflen);

/* Non-blocking tick functions for daemon to call */
extern librale_status_t librale_dstore_server_tick(void);
extern librale_status_t librale_dstore_client_tick(void);
extern librale_status_t librale_unix_socket_tick(void);
extern librale_status_t librale_rale_tick(void);

extern void librale_dstore_put_from_command(const char *command, char *errbuf, size_t errbuflen);
extern void librale_dstore_replicate_to_followers(const char *key, const char *value, char *errbuf, size_t errbuflen);

extern librale_status_t librale_db_get(const char *key, char *value, size_t value_size, char *errbuf, size_t errbuflen);

extern uint32_t librale_cluster_get_node_count(void);
extern librale_status_t librale_cluster_get_node(int32_t node_id, librale_node_t *node);
extern int32_t librale_cluster_get_self_id(void);
extern librale_status_t librale_cluster_set_state_file(const char *path);

extern librale_status_t librale_rale_init(const librale_config_t *config);
extern librale_status_t librale_rale_finit(void);
extern int32_t librale_get_current_role(void);

/* Logging is handled by raled daemon, not librale */

#endif							/* LIBRALE_H */ 
