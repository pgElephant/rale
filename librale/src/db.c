/*-------------------------------------------------------------------------
 *
 * db.c
 *		Database management functions for librale
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *------------------------------------------------------------------------- */

/** Local headers */
#include "librale_internal.h"

/** System headers */
#include <sys/stat.h>
#include <errno.h>

/** Constants */
#define SAFE_DB_FILE                 (global_cluster_db.db_file[0] ? global_cluster_db.db_file : "<unset>")
#define MODULE                       "DSTORE"
#define CLUSTER_DB_FILE              "rale.db"

/** Error codes for db.c operations */
#define DB_SUCCESS                   0
#define DB_INIT_NEW_EMPTY            0
#define DB_INIT_LOADED_OK            1
#define DB_ERR_GENERAL               -1
#define DB_ERR_NO_MEM                -2
#define DB_ERR_FILE_IO               -3
#define DB_ERR_REMOVE_FAILED         -4

/** Static variables */
static pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

/** Global variables */
cluster_db_t global_cluster_db;

/** Function declarations */
static int db_load_nolock(char *errbuf, size_t errbuflen);
static void db_destroy(void);

int
db_init(const config_t *config)
{
	int load_ret;

	if (config == NULL)
	{
		return DB_ERR_GENERAL;
	}

	/** Defensive check for db.path */
	if (config->db.path[0] == '\0' || strlen(config->db.path) == 0)
	{
		return DB_ERR_GENERAL;
	}

	/** Ensure the db path exists */
	if (mkdir(config->db.path, LIBRALE_DIR_PERMISSIONS) != 0 && errno != EEXIST)
	{
		return DB_ERR_FILE_IO;
	}

	pthread_mutex_lock(&db_mutex);

	/** Construct the full path to the cluster storage file */
	snprintf(global_cluster_db.db_file, sizeof(global_cluster_db.db_file), "%s/%s",
			 config->db.path, CLUSTER_DB_FILE);
	global_cluster_db.db_file[sizeof(global_cluster_db.db_file) - 1] = '\0';

	/** Allocate and initialize the hash table */
	global_cluster_db.hash_table = (hash_table_t *) rmalloc(sizeof(hash_table_t));
	if (global_cluster_db.hash_table == NULL)
	{
		pthread_mutex_unlock(&db_mutex);
		return DB_ERR_NO_MEM;
	}
	hash_init(global_cluster_db.hash_table, NULL, 0);

	/** Check if the cluster storage file exists */
	if (db_initialized(NULL, 0))
	{
		load_ret = db_load_nolock(NULL, 0);
		if (load_ret != DB_SUCCESS)
		{
			pthread_mutex_unlock(&db_mutex);
			return load_ret;
		}
		pthread_mutex_unlock(&db_mutex);
		return DB_INIT_LOADED_OK;
	}
	else
	{
		pthread_mutex_unlock(&db_mutex);
		return DB_INIT_NEW_EMPTY;
	}
}

/**
 * Internal function to load data from the database file into the hash table.
 */
static int
db_load_nolock(char *errbuf, size_t errbuflen)
{
	int			hash_load_ret;

	hash_load_ret = hash_load(global_cluster_db.hash_table, global_cluster_db.db_file, errbuf, errbuflen);
	if (hash_load_ret != 0)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "DB_ERR_FILE_IO: Failed to load cluster storage from file (%s).",
					 global_cluster_db.db_file);
		}
		return DB_ERR_FILE_IO;
	}
	else
	{
		return DB_SUCCESS;
	}
}

/**
 * Save the hash table to the storage file.
 */
int
db_save(char *errbuf, size_t errbuflen)
{
	int			hash_save_ret;

	pthread_mutex_lock(&db_mutex);

	hash_save_ret = hash_save(global_cluster_db.hash_table, global_cluster_db.db_file, errbuf, errbuflen);
	if (hash_save_ret != 0)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "DB_ERR_FILE_IO: Failed to save cluster storage to file (%s).",
					 global_cluster_db.db_file);
		}
		pthread_mutex_unlock(&db_mutex);
		return DB_ERR_FILE_IO;
	}

	pthread_mutex_unlock(&db_mutex);
	return DB_SUCCESS;
}

/**
 * Load the hash table from the storage file.
 */
int
db_load(char *errbuf, size_t errbuflen)
{
	int			ret;

	pthread_mutex_lock(&db_mutex);
	ret = db_load_nolock(errbuf, errbuflen);
	pthread_mutex_unlock(&db_mutex);
	return ret;
}

/**
 * Remove the cluster storage file.
 */
int
db_remove(char *errbuf, size_t errbuflen)
{
	int			ret;

	pthread_mutex_lock(&db_mutex);
			if (unlink(global_cluster_db.db_file) == -1)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "DB_ERR_REMOVE_FAILED: Failed to remove database file (%s): %s.",
					 global_cluster_db.db_file, strerror(errno));
		}
		ret = DB_ERR_REMOVE_FAILED;
	}
	else
	{
		ret = DB_SUCCESS;
	}
	pthread_mutex_unlock(&db_mutex);
	return ret;
}

/**
 * Check if the Cluster storage file is initialized.
 */
int
db_initialized(char *errbuf, size_t errbuflen)
{
	(void) errbuf;
	(void) errbuflen;

			if (access(global_cluster_db.db_file, F_OK) == 0)
	{
		return 1;
	}

	return 0;
}

/**
 * Retrieve a value by key from the cluster storage.
 */
int
db_get(const char *key, char *value, size_t value_size, char *errbuf, size_t errbuflen)
{
	int			get_ret;

	get_ret = hash_get(global_cluster_db.hash_table, key, value, value_size, errbuf, errbuflen);
	if (get_ret != 0)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Key not found");
		}
		return -1;
	}
	return 0;
}

/**
 * Insert a key-value pair into the cluster storage.
 */
int
db_insert(const char *key, const char *value, char *errbuf, size_t errbuflen)
{
	hash_put(global_cluster_db.hash_table, key, value, errbuf, errbuflen);
	return DB_SUCCESS;
}

/**
 * Remove a key-value pair from the cluster storage.
 */
int
db_delete(const char *key, char *errbuf, size_t errbuflen)
{
	hash_delete(global_cluster_db.hash_table, key, errbuf, errbuflen);
	return DB_SUCCESS;
}

/**
 * Finalize the cluster database.
 */
int
db_finit(char *errbuf, size_t errbuflen)
{
	(void) errbuf;
	(void) errbuflen;
	db_destroy();
	return 0;
}

/**
 * Destroy the cluster database and free all resources.
 */
static void
db_destroy(void)
{
	pthread_mutex_lock(&db_mutex);
	if (global_cluster_db.hash_table != NULL)
	{
		hash_destroy(global_cluster_db.hash_table, NULL, 0);
		rfree((void **) &global_cluster_db.hash_table);
		global_cluster_db.hash_table = NULL;
	}
	pthread_mutex_unlock(&db_mutex);
}
