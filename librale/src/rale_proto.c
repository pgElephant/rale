/*-------------------------------------------------------------------------
 *
 * rale_proto.c
 *		RALE protocol implementation.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "rale_proto.h"
#include "config.h"
#include "cluster.h"
#include "node.h"
#include "rale.h"
#include "udp.h"
#include "rale_error.h"
/* Notify DStore of leader elections for cluster-wide sync */
#include "dstore.h"

/* Election/heartbeat timing */
#define DEFAULT_ELECTION_TIMEOUT 5   /* seconds; used if config not set */
#define DEFAULT_HEARTBEAT_INTERVAL 1 /* seconds */

config_t rale_config;
rale_state_t current_rale_state;
extern cluster_t cluster;
static connection_t *rale_udp_conn = NULL;
/* Track whether DStore was initialized so the application can run loops */
static int dstore_initialized = 0;

/* Election state (transient) */
static int      votes_received = 0;
static int      election_active = 0;
static time_t   next_heartbeat_at = 0;
static time_t   next_vote_request_at = 0;

static int get_keep_alive_timeout(void)
{
	if (rale_config.dstore.keep_alive_timeout > 0)
		return (int) rale_config.dstore.keep_alive_timeout;
	return DEFAULT_ELECTION_TIMEOUT;
}

static int get_heartbeat_interval(void)
{
	if (rale_config.dstore.keep_alive_interval > 0)
		return (int) rale_config.dstore.keep_alive_interval;
	return DEFAULT_HEARTBEAT_INTERVAL;
}

static time_t compute_election_deadline(void)
{
	/* Randomize between [timeout, timeout*2] to reduce split vote */
	int base = get_keep_alive_timeout();
	int jitter = base + (rand() % (base > 0 ? base : 1));
	return time(NULL) + jitter;
}

int rale_state_save(rale_state_t *state);
static int rale_state_load(rale_state_t *state);
static void rale_handle_message(const char *msg,
							   const char *sender_ip,
							   int sender_port);
static void rale_send_message(const char *message,
							 const char *target_ip,
							 int target_port);

static void rale_note_leader(int leader_id)
{
	if (leader_id >= 0 && leader_id != current_rale_state.leader_id)
	{
		current_rale_state.leader_id = leader_id;
		(void) rale_state_save(&current_rale_state);
	}
}

static void rale_become_leader(void)
{
	current_rale_state.role = rale_role_leader;
	current_rale_state.leader_id = rale_config.node.id;
	election_active = 0;
	votes_received = 0;
	(void) rale_state_save(&current_rale_state);
	/* Notify DStore so it can broadcast snapshot */
	{
		char leader_cmd[64];
		snprintf(leader_cmd, sizeof(leader_cmd), "LEADER_ELECTED %d %d",
				 current_rale_state.current_term, current_rale_state.leader_id);
		dstore_put_from_command(leader_cmd, NULL, 0);
	}
	next_heartbeat_at = 0; /* send immediately */
}

static void rale_become_follower(int known_leader)
{
	current_rale_state.role = rale_role_follower;
	if (known_leader >= 0)
		rale_note_leader(known_leader);
	election_active = 0;
	votes_received = 0;
	current_rale_state.election_deadline = compute_election_deadline();
	(void) rale_state_save(&current_rale_state);
}

int
rale_proto_init(uint16_t port, const config_t *config)
{
	int result;

	if (config != NULL)
	{
		memcpy(&rale_config, config, sizeof(config_t));
	}
	else
	{
		memset(&rale_config, 0, sizeof(config_t));
		rale_config.node.rale_port = port;
	}

	memset(&current_rale_state, 0, sizeof(rale_state_t));
	current_rale_state.current_term = 0;
	current_rale_state.voted_for = -1;
	current_rale_state.leader_id = -1;
	current_rale_state.role = rale_role_follower;
	current_rale_state.last_log_index = 0;
	current_rale_state.last_log_term = 0;
	current_rale_state.commit_index = 0;
	current_rale_state.last_applied = 0;
	current_rale_state.last_heartbeat = time(NULL);
	/* Seed PRNG once for jitter */
	srand((unsigned int) time(NULL) ^ (unsigned int) getpid());
	current_rale_state.election_deadline = compute_election_deadline();
	next_heartbeat_at = time(NULL) + get_heartbeat_interval();

	result = rale_state_load(&current_rale_state);
	if (result != 0)
	{
		rale_debug_log("Failed to load saved state, using defaults");
	}

	if (rale_setup_socket(port) == NULL)
	{
		rale_set_error(RALE_ERROR_NETWORK_INIT, "rale_init",
					   "Failed to setup UDP socket",
					   "Could not initialize UDP communication",
					   "Check if port is already in use or network configuration");
		return -1;
	}

	rale_debug_log("RALE protocol initialized on port %d", port);
	return 0;
}

