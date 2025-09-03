/*-------------------------------------------------------------------------
 *
 * udp.c
 *    Implementation of UDP communication utilities for RALE.
 *
 *    This file provides functions for creating, binding, sending,
 *    receiving, and managing UDP sockets. It supports both client and
 *    server UDP operations.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/udp.c
 *
 *-------------------------------------------------------------------------
 */

/** System headers */
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdbool.h>

/** Local headers */
#include "librale_internal.h"
#include "rale_error.h"

/** Constants */
#define MODULE "UDP"
#define MAX_UDP_MESSAGE_SIZE 1024
#define MAX_UDP_BUFFER_SIZE 2048

/* UDP status structure */
typedef struct {
	bool initialized;
	int socket_fd;
	uint16_t bound_port;
	uint32_t bound_ip;
} udp_status_t;

static int udp_socket = -1;
static struct sockaddr_in udp_addr;
static bool udp_initialized = false;

librale_status_t
udp_init(uint16_t port)
{
	if (udp_initialized)
	{
		rale_debug_log("UDP already initialized on port %d", port);
		return RALE_SUCCESS;
	}

	udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_socket < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "udp_init",
			"Failed to create UDP socket",
			"Socket creation failed",
			"Check system resources and permissions");
		return RALE_ERROR_GENERAL;
	}

	memset(&udp_addr, 0, sizeof(udp_addr));
	udp_addr.sin_family = AF_INET;
	udp_addr.sin_addr.s_addr = INADDR_ANY;
	udp_addr.sin_port = htons(port);

	if (bind(udp_socket, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "udp_init",
			"Failed to bind UDP socket to port",
			"Bind operation failed",
			"Check if port is available and permissions");
		close(udp_socket);
		return RALE_ERROR_GENERAL;
	}

	udp_initialized = true;
	rale_debug_log("UDP initialized successfully on port %d", port);
	return RALE_SUCCESS;
}

librale_status_t
udp_send(const char *ip, uint16_t port, const void *data, size_t data_len)
{
	if (!udp_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "udp_send",
			"UDP not initialized",
			"UDP system not ready",
			"Call udp_init() first");
		return RALE_ERROR_GENERAL;
	}

	if (data == NULL || data_len == 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "udp_send",
			"Invalid data parameter",
			"Data is NULL or empty",
			"Provide valid data to send");
		return RALE_ERROR_GENERAL;
	}

	if (data_len > MAX_UDP_MESSAGE_SIZE)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "udp_send",
			"Data too large for UDP",
			"Message exceeds maximum size",
			"Reduce message size or use TCP");
		return RALE_ERROR_GENERAL;
	}

	struct sockaddr_in dest_addr;
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(port);

	if (inet_pton(AF_INET, ip, &dest_addr.sin_addr) <= 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "udp_send",
			"Invalid IP address",
			"IP address format error",
			"Use valid IPv4 address format");
		return RALE_ERROR_GENERAL;
	}

	ssize_t sent = sendto(udp_socket, data, data_len, 0,
						 (struct sockaddr *)&dest_addr, sizeof(dest_addr));

	if (sent < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "udp_send",
			"Failed to send UDP message",
			"Send operation failed",
			"Check network connectivity and permissions");
		return RALE_ERROR_GENERAL;
	}

	if ((size_t)sent != data_len)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "udp_send",
			"Partial UDP message sent",
			"Not all data was transmitted",
			"Check network conditions");
		return RALE_ERROR_GENERAL;
	}

	rale_debug_log("UDP message sent successfully to %s:%d (%zu bytes)", ip, port, data_len);
	return RALE_SUCCESS;
}

