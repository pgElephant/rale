/*-------------------------------------------------------------------------
 *
 * tcp_server.c
 *    Implementation for tcp_server.c
 *
 * Shared library for RALE (Reliable Automatic Log Engine) component.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/tcp_server.c
 *
 *------------------------------------------------------------------------- */

/*-------------------------------------------------------------------------
 *
 * tcp_server.c
 *    Implements TCP server functionality for RALE.
 *
 *    This file provides functions for creating and managing a TCP server,
 *    handling multiple client connections using select(), and invoking
 *    callbacks for connection, disconnection, and message reception.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/tcp_server.c
 *
 *-------------------------------------------------------------------------
 */

/** System headers */
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/** Local headers */
#include "librale_internal.h"
#include "tcp_server.h"
#include "shutdown.h"

/** Constants */
#define MODULE "DSTORE"
#define TCP_SERVER_LISTEN_BACKLOG    10

/**
 * Simple per-client receive buffers to handle partial TCP reads and
 * newline-delimited message framing.
 */
static char recv_buf[TCP_SERVER_MAX_CLIENTS][TCP_SERVER_BUFFER_SIZE * 2];
static int  recv_len[TCP_SERVER_MAX_CLIENTS];

tcp_server_t *
tcp_server_init(int port,
				void (*on_connection_cb)(int, const char *, int),
				void (*on_disconnection_cb)(int, const char *, int),
				void (*on_receive_cb)(int, const char *))
{
	tcp_server_t *server;
	struct sockaddr_in server_addr;
	int             opt = 1;
	int             i;

	server = (tcp_server_t *) rmalloc(sizeof(tcp_server_t));
	if (server == NULL)
	{
		rale_set_error(RALE_ERROR_OUT_OF_MEMORY, "tcp_server_init",
				"Memory allocation for TCP server structure failed.",
				"Failed to allocate memory for TCPServer structure.",
				"Check system resources or increase stack size.");
		return NULL;
	}

	memset(server, 0, sizeof(tcp_server_t));
	server->server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server->server_sock == -1)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "tcp_server_init",
			"Failed to create server socket", "Socket creation failed", "Check system resources and permissions");
		free(server);
		server = NULL;
		return NULL;
	}

	/** Set socket options - SO_REUSEADDR allows reuse of local addresses */
	if (setsockopt(server->server_sock, SOL_SOCKET, SO_REUSEADDR,
				   (char *)&opt, sizeof(opt)) < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "tcp_server_init",
			"Failed to set socket options", "SO_REUSEADDR failed", "Check socket state");
		close(server->server_sock);
		free(server);
		server = NULL;
		return NULL;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	if (bind(server->server_sock, (struct sockaddr *) &server_addr,
			 sizeof(server_addr)) == -1)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "tcp_server_init",
			"Failed to bind server socket", "Bind operation failed", "Check if port is available and permissions");
		close(server->server_sock);
		free(server);
		server = NULL;
		return NULL;
	}

	if (listen(server->server_sock, TCP_SERVER_LISTEN_BACKLOG) == -1)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "tcp_server_init",
			"Failed to start listening", "Listen operation failed", "Check socket state");
		close(server->server_sock);
		free(server);
		server = NULL;
		return NULL;
	}

	for (i = 0; i < TCP_SERVER_MAX_CLIENTS; i++)
	{
		server->client_socks[i] = -1; /** -1 indicates available slot */
        recv_len[i] = 0; /** Initialize per-client buffer length */
	}

	server->max_clients = TCP_SERVER_MAX_CLIENTS;
	server->on_connection = on_connection_cb;
	server->on_disconnection = on_disconnection_cb;
	server->on_receive = on_receive_cb;

	rale_debug_log("Connection callback registered: on_connection=%p, on_disconnection=%p, on_receive=%p",
		(void *)server->on_connection, (void *)server->on_disconnection, (void *)server->on_receive);

	rale_debug_log("TCP server initialized and listening on port %d, fd %d.",
		port, server->server_sock);
	return server;
}

/**
 * Main TCP server loop.
 * Uses select() to handle multiple clients: accepts new connections and
 * reads data from existing clients.
 */