connection_t *
rale_setup_socket(const uint16_t port)
{
	connection_t *conn;

	conn = udp_server_init(port, rale_handle_message);
	if (conn == NULL)
	{
		rale_set_error(RALE_ERROR_SOCKET_CREATE, "rale_setup_socket",
					   "Failed to create UDP server",
					   "Could not initialize UDP server socket",
					   "Check if port is available and network configuration is correct");
		return NULL;
	}
	/* Save for processing and cleanup */
	rale_udp_conn = conn;
	rale_debug_log("UDP server setup complete on port %d", port);
	return conn;
}

int
rale_state_save(rale_state_t *state)
{
	FILE *file;
	char filename[512];
	const char *base_path = NULL;

	if (state == NULL)
		return -1;

	/*
	 * Align RALE state persistence with DStore:
	 * Use text file at <db.path>/rale.state with fields:
	 *   current_term voted_for leader_id last_log_index last_log_term\n
	 */
	base_path = (rale_config.db.path[0] != '\0') ? rale_config.db.path :
				(rale_config.node.db_path[0] != '\0' ? rale_config.node.db_path : NULL);
	if (base_path == NULL)
	{
		rale_set_error(RALE_ERROR_CONFIG_MISSING, "rale_state_save",
					   "No database path configured for state file",
					   "Both db.path and node.db_path are not set",
					   "Configure database path in configuration file");
		return -1;
	}
	snprintf(filename, sizeof(filename), "%s/rale.state", base_path);

	file = fopen(filename, "w");
	if (file == NULL)
	{
		rale_set_error_errno(RALE_ERROR_FILE_ACCESS, "rale_state_save",
							 "Failed to open state file for writing",
							 "Cannot create or write to state file",
							 "Check directory permissions and disk space", errno);
		return -1;
	}

	/* Write only the shared fields used elsewhere; other fields are runtime-only */
	if (fprintf(file, "%d %d %d %d %d\n",
				state->current_term,
				state->voted_for,
				state->leader_id,
				state->last_log_index,
				state->last_log_term) < 0)
	{
		fclose(file);
		rale_set_error_errno(RALE_ERROR_FILE_ACCESS, "rale_state_save",
							 "Failed to write state to file",
							 "Write operation to state file failed",
							 "Check disk space and file system permissions", errno);
		return -1;
	}

	fclose(file);
	rale_debug_log("State saved successfully to %s", filename);
	return 0;
}

static int
rale_state_load(rale_state_t *state)
{
	FILE *file;
	char filename[512];
	const char *base_path = NULL;
	int current_term = -1, voted_for = -1, leader_id = -1, last_log_index = -1, last_log_term = -1;

	if (state == NULL)
		return -1;

	/* Read from the same text file used by DStore */
	base_path = (rale_config.db.path[0] != '\0') ? rale_config.db.path :
				(rale_config.node.db_path[0] != '\0' ? rale_config.node.db_path : NULL);
	if (base_path == NULL)
	{
		rale_set_error(RALE_ERROR_CONFIG_MISSING, "rale_state_load",
					   "No database path configured for state file",
					   "Both db.path and node.db_path are not set",
					   "Configure database path in configuration file");
		rale_debug_log("No database path configured; skipping state load");
		return -1;
	}
	snprintf(filename, sizeof(filename), "%s/rale.state", base_path);

	file = fopen(filename, "r");
	if (file == NULL)
	{
		rale_set_error(RALE_ERROR_FILE_NOT_FOUND, "rale_state_load",
					   "State file not found",
					   "No saved state file at expected location",
					   "This is normal for first startup");
		rale_debug_log("No saved state file found: %s", filename);
		return -1;
	}

	if (fscanf(file, "%d %d %d %d %d",
			   &current_term, &voted_for, &leader_id, &last_log_index, &last_log_term) != 5)
	{
		fclose(file);
		rale_set_error(RALE_ERROR_FILE_FORMAT, "rale_state_load",
					   "Failed to parse state file",
					   "State file format is invalid or corrupted",
					   "Remove corrupted state file to restart with clean state");
		return -1;
	}

	fclose(file);

	if (current_term >= 0) state->current_term = current_term;
	if (voted_for   >= 0) state->voted_for   = voted_for;
	if (leader_id   >= 0) state->leader_id   = leader_id;
	if (last_log_index >= 0) state->last_log_index = last_log_index;
	if (last_log_term  >= 0) state->last_log_term  = last_log_term;

	rale_debug_log("State loaded successfully from %s (term=%d, voted_for=%d, leader_id=%d)",
				   filename, state->current_term, state->voted_for, state->leader_id);
	return 0;
}

