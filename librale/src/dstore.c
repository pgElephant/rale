/*-------------------------------------------------------------------------
 *
 * dstore.c
 *    Implementation for dstore.c
 *
 * Shared library for RALE (Reliable Automatic Log Engine) component.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/dstore.c
 *
 *------------------------------------------------------------------------- */

/*-------------------------------------------------------------------------
 *
 * dstore.c
 *		Distributed Log Store for RALE.
 *
 *		This module is responsible for managing the distributed log,
 *		handling client connections, and replicating data to followers. It
 *		integrates with the RALE consensus protocol by persisting log entries
 *		and state transitions.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		librale/src/dstore.c
 *
 *-------------------------------------------------------------------------
 */

/** System headers */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** Local headers */
#include "librale_internal.h"
#include "shutdown.h"

/** Constants */
#define MODULE							"DSTORE"
#define TCP_SERVER_BUFFER_SIZE		1024
#define MAX_NODES					10
#define KEEP_ALIVE_MESSAGE			"KEEP_ALIVE"
#define CONNECTION_RETRY_INTERVAL	5		/** Retry connections every 5 seconds */
#define REPLICATION_MESSAGE_BUFFER_SIZE (MAX_KEY_SIZE + MAX_VALUE_SIZE + 10)
	/** "PUT " + key + "=" + value + null + leeway */

/** Default keep-alive interval if not configured */
#define DEFAULT_KEEP_ALIVE_INTERVAL 5

/** Static variables */
static tcp_server_t *tcp_server_ptr;
static tcp_client_t *tcp_clients[MAX_NODES];
static config_t dstore_config;
static time_t last_keep_alive_sent[MAX_NODES];	/** Track last keep-alive time for each node */
static int connection_status[MAX_NODES];		/** Track connection status for each node */
static time_t last_connection_attempt[MAX_NODES];	/** Track connection attempt time */
static int connection_attempt_count[MAX_NODES];	/** Track connection attempt count */
static int client_socket_to_node[TCP_SERVER_MAX_CLIENTS];	/** Map client socket index to node ID */

/** Helper function to get current keep-alive interval */
static int
get_keep_alive_interval(void)
{
	/** Use configuration if available, otherwise fall back to default */
	if (dstore_config.dstore.keep_alive_interval > 0)
		                return (int)dstore_config.dstore.keep_alive_interval;

	return DEFAULT_KEEP_ALIVE_INTERVAL;
}

/** Function prototypes */
static void dstore_server_on_connection(
	int client_sock_idx,
	const char *client_ip,
	int client_port);
static void dstore_server_on_receive(
	int client_sock_idx,
	const char *message);
static void dstore_client_on_receive(
	int client_sock,
	const char *message);
static void dstore_server_on_disconnection(
	int client_sock_idx,
	const char *client_ip,
	int client_port);
static void dstore_client_on_disconnection(
	int client_sock,
	const char *client_ip,
	int client_port);
static void dstore_init_client(uint32_t node_idx);
static int dstore_init_server(int port);
static void dstore_save_to_rale_db(const char *key, const char *value);

static void dstore_send_keep_alive(void);
static void dstore_server_send_keep_alive(void);
int dstore_is_current_leader(void);
int dstore_get_current_leader(void);
static void dstore_send_cluster_snapshot_to_client(int client_sock_idx);
static void dstore_send_cluster_snapshot_to_target_idx(uint32_t node_idx);
int dstore_is_node_connected(int node_id);
static void dstore_broadcast_leader_snapshot(int term, int leader_id);

/**
 * Write RALE leader state to the local rale.state file.
 * Attempts to preserve existing fields when possible.
 */
static void
write_rale_state_leader(int term, int leader_id)
{
	char rale_state_path[512];
	FILE *fp;
	int current_term = -1;
	int voted_for = -1;
	int existing_leader_id = -1;
	int last_log_index = -1;
	int last_log_term = -1;

	/** Construct path using configured db path */
	snprintf(rale_state_path, sizeof(rale_state_path), "%s/rale.state",
		 dstore_config.db.path);

	/** Try to read existing state to preserve other fields */
	fp = fopen(rale_state_path, "r");
	if (fp != NULL)
	{
		char line[256];
		if (fgets(line, sizeof(line), fp) != NULL)
		{
			/* Expected format: current_term voted_for leader_id last_log_index last_log_term */
			char *saveptr = NULL;
			char *token = strtok_r(line, " \n", &saveptr);
			if (token) current_term = atoi(token);
			token = strtok_r(NULL, " \n", &saveptr);
			if (token) voted_for = atoi(token);
			token = strtok_r(NULL, " \n", &saveptr);
			if (token) existing_leader_id = atoi(token);
			token = strtok_r(NULL, " \n", &saveptr);
			if (token) last_log_index = atoi(token);
			token = strtok_r(NULL, " \n", &saveptr);
			if (token) last_log_term = atoi(token);
		}
		fclose(fp);
	}

	/* Overwrite with provided leader and term (if term is non-negative) */
	if (term >= 0)
		current_term = term;
	existing_leader_id = leader_id;

	fp = fopen(rale_state_path, "w");
	if (fp == NULL)
	{
		rale_debug_log("Failed to open %s for writing leader state", rale_state_path);
		return;
	}
	
	/** Use snprintf for safe formatting */
	char state_line[256];
	snprintf(state_line, sizeof(state_line), "%d %d %d %d %d\n",
		(current_term >= 0 ? current_term : 0),
		(voted_for >= 0 ? voted_for : -1),
		existing_leader_id,
		(last_log_index >= 0 ? last_log_index : 0),
		(last_log_term >= 0 ? last_log_term : 0));
	fputs(state_line, fp);
	fclose(fp);
	rale_debug_log("Cluster leadership state updated: leader_id=%d, term=%d",
		leader_id, current_term);
}

/** Propagation functions for automatic cluster management */
int dstore_propagate_node_addition(int32_t new_node_id, const char *name, 
								  const char *ip, uint16_t rale_port, uint16_t dstore_port);
int dstore_propagate_node_removal(int node_id);
static int dstore_handle_propagated_add(const char *command);
static int dstore_handle_propagated_remove(const char *command);

/** External variables */
extern cluster_t cluster;
extern volatile int system_exit;

/**
 * dstore_save_to_rale_db
 *
 * Appends a key-value pair to the rale.db file in the configured database path.
 *
 * @param key   The key to store.
 * @param value The value to store.
 */
static void
dstore_save_to_rale_db(const char *key, const char *value)
{
	char db_path[512];
	FILE *fp;

	/** Construct path to rale.db in the configured database path */
	snprintf(db_path, sizeof(db_path), "%s/rale.db",
		dstore_config.db.path);

	fp = fopen(db_path, "a");
	if (fp == NULL)
	{
		return;
	}

	/** Write key-value pair to file */
	char kv_line[1024];
	snprintf(kv_line, sizeof(kv_line), "%s=%s\n", key, value);
	fputs(kv_line, fp);
	fclose(fp);
}

int
dstore_init(const uint16_t dstore_port, const config_t *config)
{
	int i;
	int ret;

	rale_debug_log(
		"Initializing DStore subsystem: port %d, configuration %p",
		dstore_port, (void *)config);

	if (config != NULL)
	{
		                memcpy(&dstore_config, config, sizeof(config_t));
	}

	/** Initialize connection status and keep-alive arrays */
	for (i = 0; i < MAX_NODES; i++)
	{
		connection_status[i] = 0; /** Mark all nodes as disconnected initially */
		last_keep_alive_sent[i] = 0; /** Initialize keep-alive timers */
		last_connection_attempt[i] = 0; /** Initialize connection attempt timers */
		connection_attempt_count[i] = 0; /** Initialize connection attempt counts */
	}
	
	/** Initialize client socket to node mapping */
	for (i = 0; i < TCP_SERVER_MAX_CLIENTS; i++)
	{
		client_socket_to_node[i] = -1; /** Mark as unused */
	}

	rale_debug_log(
		"Starting DStore server initialization on port %d",
		dstore_port);
		
	ret = dstore_init_server(dstore_port);
	rale_debug_log(
		"dstore_init_server returned %d", ret);
	if (ret < 0)
	{
		rale_set_error_fmt(RALE_ERROR_NETWORK_INIT, "dstore_init",
			"DStore server initialization failed on port %d", dstore_port);
		return -1;
	}
	
	/** Start the server to ensure it's listening before any client connections */
	if (tcp_server_ptr != NULL)
	{
		rale_debug_log(
			"DStore server started and listening on port %d, tcp_server_ptr=%p",
			dstore_port, (void *)tcp_server_ptr);
	}
	else
	{
		rale_set_error_fmt(RALE_ERROR_NETWORK_INIT, "dstore_init",
			"DStore server pointer is NULL after initialization on port %d", dstore_port);
		return -1;
	}
	
	dlog_init();
	return 0;
}

/**
 * Helper function to find the array index for a given node ID
 */
static uint32_t
find_node_index_by_id(int node_id)
{
	uint32_t i;

	for (i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id == node_id)
		{
			return i;
		}
	}
	return MAX_NODES; /** Node not found */
}

