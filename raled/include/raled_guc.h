/*-------------------------------------------------------------------------
 *
 * guc.h
 *    Grand Unified Configuration (GUC) interface for RALED.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    raled/include/guc.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALED_GUC_H
#define RALED_GUC_H

/** System headers */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/** Local headers */
#include "raled_config.h"

/** Enum for GUC value types */
typedef enum guc_type_t
{
	GUC_BOOL,   /** Boolean configuration type */
	GUC_INT,    /** Integer configuration type */
	GUC_STRING, /** String configuration type */
	GUC_ENUM    /** Enumeration configuration type */
} guc_type_t;

/** Structure for GUC configuration entries */
typedef struct guc_entry_t
{
	const char *name;          /** Configuration parameter name */
	guc_type_t     type;          /** Type of the value */
	void       *var;           /** Pointer to the variable */
	const char *default_value; /** Default value as string */
	const char *description;   /** Parameter description */
	int         min;           /** Minimum value (for numeric types) */
	int         max;           /** Maximum value (for numeric types) */
	bool        reloadable;    /** Whether parameter can be reloaded */
	int (*parser)(const char *); /** Parser function for enum types */
} guc_entry_t;

/** External variables */
extern guc_entry_t guc_table[];
extern int guc_table_size;

/** Function declarations */
int guc_set(const char *name, const char *value);
int guc_show(const char *name, char *out, size_t outlen);
void guc_show_all(void);

#endif												/* RALED_GUC_H */ 
