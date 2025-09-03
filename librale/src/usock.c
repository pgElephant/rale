/*-------------------------------------------------------------------------
 *
 * usock.c
 *    Implementation for usock.c
 *
 * Shared library for RALE (Reliable Automatic Log Engine) component.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/usock.c
 *
 *------------------------------------------------------------------------- */

/*-------------------------------------------------------------------------
 *
 * usock.c
 *    Implementation of Unix domain socket communication for RALE.
 *
 *    This file provides functions for creating and managing a Unix domain
 *    socket server, as well as client functions to connect and send/receive
 *    messages. It is used for local inter-process communication.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/usock.c
 *
 *-------------------------------------------------------------------------
 */

/** System headers */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

/** Local headers */
#include "librale_internal.h"
#include "rale_error.h"

/** Constants */
#define UXSOCK_BACKLOG                64
#define MODULE                        "COMM"
#define UXSOCK_RESPONSE_BUFFER_SIZE   4096
#define UXSOCK_READ_BUFFER_SIZE       1024

/** Static variables */
static const config_t *usock_config_ptr = NULL;
static int server_fd = -1;

/** Function declarations */
static int unix_socket_on_receive(int fd, const char *buf, int len);

void
usock_set_config(const config_t *cfg)
{
	usock_config_ptr = cfg;
}

librale_status_t
unix_socket_server_init(const char *socket_path, void (*on_message)(int, const char *, size_t),
						void (*on_connect)(int), void (*on_disconnect)(int))
{
	if (unix_socket_server_initialized)
	{
		rale_debug_log("Unix socket server already initialized on %s", socket_path);
		return RALE_SUCCESS;
	}

	if (socket_path == NULL || strlen(socket_path) == 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "unix_socket_server_init",
			"Invalid socket path", "Socket path is NULL or empty", "Provide valid socket path");
		return RALE_ERROR_GENERAL;
	}

	if (strlen(socket_path) >= MAX_UNIX_SOCKET_PATH)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "unix_socket_server_init",
			"Socket path too long", "Path exceeds maximum length", "Use shorter socket path");
		return RALE_ERROR_GENERAL;
	}

	// Remove existing socket file if it exists
	unlink(socket_path);

	// Initialize server structure
	memset(&unix_socket_server, 0, sizeof(unix_socket_server_t));
	strlcpy(unix_socket_server.socket_path, socket_path, sizeof(unix_socket_server.socket_path));
	unix_socket_server.on_message = on_message;
	unix_socket_server.on_connect = on_connect;
	unix_socket_server.on_disconnect = on_disconnect;

	// Create server socket
	unix_socket_server.server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (unix_socket_server.server_socket < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_server_init",
			"Failed to create server socket", "Socket creation failed", "Check system resources and permissions");
		return RALE_ERROR_GENERAL;
	}

	// Setup server address
	struct sockaddr_un server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sun_family = AF_UNIX;
	strlcpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path));

	// Bind socket to path
	if (bind(unix_socket_server.server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_server_init",
			"Failed to bind server socket", "Bind operation failed", "Check path permissions and availability");
		close(unix_socket_server.server_socket);
		return RALE_ERROR_GENERAL;
	}

	// Set socket permissions
	if (chmod(socket_path, 0666) < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_server_init",
			"Failed to set socket permissions", "chmod failed", "Check path permissions");
		close(unix_socket_server.server_socket);
		unlink(socket_path);
		return RALE_ERROR_GENERAL;
	}

	// Start listening
	if (listen(unix_socket_server.server_socket, 10) < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_server_init",
			"Failed to start listening", "Listen operation failed", "Check socket state");
		close(unix_socket_server.server_socket);
		unlink(socket_path);
		return RALE_ERROR_GENERAL;
	}

	unix_socket_server_initialized = true;
	rale_debug_log("Unix socket server initialized successfully on %s", socket_path);
	return RALE_SUCCESS;
}