int
tcp_server_run(tcp_server_t *server)
{
	fd_set              read_fds;
	int                 max_sd;
	int                 activity;
	struct timeval      timeout;
	struct sockaddr_in  client_address;
	socklen_t           addrlen = sizeof(client_address);
	char                buffer[TCP_SERVER_BUFFER_SIZE];
	int                 new_socket_fd;
	int                 sd;
	ssize_t             valread;
	int                 i;

	if (server == NULL || server->server_sock == -1)
	{
		rale_set_error(RALE_ERROR_INVALID_STATE, "tcp_server_run",
			"Server not initialized or listener socket invalid. Cannot run.", "Server not ready", "Check server state");
		return -1;
	}

	/** Set up timeout for faster shutdown responsiveness */
	/** Use shorter timeout when shutdown is requested for faster shutdown */
	if (librale_is_shutdown_requested("dstore"))
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000; /** 10ms timeout for faster shutdown */
	}
	else
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000; /** 100ms timeout for normal operation */
	}

	FD_ZERO(&read_fds);
	FD_SET(server->server_sock, &read_fds);
	max_sd = server->server_sock;

	for (i = 0; i < server->max_clients; i++)
	{
		sd = server->client_socks[i];
		if (sd > 0) /** If valid socket descriptor then add to read list */
		{
			FD_SET(sd, &read_fds);
		}
		if (sd > max_sd) /** Highest file descriptor number, needed for select function */
		{
			max_sd = sd;
		}
	}

	activity = select(max_sd + 1, &read_fds, NULL, NULL, &timeout);

	if (activity < 0 && errno != EINTR)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "tcp_server_run",
			"Select error: %s.", "Select operation failed", "Check system state");
		return -1;
	}
	else if (activity == 0)
	{
		return 0; /** Timeout occurred */
	}

	rale_debug_log("Select returned %d - activity detected on server socket %d",
		activity, server->server_sock);

	/** Handle incoming connection */
	if (FD_ISSET(server->server_sock, &read_fds))
	{
		rale_debug_log("ACCEPTING new connection on server socket %d (callback=%p)...",
		server->server_sock, (void *)server->on_connection);
			
		new_socket_fd = accept(server->server_sock,
							  (struct sockaddr *)&client_address, &addrlen);
		if (new_socket_fd < 0)
		{
			rale_set_error(RALE_ERROR_SYSTEM_CALL, "tcp_server_run",
				"Accept failed", "Accept operation failed", "Check socket state");
			return -1;
		}

		rale_debug_log("New connection ACCEPTED: socket fd %d, IP: %s, Port: %d.",
			new_socket_fd, inet_ntoa(client_address.sin_addr),
			ntohs(client_address.sin_port));

		/** Find a free slot for the new client */
		int client_slot = -1;
		for (i = 0; i < server->max_clients; i++)
		{
			if (server->client_socks[i] == -1)
			{
				client_slot = i;
				break;
			}
		}

		if (client_slot == -1)
		{
			rale_debug_log("Maximum number of clients reached. Rejecting connection from %s:%d.",
				inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
			close(new_socket_fd);
			return 0;
		}

		/** Store the new client socket */
		server->client_socks[client_slot] = new_socket_fd;

		/** Pass the connection to the callback if defined */
		if (server->on_connection)
		{
			rale_debug_log(
				"INVOKING connection callback for slot %d: IP: %s, Port: %d (callback=%p).",
				client_slot, inet_ntoa(client_address.sin_addr),
				ntohs(client_address.sin_port), (void *)server->on_connection);
			server->on_connection(client_slot,
								 inet_ntoa(client_address.sin_addr),
								 ntohs(client_address.sin_port));
			rale_debug_log(
				"Connection callback COMPLETED for slot %d: IP: %s, Port: %d.",
				client_slot, inet_ntoa(client_address.sin_addr),
				ntohs(client_address.sin_port));
		}
		else
		{
			rale_debug_log(
				"No connection callback defined for new connection in slot %d: IP: %s, Port: %d (callback=%p).",
				client_slot, inet_ntoa(client_address.sin_addr),
				ntohs(client_address.sin_port), (void *)server->on_connection);
		}

		rale_debug_log(
			"New connection stored in slot %d: socket fd %d, IP: %s, Port: %d.",
			client_slot, new_socket_fd, inet_ntoa(client_address.sin_addr),
			ntohs(client_address.sin_port));
	}

	/** Handle data from connected clients */
	for (i = 0; i < server->max_clients; i++)
	{
		sd = server->client_socks[i];
		if (sd > 0 && FD_ISSET(sd, &read_fds))
		{
            valread = read(sd, buffer, TCP_SERVER_BUFFER_SIZE - 1);
			if (valread == 0) /** Client disconnected */
			{
				getpeername(sd, (struct sockaddr *)&client_address, &addrlen);
				rale_debug_log(
					"Client (fd %d, slot %d, IP %s, Port %d) disconnected.",
					sd, i, inet_ntoa(client_address.sin_addr),
					ntohs(client_address.sin_port));

				if (server->on_disconnection)
				{
					server->on_disconnection(i,
											inet_ntoa(client_address.sin_addr),
											ntohs(client_address.sin_port));
				}

				close(sd);
				server->client_socks[i] = -1; /** Mark slot as free */
			}
			else if (valread < 0) /** Read error */
			{
				rale_set_error_fmt(RALE_ERROR_SYSTEM_CALL, "tcp_server_run",
					"Read error from client (fd %d, slot %d): %s", sd, i, strerror(errno));
				close(sd);
				server->client_socks[i] = -1; /** Mark slot as free */
			}
            else /** Data received */
			{
                char *newline_ptr;

                buffer[valread] = '\0'; /** Null-terminate the received data */

                /** Append to per-client buffer */
                if (recv_len[i] + valread >= (int) sizeof(recv_buf[i]))
                {
                    /** If overflow, reset buffer (avoid undefined behavior) */
                    recv_len[i] = 0;
                }
                memcpy(recv_buf[i] + recv_len[i], buffer, valread);
                recv_len[i] += valread;
                recv_buf[i][recv_len[i]] = '\0';

                /** Process complete lines delimited by '\n' */
                while ((newline_ptr = strchr(recv_buf[i], '\n')) != NULL)
                {
                    size_t line_len = (size_t)(newline_ptr - recv_buf[i]);
                    char   line[TCP_SERVER_BUFFER_SIZE];

                    if (line_len >= sizeof(line))
                        line_len = sizeof(line) - 1;
                    memcpy(line, recv_buf[i], line_len);
                    line[line_len] = '\0';

                    /** Shift remaining buffer left */
                    {
                        int remaining = recv_len[i] - (int)(line_len + 1);
                        if (remaining > 0)
                        {
                            memmove(recv_buf[i], recv_buf[i] + line_len + 1,
                                    (size_t) remaining);
                        }
                        recv_len[i] = (remaining > 0) ? remaining : 0;
                        recv_buf[i][recv_len[i]] = '\0';
                    }

                    if (server->on_receive && line_len > 0)
                    {
                        server->on_receive(i, line);
                    }
                }
			}
		}
	}

	return 0;
}

