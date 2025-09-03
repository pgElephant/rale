/*-------------------------------------------------------------------------
 *
 * signal.h
 *    Signal handling interface for RALED.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/include/signal.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef SIGNAL_H
#define SIGNAL_H

/** Local headers */
#include "raled_inc.h"

extern int setup_signal_handlers(void);
extern int is_shutdown_requested(void);
extern int is_reload_requested(void);
extern int is_status_requested(void);
extern void clear_reload_request(void);
extern void clear_status_request(void);
extern void cleanup_signal_handlers(void);
extern void graceful_shutdown(void);

#endif												/* SIGNAL_H */
