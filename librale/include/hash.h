/*-------------------------------------------------------------------------
 *
 * hash.h
 *		Hash table interface.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		librale/include/hash.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALE_HASH_H
#define RALE_HASH_H

/** System headers */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Local headers */
#include "config.h"

/** Hash table constants */
#define HASH_SIZE		1024
#define MAX_KEY_SIZE	255
#define MAX_VALUE_SIZE	1024

/** Hash table iteration macros */
#define HASH_ITERATE(table, entry, bucket) \
	for ((bucket) = 0; (bucket) < HASH_SIZE; (bucket)++) \
		for ((entry) = (table)->entries[bucket]; (entry); (entry) = (entry)->next)

#define HASH_ENTRY_FOREACH(entry, next, bucket) \
	for (; (entry); (entry) = (next)) \
		if ((next = (entry)->next), 1)

/** Hash entry structure */
typedef struct hash_entry_t
{
	char				key[MAX_KEY_SIZE];		/** Key string */
	char				value[MAX_VALUE_SIZE];	/** Value string */
	struct hash_entry_t  *next;					/** Next entry in chain */
} hash_entry_t;

/** Hash table structure */
typedef struct hash_table_t
{
	hash_entry_t		*entries[HASH_SIZE];		/** Array of entry pointers */
	pthread_mutex_t	mutex;						/** Mutex for thread safety */
} hash_table_t;

/** Function declarations */
static inline unsigned int hash_func(const char *key);
int hash_init(hash_table_t *table, char *errbuf, size_t errbuflen);
int hash_destroy(hash_table_t *table, char *errbuf, size_t errbuflen);
int hash_put(hash_table_t *table, const char *key, const char *value, char *errbuf, size_t errbuflen);
int hash_get(hash_table_t *table, const char *key, char *value, size_t value_size, char *errbuf, size_t errbuflen);
int hash_delete(hash_table_t *table, const char *key, char *errbuf, size_t errbuflen);
int hash_save(hash_table_t *table, const char *filename, char *errbuf, size_t errbuflen);
int hash_load(hash_table_t *table, const char *filename, char *errbuf, size_t errbuflen);

#endif							/* RALE_HASH_H */
