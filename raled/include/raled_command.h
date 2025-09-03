/*-------------------------------------------------------------------------
 *
 * raled_command.h
 *		RALE Daemon - Command processing interface.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALED_COMMAND_H
#define RALED_COMMAND_H

#include "librale.h"

/*
 * Process a command received from a client and generate a response.
 * 
 * Args:
 *   command: The command string to process (JSON or plain text)
 *   response: Buffer to store the response
 *   response_size: Size of the response buffer
 * 
 * Returns:
 *   librale_status_t indicating success or failure
 */
librale_status_t raled_process_command(const char *command, char *response, size_t response_size);

#endif												/* RALED_COMMAND_H */
