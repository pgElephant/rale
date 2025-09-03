/*-------------------------------------------------------------------------
 *
 * librale_internal.h
 *		Internal header aggregator for librale implementation files.
 *		Not for external consumption. External users must include librale.h.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		librale/include/librale_internal.h
 *
 *-------------------------------------------------------------------------*/

#ifndef LIBRALE_INTERNAL_H
#define LIBRALE_INTERNAL_H

/** System headers */
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

/** Local headers */
#include "cluster.h"
#include "config.h"
#include "constants.h"
/* context.h not required */
#include "macros.h"
#include "hash.h"
#include "db.h"
#include "dlog.h"
#include "dstore.h"
#define LIBRALE_INTERNAL_USE 1
#include "rale_error.h"

#include "node.h"
#include "rale.h"
#include "rale_proto.h"
#include "tcp_client.h"
#include "tcp_server.h"
#include "udp.h"
#include "util.h"
#include "validation.h"

/** Global variable declarations */
extern cluster_t cluster;

#endif /* LIBRALE_INTERNAL_H */