/**
 * Initialize the server component of the dstore.
 */
static int
dstore_init_server(int port)
{
	rale_debug_log(
		"Initializing DStore server on port %d with connection callback",
		port);
		
	/** Allow initialization even when cluster is empty */
	tcp_server_ptr = tcp_server_init(
		port,
		dstore_server_on_connection,
		dstore_server_on_disconnection,
		dstore_server_on_receive);
	rale_debug_log(
		"tcp_server_init returned %p", (void *)tcp_server_ptr);
	if (tcp_server_ptr == NULL)
	{
		rale_set_error_fmt(RALE_ERROR_NETWORK_INIT, "init_dstore_server",
			"Failed to initialize TCP server on port %d", port);
		return -1;
	}
	
	rale_debug_log(
		"DStore server initialized successfully on port %d, tcp_server_ptr=%p",
		port, (void *)tcp_server_ptr);
	return 0;
}

/**
 * Initialize a client connection to a specific node (by index).
 */
static void
dstore_init_client(uint32_t node_idx)
{
	rale_debug_log("Initializing DStore client connection: Node %d establishing connection to target index %d",
		cluster.self_id, node_idx);

	if (cluster.node_count == 0) /** Use cluster.node_count */
	{
		rale_debug_log("DStore client initialization failed: Cluster not yet initialized (target node index: %d)",
			node_idx);
		return;
	}

	if (node_idx < 0 || node_idx >= MAX_NODES)
	{
		rale_debug_log("DStore client initialization failed: Invalid node index %d (valid range: 0-%d, requesting node: %d)",
			node_idx, MAX_NODES - 1, cluster.self_id);
		return;
	}

	if (node_idx >= cluster.node_count) /** Use cluster.node_count */
	{
		rale_debug_log("DStore client initialization failed: Node index %d exceeds cluster size %d (requesting node: %d)",
			node_idx, cluster.node_count, cluster.self_id);
		return;
	}

	/** Check if the target node is configured (not id -1) */
	if (cluster.nodes[node_idx].id == -1)
	{
		rale_debug_log("DStore client initialization failed: Target node at index %d is not configured (requesting node: %d)",
			node_idx, cluster.self_id);
		return;
	}

	if (tcp_clients[node_idx] != NULL)
	{
		rale_debug_log("DStore client connection already established for node index %d (requesting node: %d)",
			node_idx, cluster.self_id);
		return;
	}

	rale_debug_log("Creating TCP client connection: target node index %d at %s:%d (requesting node: %d)",
		node_idx, cluster.nodes[node_idx].ip,
		cluster.nodes[node_idx].dstore_port, cluster.self_id);

	/**
	 * CreateTCPClient is expected to return a malloc'd TCPClient struct,
	 * or NULL on failure.
	 */
	tcp_clients[node_idx] = tcp_client_init(cluster.nodes[node_idx].ip,
										   cluster.nodes[node_idx].dstore_port,
										   dstore_client_on_receive,
										   dstore_client_on_disconnection,
										   NULL, 0);
	if (tcp_clients[node_idx] == NULL)
	{
		rale_debug_log("failed to create TCPClient for node_idx %d (IP: %s, Port: %d) from self_id %d",
			node_idx, cluster.nodes[node_idx].ip,
			cluster.nodes[node_idx].dstore_port, cluster.self_id);
		return;
	}

	rale_debug_log("DStore client connection established: Node %d successfully connected to target index %d at %s:%d",
		cluster.self_id, node_idx, cluster.nodes[node_idx].ip,
		cluster.nodes[node_idx].dstore_port);
}

static void
dstore_server_on_connection(
	int client_sock_idx,
	const char *client_ip,
	int client_port)
{
	uint32_t i;
	uint32_t node_idx = 0;

	rale_debug_log(
		"New DStore client connection established: socket index %d from %s:%d",
		client_sock_idx, client_ip, client_port);

	/** Find which node this connection belongs to */
	for (i = 0; i < cluster.node_count; i++)
	{
		/** Match by IP address only, since client port is random */
		/** Skip if this is our own node (same ID) */
		if (strcmp(cluster.nodes[i].ip, client_ip) == 0 && 
			cluster.nodes[i].id != cluster.self_id)
		{
			                                                                        node_idx = i;
			break;
		}
	}

    /**
     * Do not guess peer identity by IP:port; wait for HELLO to map the socket
     * and mark connection_status. This avoids misattribution on 127.0.0.1.
     */
    client_socket_to_node[client_sock_idx] = -1;
    rale_debug_log(
             "DStore connection ESTABLISHED from %s:%d to our Node %d (socket %d) — awaiting HELLO",
             client_ip, client_port, cluster.self_id, client_sock_idx);

	/** Log cluster state for debugging */
	rale_debug_log("Current cluster.self_id=%d, cluster.node_count=%d",
		cluster.self_id, cluster.node_count);
	for (i = 0; i < cluster.node_count; i++)
	{
		rale_debug_log("Node %d: id=%d, ip=%s, dstore_port=%d, connected=%d",
			i, cluster.nodes[i].id, cluster.nodes[i].ip,
			cluster.nodes[i].dstore_port, connection_status[i]);
	}
	
    /**
     * Do not mark connection_status by IP-guess. We will mark the peer as
     * connected only after receiving a HELLO with its node_id in
     * dstore_server_on_receive().
     */

		/** Send immediate keep-alive response to establish bidirectional communication */
	if (node_idx != MAX_NODES && tcp_server_ptr != NULL)
	{
		tcp_server_send(tcp_server_ptr, client_sock_idx, KEEP_ALIVE_MESSAGE);
		rale_debug_log("DStore keep-alive response sent: Server (Node %d) -> Client (Node %d)",
			cluster.self_id, cluster.nodes[node_idx].id);

        /** Send our current cluster snapshot so the peer learns all nodes */
        dstore_send_cluster_snapshot_to_client(client_sock_idx);

			/** Also send leader snapshot so a newly connected follower learns leader */
			{
				int leader_id = dstore_get_current_leader();
				char leader_msg[128];
				int term = 0;
				if (leader_id >= 0)
				{
					snprintf(leader_msg, sizeof(leader_msg), "LEADER %d %d", term, leader_id);
					tcp_server_send(tcp_server_ptr, client_sock_idx, leader_msg);
					rale_debug_log(
						"Sent leader snapshot to Node %d: %s",
						cluster.nodes[node_idx].id, leader_msg);
				}
			}
	}
}

