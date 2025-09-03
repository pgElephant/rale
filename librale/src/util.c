/*-------------------------------------------------------------------------
 *
 * util.c
 *    Utility functions for the RALE system.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/util.c
 *
 *-------------------------------------------------------------------------
 */

/** System headers */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** Local headers */
#include "librale_internal.h"
#include "rale_error.h"

int
get_current_timestamp(char *timestamp_buf, size_t buf_size)
{
	time_t      now;
	struct tm  *tm_info;

	now = time(NULL);
	if (now == (time_t)(-1) || timestamp_buf == NULL || buf_size < 20)
	{
		return -1;
	}

	tm_info = localtime(&now);
	if (tm_info == NULL)
	{
		return -1;
	}

	if (strftime(timestamp_buf, buf_size, "%Y-%m-%d %H:%M:%S", tm_info) == 0)
	{
		return -1;
	}
	return 0;
}

char *
trim_whitespace(char *str)
{
	char *end;

	if (str == NULL)
	{
		return NULL;
	}

	/** Trim leading space */
	while (isspace((unsigned char)*str))
	{
		str++;
	}

	if (*str == 0) /** All spaces? */
	{
		return str;
	}

	/** Trim trailing space */
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
	{
		end--;
	}

	/** Null terminate after the last non-space character */
	*(end + 1) = '\0';

	return str;
}

int
file_exists(const char *filename)
{
	FILE *file;

	if (filename == NULL)
	{
		return -1;
	}

	file = fopen(filename, "r");
	if (file)
	{
		fclose(file);
		return 1; /** File exists */
	}
	return 0; /** File does not exist */
}

/**
 * Robust malloc wrapper with comprehensive safety checks.
 *
 * This function provides a safe malloc replacement with:
 * - Size validation to prevent integer overflow
 * - Zero-initialization for security
 * - Detailed error reporting
 * - Memory alignment guarantees
 *
 * @param size The size of the memory to allocate (must be > 0)
 * @return A pointer to zero-initialized memory, or NULL on error
 */
void *
rmalloc(size_t size)
{
	void *ptr;

	/** Validate size parameter */
	if (size == 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "rmalloc",
					   "Attempted to allocate zero bytes",
					   "size parameter is zero",
					   "Use a non-zero size for memory allocation");
		return NULL;
	}

	/** Check for unreasonably large allocations */
	if (size > (SIZE_MAX / 2))
	{
		rale_set_error(RALE_ERROR_RESOURCE_LIMIT, "rmalloc",
					   "Allocation size too large",
					   "Requested size exceeds reasonable limits",
					   "Reduce allocation size or use streaming approach");
		return NULL;
	}

	/** Allocate and zero-initialize memory */
	ptr = calloc(1, size);
	if (ptr == NULL)
	{
		rale_set_error_errno(RALE_ERROR_OUT_OF_MEMORY, "rmalloc",
							 "Memory allocation failed",
							 "calloc() returned NULL",
							 "Check system memory availability", errno);
		return NULL;
	}

	rale_debug_log("Allocated %zu bytes at %p", size, ptr);
	return ptr;
}

/**
 * Robust free wrapper with comprehensive safety checks.
 *
 * This function provides safe memory deallocation with:
 * - Double-free protection
 * - NULL pointer safety
 * - Automatic pointer nullification
 * - Debug tracking
 * - Memory corruption detection hints
 *
 * @param ptr A pointer to a pointer to the memory to free
 * @return 0 on success, -1 if ptr is NULL
 */
int
rfree(void **ptr)
{
	/** Validate the pointer to pointer */
	if (ptr == NULL)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, "rfree",
					   "rfree called with NULL pointer-to-pointer",
					   "ptr parameter is NULL",
					   "Pass a valid pointer to a pointer");
		return -1;
	}

	/** Check if the actual pointer is NULL (not an error) */
	if (*ptr == NULL)
	{
		rale_debug_log("rfree called with already-NULL pointer");
		return 0;
	}

	rale_debug_log("Freeing memory at %p", *ptr);

	/** Free the memory and nullify the pointer */
	free(*ptr);
	*ptr = NULL;

	return 0;
}

