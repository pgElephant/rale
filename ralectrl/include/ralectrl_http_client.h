/*-------------------------------------------------------------------------
 *
 * ralectrl_http_client.h
 *		HTTP client for ralectrl to communicate with raled REST API
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALECTRL_HTTP_CLIENT_H
#define RALECTRL_HTTP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*-------------------------------------------------------------------------
 * HTTP Client Configuration
 *-------------------------------------------------------------------------*/

#define RALECTRL_HTTP_DEFAULT_TIMEOUT   30
#define RALECTRL_HTTP_BUFFER_SIZE       8192
#define RALECTRL_HTTP_MAX_REDIRECTS     5

typedef struct {
    char        *server_host;           /* Server hostname or IP */
    uint16_t    server_port;            /* Server port */
    char        *api_key;               /* Optional API key */
    int         timeout_seconds;        /* Request timeout */
    bool        use_ssl;                /* Use HTTPS */
    bool        verify_ssl;             /* Verify SSL certificates */
    char        *ca_cert_file;          /* CA certificate file */
    int         max_redirects;          /* Maximum HTTP redirects to follow */
} ralectrl_http_config_t;

typedef struct {
    int         status_code;            /* HTTP status code */
    char        *body;                  /* Response body */
    size_t      body_length;            /* Response body length */
    char        *content_type;          /* Content-Type header */
    long        response_time_ms;       /* Response time in milliseconds */
} ralectrl_http_response_t;

/*-------------------------------------------------------------------------
 * HTTP Client Functions
 *-------------------------------------------------------------------------*/

/**
 * Initialize HTTP client configuration with defaults
 */
void ralectrl_http_config_init(ralectrl_http_config_t *config);

/**
 * Cleanup HTTP client configuration
 */
void ralectrl_http_config_cleanup(ralectrl_http_config_t *config);

/**
 * Make HTTP GET request
 */
int ralectrl_http_get(const ralectrl_http_config_t *config, const char *path, 
                      ralectrl_http_response_t *response);

/**
 * Make HTTP POST request with JSON body
 */
int ralectrl_http_post_json(const ralectrl_http_config_t *config, const char *path, 
                            const char *json_body, ralectrl_http_response_t *response);

/**
 * Make HTTP PUT request with JSON body
 */
int ralectrl_http_put_json(const ralectrl_http_config_t *config, const char *path, 
                           const char *json_body, ralectrl_http_response_t *response);

/**
 * Make HTTP DELETE request
 */
int ralectrl_http_delete(const ralectrl_http_config_t *config, const char *path, 
                         ralectrl_http_response_t *response);

/**
 * Cleanup HTTP response
 */
void ralectrl_http_response_cleanup(ralectrl_http_response_t *response);

/*-------------------------------------------------------------------------
 * REST API Wrapper Functions
 *-------------------------------------------------------------------------*/

/**
 * Get cluster status from raled
 */
int ralectrl_api_get_status(const ralectrl_http_config_t *config, char *status_json, size_t buffer_size);

/**
 * List all nodes in the cluster
 */
int ralectrl_api_list_nodes(const ralectrl_http_config_t *config, char *nodes_json, size_t buffer_size);

/**
 * Get specific node information
 */
int ralectrl_api_get_node(const ralectrl_http_config_t *config, int node_id, char *node_json, size_t buffer_size);

/**
 * Add new node to cluster
 */
int ralectrl_api_add_node(const ralectrl_http_config_t *config, int node_id, const char *name, 
                          const char *ip, int rale_port, int dstore_port, char *response_json, size_t buffer_size);

/**
 * Remove node from cluster
 */
int ralectrl_api_remove_node(const ralectrl_http_config_t *config, int node_id, char *response_json, size_t buffer_size);

/**
 * Trigger leader election
 */
int ralectrl_api_trigger_election(const ralectrl_http_config_t *config, char *response_json, size_t buffer_size);

/**
 * Request leader to step down
 */
int ralectrl_api_step_down(const ralectrl_http_config_t *config, char *response_json, size_t buffer_size);

/**
 * Get health status
 */
int ralectrl_api_get_health(const ralectrl_http_config_t *config, char *health_json, size_t buffer_size);

/**
 * Get system metrics
 */
int ralectrl_api_get_metrics(const ralectrl_http_config_t *config, char *metrics_json, size_t buffer_size);

/**
 * Request graceful shutdown
 */
int ralectrl_api_shutdown(const ralectrl_http_config_t *config, char *response_json, size_t buffer_size);

/*-------------------------------------------------------------------------
 * Utility Functions
 *-------------------------------------------------------------------------*/

/**
 * Parse server URL into host and port
 */
int ralectrl_parse_server_url(const char *url, char **host, uint16_t *port, bool *use_ssl);

/**
 * Build full API URL from path
 */
int ralectrl_build_api_url(const ralectrl_http_config_t *config, const char *path, 
                           char *url, size_t url_size);

/**
 * Check if response is successful (2xx status)
 */
bool ralectrl_http_is_success(const ralectrl_http_response_t *response);

/**
 * Extract error message from JSON error response
 */
int ralectrl_extract_error_message(const char *json_response, char *error_message, size_t buffer_size);

/**
 * Format JSON error for display
 */
void ralectrl_print_api_error(const ralectrl_http_response_t *response);

/*-------------------------------------------------------------------------
 * Connection Testing
 *-------------------------------------------------------------------------*/

/**
 * Test connection to raled server
 */
int ralectrl_test_connection(const ralectrl_http_config_t *config);

/**
 * Check API health endpoint
 */
int ralectrl_check_api_health(const ralectrl_http_config_t *config);

#endif												/* RALECTRL_HTTP_CLIENT_H */