static void
dstore_server_on_receive(
	int client_sock_idx,
	const char *message)
{
    char buf[1024];
    char *saveptr = NULL;
    char *line;

    rale_debug_log("Server (self_id %d) received from client socket_idx %d: \"%s\"",
        (cluster.self_id >= 0) ? cluster.self_id : -1, client_sock_idx,
        message);

    strncpy(buf, message, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    line = strtok_r(buf, "\n", &saveptr);
    while (line != NULL)
    {
        if (strcmp(line, KEEP_ALIVE_MESSAGE) == 0)
        {
            rale_debug_log(
                "DStore keep-alive received: Server (Node %d) from client socket %d",
                cluster.self_id, client_sock_idx);
        }
        else if (*line != '\0')
        {
			/** Handle client identity message to improve mapping */
			if (strncmp(line, "HELLO ", 6) == 0)
			{
				int peer_id = -1;
				const char *peer_str = line + 6;
				/** Manual parsing for safety */
				if (*peer_str != '\0' && isdigit(*peer_str))
				{
					peer_id = atoi(peer_str);
				}
				
				if (peer_id >= 0)
				{
					uint32_t peer_idx = find_node_index_by_id(peer_id);
					if (peer_idx != MAX_NODES)
					{
						client_socket_to_node[client_sock_idx] = peer_id;
						connection_status[peer_idx] = 1;
						last_keep_alive_sent[peer_idx] = time(NULL);
						rale_debug_log(
							"DStore HELLO mapped client socket %d to Node %d",
							client_sock_idx, peer_id);
					}
				}
				line = strtok_r(NULL, "\n", &saveptr);
				continue;
			}

            /*
             * Key-value commands:
             * - GET key           → respond VALUE/NOT_FOUND
             * - DELETE key        → leader applies, replicates; follower forwards
             * - FORWARD_DELETE k  → leader applies and replicates (from follower)
             * - PUT key=value     → existing handler
             */
            if (strncmp(line, "GET ", 4) == 0)
            {
                const char *key = line + 4;
                char        value[MAX_VALUE_SIZE];
                char        resp[512];

                if (db_get(key, value, sizeof(value), NULL, 0) == 0)
                {
                    snprintf(resp, sizeof(resp), "VALUE %s=%s", key, value);
                    if (tcp_server_ptr != NULL)
                        tcp_server_send(tcp_server_ptr, client_sock_idx, resp);
                }
                else
                {
                    snprintf(resp, sizeof(resp), "NOT_FOUND %s", key);
                    if (tcp_server_ptr != NULL)
                        tcp_server_send(tcp_server_ptr, client_sock_idx, resp);
                }
            }
            else if (strncmp(line, "FORWARD_DELETE ", 15) == 0 ||
                     strncmp(line, "DELETE ", 7) == 0)
            {
                const int   is_forward = (strncmp(line, "FORWARD_DELETE ", 15) == 0);
                const char *key = line + (is_forward ? 15 : 7);

                if (!is_forward && !dstore_is_current_leader())
                {
                    int current_leader = dstore_get_current_leader();
                    if (current_leader != -1 && current_leader != cluster.self_id)
                    {
                        uint32_t leader_idx = find_node_index_by_id(current_leader);
                        if (leader_idx != MAX_NODES && connection_status[leader_idx] == 1)
                        {
                            char fwd[512];
                            snprintf(fwd, sizeof(fwd), "FORWARD_DELETE %s", key);
                            dstore_send_message(leader_idx, fwd);
                        }
                    }
                }
                else
                {
                    (void) db_delete(key, NULL, 0);

                    {
                        char msg[512];
                        snprintf(msg, sizeof(msg), "DELETE %s", key);
                        for (uint32_t i = 0; i < cluster.node_count; i++)
                        {
                            if (cluster.nodes[i].id == cluster.self_id)
                                continue;
                            if (tcp_clients[i] != NULL && tcp_clients[i]->is_connected)
                                (void) dstore_send_message(i, msg);
                        }
                    }
                }
            }
            else
            {
                dstore_put_from_command(line, NULL, 0);
            }
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
}

static void
dstore_client_on_receive(
	int client_sock,
	const char *message)
{
    char buf[1024];
    char *saveptr = NULL;
    char *line;

    rale_debug_log("Client (self_id %d) received from server_sock %d: \"%s\"",
        (cluster.self_id >= 0) ? cluster.self_id : -1, client_sock,
        message);

    strncpy(buf, message, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    line = strtok_r(buf, "\n", &saveptr);
    while (line != NULL)
    {
        if (strcmp(line, KEEP_ALIVE_MESSAGE) == 0)
        {
            rale_debug_log(
                "DStore keep-alive received: Client (Node %d) from server socket %d",
                cluster.self_id, client_sock);
        }
        else if (*line != '\0')
        {
            dstore_put_from_command(line, NULL, 0);
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
}

/**
 * Main loop for client connections. Attempts to connect/reconnect to other nodes.
 */
int
dstore_client_loop(char *errbuf, size_t errbuflen)
{
	uint32_t i;
	int ret;
	time_t current_time = time(NULL);

	(void) errbuf;
	(void) errbuflen;

	if (cluster.node_count == 0) /** Use cluster.node_count */
	{
		rale_debug_log("Cluster not initialized in dstore_client_loop.");
		return -1; /** Or some other appropriate action */
	}

	/** Check if we have other nodes to connect to */
	if (cluster.node_count == 1)
	{
    	return 0; /** No other nodes to connect to */
	}

	/** Add a small delay to ensure server is fully ready */
	static int first_run = 1;
	if (first_run)
	{
		rale_debug_log("%s", "First client loop run, ensuring server is ready...");
		first_run = 0;
		return 0; /** Skip first run to let server fully initialize */
	}

	/** Handle peer-to-peer connections for all nodes */
	for (i = 0; i < MAX_NODES; i++)
	{
		if (i >= cluster.node_count || cluster.nodes[i].id == -1 ||
			cluster.nodes[i].id == cluster.self_id)
		{
			continue;
		}

		/**
		 * Initiate client connections to all peers (except self). Allowing
		 * bidirectional TCP links ensures each node can independently mark
		 * peers online and avoids asymmetric "offline" views.
		 */

		if (tcp_clients[i] == NULL)
		{
			                                                                        dstore_init_client(i);
			if (tcp_clients[i] == NULL) /** Check if init failed */
			{
				char warn_msg[256];
				snprintf(warn_msg, sizeof(warn_msg), 
					"Failed to initialize client for node_idx %d in client loop",
					i);
				/* Warning: initialization failed for TCP client */
				continue;
			}
		}

		if (tcp_clients[i]->is_connected == 1)
		{
			tcp_client_run(tcp_clients[i]); /** Process any incoming messages for this client */
			continue;
		}

		/** Check if enough time has passed since last connection attempt */
		/** Use exponential backoff for persistent failures */
		int retry_interval = CONNECTION_RETRY_INTERVAL;
		if (connection_attempt_count[i] > 5) {
			retry_interval = CONNECTION_RETRY_INTERVAL * 2; /** 10 seconds after 5 failures */
		}
		if (connection_attempt_count[i] > 10) {
			retry_interval = CONNECTION_RETRY_INTERVAL * 4; /** 20 seconds after 10 failures */
		}
		
		if (current_time - last_connection_attempt[i] < retry_interval)
		{
			continue; /** Wait before retrying */
		}

		last_connection_attempt[i] = current_time;
		connection_attempt_count[i]++;

		/** Attempt to connect if not connected */
		/* Reduce logging noise - only log every 5th attempt or first attempt */
		if (connection_attempt_count[i] == 1 || connection_attempt_count[i] % 5 == 0) {
			rale_debug_log(
				"Node (%d) attempting to connect to node_idx %d (IP: %s, Port: %d) - Attempt %d",
				cluster.self_id, i, cluster.nodes[i].ip,
				cluster.nodes[i].dstore_port, connection_attempt_count[i]);
		}

		ret = tcp_client_connect(tcp_clients[i],
								cluster.nodes[i].ip,
								cluster.nodes[i].dstore_port);
		if (ret != 0)
		{
			char warn_msg[256];
			snprintf(warn_msg, sizeof(warn_msg), 
				"Node (%d) failed to connect to node_idx %d (IP: %s, Port: %d). "
				"TCP connect error: %d. Server may not be ready yet. "
				"Will retry in %d seconds.",
				cluster.self_id, i, cluster.nodes[i].ip,
				cluster.nodes[i].dstore_port, ret, CONNECTION_RETRY_INTERVAL);
			/* Warning: initialization failed for TCP client */
			connection_status[i] = 0; /** Mark as disconnected */
			continue;
		}
		
		/** Connection successful - log with detailed information */
		rale_debug_log(
			"DStore connection ESTABLISHED: Our raled Node %d successfully connected to raled Node %d (%s:%d)",
			cluster.self_id, cluster.nodes[i].id, cluster.nodes[i].ip,
			cluster.nodes[i].dstore_port);
		connection_status[i] = 1; /** Mark as connected */
		cluster.nodes[i].state = NODE_STATE_CANDIDATE; /** Mark peer reachable */
		last_keep_alive_sent[i] = current_time; /** Initialize keep-alive timer */
		connection_attempt_count[i] = 0; /** Reset attempt count on success */
		
		/** Identify ourselves to the server so it can map this socket to our node_id */
		{
			char hello_msg[64];
			snprintf(hello_msg, sizeof(hello_msg), "HELLO %d", cluster.self_id);
			tcp_client_send(tcp_clients[i], hello_msg);
		rale_debug_log("DStore HELLO sent: Node %d -> Node %d: %s",
			cluster.self_id, cluster.nodes[i].id, hello_msg);
		}

		/** Send initial keep-alive message */
		tcp_client_send(tcp_clients[i], KEEP_ALIVE_MESSAGE);
		rale_debug_log(
			"DStore keep-alive sent: Node %d -> Node %d",
			cluster.self_id, cluster.nodes[i].id);

		/** Send our current cluster snapshot so the server learns about all nodes */
		                                                dstore_send_cluster_snapshot_to_target_idx(i);
	}
	
	/** Send periodic keep-alive messages to all connected nodes */
	dstore_send_keep_alive();
	
	return 0;
}

/**
 * Non-blocking client tick - process one client iteration
 * Called by daemon in its main loop
 */
int
dstore_client_tick(void)
{
	static int first_run = 1;
	static time_t last_connect_attempt = 0;
	time_t current_time = time(NULL);
	uint32_t i;
	int ret;

	/** Check basic state */
	if (cluster.node_count == 0)
	{
		return 0; /** No cluster yet */
	}

	if (cluster.node_count == 1)
	{
		return 0; /** No other nodes to connect to */
	}

	/** Skip first run to let server initialize */
	if (first_run)
	{
		first_run = 0;
		return 0;
	}

	/** Only attempt connections periodically, not every tick */
	if (current_time - last_connect_attempt < 1)
	{
		/** Still process existing connections */
		for (i = 0; i < MAX_NODES; i++)
		{
			if (i >= cluster.node_count || cluster.nodes[i].id == -1 ||
				cluster.nodes[i].id == cluster.self_id)
			{
				continue;
			}

			if (tcp_clients[i] != NULL && tcp_clients[i]->is_connected == 1)
			{
				tcp_client_run(tcp_clients[i]);
			}
		}
		return 0;
	}

	last_connect_attempt = current_time;

	/** Process connection attempts for one node per tick to avoid blocking */
	for (i = 0; i < MAX_NODES; i++)
	{
		if (i >= cluster.node_count || cluster.nodes[i].id == -1 ||
			cluster.nodes[i].id == cluster.self_id)
		{
			continue;
		}

		/** Initialize client if needed */
		if (tcp_clients[i] == NULL)
		{
			dstore_init_client(i);
			if (tcp_clients[i] == NULL)
			{
				continue;
			}
		}

		/** Process existing connection */
		if (tcp_clients[i]->is_connected == 1)
		{
			tcp_client_run(tcp_clients[i]);
			continue;
		}

		/** Attempt connection with backoff */
		int retry_interval = CONNECTION_RETRY_INTERVAL;
		if (connection_attempt_count[i] > 5) {
			retry_interval = CONNECTION_RETRY_INTERVAL * 2;
		}
		if (connection_attempt_count[i] > 10) {
			retry_interval = CONNECTION_RETRY_INTERVAL * 4;
		}
		
		if (current_time - last_connection_attempt[i] < retry_interval)
		{
			continue;
		}

		last_connection_attempt[i] = current_time;
		connection_attempt_count[i]++;

		/** Attempt to connect */
		ret = tcp_client_connect(tcp_clients[i],
								cluster.nodes[i].ip,
								cluster.nodes[i].dstore_port);
		if (ret == 0)
		{
			/** Connection successful */
			rale_debug_log("DStore connection ESTABLISHED: Node %d -> Node %d (%s:%d)",
				cluster.self_id, cluster.nodes[i].id, cluster.nodes[i].ip,
				cluster.nodes[i].dstore_port);
			connection_status[i] = 1;
			cluster.nodes[i].state = NODE_STATE_CANDIDATE;
			last_keep_alive_sent[i] = current_time;
			connection_attempt_count[i] = 0;
			
			/** Send hello and initial keep-alive */
			char hello_msg[64];
			snprintf(hello_msg, sizeof(hello_msg), "HELLO %d", cluster.self_id);
			tcp_client_send(tcp_clients[i], hello_msg);
			tcp_client_send(tcp_clients[i], KEEP_ALIVE_MESSAGE);
			dstore_send_cluster_snapshot_to_target_idx(i);
		}
		
		/** Only process one connection attempt per tick to avoid blocking */
		break;
	}

	return 0;
}

/**
 * Send current cluster membership to a connected client target by node index.
 */
static void
dstore_send_cluster_snapshot_to_target_idx(uint32_t node_idx)
{
    char cmd[256];

    if (node_idx >= cluster.node_count)
        return;
    if (tcp_clients[node_idx] == NULL || tcp_clients[node_idx]->is_connected != 1)
        return;

    for (uint32_t j = 0; j < cluster.node_count; j++)
    {
        if (cluster.nodes[j].id == -1)
            continue;
        snprintf(cmd, sizeof(cmd),
                 "PROPAGATE_ADD %d %s %s %d %d",
                 cluster.nodes[j].id,
                 cluster.nodes[j].name,
                 cluster.nodes[j].ip,
                 cluster.nodes[j].rale_port,
                 cluster.nodes[j].dstore_port);
        tcp_client_send(tcp_clients[node_idx], cmd);
        char debug_msg[256];
        snprintf(debug_msg, sizeof(debug_msg), 
             "Sent snapshot entry to node_idx %d: %s",
             node_idx, cmd);
        rale_debug_log("%s", debug_msg);
    }

	/** Also send leader snapshot so the server peer learns the current leader */
	{
		int leader_id = dstore_get_current_leader();
		char leader_msg[128];
		int term = 0;
		if (leader_id >= 0)
		{
			snprintf(leader_msg, sizeof(leader_msg), "LEADER %d %d", term, leader_id);
			tcp_client_send(tcp_clients[node_idx], leader_msg);
			rale_debug_log(
				"Sent leader snapshot to node_idx %d: %s",
				node_idx, leader_msg);
		}
	}
}

/**
 * Main loop for the server component. Initializes server if needed and runs it.
 */
void
dstore_server_loop(char *errbuf, size_t errbuflen)
{
	uint32_t self_idx;

	(void) errbuf;
	(void) errbuflen;

	/** Server should already be initialized in dstore_init(), just run it */
	if (tcp_server_ptr == NULL)
	{
		rale_set_error_fmt(RALE_ERROR_NETWORK_INIT, MODULE,
			"TCP server not initialized. Server should be initialized in dstore_init().");
		return;
	}

	/** Only check cluster state for client operations, not server operations */
	if (cluster.node_count == 0)
	{
		rale_debug_log("cluster not initialized in dstore_server_loop, skipping client operations");
		/** Still run the server to accept incoming connections */
	}
	else
	{
		/** Find the array index for self_id */
		self_idx = find_node_index_by_id(cluster.self_id);
		if (self_idx == MAX_NODES)
		{
			rale_debug_log("Self node %d not found in cluster yet, skipping client operations",
			cluster.self_id);
		}
	}

	/** Add debug before running server */
	rale_debug_log("Running tcp_server_run for self_id %d, tcp_server_ptr=%p",
		(cluster.self_id >= 0) ? cluster.self_id : -1, (void *)tcp_server_ptr);
		
	rale_debug_log("About to call tcp_server_run with server pointer %p",
		(void *)tcp_server_ptr);

	if (tcp_server_ptr == NULL)
	{
		rale_set_error_fmt(RALE_ERROR_NETWORK_INIT, MODULE,
			"tcp_server_ptr is NULL! Server not initialized properly.");
		return;
	}

	rale_debug_log("Calling tcp_server_run...");
		
	/** Main loop: keep daemon alive until shutdown signal */
	while (!librale_is_shutdown_requested("dstore"))
	{
		int result = tcp_server_run(tcp_server_ptr);
		
		/** Check shutdown more frequently for faster shutdown response */
		if (librale_is_shutdown_requested("dstore"))
		{
			rale_debug_log(
				"Shutdown requested for DStore server, beginning cleanup");
			break;
		}
		
		/** Send periodic keep-alive messages to all connected nodes */
		/** Only send keep-alive when it's time, not every loop iteration */
		static time_t last_keep_alive_check = 0;
		time_t current_time = time(NULL);
		
		if (last_keep_alive_check == 0 || 
			(current_time - last_keep_alive_check) >= get_keep_alive_interval())
		{
			dstore_send_keep_alive();
			last_keep_alive_check = current_time;
		}
		
		/** Additional check for shutdown after keep-alive */
		if (librale_is_shutdown_requested("dstore"))
		{
			rale_debug_log(
				"Shutdown requested after keep-alive, beginning cleanup");
			break;
		}
		
		/** Add small delay to prevent tight loop when no activity */
		if (result == 0) /** Timeout occurred */
		{
			usleep(100000); /** 100ms delay */
		}
	}
	
	/** Perform cleanup before exiting */
	rale_debug_log(
		"DStore server loop exiting for self_id %d, performing cleanup",
		(cluster.self_id >= 0) ? cluster.self_id : -1);
	
	/** Close all client connections gracefully */
	for (int i = 0; i < TCP_SERVER_MAX_CLIENTS; i++)
	{
		if (client_socket_to_node[i] != -1)
		{
			rale_debug_log("Closing client connection in slot %d for node %d",
				i, client_socket_to_node[i]);
			tcp_server_client_disconnect(tcp_server_ptr, i);
			client_socket_to_node[i] = -1;
		}
	}
	
	/** Clear connection status for all nodes */
	for (uint32_t i = 0; i < cluster.node_count; i++)
	{
		connection_status[i] = 0;
	}
	
	rale_debug_log(
		"DStore server cleanup completed for self_id %d",
		(cluster.self_id >= 0) ? cluster.self_id : -1);
	
	/** Signal that DStore shutdown is complete */
	librale_signal_shutdown_complete("dstore");
}

/**
 * Non-blocking server tick - process one server iteration
 * Called by daemon in its main loop
 */
int
dstore_server_tick(void)
{
	static time_t last_keep_alive_check = 0;
	time_t current_time;
	int result;

	/** Check if server is initialized */
	if (tcp_server_ptr == NULL)
	{
		rale_set_error_fmt(RALE_ERROR_NETWORK_INIT, MODULE,
			"TCP server not initialized for tick operation.");
		return -1;
	}

	/** Check shutdown state */
	if (librale_is_shutdown_requested("dstore"))
	{
		return -1; /** Signal shutdown to daemon */
	}

	/** Process one server iteration (non-blocking) */
	result = tcp_server_run(tcp_server_ptr);

	/** Handle periodic keep-alive */
	current_time = time(NULL);
	if (last_keep_alive_check == 0 || 
		(current_time - last_keep_alive_check) >= get_keep_alive_interval())
	{
		dstore_send_keep_alive();
		last_keep_alive_check = current_time;
	}

	return result;
}

/**
 * Callback for when a client disconnects from our server.
 */
static void
dstore_server_on_disconnection(
	int client_sock_idx,
	const char *client_ip,
	int client_port)
{
	static time_t last_log_time = 0;
	static int    suppressed = 0;
	time_t        now = time(NULL);

	if (last_log_time == 0 || (now - last_log_time) >= 1)
	{
		if (suppressed > 0)
		{
			rale_debug_log(
				"Suppressed %d repeated disconnect logs in last interval",
				suppressed);
			suppressed = 0;
		}
		rale_debug_log(
			"DStore connection LOST: Raled client %s:%d disconnected from our raled Node %d (socket %d)",
			client_ip, client_port, cluster.self_id, client_sock_idx);
		last_log_time = now;
	}
	else
	{
		/* Throttle noisy repeats within the same second */
		suppressed++;
	}
	/* Reduce logging noise - remove excessive DEBUG messages */
	
	/** Clear the client socket to node mapping */
	if (client_sock_idx >= 0 && client_sock_idx < TCP_SERVER_MAX_CLIENTS)
	{
		int node_id = client_socket_to_node[client_sock_idx];
		if (node_id != -1)
		{
			/** Find the node index and mark as disconnected */
			uint32_t node_idx = find_node_index_by_id(node_id);
			if (node_idx != MAX_NODES)
			{
				connection_status[node_idx] = 0; /** Mark as disconnected */
			}
			client_socket_to_node[client_sock_idx] = -1; /** Clear the mapping */
		}
	}
	
	tcp_server_client_disconnect(tcp_server_ptr, client_sock_idx);
}

/**
 * Callback for when our client connection to a server is disconnected.
 */
static void
dstore_client_on_disconnection(int client_sock, const char *client_ip,
							  int client_port)
{
	uint32_t node_idx = MAX_NODES;

	/** Find which node this disconnection belongs to */
	for (uint32_t j = 0; j < cluster.node_count; j++)
	{
		if (strcmp(cluster.nodes[j].ip, client_ip) == 0 && 
			cluster.nodes[j].dstore_port == client_port)
		{
			node_idx = j;
			break;
		}
	}

	/** Log the disconnection with detailed information */
	if (node_idx != MAX_NODES)
	{
		rale_debug_log( 
			"DStore connection LOST: Our raled Node %d disconnected from raled Node %d (%s:%d, socket %d)",
			cluster.self_id, cluster.nodes[node_idx].id, client_ip, client_port, client_sock);
		connection_status[node_idx] = 0; /** Mark as disconnected */
		cluster.nodes[node_idx].state = NODE_STATE_OFFLINE;
	}
	else
	{
		rale_debug_log( 
			"DStore connection LOST: Unknown client %s:%d disconnected (socket %d)",
			client_ip, client_port, client_sock);
	}

	rale_debug_log("Tcp_client[] pointers: %p", (void*)tcp_clients);
	for (uint32_t k = 0; k < MAX_NODES; k++)
	{
		char debug_msg2[256];
		snprintf(debug_msg2, sizeof(debug_msg2), "Tcp_client[%d]=%p", k, (void*)tcp_clients[k]);
		rale_debug_log("%s", debug_msg2);
	}
	for (uint32_t j = 0; j < MAX_NODES; j++) /** MAX_NODES should be cluster->node_count */
	{
		if (cluster.node_count == 0) /** Use cluster.node_count */
		{
			break;
		}
		if (tcp_clients[j] != NULL && tcp_clients[j]->sock == client_sock)
		{
			char client_cleanup_msg[256];
			snprintf(client_cleanup_msg, sizeof(client_cleanup_msg), 
				"cleaning up TCP client structure for node_idx %d (socket %d)",
				j, client_sock);
			rale_debug_log("%s", client_cleanup_msg);
			tcp_client_cleanup(tcp_clients[j]);
			free(tcp_clients[j]);
			tcp_clients[j] = NULL;
			connection_status[j] = 0; /** Mark as disconnected */
			break; /** Found and cleaned up the client */
		}
	}
}

/**
 * Send a message to the primary node.
 */


/**
 * Send a message to a target node.
 */
int
dstore_send_message(uint32_t target_node_idx, const char *message)
{
	const char *reason;

	if (cluster.node_count == 0) /** Use cluster.node_count */
	{
		rale_set_error_fmt(RALE_ERROR_NOT_INITIALIZED, MODULE, "Cluster not initialized for dstore_send_message.");
		return -1;
	}

	if (target_node_idx >= cluster.node_count) /** Use cluster.node_count */
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_PARAMETER, MODULE,
			"Target node_idx %d is out of bounds (max: %d).",
			target_node_idx, cluster.node_count - 1);
		return -1;
	}

	if (tcp_clients[target_node_idx] != NULL && tcp_clients[target_node_idx]->is_connected)
	{
		/** tcp_client_send is void, so we can't directly check for send errors here. */
		/** Underlying TCP implementation might handle some retries or log errors. */
		rale_debug_log("Sending message from self_id %d to node_idx %d: \"%s\"",
			cluster.self_id, target_node_idx, message);
		tcp_client_send(tcp_clients[target_node_idx], message);
		return 0;
	}
	else
	{
		reason = (tcp_clients[target_node_idx] == NULL) ? "client not initialized" :
				 "client not connected";
		rale_set_error_fmt(RALE_ERROR_NETWORK_UNREACHABLE, MODULE,
			"cannot send message from self_id %d to node_idx %d (%s)",
			cluster.self_id, target_node_idx, reason);
		return -1;
	}
}

/**
 * Replicates a key-value pair to all follower nodes.
 */
void
dstore_replicate_to_followers(const char *key, const char *value, char *errbuf, size_t errbuflen)
{
	uint32_t      i;
	char     message[REPLICATION_MESSAGE_BUFFER_SIZE];
	int      send_ret;
	const char *reason;

	(void) errbuf;
	(void) errbuflen;

	if (cluster.node_count == 0) /** Use cluster.node_count */
	{
		rale_set_error_fmt(RALE_ERROR_NOT_INITIALIZED, MODULE, "Cluster not initialized in dstore_replicate_to_followers.");
		return;
	}

	/** Construct the message */
	int written = snprintf(message, sizeof(message), "PUT %s=%s", key, value);
	if (written >= (int)sizeof(message))
	{
		rale_set_error_fmt(RALE_ERROR_MESSAGE_TOO_LARGE, MODULE, "Message buffer too small for key-value pair");
		return;
	}

	for (i = 0; i < cluster.node_count; i++) /** Use cluster.node_count */
	{
		/** Skip self (primary node) */
		if ((int32_t)i == cluster.self_id)
		{
			continue;
		}

		/** Skip nodes that are not part of the cluster (e.g. id is -1) */
		if (cluster.nodes[i].id == -1)
		{
			continue;
		}

		rale_debug_log("Replicating to follower node_idx %d (NodeID %d): \"%s\"",
			i, cluster.nodes[i].id, message);

		/** Ensure client is initialized for the target node */
		if (tcp_clients[i] == NULL)
		{
			dstore_init_client(i);
			if (tcp_clients[i] == NULL) /** Check if init failed */
			{
				char warn_msg[256];
				snprintf(warn_msg, sizeof(warn_msg), 
					"failed to initialize client for replication to node_idx %d",
					i);
				/* Warning: initialization failed for TCP client */
				continue; /** Skip this follower if client init failed */
			}
		}

		/** Send the message if the client is connected */
		if (tcp_clients[i] != NULL && tcp_clients[i]->is_connected)
		{
			send_ret = dstore_send_message(i, message);
			if (send_ret != 0)
			{
				char warn_msg2[256];
				snprintf(warn_msg2, sizeof(warn_msg2), 
					"failed to send message to follower node_idx %d",
					i);
				/* Warning: failed to send message to follower */
			}
		}
		else
		{
			reason = (tcp_clients[i] == NULL) ? "client structure is NULL" :
					 "client not connected";
			char warn_msg3[256];
			snprintf(warn_msg3, sizeof(warn_msg3), 
				"cannot replicate to follower node_idx %d: %s",
				i, reason);
			/* Warning: cannot replicate to follower */
			/** Optionally, you might want to queue the message or handle the failure in another way */
		}
	}
}

/**
 * Handle PUT command from external source (e.g., client interface).
 * For primary nodes: stores locally and replicates to followers.
 * For follower nodes: forwards to primary node.
 */
int
dstore_handle_put(const char *key, const char *value, char *errbuf, size_t errbuflen)
{
	int db_ret;

	if (key == NULL || value == NULL)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "invalid parameters: key or value is NULL");
		}
		return -1;
	}

	/** Store locally */
	db_ret = db_insert(key, value, errbuf, errbuflen);
	if (db_ret < 0)
	{
		if (errbuf != NULL && errbuflen > 0)
		{
			snprintf(errbuf, errbuflen, "failed to store key-value pair ('%s') locally. DB error: %d", key, db_ret);
		}
		return -1;
	}
	dstore_save_to_rale_db(key, value);
	dstore_replicate_to_followers(key, value, errbuf, errbuflen);
	return 0;
}

/**
 * Parses and processes a "PUT key=value" command.
 * Stores the data locally and then replicates it to followers.
 */
void
dstore_put_from_command(const char *command, char *errbuf, size_t errbuflen)
{
	const char *kv_pair;
	char      *separator;
	size_t     key_len;
	size_t     value_len;
	char       key_buf[MAX_KEY_SIZE]; /** From hash.h, for the key part */
	char       value_buf[MAX_VALUE_SIZE]; /** From hash.h, for the value part, aligning with db storage */
	int        db_ret;
	const char *value_start;

	(void) errbuf;
	(void) errbuflen;

	if (cluster.node_count == 0) /** Use cluster.node_count */
	{
		rale_set_error_fmt(RALE_ERROR_NOT_INITIALIZED, MODULE, "Cluster not initialized in dstore_put_from_command.");
		return;
	}

	if (command == NULL)
	{
		rale_set_error_fmt(RALE_ERROR_NULL_POINTER, MODULE, "PUT command is NULL.");
		return;
	}

	/** Handle RALE leader election commands */
	if (strncmp(command, "LEADER_ELECTED ", 15) == 0)
	{
		int term = -1, leader_id = -1;
		const char *params = command + 15;
		/** Manual parsing for safety */
		char *space = strchr(params, ' ');
		if (space != NULL)
		{
			term = atoi(params);
			leader_id = atoi(space + 1);
		}
		
		if (term >= 0 && leader_id >= 0)
		{
			rale_debug_log( "RALE leader election: term=%d, leader=%d", term, leader_id);
			
			/** Store leader election state in DStore for synchronization */
			snprintf(key_buf, sizeof(key_buf), "rale_leader_term_%d", term);
			snprintf(value_buf, sizeof(value_buf), "%d", leader_id);
			
			db_ret = db_insert(key_buf, value_buf, errbuf, errbuflen);
			if (db_ret < 0)
			{
				rale_set_error_fmt(RALE_ERROR_DB_WRITE, MODULE, "Failed to store leader election state: term=%d, leader=%d", term, leader_id);
			}
			else
			{
				rale_debug_log("Stored leader election state: term=%d, leader=%d", term, leader_id);
			}

			/** Also update local RALE state so followers immediately know the leader */
			write_rale_state_leader(term, leader_id);

			/** Broadcast leader snapshot to all connected peers (both directions) */
			dstore_broadcast_leader_snapshot(term, leader_id);
		}
		return;
	}

	/** Handle leader snapshot messages sent to new nodes: "LEADER <term> <leader_id>" */
	if (strncmp(command, "LEADER ", 7) == 0)
	{
		int term = -1, leader_id = -1;
		const char *params = command + 7;
		/** Manual parsing for safety */
		char *space = strchr(params, ' ');
		if (space != NULL)
		{
			term = atoi(params);
			leader_id = atoi(space + 1);
		}
		
		if (term >= 0 && leader_id >= 0)
		{
			/* Update local RALE state to reflect known leader snapshot */
			write_rale_state_leader(term, leader_id);
		}
		return;
	}

	/** Handle forwarded PUT commands from non-leader nodes */
	if (strncmp(command, "FORWARD_PUT ", 12) == 0)
	{
		rale_debug_log( "Received forwarded PUT command from non-leader node");
		
		/** Extract the key=value part */
		const char *kv_part = command + 12;
		separator = strchr(kv_part, '=');
		
		if (separator == NULL)
		{
			rale_set_error_fmt(RALE_ERROR_INVALID_PARAMETER, MODULE, "Forwarded PUT command missing '=' separator");
			return;
		}
		
		/** Extract key and value */
		key_len = (size_t)(separator - kv_part);
		if (key_len == 0 || key_len >= sizeof(key_buf))
		{
			rale_set_error_fmt(RALE_ERROR_INVALID_PARAMETER, MODULE, "Invalid key in forwarded PUT command");
			return;
		}
		strncpy(key_buf, kv_part, key_len);
		key_buf[key_len] = '\0';
		
		value_start = separator + 1;
		value_len = strlen(value_start);
		if (value_len >= sizeof(value_buf))
		{
			rale_set_error_fmt(RALE_ERROR_INVALID_PARAMETER, MODULE, "Value too long in forwarded PUT command");
			return;
		}
		strcpy(value_buf, value_start);
		
		rale_debug_log( "Processing forwarded PUT: key='%s', value_len=%zu", key_buf, value_len);
		
		/** Continue with normal PUT processing (leader will accept this) */
	}

	/** Handle propagated ADD commands from other nodes */
	if (strncmp(command, "PROPAGATE_ADD ", 14) == 0)
	{
		dstore_handle_propagated_add(command);
		return;
	}

	/** Handle propagated REMOVE commands from other nodes */
	if (strncmp(command, "PROPAGATE_REMOVE ", 17) == 0)
	{
		dstore_handle_propagated_remove(command);
		return;
	}

	/** Command format: "PUT key=value" */
	if (strncmp(command, "PUT ", 4) != 0)
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_PARAMETER, MODULE, "Command does not start with 'PUT ': \"%s\".",
			 command);
		return;
	}

	kv_pair = command + 4; /** Skip "PUT " */
	separator = strchr(kv_pair, '=');

		if (separator == NULL)
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_PARAMETER, MODULE, "PUT command missing '=' separator: %s", command);
		return;
	}

	/** Extract key */
	key_len = (size_t)(separator - kv_pair);
		if (key_len == 0)
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_PARAMETER, MODULE, "PUT command has empty key: %s", command);
		return;
	}
		if (key_len >= sizeof(key_buf))
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_PARAMETER, MODULE, "Key too long in PUT command (max %zu): %zu", sizeof(key_buf) - 1, key_len);
		return;
	}
	strncpy(key_buf, kv_pair, key_len);
	key_buf[key_len] = '\0';

	/** Extract value */
	value_start = separator + 1;
	value_len = strlen(value_start); /** Calculate length of value */

	if (value_len == 0)
	{
		char warn_msg[256];
		snprintf(warn_msg, sizeof(warn_msg), "value is empty in PUT command: key='%s'", key_buf);
		/* Warning: connection failed */
	}

	if (value_len >= sizeof(value_buf))
	{
		rale_set_error_fmt(RALE_ERROR_BUFFER_OVERFLOW, MODULE,
			"Value too long in PUT command (max %zu): current length %zu, key: \"%s\".",
			sizeof(value_buf) - 1, value_len, key_buf);
		return;
	}
	strcpy(value_buf, value_start); /** strcpy is safe due to length check above */

	rale_debug_log(
		"processing PUT command: key='%s', value_len=%zu",
		key_buf, value_len); /** Avoid logging potentially large value */

	/** Split-brain prevention: Only leader can accept writes */
	if (!dstore_is_current_leader())
	{
		int current_leader = dstore_get_current_leader();
		char warn_msg[256];
		snprintf(warn_msg, sizeof(warn_msg), 
			"Rejected write request: Node %d is not leader. Current leader is Node %d. "
			"Forwarding request to leader.",
			cluster.self_id, current_leader);
		/* Warning: connection failed */
		
		/** Forward the request to the leader if we know who it is */
		if (current_leader != -1 && current_leader != cluster.self_id)
		{
			/** Find the leader's node index */
			                                                uint32_t leader_idx = find_node_index_by_id(current_leader);
                        if (leader_idx != MAX_NODES && connection_status[leader_idx] == 1)
			{
				/** Forward the PUT command to the leader */
				char forward_cmd[512];
				snprintf(forward_cmd, sizeof(forward_cmd), "FORWARD_PUT %s=%s", key_buf, value_buf);
				dstore_send_message(leader_idx, forward_cmd);
				rale_debug_log( "Forwarded PUT request to leader Node %d", current_leader);
			}
			else
			{
				rale_set_error_fmt(RALE_ERROR_NETWORK_UNREACHABLE, MODULE, "Cannot forward to leader Node %d: not connected", current_leader);
			}
		}
		return;
	}

	/** Store locally */
	db_ret = db_insert(key_buf, value_buf, errbuf, errbuflen);
	if (db_ret < 0)
	{
		rale_set_error_fmt(RALE_ERROR_DB_WRITE, MODULE,
			"Failed to store key-value pair ('%s') locally. DB error: %d.",
			key_buf, db_ret);
		/** Depending on requirements, you might want to stop replication if local store fails */
		return;
	}
	rale_debug_log("successfully stored key-value pair locally: key='%s'", key_buf);

	/** Save to rale.db file */
	dstore_save_to_rale_db(key_buf, value_buf);

	/**
	 * Replicate to followers.
	 * This should only happen on the primary node.
	 * Assuming the caller of dstore_put_from_command ensures this,
	 * or dstore_replicate_to_followers handles it (which it does by skipping self_id).
	 */
	dstore_replicate_to_followers(key_buf, value_buf, errbuf, errbuflen);
}

