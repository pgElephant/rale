/*-------------------------------------------------------------------------
 *
 * assert.c
 *     Simple assertion system implementation for librale
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *     librale/src/assert.c
 *
 *-------------------------------------------------------------------------
 */

#include "assert.h"
#include "rale_error.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void
librale_assert_fail(const char *file, int line, const char *func,
                     const char *expr)
{
	/* Set error for assertion failure */
	rale_set_error(RALE_ERROR_INTERNAL, "librale_assert_fail",
				   "Assertion failed", 
				   expr,
				   "Check the condition and fix the logic error");
	
	/* Debug log the assertion failure */
	rale_debug_log("Assertion failed at %s:%d in function %s(): %s", 
			       file, line, func, expr);
	
	/* In debug builds, we could abort here */
	#ifdef LIBRALE_DEBUG
		abort();
	#endif
} 
