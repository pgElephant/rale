/*-------------------------------------------------------------------------
 *
 * udp.h
 *		Declarations for UDP communication.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		librale/include/udp.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef UDP_H
#define UDP_H

/** System headers */
#include <stddef.h>
#include <netinet/in.h>

/** Maximum size for the UDP receive buffer */
#define UDP_BUFFER_SIZE	1024

/** Structure to represent a connection, holding socket and address info */
typedef struct connection_t
{
	int					sockfd;		/** Socket file descriptor */
	struct sockaddr_in	server_addr;	/** Server address */
	struct sockaddr_in	client_addr;	/** Client address */
	socklen_t			addr_len;		/** Length of the address structure */
	void	(*on_receive) (const char *message, const char *sender_address, int sender_port);	/** Message reception callback */
} connection_t;

/** Function declarations */
extern connection_t *udp_create(void);
extern connection_t *udp_client_init(int port,
								   void (*on_receive_cb) (const char *message,
														 const char *sender_address, int sender_port));
extern connection_t *udp_server_init(int port,
								   void (*on_receive_cb) (const char *message,
														 const char *sender_address, int sender_port));
extern void udp_destroy(connection_t *udp);
extern int udp_bind(connection_t *udp, int port);
extern int udp_sendto(connection_t *udp, const char *message, const char *address, int port);
extern int udp_recvfrom(connection_t *udp, char *buffer, size_t buffer_len,
						char *sender_address_buf, int *sender_port);
extern void udp_loop(connection_t *udp);
extern void udp_process_messages(connection_t *udp);

#endif							/* UDP_H */