librale_status_t
unix_socket_server_start(void)
{
	if (!unix_socket_server_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "unix_socket_server_start",
			"Unix socket server not initialized", "Server not ready", "Call unix_socket_server_init() first");
		return RALE_ERROR_GENERAL;
	}

	if (unix_socket_server.running)
	{
		rale_debug_log("Unix socket server already running");
		return RALE_SUCCESS;
	}

	// Create accept thread
	if (pthread_create(&unix_socket_server.accept_thread, NULL, unix_socket_server_accept_thread, NULL) != 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_server_start",
			"Failed to create accept thread", "pthread_create failed", "Check system resources");
		return RALE_ERROR_GENERAL;
	}

	unix_socket_server.running = true;
	rale_debug_log("Unix socket server started successfully");
	return RALE_SUCCESS;
}

librale_status_t
unix_socket_server_stop(void)
{
	if (!unix_socket_server_initialized)
	{
		rale_set_error(RALE_ERROR_NOT_INITIALIZED, "unix_socket_server_stop",
			"Unix socket server not initialized", "Server not ready", "Call unix_socket_server_init() first");
		return RALE_ERROR_GENERAL;
	}

	if (!unix_socket_server.running)
	{
		rale_debug_log("Unix socket server not running");
		return RALE_SUCCESS;
	}

	// Signal thread to stop
	unix_socket_server.running = false;

	// Wait for accept thread to finish
	if (pthread_join(unix_socket_server.accept_thread, NULL) != 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_server_stop",
			"Failed to join accept thread", "pthread_join failed", "Check thread state");
		return RALE_ERROR_GENERAL;
	}

	rale_debug_log("Unix socket server stopped successfully");
	return RALE_SUCCESS;
}

librale_status_t
unix_socket_server_cleanup(void)
{
	if (!unix_socket_server_initialized)
	{
		rale_debug_log("Unix socket server not initialized, nothing to cleanup");
		return RALE_SUCCESS;
	}

	// Stop server if running
	if (unix_socket_server.running)
	{
		unix_socket_server_stop();
	}

	// Close server socket
	if (unix_socket_server.server_socket >= 0)
	{
		close(unix_socket_server.server_socket);
		unix_socket_server.server_socket = -1;
	}

	// Remove socket file
	if (strlen(unix_socket_server.socket_path) > 0)
	{
		unlink(unix_socket_server.socket_path);
	}

	unix_socket_server_initialized = false;
	rale_debug_log("Unix socket server cleanup completed");
	return RALE_SUCCESS;
}

librale_status_t
unix_socket_client_connect(const char *socket_path)
{
	if (socket_path == NULL || strlen(socket_path) == 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "unix_socket_client_connect",
			"Invalid socket path", "Socket path is NULL or empty", "Provide valid socket path");
		return RALE_ERROR_GENERAL;
	}

	if (strlen(socket_path) >= MAX_UNIX_SOCKET_PATH)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "unix_socket_client_connect",
			"Socket path too long", "Path exceeds maximum length", "Use shorter socket path");
		return RALE_ERROR_GENERAL;
	}

	// Check if socket file exists
	struct stat st;
	if (stat(socket_path, &st) != 0)
	{
		rale_set_error(RALE_ERROR_FILE_NOT_FOUND, "unix_socket_client_connect",
			"Socket file not found", "Socket path does not exist", "Check if server is running");
		return RALE_ERROR_GENERAL;
	}

	// Create client socket
	int client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (client_sock < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_client_connect",
			"Failed to create client socket", "Socket creation failed", "Check system resources");
		return RALE_ERROR_GENERAL;
	}

	// Setup server address
	struct sockaddr_un server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sun_family = AF_UNIX;
	strlcpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path));

	// Connect to server
	if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_client_connect",
			"Failed to connect to server", "Connect operation failed", "Check server availability");
		close(client_sock);
		return RALE_ERROR_GENERAL;
	}

	rale_debug_log("Unix socket client connected successfully to %s", socket_path);
	return client_sock;
}

librale_status_t
unix_socket_client_send(int client_sock, const void *data, size_t data_len)
{
	if (client_sock < 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "unix_socket_client_send",
			"Invalid client socket", "Socket descriptor is negative", "Use valid socket descriptor");
		return RALE_ERROR_GENERAL;
	}

	if (data == NULL || data_len == 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "unix_socket_client_send",
			"Invalid data parameter", "Data is NULL or empty", "Provide valid data to send");
		return RALE_ERROR_GENERAL;
	}

	ssize_t sent = send(client_sock, data, data_len, 0);
	if (sent < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_client_send",
			"Failed to send data", "Send operation failed", "Check connection state");
		return RALE_ERROR_GENERAL;
	}

	if ((size_t)sent != data_len)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_client_send",
			"Partial data sent", "Not all data was transmitted", "Check connection state");
		return RALE_ERROR_GENERAL;
	}

	rale_debug_log("Sent %zu bytes via unix socket", data_len);
	return RALE_SUCCESS;
}

