/*-------------------------------------------------------------------------
 *
 * tcp_client.c
 *    Implements TCP client functionality for RALE.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *    librale/src/tcp_client.c
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
#include <arpa/inet.h>
#include <stdbool.h>

/** Local headers */
#include "tcp_client.h"
#include "rale_error.h"

tcp_client_t *
tcp_client_init(const char *ip_address, int port,
               void (*on_receive)(int, const char *),
               void (*on_disconnection)(int, const char *, int),
               char *errbuf, size_t errbuflen)
{
    tcp_client_t *client;
    
    if (!ip_address || !on_receive || !on_disconnection || !errbuf) {
        if (errbuf && errbuflen > 0) {
            snprintf(errbuf, errbuflen, "Invalid parameters");
        }
        return NULL;
    }
    
    client = malloc(sizeof(tcp_client_t));
    if (!client) {
        if (errbuf && errbuflen > 0) {
            snprintf(errbuf, errbuflen, "Memory allocation failed");
        }
        return NULL;
    }
    
    memset(client, 0, sizeof(tcp_client_t));
    client->sock = -1;
    client->is_connected = 0;
    client->ip_address = strdup(ip_address);
    client->on_receive = on_receive;
    client->on_disconnection = on_disconnection;
    
    // Setup server address
    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip_address, &client->server_addr.sin_addr) <= 0) {
        if (errbuf && errbuflen > 0) {
            snprintf(errbuf, errbuflen, "Invalid IP address format");
        }
        free(client->ip_address);
        free(client);
        return NULL;
    }
    
    if (!client->ip_address) {
        if (errbuf && errbuflen > 0) {
            snprintf(errbuf, errbuflen, "Failed to duplicate IP address");
        }
        free(client);
        return NULL;
    }
    
    return client;
}

int
tcp_client_connect(tcp_client_t *client, const char *ip_address, int port)
{
    if (!client || !ip_address || port <= 0) {
        return -1;
    }

    // Close existing socket if connected
    if (client->is_connected && client->sock >= 0) {
        close(client->sock);
        client->sock = -1;
        client->is_connected = 0;
    }

    // Create new socket
    client->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client->sock < 0) {
        return -1;
    }

    // Setup server address
    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_address, &client->server_addr.sin_addr) <= 0) {
        close(client->sock);
        client->sock = -1;
        return -1;
    }

    // Attempt connection
    if (connect(client->sock, (struct sockaddr *)&client->server_addr, sizeof(client->server_addr)) < 0) {
        close(client->sock);
        client->sock = -1;
        return -1;
    }

    // Update client state
    client->is_connected = 1;
    
    return 0;
}

void
tcp_client_send(tcp_client_t *client, const char *message)
{
    if (!client || !message) {
        return;
    }

    if (!client->is_connected || client->sock < 0) {
        return;
    }

    size_t message_len = strlen(message);
    ssize_t sent = send(client->sock, message, message_len, 0);
    
    if (sent < 0 || (size_t)sent != message_len) {
        // Send failed or partial send
        client->is_connected = 0;
        if (client->on_disconnection) {
            client->on_disconnection(client->sock, client->ip_address, errno);
        }
    }
}

void
tcp_client_receive(tcp_client_t *client)
{
    char buffer[TCP_CLIENT_BUFFER_SIZE];
    ssize_t recv_len;
    
    if (!client) {
        return;
    }

    if (!client->is_connected || client->sock < 0) {
        return;
    }

    recv_len = recv(client->sock, buffer, sizeof(buffer) - 1, 0);
    if (recv_len <= 0) {
        // Connection closed or error
        client->is_connected = 0;
        if (client->on_disconnection) {
            client->on_disconnection(client->sock, client->ip_address, errno);
        }
        return;
    }

    // Null terminate and call receive callback
    buffer[recv_len] = '\0';
    if (client->on_receive) {
        client->on_receive(client->sock, buffer);
    }
}

void
tcp_client_cleanup(tcp_client_t *client)
{
    if (!client) {
        return;
    }
    
    if (client->sock >= 0) {
        close(client->sock);
        client->sock = -1;
    }
    
    if (client->ip_address) {
        free(client->ip_address);
        client->ip_address = NULL;
    }
    
    client->is_connected = 0;
    free(client);
}

void
tcp_client_run(tcp_client_t *client)
{
    if (!client) {
        return;
    }
    
    // Simple implementation - just receive once
    tcp_client_receive(client);
}
