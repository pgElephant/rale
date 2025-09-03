/*-------------------------------------------------------------------------
 *
 * rale_error.c
 *		Error handling implementation for librale library
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "rale_error.h"

/* Thread-local error information */
__thread rale_error_info_t rale_last_error = {0};

/* Debug logging flag - controlled by daemon */
bool rale_debug_enabled = false;

/*
 * Set error information
 */
void
rale_set_error(int error_code, const char *source, const char *message,
               const char *detail, const char *hint)
{
    rale_last_error.error_code = error_code;
    rale_last_error.error_source = source;
    rale_last_error.error_message = message;
    rale_last_error.error_detail = detail;
    rale_last_error.error_hint = hint;
    rale_last_error.system_errno = 0;
    
    rale_debug_log("Error set: code=%d, source=%s, message=%s", 
                   error_code, source ? source : "unknown", 
                   message ? message : "no message");
}

/*
 * Set error with system errno
 */
void
rale_set_error_errno(int error_code, const char *source, const char *message,
                     const char *detail, const char *hint, int sys_errno)
{
    rale_last_error.error_code = error_code;
    rale_last_error.error_source = source;
    rale_last_error.error_message = message;
    rale_last_error.error_detail = detail;
    rale_last_error.error_hint = hint;
    rale_last_error.system_errno = sys_errno;
    
    rale_debug_log("Error set with errno: code=%d, source=%s, message=%s, errno=%d (%s)", 
                   error_code, source ? source : "unknown", 
                   message ? message : "no message", sys_errno, strerror(sys_errno));
}

/*
 * Set error with printf-style formatting
 */
void
rale_set_error_fmt(int error_code, const char *source, const char *message_fmt, ...)
{
    static char formatted_message[512];
    va_list args;
    
    va_start(args, message_fmt);
    vsnprintf(formatted_message, sizeof(formatted_message), message_fmt, args);
    va_end(args);
    
    rale_last_error.error_code = error_code;
    rale_last_error.error_source = source;
    rale_last_error.error_message = formatted_message;
    rale_last_error.error_detail = formatted_message; /* Use formatted message as detail too */
    rale_last_error.error_hint = "Check the detailed error message";
    rale_last_error.system_errno = 0;
    
    rale_debug_log("Error set: code=%d, source=%s, message=%s", 
                   error_code, source ? source : "unknown", formatted_message);
}

/*
 * Get last error information
 */
const rale_error_info_t *
rale_get_last_error(void)
{
    return &rale_last_error;
}

/*
 * Clear error information
 */
void
rale_clear_error(void)
{
    memset(&rale_last_error, 0, sizeof(rale_error_info_t));
}

/*
 * Convert error code to string
 */