librale_status_t
unix_socket_client_receive(int client_sock, void *buffer, size_t buffer_size, size_t *received_len)
{
	if (client_sock < 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "unix_socket_client_receive",
			"Invalid client socket", "Socket descriptor is negative", "Use valid socket descriptor");
		return RALE_ERROR_GENERAL;
	}

	if (buffer == NULL || buffer_size == 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "unix_socket_client_receive",
			"Invalid buffer parameter", "Buffer is NULL or empty", "Provide valid buffer for receiving data");
		return RALE_ERROR_GENERAL;
	}

	if (buffer_size > UNIX_SOCKET_BUFFER_SIZE)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "unix_socket_client_receive",
			"Buffer too large", "Buffer exceeds maximum size", "Use appropriate buffer size");
		return RALE_ERROR_GENERAL;
	}

	// Set receive timeout
	struct timeval timeout;
	timeout.tv_sec = UNIX_SOCKET_TIMEOUT / 1000;
	timeout.tv_usec = (UNIX_SOCKET_TIMEOUT % 1000) * 1000;

	if (setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_client_receive",
			"Failed to set receive timeout", "setsockopt SO_RCVTIMEO failed", "Check socket state");
		return RALE_ERROR_GENERAL;
	}

	ssize_t recv_len = recv(client_sock, buffer, buffer_size, 0);
	if (recv_len < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			// No data available (non-blocking mode)
			*received_len = 0;
			return RALE_SUCCESS;
		}

		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_client_receive",
			"Failed to receive data", "Receive operation failed", "Check connection state");
		return RALE_ERROR_GENERAL;
	}

	if (recv_len == 0)
	{
		// Connection closed by server
		rale_set_error(RALE_ERROR_NETWORK_INIT, "unix_socket_client_receive",
			"Connection closed by server", "Server terminated connection", "Reconnect to server if needed");
		return RALE_ERROR_GENERAL;
	}

	*received_len = (size_t)recv_len;
	rale_debug_log("Received %zu bytes via unix socket", *received_len);
	return RALE_SUCCESS;
}

librale_status_t
unix_socket_client_disconnect(int client_sock)
{
	if (client_sock < 0)
	{
		rale_debug_log("Invalid client socket, nothing to disconnect");
		return RALE_SUCCESS;
	}

	close(client_sock);
	rale_debug_log("Unix socket client disconnected successfully");
	return RALE_SUCCESS;
}

/**
 * Callback for handling received messages on the server side.
 * Processes the command and sends a response back to the client.
 */
static int
unix_socket_on_receive(
	int fd,
	const char *buf,
	int len)
{
	char response[UXSOCK_RESPONSE_BUFFER_SIZE];
	size_t write_len;

	rale_debug_log("Received message on fd %d (Length %d): \"%.*s\"",
		fd, len, len, buf);

	/** Process the received RALE command */
	if (rale_process_command(buf, response, sizeof(response)) != 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, "unix_socket_on_receive",
			"Failed to process RALE command on fd %d.", NULL, NULL,
			fd);
		snprintf(response, sizeof(response),
			"{\"status\":\"RALE_ERROR\", \"message\":\"Failed to process command\"}");
	}

	/** Send the response back to the client */
	write_len = strlen(response);
	ssize_t bytes_written = write(fd, response, write_len);
	if ((size_t)bytes_written != write_len) /** Check if all bytes were written */
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_on_receive",
			"Failed to send full response on fd %d: %s. Sent %zd of %d bytes.",
			fd, strerror(errno), bytes_written, write_len);
		return -1;
	}

	rale_debug_log("Sent response on fd %d: \"%s\"",
		fd, response);
	return 0;
}

/**
 * Main server loop for handling client connections on a Unix domain socket.
 * Uses select() for non-blocking I/O to handle connection requests and data.
 * (Original name: comm_loop)
 */