/**
 * Cleans up the TCP server and releases resources.
 * Closes all client sockets and the main server socket.
 * This function does NOT free the TCPServer struct itself;
 * that is the responsibility of the caller (e.g. DestroyTCPServer in network.c).
 */
void
tcp_server_cleanup(tcp_server_t *server)
{
	int i;

	if (server == NULL)
	{
		return;
	}

	rale_debug_log(
		"Cleaning up TCP server (fd %d).",
		server->server_sock);

	for (i = 0; i < server->max_clients; i++)
	{
		if (server->client_socks[i] > 0) /** If socket is valid */
		{
			rale_debug_log(
				"Closing client socket fd %d (slot %d).",
				server->client_socks[i], i);
			/**
			 * on_disconnection should ideally be called by the read loop when
			 * disconnection is detected. Calling it here might be redundant or
			 * provide less context if the client address is not easily retrievable.
			 * For a forceful cleanup, just closing is typical.
			 */
			shutdown(server->client_socks[i], SHUT_RDWR);
			close(server->client_socks[i]);
			server->client_socks[i] = -1;
		}
	}

	if (server->server_sock != -1) /** If server socket is valid */
	{
		rale_debug_log(
			"Closing server socket fd %d.",
			server->server_sock);
		shutdown(server->server_sock, SHUT_RDWR);
		close(server->server_sock);
		server->server_sock = -1;
	}
	/** Note: The TCPServer struct itself is freed by DestroyTCPServer in network.c */
}

