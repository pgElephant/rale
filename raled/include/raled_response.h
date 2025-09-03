/*-------------------------------------------------------------------------
 *
 * response.h
 *    Response handling interface for RALED.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/include/response.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef RESPONSE_H
#define RESPONSE_H

/** System headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Config types are available through raled_inc.h */

/** Maximum length for response messages */
#define MAX_RESPONSE_LENGTH 512

/** Structure to hold response data */
typedef struct response_t
{
	int  status_code;                    /** Status code of the response */
	char message[MAX_RESPONSE_LENGTH];   /** Response message */
} response_t;

/** Function declarations */
int handle_list_command(void);
void init_response(response_t *response, int status_code, const char *message);
char *response_to_json(const response_t *response);

#endif												/* RESPONSE_H */
