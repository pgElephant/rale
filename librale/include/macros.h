/*-------------------------------------------------------------------------
 *
 * macros.h
 *		Essential macros for librale
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef LIBRALE_MACROS_H
#define LIBRALE_MACROS_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "config.h"
#include "constants.h"

/* Null pointer checks */
#define IS_NULL(ptr)			((ptr) == NULL)
#define IS_NOT_NULL(ptr)		((ptr) != NULL)

/* String validation */
#define IS_EMPTY_STRING(str)	(IS_NULL(str) || (str)[0] == '\0')
#define IS_VALID_STRING(str)	(IS_NOT_NULL(str) && (str)[0] != '\0')

/* Array and buffer operations */
#define ARRAY_SIZE(arr)			(sizeof(arr) / sizeof((arr)[0]))
#define BUFFER_SIZE(buf)		sizeof(buf)
#define ZERO_BUFFER(buf)		memset((buf), 0, sizeof(buf))
#define ZERO_STRUCT(ptr)		memset((ptr), 0, sizeof(*(ptr)))

/* Range validation */
#define IN_RANGE(val, min, max) ((val) >= (min) && (val) <= (max))

/* Port validation */
#define IS_VALID_PORT(port)		IN_RANGE(port, LIBRALE_MIN_PORT, LIBRALE_MAX_PORT)

/* Safe memory allocation */
#define SAFE_MALLOC(ptr, size) do { \
	(ptr) = rmalloc(size); \
	if (IS_NULL(ptr)) return LIBRALE_ERROR_MEMORY; \
	ZERO_BUFFER(ptr); \
} while(0)

#define SAFE_FREE(ptr) do { \
	if (IS_NOT_NULL(ptr)) { \
		rfree(ptr); \
		(ptr) = NULL; \
	} \
} while(0)

/* String duplication */
#define SAFE_STRDUP(dest, src) do { \
	(dest) = rstrdup(src); \
	if (IS_NULL(dest)) return LIBRALE_ERROR_MEMORY; \
} while(0)

/* Simplified logging */
#define LOG_ERROR_MSG(module, msg)		elog(LOG_ERROR, module, msg)
#define LOG_WARNING_MSG(module, msg)	elog(LOG_WARNING, module, msg)
#define LOG_INFO_MSG(module, msg)		elog(LOG_INFO, module, msg)
#define LOG_DEBUG_MSG(module, msg)		elog(LOG_DEBUG, module, msg)

/* Formatted logging */
#define LOG_ERROR_FMT(module, fmt, ...)		elog(LOG_ERROR, module, fmt, __VA_ARGS__)
#define LOG_WARNING_FMT(module, fmt, ...)	elog(LOG_WARNING, module, fmt, __VA_ARGS__)
#define LOG_INFO_FMT(module, fmt, ...)		elog(LOG_INFO, module, fmt, __VA_ARGS__)
#define LOG_DEBUG_FMT(module, fmt, ...)		elog(LOG_DEBUG, module, fmt, __VA_ARGS__)

/* Socket validation */
#define IS_VALID_SOCKET(sock)   ((sock) >= 0)
#define IS_INVALID_SOCKET(sock) ((sock) < 0)

/* Address formatting */
#define FORMAT_ADDR(buf, ip, port) \
    snprintf(buf, sizeof(buf), "%s:%d", ip, port)

/* Configuration field copying */
#define COPY_CONFIG_STRING(dest, src, field) do { \
    if (IS_VALID_STRING((src)->field)) { \
        strlcpy((dest)->field, (src)->field, sizeof((dest)->field)); \
    } \
} while(0)

#define COPY_CONFIG_INT(dest, src, field) \
    (dest)->field = (src)->field

/* Time operations */
#define CURRENT_TIME()          time(NULL)
#define TIME_DIFF(start, end)   ((end) - (start))
#define IS_TIMEOUT(start, timeout) (TIME_DIFF(start, CURRENT_TIME()) > (timeout))

