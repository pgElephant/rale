/*-------------------------------------------------------------------------
 *
 * comm.c
 *    Communication subsystem for RALED daemon.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/src/comm.c
 *
 *-------------------------------------------------------------------------
 */

/** Local headers */
#include "librale.h"
#include "raled_inc.h"

int
comm_init(void)
{
    /** Unix socket server is already initialized by DStore in librale_rale_init() */
    /* No need to initialize it again here */
    return 0;
}

int
comm_finit(void)
{
	/** Unix socket server and DStore are managed by the RALE subsystem */
	/** No need to finalize them here */
	return 0;
}