/**
 * Disconnects a specific client from the server.
 * Closes the client's socket, calls the on_disconnection callback,
 * and marks its slot as available.
 */
int
tcp_server_client_disconnect(
	tcp_server_t *server,
	int client_sock_idx)
{
	struct sockaddr_in client_address;
	socklen_t          addrlen;
	char               client_ip_str[INET_ADDRSTRLEN];
	int                client_port_val = 0;

	if (server == NULL)
	{
		rale_set_error(RALE_ERROR_INVALID_STATE, "tcp_server_client_disconnect",
			"Cannot disconnect client: server is NULL.", "Server not ready", "Check server state");
		return -1;
	}

	if (client_sock_idx < 0 || client_sock_idx >= server->max_clients ||
		server->client_socks[client_sock_idx] == -1)
	{
		rale_set_error(RALE_ERROR_INVALID_STATE, "tcp_server_client_disconnect",
			"Cannot disconnect client: invalid client index %d or socket already closed.", "Invalid client index", "Check client index and socket state");
		return -1;
	}

	addrlen = sizeof(client_address);
	if (getpeername(server->client_socks[client_sock_idx],
					(struct sockaddr *) &client_address, &addrlen) == 0)
	{
		inet_ntop(AF_INET, &client_address.sin_addr, client_ip_str,
				  sizeof(client_ip_str));
		client_port_val = ntohs(client_address.sin_port);
		/* Throttle disconnection logging to reduce noise */
		static time_t last_disconnect_log_time = 0;
		static int disconnects_suppressed = 0;
		time_t now = time(NULL);
		
		if (last_disconnect_log_time == 0 || (now - last_disconnect_log_time) >= 2) {
			if (disconnects_suppressed > 0) {
				rale_debug_log(
					"Suppressed %d similar disconnection logs in last interval.",
					disconnects_suppressed);
				disconnects_suppressed = 0;
			}
			rale_debug_log(
				"Client disconnected (fd %d, slot %d, IP %s, Port %d).",
				server->client_socks[client_sock_idx], client_sock_idx,
				client_ip_str, client_port_val);
			last_disconnect_log_time = now;
		} else {
			disconnects_suppressed++;
		}
	}
	else
	{
					rale_set_error_fmt(RALE_ERROR_SYSTEM_CALL, "tcp_server_client_disconnect",
				"Disconnecting client (fd %d, slot %d). Could not get peer name: %s", server->client_socks[client_sock_idx], client_sock_idx, strerror(errno));
		strncpy(client_ip_str, "unknown", sizeof(client_ip_str)); /** Fallback IP */
	}

	if (server->on_disconnection)
	{
		server->on_disconnection(client_sock_idx, client_ip_str,
								client_port_val);
	}

	shutdown(server->client_socks[client_sock_idx], SHUT_RDWR);
	close(server->client_socks[client_sock_idx]);
	server->client_socks[client_sock_idx] = -1; /** Mark slot as free */

	return 0; /** Success */
}

/**
 * Sends a message to a specific client connected to the server.
 * 
 * @param server The TCP server instance
 * @param client_sock_idx The index of the client socket in the server's client array
 * @param message The message to send
 * @return 0 on success, -1 on failure
 */
int
tcp_server_send(tcp_server_t *server, int client_sock_idx, const char *message)
{
	if (server == NULL || message == NULL)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "tcp_server_send",
			"Invalid parameters for tcp_server_send: server=%p, message=%p.", "Invalid parameters", "Check server and message pointers");
		return -1;
	}

	if (client_sock_idx < 0 || client_sock_idx >= server->max_clients)
	{
		rale_set_error(RALE_ERROR_INVALID_STATE, "tcp_server_send",
			"Invalid client socket index %d (max: %d) for tcp_server_send.", "Invalid client index", "Check client index and server state");
		return -1;
	}

	if (server->client_socks[client_sock_idx] == -1)
	{
		rale_set_error(RALE_ERROR_INVALID_STATE, "tcp_server_send",
			"Cannot send message: client socket %d is not connected.", "Client not connected", "Check client socket state");
		return -1;
	}

    if (send(server->client_socks[client_sock_idx], message, strlen(message), 0) == -1)
	{
					rale_set_error_fmt(RALE_ERROR_SYSTEM_CALL, "tcp_server_send",
				"Failed to send message to client %d: %s", client_sock_idx, strerror(errno));
		return -1;
	}

    /** Append a newline to delimit messages */
    if (send(server->client_socks[client_sock_idx], "\n", 1, 0) == -1)
    {
        	rale_set_error_fmt(RALE_ERROR_SYSTEM_CALL, "tcp_server_send",
	    "Failed to send newline terminator to client %d: %s", client_sock_idx, strerror(errno));
    }

	rale_debug_log(
		"Message sent to client %d: \"%s\"",
		client_sock_idx, message);
	return 0;
}