int
unix_socket_server_loop(void)
{
	int            client_fd;
	char           buf[UXSOCK_READ_BUFFER_SIZE];	/** Buffer for reading client messages */
	ssize_t        num_read;
	struct timeval timeout;
	fd_set         read_fds;
	int            ret;

	/**
	 * Ensure server is initialized and socket file exists.
	 * If server_fd is invalid or the socket file is missing, try to re-init.
	 */
	if (server_fd == -1)
	{
		if (unix_socket_server_init() < 0)
		{
			rale_set_error(RALE_ERROR_INVALID_PARAMETER, "unix_socket_server_loop",
				"Server not initialized. Cannot run server loop.");
			return -1;
		}
	}
    /** Avoid re-initializing during runtime to prevent transient non-listening windows */

    /** Use short timeout for responsiveness */
    timeout.tv_sec = 0;
    	timeout.tv_usec = LIBRALE_SOCKET_TIMEOUT_US; /** 100ms */

	FD_ZERO(&read_fds);
	FD_SET(server_fd, &read_fds);

	/** Wait for a client connection with timeout */
	ret = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
	if (ret == -1)
	{
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_server_loop",
			"Select error on server socket %d: %s.",
			server_fd, strerror(errno));
		return -1;
	}
	else if (ret == 0)
	{
		return 0;
	}

	/** Accept a new client connection (blocking call) */
	client_fd = accept(server_fd, NULL, NULL);
	if (client_fd == -1)
	{
		if (errno == EINTR)	/** Interrupted by a signal */
		{
			rale_debug_log("Accept call interrupted by signal. Exiting loop.");
			return 0;
		}
		rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_server_loop",
			"Accept error on server socket %d: %s.",
			server_fd, strerror(errno));
		return -1;
	}

	rale_debug_log("Client connected");

	/** Handle data from connected client (blocking read) */
	while ((num_read = read(client_fd, buf, sizeof(buf) - 1)) > 0)
	{
		buf[num_read] = '\0';	/** Null-terminate the received data */

		if (unix_socket_on_receive(client_fd, buf, (int)num_read) != 0)
		{
			rale_debug_log("Error handling client message on fd %d.",
				client_fd);
			break;
		}
	}

	if (num_read == -1)
	{
		rale_debug_log("Read error from client fd %d: %s.",
			client_fd, strerror(errno));
	}
	else if (num_read == 0)
	{
		rale_debug_log("Client disconnected gracefully");
	}

	close(client_fd);
	rale_debug_log("Client connection closed");
	return 0;
}

static void *
unix_socket_server_accept_thread(void *arg)
{
	(void)arg; // Unused parameter

	fd_set read_fds;
	struct timeval timeout;

	rale_debug_log("Unix socket server accept thread started");

	while (unix_socket_server.running)
	{
		FD_ZERO(&read_fds);
		FD_SET(unix_socket_server.server_socket, &read_fds);

		timeout.tv_sec = 1;  // 1 second timeout
		timeout.tv_usec = 0;

		int select_result = select(unix_socket_server.server_socket + 1, &read_fds, NULL, NULL, &timeout);
		if (select_result < 0)
		{
			if (errno == EINTR)
			{
				// Interrupted by signal, continue
				continue;
			}
			rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_server_accept_thread",
				"Select error in accept thread", "Select operation failed", "Check system state");
			break;
		}

		if (select_result == 0)
		{
			// Timeout, continue loop
			continue;
		}

		if (FD_ISSET(unix_socket_server.server_socket, &read_fds))
		{
			int client_sock = accept(unix_socket_server.server_socket, NULL, NULL);
			if (client_sock < 0)
			{
				rale_set_error(RALE_ERROR_SYSTEM_CALL, "unix_socket_server_accept_thread",
					"Accept failed", "Accept operation failed", "Check socket state");
				continue;
			}

			// Handle new connection
			if (tcp_server_handle_connection(client_sock, "localhost", 0) != RALE_SUCCESS)
			{
				close(client_sock);
			}
		}
	}

	rale_debug_log("Unix socket server accept thread stopped");
	return NULL;
}

static librale_status_t
unix_socket_server_handle_connection(int client_sock)
{
	// Call connect callback
	if (unix_socket_server.on_connect)
	{
		unix_socket_server.on_connect(client_sock);
	}

	rale_debug_log("New unix socket connection (socket %d)", client_sock);
	return RALE_SUCCESS;
}
