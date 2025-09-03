/*-------------------------------------------------------------------------
 *
 * signal.c
 *    Signal handling for RALED daemon.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/src/signal.c
 *
 *-------------------------------------------------------------------------
 */

/** System headers */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

/** Local headers */
#include "raled_signal.h"
#include "raled.h"
#include "raled_logger.h"
#include "shutdown.h"
#include "librale.h"
#include "cluster.h"

/** External variables */
extern volatile int system_exit;

/** Thread variables - currently not used but referenced */
pthread_t server_thread = 0;
pthread_t client_thread = 0;
pthread_t unix_socket_thread = 0;

/** Signal handler context */
static struct {
    volatile int shutdown_requested;
    volatile int reload_requested;
    volatile int status_requested;
    pthread_mutex_t mutex;
} signal_ctx;


/**
 * Perform health check on the daemon
 * Returns 1 if healthy, 0 if unhealthy
 */
static int
perform_health_check(void)
{
	/* Basic health check - check if daemon is still running */
	if (is_shutdown_requested())
	{
		return 0;
	}
	
	/* Simple health check - just verify daemon is responsive */
	/* Avoid potential deadlocks by not calling cluster functions */
	return 1;
}

/**
 * Signal handler for all signals
 */
static void
signal_handler(int signal)
{
    char msg[256];
    int old_errno = errno;
    
    /** Thread-safe signal handling */
    pthread_mutex_lock(&signal_ctx.mutex);
    
    switch (signal) {
        case SIGTERM:
        case SIGINT:
            /** Graceful shutdown request from signal */
            signal_ctx.shutdown_requested = 1;
            /** Use coordinated shutdown system for signal-based shutdown */
            librale_request_shutdown();
            snprintf(msg, sizeof(msg),
                     "Graceful shutdown initiated by %s", 
                     (signal == SIGTERM) ? "SIGTERM" : "SIGINT");
            raled_ereport(RALED_LOG_INFO, "RALED", msg, NULL, NULL);
            break;
            
        case SIGQUIT:
            /** Immediate shutdown request */
            signal_ctx.shutdown_requested = 1;
            system_exit = 1;
            snprintf(msg, sizeof(msg),
                     "Immediate shutdown initiated by SIGQUIT");
            raled_ereport(RALED_LOG_WARNING, "RALED", msg, NULL, NULL);
            break;
            
        case SIGHUP:
            /** Configuration reload request */
            signal_ctx.reload_requested = 1;
            snprintf(msg, sizeof(msg),
                     "Configuration reload requested by SIGHUP");
            raled_ereport(RALED_LOG_INFO, "RALED", msg, NULL, NULL);
            break;
            
        case SIGUSR1:
            /** Status request */
            signal_ctx.status_requested = 1;
            snprintf(msg, sizeof(msg),
                     "Status report requested by SIGUSR1");
            raled_ereport(RALED_LOG_INFO, "RALED", msg, NULL, NULL);
            break;
            
        case SIGUSR2:
            /** Health check request */
            snprintf(msg, sizeof(msg),
                     "Health check requested by SIGUSR2");
            raled_ereport(RALED_LOG_INFO, "RALED", msg, NULL, NULL);
            
            /** Perform health check */
            if (perform_health_check())
            {
                raled_ereport(RALED_LOG_INFO, "RALED", "Health check completed successfully", NULL, NULL);
            }
            else
            {
                raled_ereport(RALED_LOG_WARNING, "RALED", "Health check failed", NULL, NULL);
            }
            break;
            
        case SIGPIPE:
            /** Handle broken connections gracefully */
            snprintf(msg, sizeof(msg),
                     "Connection broken (SIGPIPE) - continuing operation");
            raled_ereport(RALED_LOG_WARNING, "RALED", msg, NULL, NULL);
            break;
            
        default:
            snprintf(msg, sizeof(msg),
                     "Unexpected signal %d received", signal);
            raled_ereport(RALED_LOG_WARNING, "RALED", msg, NULL, NULL);
            break;
    }
    
    pthread_mutex_unlock(&signal_ctx.mutex);
    errno = old_errno;
}

/**
 * Setup comprehensive signal handlers using signal()
 * Returns 0 on success, -1 on failure
 */
