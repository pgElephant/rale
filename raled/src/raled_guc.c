/*-------------------------------------------------------------------------
 *
 * guc.c
 *    Grand Unified Configuration (GUC) system for RALED.
 *
 *    Provides a declarative config table, set/show functions, and runtime
 *    validation for all configuration parameters.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/src/guc.c
 *
 *-------------------------------------------------------------------------*/

/** System headers */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/** Local headers */
#include "raled_guc.h"
#include "raled_inc.h"

/** External variables */
extern config_t config;

/** Parser functions for enums */
static int
parse_log_destination(const char *value)
{
	if (strcmp(value, "stderr") == 0)
		return LOG_DEST_STDERR;
	else if (strcmp(value, "stdout") == 0)
		return LOG_DEST_STDOUT;
	else if (strcmp(value, "file") == 0)
		return LOG_DEST_FILE;
	else if (strcmp(value, "syslog") == 0)
		return LOG_DEST_SYSLOG;
	else
		return LOG_DEST_STDOUT; /** Default to stdout */
}

static int
parse_protocol(const char *value)
{
    if (strcmp(value, "unix") == 0)
        return PROTOCOL_UNIX;
    /* Default to unix for now */
    return PROTOCOL_UNIX;
}

static int
parse_log_level(const char *value)
{
	if (strcmp(value, "error") == 0)
		return RALED_LOG_ERROR;
	else if (strcmp(value, "warning") == 0)
		return RALED_LOG_WARNING;
	else if (strcmp(value, "info") == 0)
		return RALED_LOG_INFO;
	else if (strcmp(value, "debug") == 0)
		return RALED_LOG_DEBUG;
	else
		return RALED_LOG_INFO; /** Default to info */
}

guc_entry_t guc_table[] = {
	{
		"node_name",
		GUC_STRING,
		&config.node.name,
		"default",
		"Node name",
		0, 0, false,
		NULL
	},
	{
		"node_id",
		GUC_INT,
		&config.node.id,
		"1",
		"Node ID",
		1, 10000, false,
		NULL
	},
	{
		"node_ip",
		GUC_STRING,
		&config.node.ip,
		"127.0.0.1",
		"Node IP address",
		0, 0, false,
		NULL
	},
	{
		"node_priority",
		GUC_INT,
		&config.node.priority,
		"1",
		"Election priority",
		1, 100, false,
		NULL
	},
	{
		"rale_port",
		GUC_INT,
		&config.node.rale_port,
		"5001",
		"RALE UDP port",
		1, 65535, false,
		NULL
	},
	{
		"dstore_port",
		GUC_INT,
		&config.node.dstore_port,
		"6001",
		"DStore TCP port",
		1, 65535, false,
		NULL
	},
	{
		"path",
		GUC_STRING,
		&config.db.path,
		"./db1",
		"Data directory",
		0, 0, false,
		NULL
	},
	{
		"max_size",
		GUC_INT,
		&config.db.max_size,
		"0",
		"Max DB size",
		0, 100000, false,
		NULL
	},
	{
		"max_connections",
		GUC_INT,
		&config.db.max_connections,
		"0",
		"Max DB connections",
		0, 10000, false,
		NULL
	},
	{
		"raled_log_destination",
		GUC_ENUM,
		&config.raled_log.destination,
		"stdout",
		"RALED log destination",
		0, 0, true,
		parse_log_destination
	},
	{
		"raled_log_file",
		GUC_STRING,
		&config.raled_log.file,
		"raled1.log",
		"RALED log file",
		0, 0, true,
		NULL
	},
	{
		"raled_log_level",
		GUC_ENUM,
		&config.raled_log.level,
		"info",
		"RALED log level",
		0, 0, true,
		parse_log_level
	},
	{
		"raled_log_rotation_size",
		GUC_INT,
		&config.raled_log.rotation_size,
		"10",
		"RALED log rotation size",
		1, 1000, true,
		NULL
	},
	{
		"raled_log_rotation_age",
		GUC_INT,
		&config.raled_log.rotation_age,
		"7",
		"RALED log rotation age",
		1, 365, true,
		NULL
	},
	{
		"dstore_log_destination",
		GUC_ENUM,
		&config.dstore_log.destination,
		"stdout",
		"DStore log destination",
		0, 0, true,
		parse_log_destination
	},
	{
		"dstore_log_file",
		GUC_STRING,
		&config.dstore_log.file,
		"dstore1.log",
		"DStore log file",
		0, 0, true,
		NULL
	},
	{
		"dstore_log_level",
		GUC_ENUM,
		&config.dstore_log.level,
		"debug",
		"DStore log level",
		0, 0, true,
		parse_log_level
	},
	{
		"dstore_log_rotation_size",
		GUC_INT,
		&config.dstore_log.rotation_size,
		"10",
		"DStore log rotation size",
		1, 1000, true,
		NULL
	},
	{
		"dstore_log_rotation_age",
		GUC_INT,
		&config.dstore_log.rotation_age,
		"7",
		"DStore log rotation age",
		1, 365, true,
		NULL
	},
	{
		"comm_log_destination",
		GUC_ENUM,
		&config.communication.log.destination,
		"file",
		"COMM log destination",
		0, 0, true,
		parse_log_destination
	},
	{
		"comm_log_level",
		GUC_ENUM,
		&config.communication.log.level,
		"debug",
		"COMM log level",
		0, 0, true,
		parse_log_level
	},
	{
		"comm_log_rotation_size",
		GUC_INT,
		&config.communication.log.rotation_size,
		"10",
		"COMM log rotation size",
		1, 1000, true,
		NULL
	},
	{
		"comm_log_rotation_age",
		GUC_INT,
		&config.communication.log.rotation_age,
		"7",
		"COMM log rotation age",
		1, 365, true,
		NULL
	},
	{
		"dstore_keep_alive_interval",
		GUC_INT,
		&config.dstore.keep_alive_interval,
		"5",
		"DStore keep-alive interval in seconds",
		1, 3600, true,
		NULL
	},
	{
		"dstore_keep_alive_timeout",
		GUC_INT,
		&config.dstore.keep_alive_timeout,
		"10",
		"DStore keep-alive timeout in seconds",
		1, 3600, true,
		NULL
	},
	{
		"log_directory",
		GUC_STRING,
		&config.log_directory,
		"./log",
		"Base directory for log files",
		0, 0, true,
		NULL
	},
    {
        "communication_protocol",
        GUC_ENUM,
        &config.communication.protocol,
        "unix",
        "Communication protocol",
        0, 0, false,
        parse_protocol
    },
	{
		"communication_socket",
		GUC_STRING,
		&config.communication.socket,
		"",
		"Unix socket path",
		0, 0, false,
		NULL
	},
	{
		"communication_timeout",
		GUC_INT,
		&config.communication.timeout,
		"5",
		"Communication timeout",
		1, 3600, false,
		NULL
	},
	{
		"communication_max_retries",
		GUC_INT,
		&config.communication.max_retries,
		"3",
		"Max communication retries",
		0, 100, false,
		NULL
	}
};

