/*-------------------------------------------------------------------------
 *
 * raled.c
 *		RALED daemon core functions with UDP integration.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		raled/src/raled.c
 *
 *------------------------------------------------------------------------- */

/** System headers */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/** Local headers */
#include "librale.h"
#include "udp.h"
#include "raled_logger.h"
#include "raled_inc.h"

/** External variables */
extern volatile int system_exit;
extern pthread_t server_thread, client_thread;
extern librale_config_t *librale_cfg;

/* UDP integration using connection-based API */
static int udp_initialized = 0;
static uint16_t udp_port = 0;
static connection_t *udp_connection = NULL;

/* Initialize RALED with UDP support */
int
raled_init(const config_t *config_param)
{
	raled_log_debug("Initialization started.");
	int result __attribute__((unused));
	char msg[128] __attribute__((unused));
	
	raled_log_info("Initializing process.");
	
	/* UDP server already initialized in initialize_librale() */
	/* No need to initialize it again here */
	if (config_param && config_param->node.rale_port > 0)
	{
		raled_log_debug("UDP already initialized on port \"%d\".", config_param->node.rale_port);
		udp_port = config_param->node.rale_port;
		udp_initialized = 1;
		/* Note: udp_connection is managed by the librale subsystem */
	}
	else
	{
		raled_log_warning("No UDP port configured, running without network communication.");
	}
	
	/* RALE consensus system already initialized in initialize_librale() */
	/* No need to initialize it again here */
	raled_log_debug("RALE consensus already initialized, skipping.");
	
	raled_log_info("RALED daemon initialized successfully.");
	raled_log_debug("Raled init completed successfully.");
	return 0;
}

/* Finalize RALED */
int
raled_finit(void)
{
	int result;
	char msg[128];
	
	raled_log_info("Finalizing RALED daemon.");
	
	/* Finalize RALE consensus system */
	result = librale_rale_finit();
	if (result != RALE_SUCCESS)
	{
		snprintf(msg, sizeof(msg), "failed to finalize RALE consensus: %d", result);
		raled_ereport(RALED_LOG_ERROR, "RALED", msg, "RALE consensus finalization failed", "check system state");
		return -1;
	}
	
	/* Cleanup UDP if initialized */
	if (udp_initialized && udp_connection)
	{
		udp_destroy(udp_connection);
		udp_connection = NULL;
		udp_initialized = 0;
		raled_log_network_event("UDP server cleaned up", NULL, 0);
	}
	
	raled_log_info("RALED daemon finalized successfully.");
	return 0;
}

/* Cleanup RALED resources */
void
raled_cleanup(void)
{
	raled_log_info("Cleaning up RALED daemon.");
	
	/** 1. Signal threads to exit */
	system_exit = 1;

	/** 2. Join threads if running (ignore errors if not started) */
	pthread_join(server_thread, NULL);
	pthread_join(client_thread, NULL);

	/** 3. Finalize communication and RALED subsystems */
	comm_finit();
	raled_finit();

	/** 4. Flush logs */
	fflush(stdout);
	fflush(stderr);
	
	raled_log_info("RALED daemon cleanup completed.");
}

/* Send UDP message to cluster nodes */
int
raled_udp_send(const char *ip, uint16_t port, const void *data, size_t data_len)
{
	if (!udp_initialized || !udp_connection)
	{
		raled_log_error("UDP not initialized, cannot send message.");
		return -1;
	}
	
	/* Convert data to string for the UDP API */
	char message[1024];
	if (data_len >= sizeof(message))
	{
		raled_log_error("Data too large for UDP message.");
		return -1;
	}
	
	memcpy(message, data, data_len);
	message[data_len] = '\0';
	
	int result = udp_sendto(udp_connection, message, ip, port);
	if (result == 0)
	{
		raled_log_network_event("UDP message sent", ip, port);
	}
	else
	{
		raled_log_error("Failed to send UDP message to \"%s\":\"%d\".", ip, port);
	}
	
	return result;
}

/* Receive UDP message from cluster nodes */
int
raled_udp_receive(void *buffer, size_t buffer_size, size_t *received_len,
                   char *sender_ip, size_t ip_size, uint16_t *sender_port)
{
	if (!udp_initialized || !udp_connection)
	{
		raled_log_error("UDP not initialized, cannot receive message.");
		return -1;
	}
	
	char message[1024];
	char sender_address[64];
	int port;
	
	int result = udp_recvfrom(udp_connection, message, sizeof(message), sender_address, &port);
	if (result == 0)
	{
		/* Copy received data */
		size_t msg_len = strlen(message);
		if (msg_len > buffer_size)
		{
			msg_len = buffer_size;
		}
		memcpy(buffer, message, msg_len);
		*received_len = msg_len;
		
		/* Copy sender info */
		if (sender_ip && ip_size > 0)
		{
			strncpy(sender_ip, sender_address, ip_size - 1);
			sender_ip[ip_size - 1] = '\0';
		}
		if (sender_port)
		{
			*sender_port = (uint16_t)port;
		}
		
		raled_log_network_event("UDP message received", sender_address, port);
	}
	
	return result;
}

/* Get UDP status */
int
raled_udp_get_status(void)
{
	if (!udp_initialized)
	{
		return -1;
	}
	
	raled_log_debug("UDP status: initialized=\"%d\", port=\"%d\".", udp_initialized, udp_port);
	return 0;
}

/* Check if UDP is available */
int
raled_udp_available(void)
{
	return udp_initialized;
}
