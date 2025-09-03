/*-------------------------------------------------------------------------
 *
 * hash.c
 *    Implementation for hash.c
 *
 * Shared library for RALE (Reliable Automatic Log Engine) component.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/hash.c
 *
 *------------------------------------------------------------------------- */

/*-------------------------------------------------------------------------
 *
 * hash.c
 *    Simple hash table implementation for librale.
 *
 *    This hash table uses chained hashing to resolve collisions. It provides
 *    basic operations like put, get, delete, and also supports saving to
 *    and loading from a file. Thread safety is managed via a mutex.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/hash.c
 *
 *-------------------------------------------------------------------------
 */

/** System headers */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Local headers */
#include "librale_internal.h"

/** Constants */
#define MODULE "DSTORE"

static unsigned int
hash_func(const char *key)
{
	unsigned int hash = 5381;
	int          c;

	while ((c = *key++))
	{
		hash = ((hash << 5) + hash) + (unsigned int)c; /** hash * 33 + c */
	}
	return hash % HASH_SIZE;
}

/** Hash function to compute the bucket index */
static unsigned int
hash(const char *key)
{
	unsigned int hash_val = 0;

	while (*key)
	{
		hash_val = (hash_val << 5) + (unsigned int)*key++;
	}

	return hash_val % HASH_SIZE;
}

int
hash_init(hash_table_t *table, char *errbuf, size_t errbuflen)
{
	int i;

	if (table == NULL)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Invalid table parameter");
		}
		return -1;
	}
	for (i = 0; i < HASH_SIZE; i++)
	{
		table->entries[i] = NULL;
	}
	if (pthread_mutex_init(&table->mutex, NULL) != 0)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Failed to initialize mutex");
		}
		return -1;
	}
	return 0;
}

int
hash_put(hash_table_t *table, const char *key, const char *value, char *errbuf,
		 size_t errbuflen)
{
	unsigned int index;
	hash_entry_t  *entry;
	hash_entry_t  *new_entry;

	if (table == NULL || key == NULL || value == NULL)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Invalid parameters for hash_put");
		}
		return -1;
	}
	if (strlen(key) >= MAX_KEY_SIZE || strlen(value) >= MAX_VALUE_SIZE)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Key or value too long");
		}
		return -1;
	}
	index = hash(key);
	pthread_mutex_lock(&table->mutex);
	for (entry = table->entries[index]; entry != NULL; entry = entry->next)
	{
		if (strcmp(entry->key, key) == 0)
		{
			strlcpy(entry->value, value, MAX_VALUE_SIZE);
			pthread_mutex_unlock(&table->mutex);
			return 0;
		}
	}
	new_entry = rmalloc(sizeof(hash_entry_t));
	if (new_entry == NULL)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Memory allocation failed");
		}
		pthread_mutex_unlock(&table->mutex);
		return -1;
	}
	strlcpy(new_entry->key, key, MAX_KEY_SIZE);
	strlcpy(new_entry->value, value, MAX_VALUE_SIZE);
	new_entry->next = table->entries[index];
	table->entries[index] = new_entry;
	pthread_mutex_unlock(&table->mutex);
	return 0;
}

int
hash_get(hash_table_t *table, const char *key, char *value, size_t value_size,
		 char *errbuf, size_t errbuflen)
{
	unsigned int hash_val;
	hash_entry_t  *entry;

	if (table == NULL || key == NULL || value == NULL || value_size == 0)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Invalid parameters for hash_get");
		}
		return -1;
	}
	pthread_mutex_lock(&table->mutex);
	hash_val = hash_func(key);
	entry = table->entries[hash_val];
	while (entry != NULL)
	{
		if (strcmp(entry->key, key) == 0)
		{
			strlcpy(value, entry->value, value_size);
			pthread_mutex_unlock(&table->mutex);
			return 0;
		}
		entry = entry->next;
	}
	pthread_mutex_unlock(&table->mutex);
	if (errbuf != NULL && errbuflen > 0)
	{
		snprintf(errbuf, errbuflen, "Key not found");
	}
	return -1;
}

