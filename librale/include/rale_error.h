/*-------------------------------------------------------------------------
 *
 * rale_error.h
 *		Error codes and error handling for librale library
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALE_ERROR_H
#define RALE_ERROR_H

#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>

/*
 * Librale Error Codes
 * 
 * The library returns error codes but does not log anything except debug messages
 * when debug level is explicitly enabled. The daemon (raled) is responsible for
 * interpreting error codes and logging appropriate messages.
 */

/* Success and general error codes */
#define RALE_SUCCESS                    0
#define RALE_ERROR_GENERAL             -1

/* Configuration and initialization errors (1xx) */
#define RALE_ERROR_INVALID_CONFIG       101
#define RALE_ERROR_CONFIG_MISSING       102
#define RALE_ERROR_CONFIG_INVALID_VALUE 103
#define RALE_ERROR_INIT_FAILED          104
#define RALE_ERROR_ALREADY_INITIALIZED  105
#define RALE_ERROR_NOT_INITIALIZED      106

/* Network and communication errors (2xx) */
#define RALE_ERROR_NETWORK_INIT         201
#define RALE_ERROR_SOCKET_CREATE        202
#define RALE_ERROR_SOCKET_BIND          203
#define RALE_ERROR_SOCKET_CONNECT       204
#define RALE_ERROR_NETWORK_TIMEOUT      205
#define RALE_ERROR_NETWORK_UNREACHABLE  206
#define RALE_ERROR_PROTOCOL_VERSION     207
#define RALE_ERROR_MESSAGE_TOO_LARGE    208

/* Consensus protocol errors (3xx) */
#define RALE_ERROR_INVALID_TERM         301
#define RALE_ERROR_INVALID_NODE_ID      302
#define RALE_ERROR_ELECTION_TIMEOUT     303
#define RALE_ERROR_SPLIT_BRAIN          304
#define RALE_ERROR_QUORUM_LOST          305
#define RALE_ERROR_LOG_INCONSISTENT     306
#define RALE_ERROR_LEADER_CONFLICT      307

/* Database and storage errors (4xx) */
#define RALE_ERROR_DB_INIT              401
#define RALE_ERROR_DB_OPEN              402
#define RALE_ERROR_DB_READ              403
#define RALE_ERROR_DB_WRITE             404
#define RALE_ERROR_DB_CORRUPT           405
#define RALE_ERROR_DB_LOCKED            406
#define RALE_ERROR_DISK_FULL            407
#define RALE_ERROR_PATH_NOT_FOUND       408

/* Memory and resource errors (5xx) */
#define RALE_ERROR_OUT_OF_MEMORY        501
#define RALE_ERROR_RESOURCE_LIMIT       502
#define RALE_ERROR_INVALID_POINTER      503
#define RALE_ERROR_BUFFER_OVERFLOW      504

/* Validation and parameter errors (6xx) */
#define RALE_ERROR_INVALID_PARAMETER    601
#define RALE_ERROR_NULL_POINTER         602
#define RALE_ERROR_INVALID_STATE        603
#define RALE_ERROR_INVALID_OPERATION    604
#define RALE_ERROR_PERMISSION_DENIED    605

/* Watchdog and monitoring errors (7xx) */
#define RALE_ERROR_WATCHDOG_INIT        701
#define RALE_ERROR_WATCHDOG_OPEN        702
#define RALE_ERROR_WATCHDOG_TIMEOUT     703
#define RALE_ERROR_WATCHDOG_FAILED      704
#define RALE_ERROR_WATCHDOG_DISABLED    705

/* Threading and concurrency errors (8xx) */
#define RALE_ERROR_THREAD_CREATE        801
#define RALE_ERROR_MUTEX_LOCK           802
#define RALE_ERROR_MUTEX_UNLOCK         803
#define RALE_ERROR_CONDITION_WAIT       804
#define RALE_ERROR_DEADLOCK             805

/* I/O and system errors (9xx) */
#define RALE_ERROR_FILE_NOT_FOUND       901
#define RALE_ERROR_FILE_ACCESS          902
#define RALE_ERROR_FILE_FORMAT          903
#define RALE_ERROR_SYSTEM_CALL          904
#define RALE_ERROR_INTERRUPTED          905
#define RALE_ERROR_INTERNAL             906

/*
 * Error information structure
 * Used to provide detailed error context to the daemon
 */
typedef struct rale_error_info_t {
    int         error_code;
    const char *error_source;      /* Module/function where error occurred */
    const char *error_message;     /* Brief description */
    const char *error_detail;      /* Detailed information */
    const char *error_hint;        /* Suggested action */
    int         system_errno;      /* System errno if applicable */
} rale_error_info_t;

/*
 * Global error information
 * Thread-local storage for error details
 */
extern __thread rale_error_info_t rale_last_error;

/*
 * Error handling functions
 */

/* Set error information (internal use) */
void rale_set_error(int error_code, const char *source, const char *message,
                   const char *detail, const char *hint);

/* Set error with printf-style formatting */
void rale_set_error_fmt(int error_code, const char *source, const char *message_fmt, ...)
    __attribute__((format(printf, 3, 4)));

/* Set error with system errno */
void rale_set_error_errno(int error_code, const char *source, const char *message,
                         const char *detail, const char *hint, int sys_errno);

/* Get last error information */
const rale_error_info_t *rale_get_last_error(void);

/* Clear error information */
void rale_clear_error(void);

/* Convert error code to string */
const char *rale_error_code_to_string(int error_code);

/* Check if error code represents a specific category */
bool rale_error_is_config_error(int error_code);
bool rale_error_is_network_error(int error_code);
bool rale_error_is_consensus_error(int error_code);
bool rale_error_is_database_error(int error_code);
bool rale_error_is_fatal_error(int error_code);

/*
 * Debug logging macros for librale internal use
 * Only active when debug level is explicitly set
 */
extern bool rale_debug_enabled;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define rale_debug_log(fmt, ...) \
    do { \
        if (rale_debug_enabled) { \
            fprintf(stderr, "[LIBRALE DEBUG] %s:%d %s(): " fmt "\n", \
                    __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        } \
    } while(0)
#pragma GCC diagnostic pop

#define rale_debug_enter() \
    rale_debug_log("entering function")

#define rale_debug_exit() \
    rale_debug_log("exiting function")

#define rale_debug_exit_error(code) \
    rale_debug_log("exiting with error %d (%s)", code, rale_error_code_to_string(code))

/*
 * Error setting macros for convenience
 */
#define RALE_SET_ERROR(code, msg) \
    rale_set_error(code, __func__, msg, NULL, NULL)

#define RALE_SET_ERROR_DETAIL(code, msg, detail) \
    rale_set_error(code, __func__, msg, detail, NULL)

#define RALE_SET_ERROR_FULL(code, msg, detail, hint) \
    rale_set_error(code, __func__, msg, detail, hint)

#define RALE_SET_ERROR_ERRNO(code, msg) \
    rale_set_error_errno(code, __func__, msg, NULL, NULL, errno)

#define RALE_SET_ERROR_ERRNO_DETAIL(code, msg, detail) \
    rale_set_error_errno(code, __func__, msg, detail, NULL, errno)

#endif /* RALE_ERROR_H */