/**
 * Send keep-alive messages to all connected nodes.
 */
static void
dstore_send_keep_alive(void)
{
	uint32_t i;
	time_t current_time = time(NULL);

	for (i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id == cluster.self_id || cluster.nodes[i].id == -1)
		{
			continue;
		}

		if (tcp_clients[i] != NULL && tcp_clients[i]->is_connected == 1 && 
			connection_status[i] == 1)
		{
			if (current_time - last_keep_alive_sent[i] >= get_keep_alive_interval())
			{
				tcp_client_send(tcp_clients[i], KEEP_ALIVE_MESSAGE);
				last_keep_alive_sent[i] = current_time;
				rale_debug_log(
					"DStore keep-alive sent: Node %d -> Node %d (%s:%d)",
					cluster.self_id, cluster.nodes[i].id, 
					cluster.nodes[i].ip, cluster.nodes[i].dstore_port);
			}

		}
	}
	
	/** Also send keep-alive to connected clients (for bidirectional communication) */
	if (tcp_server_ptr != NULL)
	{
		/** Send keep-alive to all connected clients */
		dstore_server_send_keep_alive();
	}
}

/**
 * Send keep-alive messages to all connected clients from the server side.
 */
static void
dstore_server_send_keep_alive(void)
{
	uint32_t i;
	time_t current_time = time(NULL);
	
	for (i = 0; i < TCP_SERVER_MAX_CLIENTS; i++)
	{
		if (client_socket_to_node[i] != -1)
		{
			/** Find the node index for this client socket */
			                                                uint32_t node_idx = find_node_index_by_id(client_socket_to_node[i]);
                        if (node_idx != MAX_NODES && connection_status[node_idx] == 1)
			{
				/** Check if it's time to send keep-alive */
				if (current_time - last_keep_alive_sent[node_idx] >= get_keep_alive_interval())
				{
					/** Send keep-alive to this specific client */
					if (tcp_server_ptr != NULL)
					{
						                                                int result = tcp_server_send(tcp_server_ptr, (int)i, KEEP_ALIVE_MESSAGE);
						if (result == 0)
						{
							rale_debug_log(
								"DStore server keep-alive sent: Node %d -> Node %d (socket %d)",
								cluster.self_id, client_socket_to_node[i], i);
						}
						else
						{
													char warn_msg4[256];
						snprintf(warn_msg4, sizeof(warn_msg4), 
							"Failed to send keep-alive to Node %d (socket %d)",
							client_socket_to_node[i], i);
						/* Warning: failed to send keep-alive */
						}
						last_keep_alive_sent[node_idx] = current_time;
					}
				}
			}
		}
	}
}