int
hash_delete(hash_table_t *table, const char *key, char *errbuf, size_t errbuflen)
{
	unsigned int index;
	hash_entry_t  *entry;
	hash_entry_t  *prev = NULL;

	if (table == NULL || key == NULL)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Invalid parameters for hash_delete");
		}
		return -1;
	}
	index = hash(key);
	pthread_mutex_lock(&table->mutex);
	for (entry = table->entries[index]; entry != NULL;
		 prev = entry, entry = entry->next)
	{
		if (strcmp(entry->key, key) == 0)
		{
			if (prev != NULL)
			{
				prev->next = entry->next;
			}
			else
			{
				table->entries[index] = entry->next;
			}
			if (entry != NULL)
			{
				rfree((void **) &entry);
			}
			pthread_mutex_unlock(&table->mutex);
			return 0;
		}
	}
	pthread_mutex_unlock(&table->mutex);
	if (errbuf != NULL && errbuflen > 0)
	{
		snprintf(errbuf, errbuflen, "Key not found for deletion");
	}
	return -1;
}

int
hash_destroy(hash_table_t *table, char *errbuf, size_t errbuflen)
{
	int         i;
	hash_entry_t  *entry;
	hash_entry_t  *next;

	if (table == NULL)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Invalid table parameter for hash_destroy");
		}
		return -1;
	}
	pthread_mutex_lock(&table->mutex);
	for (i = 0; i < HASH_SIZE; i++)
	{
		entry = table->entries[i];
		while (entry != NULL)
		{
			next = entry->next;
			if (entry != NULL)
			{
				rfree((void **) &entry);
			}
			entry = next;
		}
		table->entries[i] = NULL;
	}
	pthread_mutex_unlock(&table->mutex);
	if (pthread_mutex_destroy(&table->mutex) != 0)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Failed to destroy mutex");
		}
		return -1;
	}
	return 0;
}

int
hash_save(hash_table_t *table, const char *filename, char *errbuf, size_t errbuflen)
{
	FILE       *file;
	int         bucket;
	hash_entry_t  *entry;
	int         num_entries = 0;
	size_t         key_len;
	size_t         value_len;

	file = fopen(filename, "wb");
	if (file == NULL)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Failed to open file for saving: %s", filename);
		}
		return -1;
	}
	pthread_mutex_lock(&table->mutex);
	for (bucket = 0; bucket < HASH_SIZE; bucket++)
	{
		for (entry = table->entries[bucket]; entry != NULL; entry = entry->next)
		{
			num_entries++;
		}
	}
	if (fwrite(&num_entries, sizeof(int), 1, file) != 1)
	{
		if (file != NULL)
		{
			fclose(file);
			file = NULL;
		}
		pthread_mutex_unlock(&table->mutex);
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Failed to write entry count");
		}
		return -1;
	}
	for (bucket = 0; bucket < HASH_SIZE; bucket++)
	{
		for (entry = table->entries[bucket]; entry != NULL; entry = entry->next)
		{
			key_len = strlen(entry->key);
			value_len = strlen(entry->value);
			fwrite(&key_len, sizeof(int), 1, file);
			fwrite(entry->key, key_len, 1, file);
			fwrite(&value_len, sizeof(int), 1, file);
			fwrite(entry->value, value_len, 1, file);
		}
	}
	pthread_mutex_unlock(&table->mutex);
	if (file != NULL)
	{
		fclose(file);
		file = NULL;
	}
	return 0;
}

int
hash_load(hash_table_t *table, const char *filename, char *errbuf, size_t errbuflen)
{
	FILE *file;
	int   num_entries;
	int   i;
	size_t   key_len = 0;
	size_t   value_len = 0;
	char  key[MAX_KEY_SIZE];
	char  value[MAX_VALUE_SIZE];

	file = fopen(filename, "rb");
	if (file == NULL)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "File not found: %s", filename);
		}
		return -1;
	}
	pthread_mutex_lock(&table->mutex);
	if (fread(&num_entries, sizeof(int), 1, file) != 1)
	{
		if (file != NULL)
		{
			fclose(file);
			file = NULL;
		}
		pthread_mutex_unlock(&table->mutex);
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "Failed to read entry count");
		}
		return -1;
	}
	for (i = 0; i < num_entries; i++)
	{
		fread(&key_len, sizeof(int), 1, file);
		fread(key, key_len, 1, file);
		key[key_len] = '\0';
		fread(&value_len, sizeof(int), 1, file);
		fread(value, value_len, 1, file);
		value[value_len] = '\0';
		hash_put(table, key, value, NULL, 0);
	}
	pthread_mutex_unlock(&table->mutex);
	if (file != NULL)
	{
		fclose(file);
		file = NULL;
	}
	return 0;
}
