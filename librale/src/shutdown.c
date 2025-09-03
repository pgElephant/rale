/*-------------------------------------------------------------------------
 *
 * shutdown.c
 *		Shutdown management functions.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "shutdown.h"
#include "rale_error.h"
#include <errno.h>
#include <string.h>
#include <time.h>

#define MODULE "COMM"

extern volatile int system_exit;
volatile int dstore_shutdown_requested = 0;
volatile int rale_shutdown_requested = 0;
volatile int comm_shutdown_requested = 0;

static pthread_mutex_t shutdown_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t shutdown_cond = PTHREAD_COND_INITIALIZER;
static int shutdown_system_initialized = 0;

int
librale_shutdown_init(void)
{
	if (shutdown_system_initialized)
	{
		rale_debug_log("Shutdown system already initialized");
		return 0;
	}

	/** Initialize shutdown state */
	system_exit = 0;
	dstore_shutdown_requested = 0;
	rale_shutdown_requested = 0;
	comm_shutdown_requested = 0;

	/** Initialize mutex and condition variable */
	if (pthread_mutex_init(&shutdown_mutex, NULL) != 0)
	{
		rale_set_error_fmt(RALE_ERROR_SYSTEM_CALL, "shutdown", "Failed to initialize shutdown mutex: %s", strerror(errno));
		return -1;
	}

	if (pthread_cond_init(&shutdown_cond, NULL) != 0)
	{
		rale_set_error_fmt(RALE_ERROR_SYSTEM_CALL, "shutdown", "Failed to initialize shutdown condition: %s", strerror(errno));
		pthread_mutex_destroy(&shutdown_mutex);
		return -1;
	}

	shutdown_system_initialized = 1;
	rale_debug_log("Shutdown coordination system initialized");
	return 0;
}

void
librale_shutdown_cleanup(void)
{
	if (!shutdown_system_initialized)
		return;

	/** Clean up synchronization primitives */
	pthread_cond_destroy(&shutdown_cond);
	pthread_mutex_destroy(&shutdown_mutex);
	
	shutdown_system_initialized = 0;
	rale_debug_log("Shutdown coordination system cleaned up");
}

void
librale_request_shutdown(void)
{
	if (!shutdown_system_initialized)
	{
		rale_debug_log("Shutdown system not initialized, using fallback");
		system_exit = 1;
		return;
	}

	pthread_mutex_lock(&shutdown_mutex);
	
	rale_debug_log("Requesting coordinated shutdown of all subsystems");
	
	/** Set shutdown flags for all subsystems */
	system_exit = 1;
	dstore_shutdown_requested = 1;
	rale_shutdown_requested = 1;
	comm_shutdown_requested = 1;
	
	/** Broadcast shutdown signal to all waiting threads */
	pthread_cond_broadcast(&shutdown_cond);
	
	pthread_mutex_unlock(&shutdown_mutex);
	
	rale_debug_log("Shutdown request broadcasted to all subsystems");
}

int
librale_is_shutdown_requested(const char *subsystem)
{
	if (!shutdown_system_initialized)
	{
		/** Fallback to system_exit if shutdown system not initialized */
		return system_exit;
	}

	if (subsystem == NULL)
		return system_exit;
	
	if (strcmp(subsystem, SHUTDOWN_SUBSYSTEM_DSTORE) == 0)
		return dstore_shutdown_requested;
	else if (strcmp(subsystem, SHUTDOWN_SUBSYSTEM_RALE) == 0)
		return rale_shutdown_requested;
	else if (strcmp(subsystem, SHUTDOWN_SUBSYSTEM_COMM) == 0)
		return comm_shutdown_requested;
	else
		return system_exit;
}

int
librale_wait_for_shutdown_completion(int timeout_seconds)
{
	if (!shutdown_system_initialized)
	{
		rale_debug_log("Shutdown system not initialized, cannot wait");
		return -1;
	}

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += timeout_seconds;
	
	rale_debug_log("Waiting for subsystems to complete shutdown (timeout: %d seconds)", timeout_seconds);
	
	pthread_mutex_lock(&shutdown_mutex);
	
	/** Wait until all subsystems signal completion or timeout */
	int result = pthread_cond_timedwait(&shutdown_cond, &shutdown_mutex, &ts);
	
	if (result == 0)
	{
		rale_debug_log("All subsystems completed shutdown successfully");
	}
	else if (result == ETIMEDOUT)
	{
		rale_debug_log("Shutdown completion timeout reached");
	}
	else
	{
		rale_debug_log("Error waiting for shutdown completion: %s", strerror(result));
	}
	
	pthread_mutex_unlock(&shutdown_mutex);
	
	return (result == 0) ? 0 : -1;
}

void
librale_signal_shutdown_complete(const char *subsystem)
{
	if (!shutdown_system_initialized)
	{
		rale_debug_log("Shutdown system not initialized, cannot signal completion");
		return;
	}

	if (subsystem == NULL)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "shutdown", "Cannot signal shutdown completion: subsystem is NULL", "", "");
		return;
	}

	pthread_mutex_lock(&shutdown_mutex);
	
	rale_debug_log("Subsystem '%s' signaled shutdown completion", subsystem);
	
	/** Mark the specific subsystem as completed */
	if (strcmp(subsystem, SHUTDOWN_SUBSYSTEM_DSTORE) == 0)
		dstore_shutdown_requested = 0;
	else if (strcmp(subsystem, SHUTDOWN_SUBSYSTEM_RALE) == 0)
		rale_shutdown_requested = 0;
	else if (strcmp(subsystem, SHUTDOWN_SUBSYSTEM_COMM) == 0)
		comm_shutdown_requested = 0;
	else
	{
		rale_debug_log("Unknown subsystem '%s'", subsystem);
		pthread_mutex_unlock(&shutdown_mutex);
		return;
	}
	
	/** Check if all subsystems are done */
	if (!dstore_shutdown_requested && !rale_shutdown_requested && !comm_shutdown_requested)
	{
		rale_debug_log("All subsystems completed shutdown, signaling main thread");
		pthread_cond_broadcast(&shutdown_cond);
	}
	
	pthread_mutex_unlock(&shutdown_mutex);
}
