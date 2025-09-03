/*-------------------------------------------------------------------------
 *
 * configfile.c
 *    Configuration file parsing for RALED daemon.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/src/configfile.c
 *
 *-------------------------------------------------------------------------
 */

/** System headers */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Local headers */
#include "raled_guc.h"
#include "raled_inc.h"

int
read_config(const char *filename, config_t *config_param __attribute__((unused)))
{
	FILE  *file;
	char   line[MAX_LINE_LENGTH];
	char   key_o[MAX_LINE_LENGTH];
	char   value_o[MAX_LINE_LENGTH];
	char  *key;
	char  *value;
	char  *comment;
	char  *end;
	char   msg[256];

	file = fopen(filename, "r");
	if (file == NULL)
	{
		snprintf(msg, sizeof(msg), "Could not open config file: %s", filename);
		                raled_ereport(RALED_LOG_ERROR, "RALED", msg, NULL, NULL);
		return -1;
	}

	/** Parse file */
	while (fgets(line, sizeof(line), file))
	{
		/** Strip comments: find '#' and terminate the line there */
		comment = strchr(line, '#');
		if (comment != NULL)
		{
			*comment = '\0';
		}

		if (line[0] == '\n' || line[0] == '\0')
		{
			continue;
		}

		/** Find the equals sign */
		char *equals = strchr(line, '=');
		if (equals == NULL)
		{
			continue;
		}
		
		/** Split the line at equals sign */
		strncpy(key_o, line, equals - line);
		key_o[equals - line] = '\0';
		strncpy(value_o, equals + 1, sizeof(value_o) - 1);
		value_o[sizeof(value_o) - 1] = '\0';

		/** Remove quotes and whitespace */
		key = key_o;
		value = value_o;
		while (*key == ' ' || *key == '\t' || *key == '\'' || *key == '"')
		{
			key++;
		}
		while (*value == ' ' || *value == '\t' || *value == '\'' || *value == '"')
		{
			value++;
		}
		end = key + strlen(key) - 1;
		while (end > key && (*end == ' ' || *end == '\t' || *end == '\'' ||
			   *end == '"' || *end == '\n' || *end == '\r'))
		{
			*end-- = '\0';
		}
		end = value + strlen(value) - 1;
		while (end > value && (*end == ' ' || *end == '\t' || *end == '\'' ||
			   *end == '"' || *end == '\n' || *end == '\r'))
		{
			*end-- = '\0';
		}

		guc_set(key, value);
	}

	if (file != NULL)
	{
		fclose(file);
		file = NULL;
	}
    {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "Loading configuration file '%s'", filename);
        raled_ereport(RALED_LOG_INFO, "RALED", dbg, NULL, NULL);
    }
	return 0;
}
