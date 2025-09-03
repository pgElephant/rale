/*-------------------------------------------------------------------------
 *
 * configfile.h
 *    Configuration file parsing interface for RALED.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/include/configfile.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

/** Local headers */
#include "raled_config.h"

/* MAX_LINE_LENGTH is defined in librale/include/config.h */

/** Enum for configuration value types */
typedef enum config_type
{
	CONFIG_TYPE_STRING, /** String configuration type */
	CONFIG_TYPE_INT,    /** Integer configuration type */
	CONFIG_TYPE_ENUM    /** Enumeration configuration type */
} config_type_t;

/** Structure for configuration metadata */
typedef struct config_entry
{
	const char		   *key;		   /** Configuration key */
	config_type_t		type;		   /** Type of the value */
	void			   *field;		   /** Pointer to the corresponding field in Config */
	const char		   *default_value; /** Default value as a string */
	int				   (*parser)(const char *); /** Optional parser function for enums */
} config_entry_t;

/** Function declarations */
extern int read_config(const char *filename, config_t *config);

#endif												/* CONFIGFILE_H */