static void
rale_handle_message(const char *msg,
				   const char *sender_ip,
				   int sender_port)
{
	char response[1024];
	uint32_t candidate_id = 0;
	int      candidate_term = -1;
	int      voter_id = -1;
	uint32_t i;

	if (msg == NULL || sender_ip == NULL)
		return;

	rale_debug_log("Received message from %s:%d: %s", sender_ip, sender_port, msg);

	if (strncmp(msg, "VOTE_REQUEST", 12) == 0)
	{
		const char *candidate_str = msg + 12;
		if (*candidate_str == ' ')
		{
			/* Parse: VOTE_REQUEST <candidate_id> [<term>] */
			candidate_id = (uint32_t) atoi(candidate_str + 1);
			const char *maybe_term = strchr(candidate_str + 1, ' ');
			if (maybe_term && *(maybe_term + 1) != '\0')
				candidate_term = atoi(maybe_term + 1);
		}

		/* If request carries lower term, deny */
		if (candidate_term != -1 && candidate_term < current_rale_state.current_term)
		{
			snprintf(response, sizeof(response), "VOTE_DENIED %d %d",
					 rale_config.node.id, current_rale_state.current_term);
			rale_send_message(response, sender_ip, sender_port);
			return;
		}

		/* Update term if request carries higher term */
		if (candidate_term > current_rale_state.current_term)
		{
			current_rale_state.current_term = candidate_term;
			current_rale_state.voted_for = -1; /* reset */
			rale_become_follower(-1);
		}

		/* Grant rules: follow basic constraints */
		if (current_rale_state.role != rale_role_leader &&
			(current_rale_state.voted_for == -1 || current_rale_state.voted_for == (int)candidate_id))
		{
			current_rale_state.voted_for = (int) candidate_id;
			current_rale_state.election_deadline = compute_election_deadline();
			(void) rale_state_save(&current_rale_state);
			snprintf(response, sizeof(response), "VOTE_GRANTED %d %d", rale_config.node.id, current_rale_state.current_term);
			rale_send_message(response, sender_ip, sender_port);
		}
	}
	else if (strncmp(msg, "HEARTBEAT", 9) == 0)
	{
		current_rale_state.last_heartbeat = time(NULL);
		/* Optional: parse leader id and term: "HEARTBEAT <leader_id> [<term>]" */
		if (msg[9] == ' ')
		{
			int hb_leader = -1, hb_term = -1;
			const char *p = msg + 10;
			hb_leader = atoi(p);
			const char *sp = strchr(p, ' ');
			if (sp && *(sp + 1) != '\0') hb_term = atoi(sp + 1);

			if (hb_term > current_rale_state.current_term)
			{
				current_rale_state.current_term = hb_term;
				current_rale_state.voted_for = -1;
			}
			if (hb_leader >= 0)
			{
				rale_become_follower(hb_leader);
			}
		}
		
		snprintf(response, sizeof(response), "HEARTBEAT_ACK");
		rale_send_message(response, sender_ip, sender_port);
	}
	else if (strncmp(msg, "VOTE_GRANTED", 12) == 0)
	{
		/* Parse: VOTE_GRANTED <voter_id> [<term>] */
		const char *voter_str = msg + 12;
		int grant_term = -1;
		if (*voter_str == ' ')
		{
					voter_id = atoi(voter_str + 1);
		(void)voter_id; /* Parsed but not used in current implementation */
		const char *maybe_term = strchr(voter_str + 1, ' ');
		if (maybe_term && *(maybe_term + 1) != '\0')
			grant_term = atoi(maybe_term + 1);
		}
		if (!election_active)
			return;
		if (grant_term > current_rale_state.current_term)
			return; /* ignore stale grant from higher term we didn't join */
		votes_received++;
		/* Majority? */
		if (votes_received > (int)(cluster.node_count / 2))
		{
			rale_become_leader();
		}
	}
	else if (strncmp(msg, "ELECTION_TIMEOUT", 15) == 0)
	{
		time_t elapsed_time;
		time_t current_time = time(NULL);
		
		elapsed_time = current_time - current_rale_state.last_heartbeat;
		
		if (elapsed_time > (time_t)rale_config.dstore.keep_alive_timeout)
		{
			rale_debug_log("Starting election due to timeout");
			current_rale_state.role = rale_role_candidate;
			current_rale_state.current_term++;
			current_rale_state.voted_for = rale_config.node.id;
			votes_received = 1; /* vote for self */
			election_active = 1;
			current_rale_state.election_deadline = compute_election_deadline();
			(void) rale_state_save(&current_rale_state);
			next_vote_request_at = 0; /* send now */
			
			for (i = 0; i < cluster.node_count; i++)
			{
				if (cluster.nodes[i].id == rale_config.node.id)
					continue; /* skip self */
				snprintf(response, sizeof(response), "VOTE_REQUEST %d %d", rale_config.node.id, current_rale_state.current_term);
				rale_send_message(response, cluster.nodes[i].ip, cluster.nodes[i].rale_port);
			}
		}
	}
	else
	{
		rale_debug_log("Unknown message type: %s", msg);
	}
}