/* Node validation */
#define IS_VALID_NODE_ID(id)    IN_RANGE(id, 0, MAX_NODES - 1)

/* Cluster state checks */
#define IS_CLUSTER_INITIALIZED() (cluster.node_count > 0)

/* Leadership checks */
#define IS_LEADER() (current_rale_state.role == rale_role_leader)
#define IS_FOLLOWER() (current_rale_state.role == rale_role_follower)
#define IS_CANDIDATE() (current_rale_state.role == rale_role_candidate)

/* Safe string operations */
#define SAFE_STRNCPY(dest, src, size) do { \
    strlcpy(dest, src, size); \
} while(0)

#define SAFE_SNPRINTF(buf, fmt, ...) \
    snprintf(buf, sizeof(buf), fmt, __VA_ARGS__)

/* Safe iteration over cluster nodes */
#define FOR_EACH_NODE(i) \
    for (int i = 0; i < cluster.node_count; i++)

#define FOR_EACH_ACTIVE_NODE(i) \
    for (int i = 0; i < cluster.node_count; i++) \
        if (cluster.nodes[i].id != -1)

/* Debug builds */
#ifdef DEBUG
#define DEBUG_ONLY(code) code
#define DEBUG_LOG(module, fmt, ...) LOG_DEBUG_FMT(module, fmt, __VA_ARGS__)
#else
#define DEBUG_ONLY(code)
#define DEBUG_LOG(module, fmt, ...)
#endif

/* Thread safety */
#if LIBRALE_THREAD_SAFE
#define THREAD_SAFE_ONLY(code) code
#else
#define THREAD_SAFE_ONLY(code)
#endif

/* Likely/unlikely for branch prediction */
#ifdef __GNUC__
#define LIKELY(x)       __builtin_expect(!!(x), 1)
#define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)       (x)
#define UNLIKELY(x)     (x)
#endif

/* Function attributes */
#ifdef __GNUC__
#define INLINE          __inline__
#define FORCE_INLINE    __attribute__((always_inline)) inline
#define NO_INLINE       __attribute__((noinline))
#define UNUSED_PARAM    __attribute__((unused))
#define PURE_FUNCTION   __attribute__((pure))
#else
#define INLINE          inline
#define FORCE_INLINE    inline
#define NO_INLINE
#define UNUSED_PARAM
#define PURE_FUNCTION
#endif

/* Resource cleanup macros */
#define SAFE_CLOSE_FD(fd) \
    do { \
        if ((fd) >= 0) { \
            close(fd); \
            (fd) = -1; \
        } \
    } while (0)

#define SAFE_CLOSE_FILE(fp) \
    do { \
        if ((fp) != NULL) { \
            fclose(fp); \
            (fp) = NULL; \
        } \
    } while (0)

/* Numeric utilities */
#define IS_POWER_OF_TWO(x)      (((x) > 0) && (((x) & ((x) - 1)) == 0))
#define CLAMP(x, min_val, max_val) (((x) < (min_val)) ? (min_val) : (((x) > (max_val)) ? (max_val) : (x)))

/* Debug assertions */
#ifdef LIBRALE_DEBUG
#define DEBUG_ASSERT(condition) \
    do { \
        if (UNLIKELY(!(condition))) { \
            elog(LOG_ERROR, MODULE, "Debug assertion failed: " #condition \
                 " at %s:%d", __FILE__, __LINE__); \
        } \
    } while (0)

#define FUNCTION_ENTRY_LOG() \
    elog(LOG_DEBUG, MODULE, "Entering function: %s", __func__)

#define FUNCTION_EXIT_LOG() \
    elog(LOG_DEBUG, MODULE, "Exiting function: %s", __func__)
#else
#define DEBUG_ASSERT(condition) do { } while (0)
#define FUNCTION_ENTRY_LOG() do { } while (0)
#define FUNCTION_EXIT_LOG() do { } while (0)
#endif

#endif /* LIBRALE_MACROS_H */