/**
 * Broadcast leader snapshot to all connected peers.
 */
static void
dstore_broadcast_leader_snapshot(int term, int leader_id)
{
	uint32_t i;
	char msg[128];

	snprintf(msg, sizeof(msg), "LEADER %d %d", term >= 0 ? term : 0, leader_id);

	/** Send to all connected server-side clients */
	if (tcp_server_ptr != NULL)
	{
		for (i = 0; i < TCP_SERVER_MAX_CLIENTS; i++)
		{
			if (client_socket_to_node[i] != -1)
			{
				                                tcp_server_send(tcp_server_ptr, (int)i, msg);
				rale_debug_log(
					"Broadcast leader snapshot to server client socket %d: %s",
					i, msg);
			}
		}
	}

	/** Send to all connected client-side peers */
	for (i = 0; i < cluster.node_count; i++)
	{
		if (tcp_clients[i] != NULL && tcp_clients[i]->is_connected == 1)
		{
			tcp_client_send(tcp_clients[i], msg);
			rale_debug_log(
				"Broadcast leader snapshot to client node_idx %d: %s",
				i, msg);
		}
	}
}

/**
 * Send current cluster membership to a connected client socket.
 */
static void
dstore_send_cluster_snapshot_to_client(int client_sock_idx)
{
    uint32_t i;
    char cmd[256];

    if (tcp_server_ptr == NULL)
        return;

    for (i = 0; i < cluster.node_count; i++)
    {
        if (cluster.nodes[i].id == -1)
            continue;
        /** Include all nodes except the receiving peer detection is unknown here,
         * so we include self and others; the receiver will de-duplicate. */
        snprintf(cmd, sizeof(cmd),
                 "PROPAGATE_ADD %d %s %s %d %d",
                 cluster.nodes[i].id,
                 cluster.nodes[i].name,
                 cluster.nodes[i].ip,
                 cluster.nodes[i].rale_port,
                 cluster.nodes[i].dstore_port);
        tcp_server_send(tcp_server_ptr, client_sock_idx, cmd);
        rale_debug_log("Sent snapshot entry to client socket %d: %s",
             client_sock_idx, cmd);
    }
}