const char *
rale_error_code_to_string(int error_code)
{
    switch (error_code)
    {
        case RALE_SUCCESS:
            return "success";
        case RALE_ERROR_GENERAL:
            return "general error";
            
        /* Configuration errors */
        case RALE_ERROR_INVALID_CONFIG:
            return "invalid configuration";
        case RALE_ERROR_CONFIG_MISSING:
            return "configuration missing";
        case RALE_ERROR_CONFIG_INVALID_VALUE:
            return "invalid configuration value";
        case RALE_ERROR_INIT_FAILED:
            return "initialization failed";
        case RALE_ERROR_ALREADY_INITIALIZED:
            return "already initialized";
        case RALE_ERROR_NOT_INITIALIZED:
            return "not initialized";
            
        /* Network errors */
        case RALE_ERROR_NETWORK_INIT:
            return "network initialization failed";
        case RALE_ERROR_SOCKET_CREATE:
            return "socket creation failed";
        case RALE_ERROR_SOCKET_BIND:
            return "socket bind failed";
        case RALE_ERROR_SOCKET_CONNECT:
            return "socket connection failed";
        case RALE_ERROR_NETWORK_TIMEOUT:
            return "network timeout";
        case RALE_ERROR_NETWORK_UNREACHABLE:
            return "network unreachable";
        case RALE_ERROR_PROTOCOL_VERSION:
            return "protocol version mismatch";
        case RALE_ERROR_MESSAGE_TOO_LARGE:
            return "message too large";
            
        /* Consensus errors */
        case RALE_ERROR_INVALID_TERM:
            return "invalid term";
        case RALE_ERROR_INVALID_NODE_ID:
            return "invalid node ID";
        case RALE_ERROR_ELECTION_TIMEOUT:
            return "election timeout";
        case RALE_ERROR_SPLIT_BRAIN:
            return "split brain detected";
        case RALE_ERROR_QUORUM_LOST:
            return "quorum lost";
        case RALE_ERROR_LOG_INCONSISTENT:
            return "log inconsistency";
        case RALE_ERROR_LEADER_CONFLICT:
            return "leader conflict";
            
        /* Database errors */
        case RALE_ERROR_DB_INIT:
            return "database initialization failed";
        case RALE_ERROR_DB_OPEN:
            return "database open failed";
        case RALE_ERROR_DB_READ:
            return "database read failed";
        case RALE_ERROR_DB_WRITE:
            return "database write failed";
        case RALE_ERROR_DB_CORRUPT:
            return "database corruption";
        case RALE_ERROR_DB_LOCKED:
            return "database locked";
        case RALE_ERROR_DISK_FULL:
            return "disk full";
        case RALE_ERROR_PATH_NOT_FOUND:
            return "path not found";
            
        /* Memory errors */
        case RALE_ERROR_OUT_OF_MEMORY:
            return "out of memory";
        case RALE_ERROR_RESOURCE_LIMIT:
            return "resource limit exceeded";
        case RALE_ERROR_INVALID_POINTER:
            return "invalid pointer";
        case RALE_ERROR_BUFFER_OVERFLOW:
            return "buffer overflow";
            
        /* Validation errors */
        case RALE_ERROR_INVALID_PARAMETER:
            return "invalid parameter";
        case RALE_ERROR_NULL_POINTER:
            return "null pointer";
        case RALE_ERROR_INVALID_STATE:
            return "invalid state";
        case RALE_ERROR_INVALID_OPERATION:
            return "invalid operation";
        case RALE_ERROR_PERMISSION_DENIED:
            return "permission denied";
            
        /* Watchdog errors */
        case RALE_ERROR_WATCHDOG_INIT:
            return "watchdog initialization failed";
        case RALE_ERROR_WATCHDOG_OPEN:
            return "watchdog device open failed";
        case RALE_ERROR_WATCHDOG_TIMEOUT:
            return "watchdog timeout";
        case RALE_ERROR_WATCHDOG_FAILED:
            return "watchdog operation failed";
        case RALE_ERROR_WATCHDOG_DISABLED:
            return "watchdog disabled";
            
        /* Threading errors */
        case RALE_ERROR_THREAD_CREATE:
            return "thread creation failed";
        case RALE_ERROR_MUTEX_LOCK:
            return "mutex lock failed";
        case RALE_ERROR_MUTEX_UNLOCK:
            return "mutex unlock failed";
        case RALE_ERROR_CONDITION_WAIT:
            return "condition wait failed";
        case RALE_ERROR_DEADLOCK:
            return "deadlock detected";
            
        /* I/O errors */
        case RALE_ERROR_FILE_NOT_FOUND:
            return "file not found";
        case RALE_ERROR_FILE_ACCESS:
            return "file access error";
        case RALE_ERROR_FILE_FORMAT:
            return "invalid file format";
        case RALE_ERROR_SYSTEM_CALL:
            return "system call failed";
        case RALE_ERROR_INTERRUPTED:
            return "operation interrupted";
            
        default:
            return "unknown error";
    }
}

/*
 * Check if error code represents a configuration error
 */
bool
rale_error_is_config_error(int error_code)
{
    return (error_code >= 101 && error_code <= 199);
}

/*
 * Check if error code represents a network error
 */
bool
rale_error_is_network_error(int error_code)
{
    return (error_code >= 201 && error_code <= 299);
}

/*
 * Check if error code represents a consensus error
 */
bool
rale_error_is_consensus_error(int error_code)
{
    return (error_code >= 301 && error_code <= 399);
}

/*
 * Check if error code represents a database error
 */
bool
rale_error_is_database_error(int error_code)
{
    return (error_code >= 401 && error_code <= 499);
}

/*
 * Check if error code represents a fatal error
 */
bool
rale_error_is_fatal_error(int error_code)
{
    switch (error_code)
    {
        case RALE_ERROR_OUT_OF_MEMORY:
        case RALE_ERROR_DB_CORRUPT:
        case RALE_ERROR_SPLIT_BRAIN:
        case RALE_ERROR_DISK_FULL:
        case RALE_ERROR_DEADLOCK:
        case RALE_ERROR_WATCHDOG_FAILED:
            return true;
        default:
            return false;
    }
}
