/*-------------------------------------------------------------------------
 *
 * shutdown.h
 *		Shutdown coordination system for RALE
 *
 *		This module provides a coordinated shutdown mechanism that ensures all
 *		subsystems gracefully shut down before the main process exits.
 *
 * Copyright (c) 2025, RALE Project
 * All rights reserved.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALE_SHUTDOWN_H
#define RALE_SHUTDOWN_H

#include <pthread.h>

void librale_request_shutdown(void);
int librale_is_shutdown_requested(const char *subsystem);
int librale_wait_for_shutdown_completion(int timeout_seconds);
void librale_signal_shutdown_complete(const char *subsystem);
int librale_shutdown_init(void);
void librale_shutdown_cleanup(void);

#define SHUTDOWN_SUBSYSTEM_DSTORE	"dstore"
#define SHUTDOWN_SUBSYSTEM_RALE		"rale"
#define SHUTDOWN_SUBSYSTEM_COMM		"comm"

#endif							/* RALE_SHUTDOWN_H */