int
dstore_finit(char *errbuf, size_t errbuflen)
{
	uint32_t        i;
	static int cleanup_done = 0;

	(void) errbuf;
	(void) errbuflen;

	/** Prevent double cleanup */
	if (cleanup_done)
	{
		rale_debug_log("DStore cleanup already done, skipping");
		return 0;
	}

	/** Clean up TCP server */
	if (tcp_server_ptr != NULL)
	{
		rale_debug_log("Cleaning up TCP server");
		tcp_server_cleanup(tcp_server_ptr);
		tcp_server_ptr = NULL;
	}

	/** Clean up TCP clients */
	for (i = 0; i < MAX_NODES; i++)
	{
		if (tcp_clients[i] != NULL)
		{
			rale_debug_log("Cleaning up TCP client for node %d", i);
			tcp_client_cleanup(tcp_clients[i]);
			tcp_clients[i] = NULL;
		}
	}

	cleanup_done = 1;
	rale_debug_log( "DStore cleanup completed");
	return 0;
}

/**
 * Check if the current node is the leader by reading from RALE state.
 * This prevents split-brain scenarios by ensuring only the leader accepts writes.
 */
int
dstore_is_current_leader(void)
{
	char rale_state_path[512];
	FILE *fp;
	int current_term = -1, voted_for = -1, leader_id = -1, last_log_index = -1, last_log_term = -1;
	
	/** Construct path to rale.state in the configured database path */
	snprintf(rale_state_path, sizeof(rale_state_path), "%s/rale.state",
		dstore_config.db.path);

	fp = fopen(rale_state_path, "r");
	if (!fp)
	{
		rale_debug_log("Cannot read RALE state file: %s", rale_state_path);
		return 0; /** Not leader if we can't read state */
	}

	char line[256];
	if (fgets(line, sizeof(line), fp) != NULL)
	{
		/** Manual parsing for safety */
		char *token = strtok(line, " \n");
		if (token) current_term = atoi(token);
		token = strtok(NULL, " \n");
		if (token) voted_for = atoi(token);
		token = strtok(NULL, " \n");
		if (token) leader_id = atoi(token);
		token = strtok(NULL, " \n");
		if (token) last_log_index = atoi(token);
		token = strtok(NULL, " \n");
		if (token) last_log_term = atoi(token);
		
		if (current_term < 0 || voted_for < 0 || leader_id < 0 || last_log_index < 0 || last_log_term < 0)
		{
			fclose(fp);
			rale_debug_log("Failed to parse RALE state file - invalid values");
			return 0; /** Not leader if we can't parse state */
		}
	}
	else
	{
		fclose(fp);
		rale_debug_log("Failed to read RALE state file");
		return 0; /** Not leader if we can't read state */
	}

	fclose(fp);

	/** Check if current node is the leader */
	if (leader_id == cluster.self_id)
	{
		char debug_msg6[256];
		snprintf(debug_msg6, sizeof(debug_msg6), "Current node %d is leader (term %d)", cluster.self_id, current_term);
					rale_debug_log("%s", debug_msg6);
		return 1;
	}
	else
	{
		char debug_msg7[256];
		snprintf(debug_msg7, sizeof(debug_msg7), "Current node %d is not leader, leader is %d (term %d)", 
			cluster.self_id, leader_id, current_term);
		rale_debug_log("%s", debug_msg7);
		return 0;
	}
}

