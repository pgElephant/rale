/*-------------------------------------------------------------------------
 *
 * raled_inc.h
 *    Main include header for RALED daemon.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/include/raled_inc.h
 *
 *------------------------------------------------------------------------- */

#ifndef RALED_INC_H
#define RALED_INC_H

/** Library headers */
#include "raled_config.h"

/** System headers */
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/** Local headers */
#include "raled_args.h"
#include "raled_comm.h"
#include "raled_configfile.h"
#include "raled_guc.h"
#include "raled_logger.h"
#include "raled.h"
#include "raled_response.h"
#include "raled_signal.h"

/** Forward declarations for resource checks (defined in main.c) */
int check_unix_socket_availability(const char *socket_path);
int check_port_availability(int port, const char *port_name);

#endif												/* RALED_INC_H */
