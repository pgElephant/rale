/*-------------------------------------------------------------------------
 *
 * tcp_server.h
 *		Declarations for TCP server functionality.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		librale/include/tcp_server.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef TCPSERVER_H
#define TCPSERVER_H

/** System headers */
#include <stddef.h>

/** TCP server constants */
#define TCP_SERVER_MAX_CLIENTS	5
#define TCP_SERVER_BUFFER_SIZE	1024

/** Structure to represent a TCP server */
typedef struct tcp_server
{
	int					server_sock;	/** Server listening socket descriptor */
	int					client_socks[TCP_SERVER_MAX_CLIENTS]; /** Array of connected client socket descriptors */
	int					max_clients;	/** Maximum number of clients */
	void			   (*on_connection) (int client_sock_idx, const char *client_ip, int client_port); /** New connection callback */
	void			   (*on_disconnection) (int client_sock_idx, const char *client_ip, int client_port); /** Disconnection callback */
	void			   (*on_receive) (int client_sock_idx, const char *message); /** Message reception callback */
} tcp_server_t;

/** Function declarations */
extern tcp_server_t *tcp_server_init(int port,
									  void (*on_connection_cb) (int, const char *, int),
									  void (*on_disconnection_cb) (int, const char *, int),
									  void (*on_receive_cb) (int, const char *));
extern int tcp_wait_for_connection(tcp_server_t *server);
extern int tcp_server_run(tcp_server_t *server);
extern void tcp_server_cleanup(tcp_server_t *server);
extern int tcp_wait_for_data(tcp_server_t *server);
extern int tcp_server_client_disconnect(tcp_server_t *server, int client_sock_idx);
extern int tcp_server_send(tcp_server_t *server, int client_sock_idx, const char *message);

#endif							/* TCPSERVER_H */