/**
 * Get the current leader ID by reading from RALE state.
 */
int
dstore_get_current_leader(void)
{
	char rale_state_path[512];
	FILE *fp;
	int current_term = -1, voted_for = -1, leader_id = -1, last_log_index = -1, last_log_term = -1;
	
	/** Construct path to rale.state in the configured database path */
	snprintf(rale_state_path, sizeof(rale_state_path), "%s/rale.state",
		dstore_config.db.path);

	fp = fopen(rale_state_path, "r");
	if (!fp)
	{
		char debug_msg8[256];
		snprintf(debug_msg8, sizeof(debug_msg8), "Cannot read RALE state file: %s", rale_state_path);
		rale_debug_log("%s", debug_msg8);
		return -1; /** No leader if we can't read state */
	}

	char line[256];
	if (fgets(line, sizeof(line), fp) != NULL)
	{
		/** Manual parsing for safety */
		char *token = strtok(line, " \n");
		if (token) current_term = atoi(token);
		token = strtok(NULL, " \n");
		if (token) voted_for = atoi(token);
		token = strtok(NULL, " \n");
		if (token) leader_id = atoi(token);
		token = strtok(NULL, " \n");
		if (token) last_log_index = atoi(token);
		token = strtok(NULL, " \n");
		if (token) last_log_term = atoi(token);
		
		if (current_term < 0 || voted_for < 0 || leader_id < 0 || last_log_index < 0 || last_log_term < 0)
		{
			fclose(fp);
			rale_debug_log("Failed to parse RALE state file - invalid values");
			return -1; /** Not leader if we can't parse state */
		}
	}
	else
	{
		fclose(fp);
		rale_debug_log("Failed to read RALE state file");
		return -1; /** No leader if we can't read state */
	}

	fclose(fp);

	return leader_id;
}

