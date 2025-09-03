/*-------------------------------------------------------------------------
 *
 * db.h
 *		Database management interface for librale.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef DB_H
#define DB_H

/** Local headers */
#include "config.h"
#include "hash.h"

/** Database structure */
typedef struct cluster_db_t
{
	char db_file[1024];
	hash_table_t *hash_table;
} cluster_db_t;

/** Function declarations */
int db_init(const config_t *config);
int db_finit(char *errbuf, size_t errbuflen);
int db_put(const char *key, const char *value, char *errbuf, size_t errbuflen);
int db_get(const char *key, char *value, size_t value_size, char *errbuf, size_t errbuflen);
int db_delete(const char *key, char *errbuf, size_t errbuflen);
int db_save(char *errbuf, size_t errbuflen);
int db_load(char *errbuf, size_t errbuflen);
int db_initialized(char *errbuf, size_t errbuflen);
int db_insert(const char *key, const char *value, char *errbuf, size_t errbuflen);

#endif /* DB_H */
