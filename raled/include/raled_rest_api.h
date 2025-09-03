/*-------------------------------------------------------------------------
 *
 * raled_rest_api.h
 *		REST API server for raled daemon communication
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RALED_REST_API_H
#define RALED_REST_API_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

/*-------------------------------------------------------------------------
 * REST API Configuration
 *-------------------------------------------------------------------------*/

#define RALED_REST_DEFAULT_PORT     8080
#define RALED_REST_MAX_CONNECTIONS  100
#define RALED_REST_BUFFER_SIZE      8192
#define RALED_REST_TIMEOUT_SECONDS  30

typedef struct {
    char        *bind_address;          /* IP address to bind to */
    uint16_t    port;                   /* Port to listen on */
    int         max_connections;        /* Maximum concurrent connections */
    int         timeout_seconds;        /* Request timeout */
    bool        enable_cors;            /* Enable CORS headers */
    char        *api_key;               /* Optional API key for authentication */
    bool        enable_ssl;             /* Enable HTTPS */
    char        *ssl_cert_file;         /* SSL certificate file */
    char        *ssl_key_file;          /* SSL private key file */
} raled_rest_config_t;

typedef struct {
    int                     server_fd;
    pthread_t               server_thread;
    bool                    running;
    raled_rest_config_t     config;
    pthread_mutex_t         mutex;
} raled_rest_server_t;

/*-------------------------------------------------------------------------
 * HTTP Request/Response Structures
 *-------------------------------------------------------------------------*/

typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_HEAD
} http_method_t;

typedef enum {
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_CREATED = 201,
    HTTP_STATUS_ACCEPTED = 202,
    HTTP_STATUS_NO_CONTENT = 204,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_UNAUTHORIZED = 401,
    HTTP_STATUS_FORBIDDEN = 403,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
    HTTP_STATUS_CONFLICT = 409,
    HTTP_STATUS_INTERNAL_ERROR = 500,
    HTTP_STATUS_NOT_IMPLEMENTED = 501,
    HTTP_STATUS_SERVICE_UNAVAILABLE = 503
} http_status_t;

typedef struct {
    char                *key;
    char                *value;
} http_header_t;

typedef struct {
    http_method_t       method;
    char                *path;
    char                *query_string;
    http_header_t       *headers;
    int                 header_count;
    char                *body;
    size_t              body_length;
    char                *remote_addr;
    uint16_t            remote_port;
} http_request_t;

typedef struct {
    http_status_t       status;
    http_header_t       *headers;
    int                 header_count;
    char                *body;
    size_t              body_length;
    char                *content_type;
} http_response_t;

/*-------------------------------------------------------------------------
 * REST API Endpoints
 *-------------------------------------------------------------------------*/

typedef int (*rest_endpoint_handler_t)(const http_request_t *request, http_response_t *response);

typedef struct {
    char                        *path;
    http_method_t               method;
    rest_endpoint_handler_t     handler;
    bool                        requires_auth;
    char                        *description;
} rest_endpoint_t;

/*-------------------------------------------------------------------------
 * REST API Server Functions
 *-------------------------------------------------------------------------*/

/**
 * Initialize REST API server
 */
int raled_rest_server_init(raled_rest_server_t *server, const raled_rest_config_t *config);

/**
 * Start REST API server
 */
int raled_rest_server_start(raled_rest_server_t *server);

/**
 * Stop REST API server
 */
int raled_rest_server_stop(raled_rest_server_t *server);

/**
 * Cleanup REST API server
 */
void raled_rest_server_cleanup(raled_rest_server_t *server);

/**
 * Register REST API endpoint
 */
int raled_rest_register_endpoint(const char *path, http_method_t method, rest_endpoint_handler_t handler);

/*-------------------------------------------------------------------------
 * HTTP Request/Response Utilities
 *-------------------------------------------------------------------------*/

/**
 * Parse HTTP request from raw data
 */
int raled_http_parse_request(const char *raw_data, size_t data_length, http_request_t *request);

/**
 * Generate HTTP response as string
 */
int raled_http_generate_response(const http_response_t *response, char *buffer, size_t buffer_size);

/**
 * Set HTTP response header
 */
int raled_http_set_header(http_response_t *response, const char *key, const char *value);

/**
 * Set HTTP response body as JSON
 */
int raled_http_set_json_body(http_response_t *response, const char *json);

/**
 * Set HTTP response body as plain text
 */
int raled_http_set_text_body(http_response_t *response, const char *text);

/**
 * Get HTTP request header value
 */
const char *raled_http_get_header(const http_request_t *request, const char *key);

/**
 * Check if request has valid authentication
 */
bool raled_http_check_auth(const http_request_t *request, const char *api_key);

/**
 * Send CORS headers if enabled
 */
void raled_http_add_cors_headers(http_response_t *response);

/*-------------------------------------------------------------------------
 * REST API Endpoint Handlers
 *-------------------------------------------------------------------------*/

/**
 * GET /api/v1/status - Get cluster status
 */
int raled_rest_handle_status(const http_request_t *request, http_response_t *response);

/**
 * GET /api/v1/nodes - List all nodes
 */
int raled_rest_handle_list_nodes(const http_request_t *request, http_response_t *response);

/**
 * GET /api/v1/nodes/{id} - Get specific node info
 */
int raled_rest_handle_get_node(const http_request_t *request, http_response_t *response);

/**
 * POST /api/v1/nodes - Add new node
 */
int raled_rest_handle_add_node(const http_request_t *request, http_response_t *response);

/**
 * DELETE /api/v1/nodes/{id} - Remove node
 */
int raled_rest_handle_remove_node(const http_request_t *request, http_response_t *response);

/**
 * POST /api/v1/election/trigger - Trigger leader election
 */
int raled_rest_handle_trigger_election(const http_request_t *request, http_response_t *response);

/**
 * POST /api/v1/leader/step-down - Step down as leader
 */
int raled_rest_handle_step_down(const http_request_t *request, http_response_t *response);

/**
 * GET /api/v1/health - Health check endpoint
 */
int raled_rest_handle_health(const http_request_t *request, http_response_t *response);

/**
 * GET /api/v1/metrics - Get system metrics
 */
int raled_rest_handle_metrics(const http_request_t *request, http_response_t *response);

/**
 * POST /api/v1/shutdown - Graceful shutdown
 */
int raled_rest_handle_shutdown(const http_request_t *request, http_response_t *response);

/*-------------------------------------------------------------------------
 * JSON Utilities for API Responses
 *-------------------------------------------------------------------------*/

/**
 * Create JSON error response
 */
char *raled_json_create_error(const char *error_code, const char *message);

/**
 * Create JSON success response
 */
char *raled_json_create_success(const char *message, const char *data);

/**
 * Create JSON node list response
 */
char *raled_json_create_node_list(void);

/**
 * Create JSON cluster status response
 */
char *raled_json_create_cluster_status(void);

/*-------------------------------------------------------------------------
 * Utility Functions
 *-------------------------------------------------------------------------*/

/**
 * URL decode string
 */
int raled_url_decode(const char *src, char *dest, size_t dest_size);

/**
 * Extract path parameter from URL
 */
const char *raled_extract_path_param(const char *path, const char *pattern, const char *param_name);

/**
 * Check if path matches pattern
 */
bool raled_path_matches(const char *path, const char *pattern);

#endif												/* RALED_REST_API_H */
