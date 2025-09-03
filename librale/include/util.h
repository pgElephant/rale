/*-------------------------------------------------------------------------
 *
 * util.h
 *		Utility function declarations for the RALE system.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		librale/include/util.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef UTIL_H
#define UTIL_H

/** System headers */
#include <stddef.h>

/** Function declarations */

/** Basic utilities */
extern int get_current_timestamp(char *timestamp_buf, size_t buf_size);
extern char *trim_whitespace(char *str);
extern int file_exists(const char *filename);

/** Enhanced memory management */
extern void *rmalloc(size_t size);
extern int rfree(void **ptr);
extern char *rstrdup(const char *str);

/** Advanced memory safety functions */
extern void secure_memclear(void *ptr, size_t size);
extern void *safe_array_alloc(size_t count, size_t size);
extern int safe_string_append(char *dest, const char *src, size_t dest_size);

#endif							/* UTIL_H */