librale_status_t
udp_receive(void *buffer, size_t buffer_size, size_t *received_len,
			char *sender_ip, size_t ip_size, uint16_t *sender_port)
{
	if (!udp_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "udp_receive",
			"UDP not initialized",
			"UDP system not ready",
			"Call udp_init() first");
		return RALE_ERROR_GENERAL;
	}

	if (buffer == NULL || buffer_size == 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "udp_receive",
			"Invalid buffer parameter",
			"Buffer is NULL or empty",
			"Provide valid buffer for receiving data");
		return RALE_ERROR_GENERAL;
	}

	if (buffer_size > MAX_UDP_BUFFER_SIZE)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "udp_receive",
			"Buffer too large",
			"Buffer exceeds maximum size",
			"Use appropriate buffer size");
		return RALE_ERROR_GENERAL;
	}

	struct sockaddr_in sender_addr;
	socklen_t addr_len = sizeof(sender_addr);

	ssize_t recv_len = recvfrom(udp_socket, buffer, buffer_size, 0,
								(struct sockaddr *)&sender_addr, &addr_len);

	if (recv_len < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			// No data available (non-blocking mode)
			*received_len = 0;
			return RALE_SUCCESS;
		}

		rale_set_error(RALE_ERROR_SYSTEM_CALL, "udp_receive",
			"Failed to receive UDP message",
			"Receive operation failed",
			"Check network connectivity");
		return RALE_ERROR_GENERAL;
	}

	*received_len = (size_t)recv_len;

	// Extract sender information
	if (sender_ip != NULL && ip_size > 0)
	{
		if (inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, (socklen_t)ip_size) == NULL)
		{
			rale_set_error(RALE_ERROR_SYSTEM_CALL, "udp_receive",
				"Failed to convert sender IP",
				"IP address conversion failed",
				"Check IP address handling");
			return RALE_ERROR_GENERAL;
		}
	}

	if (sender_port != NULL)
	{
		*sender_port = ntohs(sender_addr.sin_port);
	}

	rale_debug_log("UDP message received from %s:%d (%zu bytes)",
		sender_ip ? sender_ip : "unknown", sender_port ? *sender_port : 0, *received_len);

	return RALE_SUCCESS;
}

librale_status_t
udp_set_nonblocking(bool nonblocking)
{
	if (!udp_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "udp_set_nonblocking",
			"UDP not initialized",
			"UDP system not ready",
			"Call udp_init() first");
		return RALE_ERROR_GENERAL;
	}

	int flags = fcntl(udp_socket, F_GETFL, 0);
	if (flags < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "udp_set_nonblocking",
			"Failed to get socket flags",
			"fcntl F_GETFL failed",
			"Check socket state");
		return RALE_ERROR_GENERAL;
	}

	if (nonblocking)
	{
		flags |= O_NONBLOCK;
	}
	else
	{
		flags &= ~O_NONBLOCK;
	}

	if (fcntl(udp_socket, F_SETFL, flags) < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "udp_set_nonblocking",
			"Failed to set socket flags",
			"fcntl F_SETFL failed",
			"Check socket state");
		return RALE_ERROR_GENERAL;
	}

	rale_debug_log("UDP socket %s mode set", nonblocking ? "non-blocking" : "blocking");
	return RALE_SUCCESS;
}

librale_status_t
udp_set_timeout(int timeout_ms)
{
	if (!udp_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "udp_set_timeout",
			"UDP not initialized",
			"UDP system not ready",
			"Call udp_init() first");
		return RALE_ERROR_GENERAL;
	}

	if (timeout_ms < 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "udp_set_timeout",
			"Invalid timeout value",
			"Timeout must be non-negative",
			"Use positive timeout value in milliseconds");
		return RALE_ERROR_GENERAL;
	}

	struct timeval tv;
	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;

	if (setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "udp_set_timeout",
			"Failed to set receive timeout",
			"setsockopt SO_RCVTIMEO failed",
			"Check socket state and permissions");
		return RALE_ERROR_GENERAL;
	}

	if (setsockopt(udp_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "udp_set_timeout",
			"Failed to set send timeout",
			"setsockopt SO_SNDTIMEO failed",
			"Check socket state and permissions");
		return RALE_ERROR_GENERAL;
	}

	rale_debug_log("UDP timeout set to %d ms", timeout_ms);
	return RALE_SUCCESS;
}

librale_status_t
udp_cleanup(void)
{
	if (!udp_initialized)
	{
		rale_debug_log("UDP not initialized, nothing to cleanup");
		return RALE_SUCCESS;
	}

	if (udp_socket >= 0)
	{
		close(udp_socket);
		udp_socket = -1;
	}

	udp_initialized = false;
	rale_debug_log("UDP cleanup completed");
	return RALE_SUCCESS;
}

librale_status_t
udp_get_status(udp_status_t *status)
{
	if (!udp_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "udp_get_status",
			"UDP not initialized",
			"UDP system not ready",
			"Call udp_init() first");
		return RALE_ERROR_GENERAL;
	}

	if (status == NULL)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "udp_get_status",
			"Status parameter is NULL",
			"Invalid parameter",
			"Provide valid status pointer");
		return RALE_ERROR_GENERAL;
	}

	status->initialized = udp_initialized;
	status->socket_fd = udp_socket;
	status->bound_port = ntohs(udp_addr.sin_port);
	status->bound_ip = udp_addr.sin_addr.s_addr;

	return RALE_SUCCESS;
}

/* Additional functions declared in udp.h but not yet implemented */

connection_t *
udp_create(void)
{
	connection_t *conn = malloc(sizeof(connection_t));
	if (conn == NULL)
	{
		return NULL;
	}
	
	memset(conn, 0, sizeof(connection_t));
	conn->sockfd = -1;
	conn->addr_len = sizeof(struct sockaddr_in);
	return conn;
}