/**
 * Robust string duplication with enhanced safety.
 *
 * This function provides safe string duplication with:
 * - NULL input handling
 * - Length validation
 * - Memory allocation error handling
 * - Explicit null termination
 *
 * @param str The string to duplicate (can be NULL)
 * @return A pointer to the newly allocated duplicate, or NULL on error
 */
char *
rstrdup(const char *str)
{
	char   *dup_str;
	size_t  len;

	/** Handle NULL input gracefully */
	if (str == NULL)
	{
		rale_debug_log("rstrdup called with NULL string");
		return NULL;
	}

	/** Validate string length */
	len = strlen(str);
	if (len > (SIZE_MAX / 2))
	{
		rale_set_error(RALE_ERROR_RESOURCE_LIMIT, "rstrdup",
					   "String too long for duplication",
					   "String length exceeds reasonable limits",
					   "Use streaming or chunked processing for large strings");
		return NULL;
	}

	/** Allocate memory for the duplicate */
	dup_str = rmalloc(len + 1);
	if (dup_str == NULL)
	{
		/* Error already set by rmalloc */
		return NULL;
	}

	/** Copy string and ensure null termination */
	memcpy(dup_str, str, len);
	dup_str[len] = '\0';

	rale_debug_log("Duplicated string of %zu chars", len);
	return dup_str;
}

/**
 * Secure memory clearing function.
 *
 * This function securely clears memory to prevent sensitive data
 * from remaining in memory after deallocation. Uses volatile
 * operations to prevent compiler optimization.
 *
 * @param ptr Pointer to memory to clear
 * @param size Number of bytes to clear
 */
void
secure_memclear(void *ptr, size_t size)
{
	volatile unsigned char *vptr = (volatile unsigned char *)ptr;
	size_t i;

	if (ptr == NULL || size == 0)
	{
		return;
	}

	/** Use volatile to prevent compiler optimization */
	for (i = 0; i < size; i++)
	{
		vptr[i] = 0;
	}

	/** Memory barrier to ensure clearing is complete */
	__asm__ __volatile__("" ::: "memory");
}

/**
 * Safe array allocation with bounds checking.
 *
 * Allocates memory for an array with overflow checking.
 *
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to allocated array, or NULL on error
 */
void *
safe_array_alloc(size_t count, size_t size)
{
	size_t total_size;

	/** Check for multiplication overflow */
	if (count > 0 && size > (SIZE_MAX / count))
	{
		rale_set_error(RALE_ERROR_BUFFER_OVERFLOW, "safe_array_alloc",
					   "Array allocation overflow detected",
					   "count * size would overflow",
					   "Reduce array size or element count");
		return NULL;
	}

	total_size = count * size;
	return rmalloc(total_size);
}

/**
 * Safe string concatenation with bounds checking.
 *
 * @param dest Destination buffer
 * @param src Source string
 * @param dest_size Total size of destination buffer
 * @return 0 on success, -1 on error
 */
int
safe_string_append(char *dest, const char *src, size_t dest_size)
{
	size_t dest_len;
	size_t src_len;
	size_t available;

	if (dest == NULL || src == NULL || dest_size == 0)
	{
		return -1;
	}

	dest_len = strlen(dest);
	src_len = strlen(src);

	/** Check if we have enough space */
	if (dest_len >= dest_size)
	{
		return -1; /** Destination already too long */
	}

	available = dest_size - dest_len - 1; /** Reserve space for null terminator */

	if (src_len > available)
	{
		/** Truncate the source string */
		strncat(dest, src, available);
	}
	else
	{
		strncat(dest, src, src_len);
	}

	/** Ensure null termination */
	dest[dest_size - 1] = '\0';
	return 0;
}
