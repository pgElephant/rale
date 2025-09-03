/*-------------------------------------------------------------------------
 *
 * validation.h
 *    Header definitions for validation.h
 *
 * Shared library for RALE (Reliable Automatic Log Engine) component.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/include/validation.h
 *
 *------------------------------------------------------------------------- */

/*-------------------------------------------------------------------------
 *
 * validation.h
 *		Comprehensive input validation functions for librale.
 *
 *		This module provides robust validation for all types of inputs
 *		including pointers, strings, network addresses, file paths, and
 *		numeric ranges. All validation functions return LIBRALE_SUCCESS
 *		on success and LIBRALE_ERROR on failure.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		librale/include/validation.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef VALIDATION_H
#define VALIDATION_H

#include <stddef.h>

/**
 * Validation constants
 */
#define MAX_REASONABLE_JSON_LEN		(64 * 1024)		/** 64KB max JSON */
#define MAX_REASONABLE_TIMEOUT_MS	(5 * 60 * 1000)	/** 5 minutes */

/**
 * Core validation functions - return LIBRALE_SUCCESS on success
 */

/** Pointer validation */
int validate_pointer(const void *ptr, const char *name);

/** String validation */
int validate_json_string(const char *str, const char *name);
int validate_node_name(const char *name, const char *param_name);

/** Network validation */
int validate_node_id(int id, const char *name);
int validate_ip_address(const char *ip, const char *name);
int validate_port(int port, const char *name);

/** File system validation */
int validate_file_path(const char *path, const char *name);

/** Numeric validation */
int validate_buffer_size(size_t size, size_t min_size, size_t max_size);
int validate_timeout(int timeout_ms);

#endif							/* VALIDATION_H */ 