int guc_table_size = sizeof(guc_table) / sizeof(guc_table[0]);

int
guc_set(const char *name, const char *value)
{
	int i;
	int v;

    /** Simple alias mapping to support existing config keys */
    if (strcmp(name, "socket_path") == 0)
    {
        name = "communication_socket";
    }
    else if (strcmp(name, "log_directory") == 0)
    {
        /** already mapped in table to config.log_directory */
    }
    else if (strcmp(name, "log_file") == 0)
    {
        name = "raled_log_file";
    }
    else if (strcmp(name, "log_level") == 0)
    {
        name = "raled_log_level";
    }

	for (i = 0; i < guc_table_size; i++)
	{
		if (strcmp(name, guc_table[i].name) == 0)
		{
			switch (guc_table[i].type)
			{
				case GUC_INT:
					v = atoi(value);
					if (v < guc_table[i].min || v > guc_table[i].max)
					{
						return -1;
					}
					*((int *) guc_table[i].var) = v;
					return 0;
				case GUC_STRING:
					strncpy((char *) guc_table[i].var, value, MAX_STRING_LENGTH - 1);
					((char *) guc_table[i].var)[MAX_STRING_LENGTH - 1] = '\0';
					return 0;
				case GUC_BOOL:
					*((int *) guc_table[i].var) = (strcmp(value, "on") == 0 ||
												   strcmp(value, "true") == 0);
					return 0;
				case GUC_ENUM:
					if (guc_table[i].parser != NULL)
					{
						*((int *) guc_table[i].var) = guc_table[i].parser(value);
						return 0;
					}
					return -1;
			}
		}
	}
	return -1;
}

int
guc_show(const char *name, char *out, size_t outlen)
{
	int i;

	for (i = 0; i < guc_table_size; i++)
	{
		if (strcmp(name, guc_table[i].name) == 0)
		{
			switch (guc_table[i].type)
			{
				case GUC_INT:
					snprintf(out, outlen, "%d", *((int *) guc_table[i].var));
					return 0;
				case GUC_STRING:
					snprintf(out, outlen, "%s", (char *) guc_table[i].var);
					return 0;
				case GUC_BOOL:
					snprintf(out, outlen, "%s", *((int *) guc_table[i].var) ? "on" : "off");
					return 0;
				case GUC_ENUM:
					/** For enums, just show the numeric value for now */
					snprintf(out, outlen, "%d", *((int *) guc_table[i].var));
					return 0;
			}
		}
	}
	return -1;
}

void
guc_show_all(void)
{
    int i;

    for (i = 0; i < guc_table_size; i++)
    {
        char line[512];
        const char *value_str = NULL;
        char buf[256];

        switch (guc_table[i].type)
        {
            case GUC_INT:
                snprintf(buf, sizeof(buf), "%d", *((int *) guc_table[i].var));
                value_str = buf;
                break;
            case GUC_STRING:
                value_str = (const char *) guc_table[i].var;
                break;
            case GUC_BOOL:
                value_str = (*((int *) guc_table[i].var)) ? "on" : "off";
                break;
            case GUC_ENUM:
                snprintf(buf, sizeof(buf), "%d", *((int *) guc_table[i].var));
                value_str = buf;
                break;
        }

        snprintf(line, sizeof(line), "%-30s = %s", guc_table[i].name, value_str ? value_str : "");
        raled_ereport(RALED_LOG_DEBUG, "RALED", line, NULL, NULL);
    }
}