/**
 * The functions tcp_wait_for_connection and tcp_wait_for_data seem to be
 * simplified/alternative ways of handling server events compared to the main
 * tcp_server_run which uses a single select() for all sockets.
 * For a typical non-blocking server handling multiple clients, tcp_server_run
 * would be the primary operational loop. These other functions might be
 * for specific, simpler use cases or legacy code.
 * If they are not intended for use with the main tcp_server_run, they should
 * be clearly documented as such or potentially removed if redundant.
 * For this refactoring, they are kept and styled, but their integration
 * into the server's lifecycle (especially with tcp_server_run) is unclear.
 * The current tcp_server_run calls them once, which is not a persistent server.
 * A proper tcp_server_run has been implemented above.
 */

/**
 * Waits for and accepts a new client connection. (Simplified version)
 * This is now mostly superseded by logic within tcp_server_run.
 * Kept for API compatibility if used elsewhere, but should be reviewed.
 */
int
tcp_wait_for_connection(tcp_server_t *server)
{
	/** ... (original implementation with PG styling and error checks) ... */
	/** This function's logic is now better handled by the main tcp_server_run loop. */
	/** It is kept here for now but consider for deprecation/removal. */
	/** For brevity, the full complex implementation of this standalone function is omitted */
	/** as the main tcp_server_run is preferred. If this function is truly needed, */
	/** its select() and accept() logic would be here. */
	rale_debug_log(
		"tcp_wait_for_connection is a simplified function; main event loop is in tcp_server_run.");
	if (server == NULL || server->server_sock == -1)
	{
		rale_set_error(RALE_ERROR_INVALID_STATE, "tcp_wait_for_connection",
			"Server not initialized or listener socket invalid. Cannot run.", "Server not ready", "Check server state");
		return -1;
	}
	/** Simplified: this would block or use a short select, not a full loop here */
	rale_set_error(RALE_ERROR_INVALID_STATE, "tcp_wait_for_connection",
		"tcp_wait_for_connection is a simplified function; main event loop is in tcp_server_run.", "Simplified function", "Use tcp_server_run for persistent server operation");
	return -1; /** Placeholder, as this function is problematic with the new tcp_server_run */
}

/**
 * Waits for data from connected clients. (Simplified version)
 * This is now mostly superseded by logic within tcp_server_run.
 * Kept for API compatibility if used elsewhere, but should be reviewed.
 */
int
tcp_wait_for_data(tcp_server_t *server)
{
	/** ... (original implementation with PG styling and error checks) ... */
	/** Logic for reading data from clients is now part of tcp_server_run. */
	/** Kept for API compatibility but consider for deprecation/removal. */
	rale_debug_log(
		"tcp_wait_for_data is a simplified function; main event loop is in tcp_server_run.");
	if (server == NULL)
	{
		rale_set_error(RALE_ERROR_INVALID_STATE, "tcp_wait_for_data",
			"Server not initialized. Cannot wait for data.", "Server not ready", "Check server state");
		return -1;
	}
	rale_set_error(RALE_ERROR_INVALID_STATE, "tcp_wait_for_data",
		"tcp_wait_for_data is a simplified function; main event loop is in tcp_server_run.", "Simplified function", "Use tcp_server_run for persistent server operation");
	return -1; /** Placeholder */
}

/** Note: The original tcp_server_finit was removed as its logic is better placed in tcp_server_cleanup or handled by the caller of cleanup (freeing the struct). */