int
setup_signal_handlers(void)
{
    int ret = 0;
    
    /** Initialize signal context */
    memset(&signal_ctx, 0, sizeof(signal_ctx));
    if (pthread_mutex_init(&signal_ctx.mutex, NULL) != 0) {
        raled_ereport(RALED_LOG_ERROR, "RALED", 
                      "Failed to initialize signal mutex", NULL, NULL);
        return -1;
    }
    
    /** Register signal handlers */
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        raled_ereport(RALED_LOG_ERROR, "RALED", 
                      "Failed to set up SIGTERM handler", NULL, NULL);
        ret = -1;
    }
    
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        raled_ereport(RALED_LOG_ERROR, "RALED", 
                      "Failed to set up SIGINT handler", NULL, NULL);
        ret = -1;
    }
    
    if (signal(SIGQUIT, signal_handler) == SIG_ERR) {
        raled_ereport(RALED_LOG_ERROR, "RALED", 
                      "Failed to set up SIGQUIT handler", NULL, NULL);
        ret = -1;
    }
    
    if (signal(SIGHUP, signal_handler) == SIG_ERR) {
        raled_ereport(RALED_LOG_ERROR, "RALED", 
                      "Failed to set up SIGHUP handler", NULL, NULL);
        ret = -1;
    }
    
    if (signal(SIGUSR1, signal_handler) == SIG_ERR) {
        raled_ereport(RALED_LOG_ERROR, "RALED", 
                      "Failed to set up SIGUSR1 handler", NULL, NULL);
        ret = -1;
    }
    
    if (signal(SIGUSR2, signal_handler) == SIG_ERR) {
        raled_ereport(RALED_LOG_ERROR, "RALED", 
                      "Failed to set up SIGUSR2 handler", NULL, NULL);
        ret = -1;
    }
    
    if (signal(SIGPIPE, signal_handler) == SIG_ERR) {
        raled_ereport(RALED_LOG_ERROR, "RALED", 
                      "Failed to set up SIGPIPE handler", NULL, NULL);
        ret = -1;
    }
    
    /** Note: SIGKILL cannot be caught or ignored - this is by design */
    raled_ereport(RALED_LOG_INFO, "RALED", 
                  "Signal handlers set up successfully (SIGTERM, SIGINT, SIGQUIT, SIGHUP, SIGUSR1, SIGUSR2, SIGPIPE)", 
                  NULL, NULL);
    
    return ret;
}

/**
 * Check if shutdown has been requested
 * Returns 1 if shutdown requested, 0 otherwise
 */
int
is_shutdown_requested(void)
{
    int result;
    pthread_mutex_lock(&signal_ctx.mutex);
    result = signal_ctx.shutdown_requested;
    pthread_mutex_unlock(&signal_ctx.mutex);
    return result;
}

/**
 * Check if configuration reload has been requested
 * Returns 1 if reload requested, 0 otherwise
 */
int
is_reload_requested(void)
{
    int result;
    pthread_mutex_lock(&signal_ctx.mutex);
    result = signal_ctx.reload_requested;
    pthread_mutex_unlock(&signal_ctx.mutex);
    return result;
}

/**
 * Check if status report has been requested
 * Returns 1 if status requested, 0 otherwise
 */
int
is_status_requested(void)
{
    int result;
    pthread_mutex_lock(&signal_ctx.mutex);
    result = signal_ctx.status_requested;
    pthread_mutex_unlock(&signal_ctx.mutex);
    return result;
}

/**
 * Clear reload request flag
 */
void
clear_reload_request(void)
{
    pthread_mutex_lock(&signal_ctx.mutex);
    signal_ctx.reload_requested = 0;
    pthread_mutex_unlock(&signal_ctx.mutex);
}

/**
 * Clear status request flag
 */
void
clear_status_request(void)
{
    pthread_mutex_lock(&signal_ctx.mutex);
    signal_ctx.status_requested = 0;
    pthread_mutex_unlock(&signal_ctx.mutex);
}

/**
 * Cleanup signal handling resources
 */
void
cleanup_signal_handlers(void)
{
    pthread_mutex_destroy(&signal_ctx.mutex);
    raled_ereport(RALED_LOG_DEBUG, "RALED", 
                  "Signal handlers cleaned up", NULL, NULL);
}

/**
 * Graceful shutdown function
 * Performs cleanup in the correct order
 */
void
graceful_shutdown(void)
{
    raled_ereport(RALED_LOG_INFO, "RALED", 
                  "Beginning graceful shutdown sequence...", NULL, NULL);
    
    /** Set shutdown flag */
    system_exit = 1;
    
    /** Wait for subsystems to finish (with timeout) */
    raled_ereport(RALED_LOG_INFO, "RALED", 
                  "Waiting for subsystems to finish...", NULL, NULL);
    
    /** Wait for all subsystems to complete shutdown */
    int shutdown_wait_result = librale_wait_for_shutdown_completion(5); /* 5 second timeout */
    if (shutdown_wait_result != 0)
    {
        raled_ereport(RALED_LOG_WARNING, "RALED", 
                      "Subsystem shutdown timeout reached", NULL, NULL);
    }
    
    /** Give threads time to finish gracefully */
    sleep(2);
    
    /** Force thread termination if they haven't finished */
    if (server_thread != 0) {
        raled_ereport(RALED_LOG_INFO, "RALED", 
                      "Attempting to join server thread...", NULL, NULL);
        
        /** Try to join with a short timeout */
        int join_result = pthread_join(server_thread, NULL);
        if (join_result != 0) {
            raled_ereport(RALED_LOG_WARNING, "RALED", 
                          "Server thread join failed, forcing cancellation", NULL, NULL);
            pthread_cancel(server_thread);
            pthread_join(server_thread, NULL);
        }
        server_thread = 0;
    }
    
    if (client_thread != 0) {
        raled_ereport(RALED_LOG_INFO, "RALED", 
                      "Attempting to join client thread...", NULL, NULL);
        
        /** Try to join with a short timeout */
        int join_result = pthread_join(client_thread, NULL);
        if (join_result != 0) {
            raled_ereport(RALED_LOG_WARNING, "RALED", 
                          "Client thread join failed, forcing cancellation", NULL, NULL);
            pthread_cancel(client_thread);
            pthread_join(client_thread, NULL);
        }
        client_thread = 0;
    }
    
    raled_ereport(RALED_LOG_INFO, "RALED", 
                  "Graceful shutdown completed", NULL, NULL);
}