connection_t *
udp_client_init(int port, void (*on_receive_cb)(const char *message, const char *sender_address, int sender_port))
{
	connection_t *conn = udp_create();
	if (conn == NULL)
	{
		return NULL;
	}
	
	conn->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (conn->sockfd < 0)
	{
		free(conn);
		return NULL;
	}
	
	conn->on_receive = on_receive_cb;
	
	/* Bind to specified port */
	if (udp_bind(conn, port) != 0)
	{
		close(conn->sockfd);
		free(conn);
		return NULL;
	}
	
	return conn;
}

connection_t *
udp_server_init(int port, void (*on_receive_cb)(const char *message, const char *sender_address, int sender_port))
{
	rale_debug_log("Initializing UDP server on port \"%d\" for network communication", port);
	
	connection_t *conn = udp_create();
	if (conn == NULL)
	{
		rale_debug_log("Failed to create UDP connection structure: memory allocation failed");
		return NULL;
	}
	
	conn->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (conn->sockfd < 0)
	{
		rale_debug_log("Socket creation failed: system call error \"%s\"", strerror(errno));
		free(conn);
		return NULL;
	}
	
	conn->on_receive = on_receive_cb;
	
	/* Bind to specified port */
	if (udp_bind(conn, port) != 0)
	{
		rale_debug_log("UDP bind failed on port \"%d\": system call error \"%s\"", port, strerror(errno));
		close(conn->sockfd);
		free(conn);
		return NULL;
	}
	
	rale_debug_log("UDP server initialized successfully on port \"%d\" and ready for network communication", port);
	return conn;
}

void
udp_destroy(connection_t *udp)
{
	if (udp == NULL)
	{
		return;
	}
	
	if (udp->sockfd >= 0)
	{
		close(udp->sockfd);
	}
	
	free(udp);
}

int
udp_bind(connection_t *udp, int port)
{
	if (udp == NULL || udp->sockfd < 0)
	{
		return -1;
	}
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	
	if (bind(udp->sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		return -1;
	}
	
	return 0;
}

int
udp_sendto(connection_t *udp, const char *message, const char *address, int port)
{
	if (udp == NULL || udp->sockfd < 0 || message == NULL || address == NULL)
	{
		return -1;
	}
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	
	if (inet_pton(AF_INET, address, &addr.sin_addr) <= 0)
	{
		return -1;
	}
	
	ssize_t sent = sendto(udp->sockfd, message, strlen(message), 0,
						  (struct sockaddr *)&addr, sizeof(addr));
	
	return (sent < 0) ? -1 : 0;
}

int
udp_recvfrom(connection_t *udp, char *buffer, size_t buffer_len,
			  char *sender_address_buf, int *sender_port)
{
	if (udp == NULL || udp->sockfd < 0 || buffer == NULL || 
		sender_address_buf == NULL || sender_port == NULL)
	{
		return -1;
	}
	
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	
	ssize_t received = recvfrom(udp->sockfd, buffer, buffer_len - 1, 0,
								(struct sockaddr *)&addr, &addr_len);
	
	if (received < 0)
	{
		return -1;
	}
	
	buffer[received] = '\0';
	
	/* Convert sender address to string */
	if (inet_ntop(AF_INET, &addr.sin_addr, sender_address_buf, INET_ADDRSTRLEN) == NULL)
	{
		return -1;
	}
	
	*sender_port = ntohs(addr.sin_port);
	return 0;
}

void
udp_loop(connection_t *udp)
{
	if (udp == NULL || udp->sockfd < 0)
	{
		return;
	}
	
	char buffer[UDP_BUFFER_SIZE];
	char sender_address[INET_ADDRSTRLEN];
	int sender_port;
	
	while (1)
	{
		if (udp_recvfrom(udp, buffer, sizeof(buffer), sender_address, &sender_port) == 0)
		{
			if (udp->on_receive)
			{
				udp->on_receive(buffer, sender_address, sender_port);
			}
		}
	}
}

void
udp_process_messages(connection_t *udp)
{
	if (udp == NULL || udp->sockfd < 0)
	{
		return;
	}
	
	char buffer[UDP_BUFFER_SIZE];
	char sender_address[INET_ADDRSTRLEN];
	int sender_port;
	
	/* Process one message (non-blocking) */
	if (udp_recvfrom(udp, buffer, sizeof(buffer), sender_address, &sender_port) == 0)
	{
		if (udp->on_receive)
		{
			udp->on_receive(buffer, sender_address, sender_port);
		}
	}
}
