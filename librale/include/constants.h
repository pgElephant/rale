/*-------------------------------------------------------------------------
 *
 * constants.h
 *		Common constants used throughout librale.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		librale/include/constants.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

/* Buffer sizes */
#define LIBRALE_SMALL_BUFFER		64
#define LIBRALE_LARGE_BUFFER		1024
#define LIBRALE_COMMAND_LEN			256
#define LIBRALE_NODE_LIST_BUFFER	1024
#define LIBRALE_RECV_BUFFER			1024
#define LIBRALE_MIN_ERROR_RESPONSE	64

/* Success/Error codes */
#define LIBRALE_SUCCESS			0
#define LIBRALE_ERROR			-1

/* Port ranges */
#define LIBRALE_MIN_PORT		1
#define LIBRALE_MAX_PORT		65535

/* File permissions */
#define LIBRALE_DIR_PERMISSIONS		0755
#define LIBRALE_SOCKET_PERMISSIONS	0666
#define LIBRALE_SOCKET_TIMEOUT_US	100000

#endif							/* CONSTANTS_H */ 
