/*-------------------------------------------------------------------------
 *
 * tcp_client.h
 *		Declarations for TCP client functionality.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		librale/include/tcp_client.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

/** System headers */
#include <stddef.h>
#include <netinet/in.h>

/** Buffer size for receiving messages */
#define TCP_CLIENT_BUFFER_SIZE	1024

/** Structure to represent a TCP client */
typedef struct tcp_client_t
{
	int					sock;			/** Socket descriptor */
	int					is_connected;	/** Connection status */
	struct sockaddr_in	server_addr;	/** Server address structure */
	char			   *ip_address;		/** IP address of the server */
	void	(*on_receive) (int client_sock, const char *message);	/** Message reception callback */
	void	(*on_disconnection) (int client_sock, const char *server_ip, int port_or_errno);	/** Disconnection callback */
} tcp_client_t;

/** Function declarations */
extern tcp_client_t *tcp_client_init(const char *ip_address, int port,
								   void (*on_receive) (int, const char *),
								   void (*on_disconnection) (int, const char *, int),
								   char *errbuf, size_t errbuflen);
extern void tcp_client_send(tcp_client_t *client, const char *message);
extern void tcp_client_receive(tcp_client_t *client);
extern void tcp_client_cleanup(tcp_client_t *client);
extern void tcp_client_run(tcp_client_t *client);
extern int tcp_client_connect(tcp_client_t *client, const char *ip_address, int port);

#endif							/* TCP_CLIENT_H */
