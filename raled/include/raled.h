/*-------------------------------------------------------------------------
 *
 * raled.h
 *		Main RALED daemon interface.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		raled/include/raled.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALED_H
#define RALED_H

/** Local headers */
#include "raled_config.h"

/** Function declarations */
extern int raled_init(const config_t *config);
extern int raled_finit(void);

#endif							/* RALED_H */