static void
rale_send_message(const char *message,
				 const char *target_ip,
				 int target_port)
{
	connection_t *conn;
	int result;

	if (message == NULL || target_ip == NULL)
		return;

	conn = udp_client_init(target_port, NULL);
	if (conn == NULL)
	{
		rale_set_error(RALE_ERROR_NETWORK_INIT, "rale_send_message",
					   "Failed to create UDP client",
					   "UDP client initialization failed",
					   "Check network configuration and port availability");
		return;
	}

	result = udp_sendto(conn, message, target_ip, target_port);
	if (result != 0)
	{
		rale_set_error(RALE_ERROR_NETWORK_UNREACHABLE, "rale_send_message",
					   "Failed to send message",
					   "UDP message transmission failed",
					   "Check network connectivity and target availability");
	}

	udp_destroy(conn);
}

librale_status_t
rale_init(const config_t *config)
{
	if (config == NULL)
	{
		return RALE_ERROR_GENERAL;
	}

	/* Initialize cluster first so membership is available to RALE */
	if (cluster_init() != RALE_SUCCESS)
	{
		return RALE_ERROR_GENERAL;
	}

	/* Set self id for cluster subsystem */
	if (cluster_set_self_id(config->node.id) != RALE_SUCCESS)
	{
		return RALE_ERROR_GENERAL;
	}

	if (rale_proto_init(config->node.rale_port, config) != 0)
	{
		return RALE_ERROR_GENERAL;
	}

	/* Initialize DStore only; application is responsible for running loops */
	if (dstore_init(config->node.dstore_port, config) != 0)
	{
		return RALE_ERROR_GENERAL;
	}
	dstore_initialized = 1;

	return RALE_SUCCESS;
}

librale_status_t
rale_finit(void)
{
	memset(&current_rale_state, 0, sizeof(current_rale_state));
	current_rale_state.role = rale_role_follower;
	if (rale_udp_conn)
	{
		udp_destroy(rale_udp_conn);
		rale_udp_conn = NULL;
	}
	if (dstore_initialized)
	{
		(void) dstore_finit(NULL, 0);
		dstore_initialized = 0;
	}
	
	return RALE_SUCCESS;
}

