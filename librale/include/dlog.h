/*-------------------------------------------------------------------------
 *
 * dlog.h
 *    Header definitions for dlog.h
 *
 * Shared library for RALE (Reliable Automatic Log Engine) component.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/include/dlog.h
 *
 *------------------------------------------------------------------------- */

/*-------------------------------------------------------------------------
 *
 * dlog.h
 *		API for distributed store log replication and management.
 *
 *		This file declares functions for interacting with a per-node log.
 *		It includes operations for getting terms, indices, appending entries,
 *		and managing commit indices.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		librale/include/dlog.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef DLOG_H
#define DLOG_H

/** Log entry structure */
typedef struct log_entry_t
{
	int		term;					/** Term number */
	char   *entry;					/** Full log entry string */
} log_entry_t;

/** Function declarations */
void dstore_log_init(void);
void dstore_log_destroy(void);
void log_append_entry(int node_id, const char *log_entry);
char *log_get_entry_at_index(int node_id, int index);
int log_get_last_index(int node_id);
int log_get_term_at_index(int node_id, int index);
int log_get_current_term(int node_id);
void log_set_commit_index(int node_id, int new_commit_index);
int log_get_commit_index(int node_id);
int dstore_log_save_data(void);
int dstore_log_load_data(void);
int dlog_append_entry(int node_id, int term, const char *entry);
int dlog_init(void);
int dlog_finit(void);
int dlog_get(const char *key, char *value, size_t value_size);
int dlog_put(const char *key, const char *value);
int dlog_compact(int node_id, int up_to_index);

#endif							/* DLOG_H */
