/*-------------------------------------------------------------------------
 *
 * rale.h
 *		Main RALE protocol interface.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALE_H
#define RALE_H

#include <stddef.h>
#include <time.h>

/** Local headers */
#include "config.h"
#include "rale_error.h"

/** RALE role constants */
typedef enum {
	rale_role_follower = 0,
	rale_role_candidate = 1,
	rale_role_leader = 2,
	rale_role_transitioning = 3
} rale_role_t;

/** RALE state structure */
typedef struct rale_state_t {
	int32_t current_term;
	int32_t voted_for;
	int32_t leader_id;
	rale_role_t role;
	int32_t last_log_index;
	int32_t last_log_term;
	int32_t commit_index;
	int32_t last_applied;
	time_t last_heartbeat;
	time_t election_deadline;
} rale_state_t;

/** Function declarations */
int rale_init(const config_t *config);
int rale_finit(void);
int rale_process_command(const char *command, char *response, size_t response_size);
int rale_get_status(char *status, size_t status_size);
int rale_quram_process(void);

#endif /* RALE_H */
