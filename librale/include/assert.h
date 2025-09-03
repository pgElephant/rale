/*-------------------------------------------------------------------------
 *
 * assert.h
 *		Simple assertion system for librale
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		librale/include/assert.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef LIBRALE_ASSERT_H
#define LIBRALE_ASSERT_H

#include "rale_error.h"
#include "macros.h"

/* Simple assertion macro - just takes a boolean condition */
#define Assert(condition) \
	do { \
		if (UNLIKELY(!(condition))) { \
			librale_assert_fail(__FILE__, __LINE__, __func__, #condition); \
		} \
	} while(0)

/* Internal assertion function */
void librale_assert_fail(const char *file, int line, const char *func,
						 const char *expr);

#endif							/* LIBRALE_ASSERT_H */ 
