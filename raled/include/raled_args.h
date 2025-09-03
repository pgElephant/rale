/*-------------------------------------------------------------------------
 *
 * args.h
 *    Command-line argument parsing for RALED.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/include/args.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef ARGS_H
#define ARGS_H

/** Local headers */
#include "raled_inc.h"

/** External variables */
extern int verbose;
extern config_t config;
extern int daemon_mode;
extern char pid_file[256];

/** Function declarations */
extern void parse_arguments(int argc, char *argv[]);

#endif							/* ARGS_H */