static void
rale_send_heartbeat(void)
{
	char heartbeat_msg[64];
	uint32_t i;
	snprintf(heartbeat_msg, sizeof(heartbeat_msg), "HEARTBEAT %d %d",
			 rale_config.node.id, current_rale_state.current_term);
	for (i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id == rale_config.node.id)
			continue; /* skip self */
		rale_send_message(heartbeat_msg, cluster.nodes[i].ip, cluster.nodes[i].rale_port);
	}
	next_heartbeat_at = time(NULL) + get_heartbeat_interval();
}

static void
rale_process_client_requests(void)
{
}

static void
rale_request_votes(void)
{
	char vote_request[64];
	uint32_t i;
	snprintf(vote_request, sizeof(vote_request), "VOTE_REQUEST %d %d",
			 rale_config.node.id, current_rale_state.current_term);
	for (i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id == rale_config.node.id)
			continue; /* skip self */
		rale_send_message(vote_request, cluster.nodes[i].ip, cluster.nodes[i].rale_port);
	}
	next_vote_request_at = time(NULL) + 1; /* throttle to 1s */
}

static void
rale_start_election(void)
{
	current_rale_state.current_term++;
	current_rale_state.voted_for = rale_config.node.id;
	current_rale_state.role = rale_role_candidate;
	election_active = 1;
	votes_received = 1; /* self-vote */
	current_rale_state.election_deadline = compute_election_deadline();
	(void) rale_state_save(&current_rale_state);
    
	rale_request_votes();
}

static void
rale_handle_leader_duties(void)
{
	if (time(NULL) >= next_heartbeat_at)
		rale_send_heartbeat();
    
	rale_process_client_requests();
}

static void
rale_handle_candidate_duties(void)
{
	if (time(NULL) >= next_vote_request_at)
		rale_request_votes();
	
	if (time(NULL) > current_rale_state.election_deadline)
	{
		rale_start_election();
	}
}

static void
rale_handle_follower_duties(void)
{
	if (time(NULL) > current_rale_state.last_heartbeat + get_keep_alive_timeout())
	{
		current_rale_state.role = rale_role_candidate;
		rale_start_election();
	}
}

librale_status_t
rale_quram_process(void)
{
	/* Poll UDP for protocol messages */
	if (rale_udp_conn)
		udp_process_messages(rale_udp_conn);

	if (current_rale_state.role == rale_role_leader)
	{
		rale_handle_leader_duties();
	}
	else if (current_rale_state.role == rale_role_candidate)
	{
		rale_handle_candidate_duties();
	}
	else
	{
		rale_handle_follower_duties();
	}
	
	return RALE_SUCCESS;
}

static const char *rale_role_to_str(rale_role_t role)
{
	switch (role)
	{
		case rale_role_follower: return "follower";
		case rale_role_candidate: return "candidate";
		case rale_role_leader: return "leader";
		case rale_role_transitioning: return "transitioning";
		default: return "unknown";
	}
}

int
rale_get_status(char *status, size_t status_size)
{
	if (status == NULL || status_size == 0)
		return -1;

	int n = snprintf(status, status_size,
					 "role=%s term=%d leader=%d voted_for=%d last_heartbeat=%ld deadline=%ld",
					 rale_role_to_str(current_rale_state.role),
					 current_rale_state.current_term,
					 current_rale_state.leader_id,
					 current_rale_state.voted_for,
					 (long) current_rale_state.last_heartbeat,
					 (long) current_rale_state.election_deadline);
	return (n < 0 || (size_t)n >= status_size) ? -1 : 0;
}

int
rale_process_command(const char *command, char *response, size_t response_size)
{
	if (command == NULL)
		return -1;

	if (strncmp(command, "STATUS", 6) == 0)
	{
		if (response && response_size > 0)
			return rale_get_status(response, response_size);
		return 0;
	}
	else if (strncmp(command, "TRIGGER_ELECTION", 16) == 0)
	{
		/* Force an election soon by expiring heartbeat */
		current_rale_state.last_heartbeat = 0;
		current_rale_state.election_deadline = time(NULL);
		if (response && response_size > 0)
			snprintf(response, response_size, "OK");
		return 0;
	}
	else if (strncmp(command, "STEP_DOWN", 9) == 0)
	{
		/* Transition to follower */
		rale_become_follower(-1);
		if (response && response_size > 0)
			snprintf(response, response_size, "OK");
		return 0;
	}

	if (response && response_size > 0)
		snprintf(response, response_size, "UNKNOWN_COMMAND");
	return -1;
}