/**
 * Check if we currently have an active TCP connection to the given node.
 * Returns 1 if connected, 0 otherwise.
 */
int
dstore_is_node_connected(int node_id)
{
    uint32_t i;
    int idx = -1;
    for (i = 0; i < cluster.node_count; i++)
    {
        if (cluster.nodes[i].id == node_id)
        {
            idx = (int)i;
            break;
        }
    }
    if (idx == -1)
        return 0;

    /** Consider connected if either side (client or server) has an active link */
    if (connection_status[idx] == 1)
        return 1;

    /** Also consider connected if our client object is actively connected */
    if (idx >= 0 && idx < MAX_NODES &&
        tcp_clients[idx] != NULL && tcp_clients[idx]->is_connected == 1)
        return 1;

    /** Fallback: scan server client sockets mapped to this node */
    {
        int s;
        for (s = 0; s < TCP_SERVER_MAX_CLIENTS; s++)
        {
            if (client_socket_to_node[s] == node_id)
                return 1;
        }
    }
    return 0;
}

/**
 * Propagate node addition to all other nodes in the cluster.
 * This function is called after a successful ADD command to automatically
 * inform all other nodes about the new node.
 */
int
dstore_propagate_node_addition(int32_t new_node_id, const char *name,
                              const char *ip, uint16_t rale_port, uint16_t dstore_port)
{
	char propagate_cmd[512];
	int propagated_count = 0;
	uint32_t i;
    	uint32_t new_node_idx = MAX_NODES;

	rale_debug_log( "Propagating node addition: Node %d (%s) at %s:%d", 
		 new_node_id, name, ip, dstore_port);

	/** Create propagation command */
	snprintf(propagate_cmd, sizeof(propagate_cmd),
			 "PROPAGATE_ADD %d %s %s %d %d", 
			 new_node_id, name, ip, rale_port, dstore_port);

	/** Send to all connected nodes except self (include the new node) */
	for (i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id != cluster.self_id &&
			connection_status[i] == 1)
		{
			int send_ret = dstore_send_message(i, propagate_cmd);
			if (send_ret == 0)
			{
				rale_debug_log( "Propagated node addition to Node %d", cluster.nodes[i].id);
				propagated_count++;
			}
			else
			{
				char warn_msg[256];
				snprintf(warn_msg, sizeof(warn_msg), "Failed to propagate node addition to Node %d", cluster.nodes[i].id);
				rale_debug_log("%s", warn_msg);
			}
		}
	}

	/** Additionally, ensure the new node learns about us (the sender).
	 *  This helps new nodes populate their cluster with the existing node. */
	for (i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id == new_node_id)
		{
			                                                                        new_node_idx = i;
			break;
		}
	}

	        if (new_node_idx != MAX_NODES && connection_status[new_node_idx] == 1)
	{
		char propagate_self_cmd[512];
		uint32_t  self_idx;

		self_idx = find_node_index_by_id(cluster.self_id);
		if (self_idx != MAX_NODES)
		{
			snprintf(propagate_self_cmd, sizeof(propagate_self_cmd),
				 "PROPAGATE_ADD %d %s %s %d %d",
				 cluster.nodes[self_idx].id,
				 cluster.nodes[self_idx].name,
				 cluster.nodes[self_idx].ip,
				 cluster.nodes[self_idx].rale_port,
				 cluster.nodes[self_idx].dstore_port);

			if (dstore_send_message(new_node_idx, propagate_self_cmd) == 0)
			{
				rale_debug_log(
					 "Propagated existing node (self) to new Node %d", new_node_id);
			}
			else
			{
				char warn_msg6[256];
				snprintf(warn_msg6, sizeof(warn_msg6), 
					 "Failed to propagate existing node (self) to new Node %d",
					 new_node_id);
				/* Warning: propagate operation failed */
			}

			/** Also propagate the current leader snapshot so the new node learns it */
			{
				int current_leader = dstore_get_current_leader();
				int current_term = 0;
				char leader_msg[128];
				if (current_leader >= 0)
				{
					/** Use term 0 here as a placeholder; leader id is what matters for UI */
					snprintf(leader_msg, sizeof(leader_msg), "LEADER %d %d",
						current_term, current_leader);
					if (dstore_send_message(new_node_idx, leader_msg) == 0)
					{
						rale_debug_log(
							 "Propagated leader snapshot (leader_id=%d) to new Node %d",
							 current_leader, new_node_id);
					}
					else
					{
						char warn_msg7[256];
						snprintf(warn_msg7, sizeof(warn_msg7), 
							 "Failed to propagate leader snapshot to new Node %d",
							 new_node_id);
						/* Warning: propagate operation failed */
					}
				}
			}
		}
	}

	rale_debug_log( "Node addition propagation completed: %d nodes notified", propagated_count);
	return propagated_count;
}

/**
 * Propagate node removal to all other nodes in the cluster.
 * This function is called after a successful REMOVE command to automatically
 * inform all other nodes about the removed node.
 */
int
dstore_propagate_node_removal(int node_id)
{
	char propagate_cmd[256];
	int propagated_count = 0;
	uint32_t i;

	rale_debug_log( "Propagating node removal: Node %d", node_id);

	/** Create propagation command */
	snprintf(propagate_cmd, sizeof(propagate_cmd), "PROPAGATE_REMOVE %d", node_id);

	/** Send to all connected nodes except the removed node */
	for (i = 0; i < cluster.node_count; i++)
	{
		if (cluster.nodes[i].id != node_id && 
			cluster.nodes[i].id != cluster.self_id &&
			connection_status[i] == 1)
		{
			int send_ret = dstore_send_message(i, propagate_cmd);
			if (send_ret == 0)
			{
				rale_debug_log( "Propagated node removal to Node %d", cluster.nodes[i].id);
				propagated_count++;
			}
					else
		{
			char warn_msg8[256];
			snprintf(warn_msg8, sizeof(warn_msg8), "Failed to propagate node removal to Node %d", cluster.nodes[i].id);
			/* Warning: propagate operation failed */
		}
		}
	}

	rale_debug_log( "Node removal propagation completed: %d nodes notified", propagated_count);
	return propagated_count;
}

/**
 * Handle propagated ADD command from another node.
 * This function is called when a node receives a PROPAGATE_ADD command.
 */
static int
dstore_handle_propagated_add(const char *command)
{
	int32_t node_id = -1;
	uint16_t rale_port = 0, dstore_port = 0;
	char name[64] = {0}, ip[16] = {0};
	int ret;

	/** Parse the propagated ADD command */
	/** Manual parsing for safety */
	const char *params = command + 13;
	char *token = strtok((char*)params, " ");
	if (token) node_id = atoi(token);
	
	token = strtok(NULL, " ");
	if (token) strncpy(name, token, sizeof(name) - 1);
	
	token = strtok(NULL, " ");
	if (token) strncpy(ip, token, sizeof(ip) - 1);
	
	token = strtok(NULL, " ");
	if (token) rale_port = (uint16_t)atoi(token);
	
	token = strtok(NULL, " ");
	if (token) dstore_port = (uint16_t)atoi(token);
	
	if (node_id >= 0 && strlen(name) > 0 && strlen(ip) > 0 && rale_port > 0 && dstore_port > 0)
	{
		rale_debug_log( "Received propagated ADD for Node %d (%s) at %s:%d", 
			 node_id, name, ip, dstore_port);

		/** Add node to local cluster */
		ret = cluster_add_node(node_id, name, ip, rale_port, dstore_port);
		if (ret == 0)
		{
			rale_debug_log( "Successfully added Node %d via propagation", node_id);
			return 0;
		}
		else
		{
			char warn_msg9[256];
			snprintf(warn_msg9, sizeof(warn_msg9), "Failed to add Node %d via propagation (may already exist)", node_id);
			/* Warning: propagate operation failed */
			return -1;
		}
	}
	else
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_PARAMETER, MODULE, "Failed to parse propagated ADD command: %s", command);
		return -1;
	}
}

/**
 * Handle propagated REMOVE command from another node.
 * This function is called when a node receives a PROPAGATE_REMOVE command.
 */
static int
dstore_handle_propagated_remove(const char *command)
{
	int node_id = -1;
	int ret;

	/** Parse the propagated REMOVE command */
	/** Manual parsing for safety */
	const char *node_str = command + 16;
	if (*node_str != '\0' && isdigit(*node_str))
	{
		node_id = atoi(node_str);
	}
	
	if (node_id >= 0)
	{
		rale_debug_log( "Received propagated REMOVE for Node %d", node_id);

		/** Remove node from local cluster */
		ret = cluster_remove_node(node_id);
		if (ret == 0)
		{
			rale_debug_log( "Successfully removed Node %d via propagation", node_id);
			return 0;
		}
		else
		{
			char warn_msg10[256];
			snprintf(warn_msg10, sizeof(warn_msg10), "Failed to remove Node %d via propagation (may not exist)", node_id);
			/* Warning: propagate operation failed */
			return -1;
		}
	}
	else
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_PARAMETER, MODULE, "Failed to parse propagated REMOVE command: %s", command);
		return -1;
	}
}
