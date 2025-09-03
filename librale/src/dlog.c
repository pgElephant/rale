/*-------------------------------------------------------------------------
 *
 * dlog.c
 *    Distributed store log replication and compaction for RALE.
 *
 *    Provides per-node log management, append, access, commit index,
 *    and log compaction (condense) for consensus protocols.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/dlog.c
 *
 *-------------------------------------------------------------------------
 */

/** System headers */
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/** Local headers */
#include "dlog.h"
#include "rale_error.h"
#include "constants.h"
#include "util.h"

/** Constants */
#define MODULE                       "DSTORE"
#define MAX_NODES                    10
#define MAX_LOG_ENTRIES              4096

/** Per-node log structure */
typedef struct node_log_t
{
	log_entry_t entries[MAX_LOG_ENTRIES]; /** Array of log entries */
	int      entry_count;             /** Number of entries in log */
	int      commit_index;            /** Current commit index */
} node_log_t;

/** Static variables */
static node_log_t node_logs[MAX_NODES];
static int node_id_map[MAX_NODES];
static int node_map_count = 0;
static pthread_mutex_t dlog_mutex = PTHREAD_MUTEX_INITIALIZER;

static int get_node_index(int node_id)
{
	int i;
	int result;
	
	pthread_mutex_lock(&dlog_mutex);
	
	for (i = 0; i < node_map_count; i++)
	{
		if (node_id_map[i] == node_id)
		{
			result = i;
			pthread_mutex_unlock(&dlog_mutex);
			return result;
		}
	}
	if (node_map_count < MAX_NODES)
	{
		node_id_map[node_map_count] = node_id;
		result = node_map_count++;
		pthread_mutex_unlock(&dlog_mutex);
		return result;
	}
	
	pthread_mutex_unlock(&dlog_mutex);
	return -1;
}

int
dlog_init(void)
{
	int i;
	int j;

	pthread_mutex_lock(&dlog_mutex);
	
	for (i = 0; i < MAX_NODES; i++)
	{
		node_logs[i].entry_count = 0;
		node_logs[i].commit_index = 0;
		for (j = 0; j < MAX_LOG_ENTRIES; j++)
		{
			node_logs[i].entries[j].term = 0;
			node_logs[i].entries[j].entry = NULL;
		}
	}
	
	pthread_mutex_unlock(&dlog_mutex);
	return 0;
}

int
dlog_finit(void)
{
	int i;
	int j;

	pthread_mutex_lock(&dlog_mutex);
	
	for (i = 0; i < MAX_NODES; i++)
	{
		for (j = 0; j < node_logs[i].entry_count; j++)
		{
			if (node_logs[i].entries[j].entry != NULL)
			{
				rfree((void **) &node_logs[i].entries[j].entry);
			}
		}
		node_logs[i].entry_count = 0;
		node_logs[i].commit_index = 0;
	}
	
	pthread_mutex_unlock(&dlog_mutex);
	return 0;
}

int
dlog_append_entry(int node_id, int term, const char *entry)
{
	int idx;
	int node_index = get_node_index(node_id);

	if (node_index < 0 || entry == NULL)
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_NODE_ID, "dlog_append_entry",
		                   "Invalid node_id or entry in dlog_append_entry");
		return -1;
	}
	
	pthread_mutex_lock(&dlog_mutex);
	
	idx = node_logs[node_index].entry_count;
	if (idx >= MAX_LOG_ENTRIES)
	{
		pthread_mutex_unlock(&dlog_mutex);
		rale_set_error_fmt(RALE_ERROR_OUT_OF_MEMORY, "dlog_append_entry",
		                   "Log full for node %d", node_id);
		return -1;
	}
	node_logs[node_index].entries[idx].term = term;
	node_logs[node_index].entries[idx].entry = rstrdup(entry);
	if (node_logs[node_index].entries[idx].entry == NULL)
	{
		pthread_mutex_unlock(&dlog_mutex);
		rale_set_error_fmt(RALE_ERROR_OUT_OF_MEMORY, "dlog_append_entry",
		                   "Memory allocation failed in dlog_append_entry");
		return -1;
	}
	node_logs[node_index].entry_count++;
	
	pthread_mutex_unlock(&dlog_mutex);
	return 0;
}

/**
 * dlog_put
 *
 * Append a new value to the log for node 0 (for simple key-value replication).
 */
int
dlog_put(const char *key __attribute__((unused)), const char *value)
{
	/** For now, ignore key and just append to node 0 */
	return dlog_append_entry(0, 0, value);
}

/**
 * dlog_get
 *
 * Get the latest value for node 0 (simulate key-value for now).
 */
int
dlog_get(const char *key __attribute__((unused)), char *value, size_t value_size)
{
	int idx;

	if (value == NULL || value_size == 0)
	{
		return -1;
	}
	
	pthread_mutex_lock(&dlog_mutex);
	
	idx = node_logs[0].entry_count;
	if (idx == 0)
	{
		pthread_mutex_unlock(&dlog_mutex);
		return -1;
	}
	strncpy(value, node_logs[0].entries[idx - 1].entry, value_size - 1);
	value[value_size - 1] = '\0';
	
	pthread_mutex_unlock(&dlog_mutex);
	return 0;
}

/**
 * dlog_compact
 *
 * Remove log entries up to up_to_index (exclusive) for a node.
 */
int
dlog_compact(int node_id, int up_to_index)
{
	int      i;
	int      remaining;
	node_log_t *log;
	int      node_index = get_node_index(node_id);

	if (node_index < 0)
	{
		return -1;
	}
	
	pthread_mutex_lock(&dlog_mutex);
	
	log = &node_logs[node_index];
	if (up_to_index > log->entry_count)
	{
		up_to_index = log->entry_count;
	}
	for (i = 0; i < up_to_index; i++)
	{
		if (log->entries[i].entry != NULL)
		{
			rfree((void **) &log->entries[i].entry);
		}
	}
	remaining = log->entry_count - up_to_index;
	for (i = 0; i < remaining; i++)
	{
		log->entries[i] = log->entries[i + up_to_index];
	}
	log->entry_count = remaining;
	log->commit_index = 0;
	
	pthread_mutex_unlock(&dlog_mutex);
	
	rale_debug_log("Log compacted for node %d, new count %d", node_id, remaining);
	return 0;
} 
