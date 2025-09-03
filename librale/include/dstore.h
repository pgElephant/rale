/*-------------------------------------------------------------------------
 *
 * dstore.h
 *		Declarations for the distributed data store, including initialization,
 *		main loop, and replication logic. This is the primary public API
 *		for interacting with the dstore module.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef DSTORE_H
#define DSTORE_H

/** Local headers */
#include "librale.h"
#include "config.h"

/** Function declarations */
extern int dstore_init(const uint16_t dstore_port, const config_t *config);
extern int dstore_finit(char *errbuf, size_t errbuflen);

/* Tick functions for daemon-driven processing */
extern int dstore_server_tick(void);
extern int dstore_client_tick(void);

extern void dstore_replicate_to_followers(const char *key, const char *value, char *errbuf, size_t errbuflen);
extern void dstore_put_from_command(const char *command, char *errbuf, size_t errbuflen);
extern int dstore_handle_put(const char *key, const char *value, char *errbuf, size_t errbuflen);
extern int dstore_send_message(uint32_t target_node_idx, const char *message);

/** Propagation functions for automatic cluster management */
extern int dstore_propagate_node_addition(int32_t new_node_id, const char *name,
										const char *ip, uint16_t rale_port, uint16_t dstore_port);
extern int dstore_propagate_node_removal(int32_t node_id);

/** Leader status helpers (exposed for status reporting) */
int dstore_is_current_leader(void);
int dstore_get_current_leader(void);

/** Connectivity helper for status reporting */
int dstore_is_node_connected(int32_t node_id);

#endif /* DSTORE_H */
