/*-------------------------------------------------------------------------
 *
 * ralectrl.h
 *		Control interface for RALE cluster management.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		ralectrl/include/ralectrl.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALECTRL_H
#define RALECTRL_H

/** Function declarations */
extern int add_node(const char *socket_path_arg, int node_id, const char *node_name, const char *node_ip, int rale_port, int dstore_port);
extern int remove_node(const char *socket_path_arg, int node_id);
extern int list_nodes(const char *socket_path_arg);

#endif							/* RALECTRL_H */
