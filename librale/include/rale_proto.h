/*-------------------------------------------------------------------------
 *
 * rale_proto.h
 *		RALE protocol interface.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALE_PROTO_H
#define RALE_PROTO_H

/** Local headers */
#include "config.h"
#include "udp.h"

/** Function declarations */
extern int rale_proto_init(uint16_t port, const config_t *config);
extern int rale_init(const config_t *config);
extern int rale_finit(void);
extern int rale_quram_process(void);
extern connection_t *rale_setup_socket(const uint16_t port);

#endif							/* RALE_PROTO_H */
