/*-------------------------------------------------------------------------
 *
 * raled_rest_api.c
 *		REST API server implementation for raled daemon
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "raled_rest_api.h"
#include "raled_logger.h"
#include "../librale/include/librale.h"
#include "../librale/include/cluster.h"
#include "../librale/include/rale.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <cjson/cJSON.h>

/*-------------------------------------------------------------------------
 * Global Variables
 *-------------------------------------------------------------------------*/

static raled_rest_server_t *g_rest_server = NULL;
static rest_endpoint_t g_endpoints[32];
static int g_endpoint_count = 0;

/*-------------------------------------------------------------------------
 * Forward Declarations
 *-------------------------------------------------------------------------*/

static void *raled_rest_server_thread(void *arg);
static void raled_rest_handle_connection(int client_fd, struct sockaddr_in *client_addr);
static int raled_rest_route_request(const http_request_t *request, http_response_t *response);
static void raled_rest_cleanup_request(http_request_t *request);
static void raled_rest_cleanup_response(http_response_t *response);

/*-------------------------------------------------------------------------
 * REST API Server Functions
 *-------------------------------------------------------------------------*/

int
raled_rest_server_init(raled_rest_server_t *server, const raled_rest_config_t *config)
{
    if (server == NULL || config == NULL)
        return -1;

    memset(server, 0, sizeof(raled_rest_server_t));
    
    /* Copy configuration */
    server->config = *config;
    if (config->bind_address)
        server->config.bind_address = strdup(config->bind_address);
    if (config->api_key)
        server->config.api_key = strdup(config->api_key);
    if (config->ssl_cert_file)
        server->config.ssl_cert_file = strdup(config->ssl_cert_file);
    if (config->ssl_key_file)
        server->config.ssl_key_file = strdup(config->ssl_key_file);

    /* Initialize mutex */
    if (pthread_mutex_init(&server->mutex, NULL) != 0) {
        	raled_log_error("Failed to initialize REST API server mutex.");
        return -1;
    }

    server->server_fd = -1;
    server->running = false;
    g_rest_server = server;

    /* Register default endpoints */
    raled_rest_register_endpoint("/api/v1/status", HTTP_METHOD_GET, raled_rest_handle_status);
    raled_rest_register_endpoint("/api/v1/nodes", HTTP_METHOD_GET, raled_rest_handle_list_nodes);
    raled_rest_register_endpoint("/api/v1/nodes", HTTP_METHOD_POST, raled_rest_handle_add_node);
    raled_rest_register_endpoint("/api/v1/election/trigger", HTTP_METHOD_POST, raled_rest_handle_trigger_election);
    raled_rest_register_endpoint("/api/v1/leader/step-down", HTTP_METHOD_POST, raled_rest_handle_step_down);
    raled_rest_register_endpoint("/api/v1/health", HTTP_METHOD_GET, raled_rest_handle_health);
    raled_rest_register_endpoint("/api/v1/metrics", HTTP_METHOD_GET, raled_rest_handle_metrics);
    raled_rest_register_endpoint("/api/v1/shutdown", HTTP_METHOD_POST, raled_rest_handle_shutdown);

    	raled_log_info("REST API server initialized on \"%s\":\"%d\".", 
                   server->config.bind_address ? server->config.bind_address : "0.0.0.0",
                   server->config.port);

    return 0;
}

int
raled_rest_server_start(raled_rest_server_t *server)
{
    struct sockaddr_in  server_addr;
    int                 opt = 1;

    if (server == NULL)
        return -1;

    /* Create socket */
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd == -1) {
        	raled_log_error("Failed to create REST API server socket: \"%s\".", strerror(errno));
        return -1;
    }

    /* Set socket options */
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        	raled_log_warning("Failed to set SO_REUSEADDR: \"%s\".", strerror(errno));
    }

    /* Setup server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server->config.port);
    
    if (server->config.bind_address && strlen(server->config.bind_address) > 0) {
        if (inet_pton(AF_INET, server->config.bind_address, &server_addr.sin_addr) <= 0) {
            	raled_log_error("Invalid bind address: \"%s\".", server->config.bind_address);
            close(server->server_fd);
            return -1;
        }
    } else {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    }

    /* Bind socket */
    if (bind(server->server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        	raled_log_error("Failed to bind REST API server socket: \"%s\".", strerror(errno));
        close(server->server_fd);
        return -1;
    }

    /* Listen for connections */
    if (listen(server->server_fd, server->config.max_connections) == -1) {
        	raled_log_error("Failed to listen on REST API server socket: \"%s\".", strerror(errno));
        close(server->server_fd);
        return -1;
    }

    /* Create server thread */
    server->running = true;
    if (pthread_create(&server->server_thread, NULL, raled_rest_server_thread, server) != 0) {
        	raled_log_error("Failed to create REST API server thread.");
        server->running = false;
        close(server->server_fd);
        return -1;
    }

    	raled_log_info("REST API server started on \"%s\":\"%d\".", 
                   server->config.bind_address ? server->config.bind_address : "0.0.0.0",
                   server->config.port);

    return 0;
}

int
raled_rest_server_stop(raled_rest_server_t *server)
{
    if (server == NULL || !server->running)
        return 0;

    	raled_log_info("Stopping REST API server.");

    /* Signal server to stop */
    pthread_mutex_lock(&server->mutex);
    server->running = false;
    pthread_mutex_unlock(&server->mutex);

    /* Close server socket */
    if (server->server_fd != -1) {
        close(server->server_fd);
        server->server_fd = -1;
    }

    /* Wait for server thread to finish */
    if (pthread_join(server->server_thread, NULL) != 0) {
        	raled_log_warning("Failed to join REST API server thread.");
    }

    	raled_log_info("REST API server stopped.");
    return 0;
}

void
raled_rest_server_cleanup(raled_rest_server_t *server)
{
    if (server == NULL)
        return;

    raled_rest_server_stop(server);

    /* Free configuration strings */
    if (server->config.bind_address) {
        free(server->config.bind_address);
        server->config.bind_address = NULL;
    }
    if (server->config.api_key) {
        free(server->config.api_key);
        server->config.api_key = NULL;
    }
    if (server->config.ssl_cert_file) {
        free(server->config.ssl_cert_file);
        server->config.ssl_cert_file = NULL;
    }
    if (server->config.ssl_key_file) {
        free(server->config.ssl_key_file);
        server->config.ssl_key_file = NULL;
    }

    /* Destroy mutex */
    pthread_mutex_destroy(&server->mutex);

    g_rest_server = NULL;
}

int
raled_rest_register_endpoint(const char *path, http_method_t method, rest_endpoint_handler_t handler)
{
    if (path == NULL || handler == NULL || g_endpoint_count >= 32)
        return -1;

    g_endpoints[g_endpoint_count].path = strdup(path);
    g_endpoints[g_endpoint_count].method = method;
    g_endpoints[g_endpoint_count].handler = handler;
    g_endpoints[g_endpoint_count].requires_auth = false;
    g_endpoints[g_endpoint_count].description = NULL;

    g_endpoint_count++;

    	raled_log_debug("Registered REST endpoint: \"%s\".", path);
    return 0;
}

/*-------------------------------------------------------------------------
 * Server Thread Implementation
 *-------------------------------------------------------------------------*/

static void *
raled_rest_server_thread(void *arg)
{
    raled_rest_server_t     *server = (raled_rest_server_t *)arg;
    struct sockaddr_in      client_addr;
    socklen_t               client_len;
    int                     client_fd;

    	raled_log_debug("REST API server thread started.");

    while (server->running) {
        client_len = sizeof(client_addr);
        client_fd = accept(server->server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_fd == -1) {
            if (errno == EINTR) {
                /* Interrupted by signal */
                	raled_log_warning("REST API accept call interrupted by signal.");
                continue;
            }
            if (server->running) {
                	raled_log_error("REST API accept failed: \"%s\".", strerror(errno));
            }
            break;
        }

        /* Handle connection */
        raled_rest_handle_connection(client_fd, &client_addr);
        close(client_fd);
    }

    	raled_log_debug("REST API server thread stopped.");
    return NULL;
}

static void
raled_rest_handle_connection(int client_fd, struct sockaddr_in *client_addr)
{
    char                buffer[RALED_REST_BUFFER_SIZE];
    http_request_t      request = {0};
    http_response_t     response = {0};
    ssize_t             bytes_read;
    char                response_buffer[RALED_REST_BUFFER_SIZE];
    
    /* Read request */
    bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        	raled_log_warning("Failed to read HTTP request: \"%s\".", strerror(errno));
        return;
    }
    buffer[bytes_read] = '\0';

    /* Store client address */
    request.remote_addr = inet_ntoa(client_addr->sin_addr);
    request.remote_port = ntohs(client_addr->sin_port);

    /* Parse request */
    if (raled_http_parse_request(buffer, (size_t)bytes_read, &request) != 0) {
        	raled_log_warning("Failed to parse HTTP request from \"%s\":\"%d\".", 
                         request.remote_addr, request.remote_port);
        
        /* Send 400 Bad Request */
        response.status = HTTP_STATUS_BAD_REQUEST;
        raled_http_set_json_body(&response, "{\"error\":\"Bad Request\",\"message\":\"Invalid HTTP request\"}");
        raled_http_generate_response(&response, response_buffer, sizeof(response_buffer));
        write(client_fd, response_buffer, strlen(response_buffer));
        raled_rest_cleanup_response(&response);
        return;
    }

    /* Route request to handler */
    if (raled_rest_route_request(&request, &response) != 0) {
        /* Send 404 Not Found */
        response.status = HTTP_STATUS_NOT_FOUND;
        raled_http_set_json_body(&response, "{\"error\":\"Not Found\",\"message\":\"Endpoint not found\"}");
    }

    /* Add CORS headers if enabled */
    if (g_rest_server && g_rest_server->config.enable_cors) {
        raled_http_add_cors_headers(&response);
    }

    /* Generate and send response */
    if (raled_http_generate_response(&response, response_buffer, sizeof(response_buffer)) == 0) {
        write(client_fd, response_buffer, strlen(response_buffer));
    }

    /* Cleanup */
    raled_rest_cleanup_request(&request);
    raled_rest_cleanup_response(&response);
}

static int
raled_rest_route_request(const http_request_t *request, http_response_t *response)
{
    int     i;

    if (request == NULL || response == NULL)
        return -1;

    /* Check for authentication if required */
    if (g_rest_server && g_rest_server->config.api_key) {
        if (!raled_http_check_auth(request, g_rest_server->config.api_key)) {
            response->status = HTTP_STATUS_UNAUTHORIZED;
            raled_http_set_json_body(response, "{\"error\":\"Unauthorized\",\"message\":\"Invalid or missing API key\"}");
            return 0;
        }
    }

    /* Find matching endpoint */
    for (i = 0; i < g_endpoint_count; i++) {
        if (g_endpoints[i].method == request->method && 
            (strcmp(g_endpoints[i].path, request->path) == 0 || 
             raled_path_matches(request->path, g_endpoints[i].path))) {
            
            	raled_log_debug("Routing \"%s\" \"%s\" to handler.", 
                           request->method == HTTP_METHOD_GET ? "GET" :
                           request->method == HTTP_METHOD_POST ? "POST" :
                           request->method == HTTP_METHOD_PUT ? "PUT" :
                           request->method == HTTP_METHOD_DELETE ? "DELETE" : "UNKNOWN",
                           request->path);
            
            return g_endpoints[i].handler(request, response);
        }
    }

    return -1; /* Not found */
}

/*-------------------------------------------------------------------------
 * HTTP Request/Response Utilities
 *-------------------------------------------------------------------------*/

int
raled_http_parse_request(const char *raw_data, size_t data_length, http_request_t *request)
{
    const char  *line_start;
    const char  *line_end;
    const char  *method_end;
    const char  *path_end;
    const char  *body_start;
    char        *method_str;
    char        method_buffer[16];
    size_t      line_len;

    if (raw_data == NULL || request == NULL)
        return -1;

    /* Parse request line */
    line_start = raw_data;
    line_end = strstr(line_start, "\r\n");
    if (line_end == NULL)
        line_end = strstr(line_start, "\n");
    if (line_end == NULL)
        return -1;

    /* Extract method */
    method_end = strchr(line_start, ' ');
    if (method_end == NULL || method_end >= line_end)
        return -1;

    line_len = (size_t)(method_end - line_start);
    if (line_len >= sizeof(method_buffer))
        return -1;
    
    strncpy(method_buffer, line_start, line_len);
    method_buffer[line_len] = '\0';
    method_str = method_buffer;

    if (strcmp(method_str, "GET") == 0)
        request->method = HTTP_METHOD_GET;
    else if (strcmp(method_str, "POST") == 0)
        request->method = HTTP_METHOD_POST;
    else if (strcmp(method_str, "PUT") == 0)
        request->method = HTTP_METHOD_PUT;
    else if (strcmp(method_str, "DELETE") == 0)
        request->method = HTTP_METHOD_DELETE;
    else if (strcmp(method_str, "OPTIONS") == 0)
        request->method = HTTP_METHOD_OPTIONS;
    else if (strcmp(method_str, "HEAD") == 0)
        request->method = HTTP_METHOD_HEAD;
    else
        return -1;

    /* Extract path */
    line_start = method_end + 1;
    path_end = strchr(line_start, ' ');
    if (path_end == NULL || path_end >= line_end)
        return -1;

    line_len = (size_t)(path_end - line_start);
    request->path = malloc(line_len + 1);
    if (request->path == NULL)
        return -1;
    strncpy(request->path, line_start, line_len);
    request->path[line_len] = '\0';

    /* Check for query string */
    char *query_start = strchr(request->path, '?');
    if (query_start != NULL) {
        *query_start = '\0';
        query_start++;
        request->query_string = strdup(query_start);
    }

    /* Find body start (after headers) */
    body_start = strstr(raw_data, "\r\n\r\n");
    if (body_start == NULL)
        body_start = strstr(raw_data, "\n\n");
    
    if (body_start != NULL) {
        body_start += (body_start[1] == '\n') ? 2 : 4;
        size_t body_length = data_length - (size_t)(body_start - raw_data);
        if (body_length > 0) {
            request->body = malloc(body_length + 1);
            if (request->body != NULL) {
                memcpy(request->body, body_start, body_length);
                request->body[body_length] = '\0';
                request->body_length = body_length;
            }
        }
    }

    return 0;
}

int
raled_http_generate_response(const http_response_t *response, char *buffer, size_t buffer_size)
{
    const char  *status_text;
    size_t      written;
    int         i;

    if (response == NULL || buffer == NULL || buffer_size == 0)
        return -1;

    /* Get status text */
    switch (response->status) {
        case HTTP_STATUS_OK: status_text = "OK"; break;
        case HTTP_STATUS_CREATED: status_text = "Created"; break;
        case HTTP_STATUS_ACCEPTED: status_text = "Accepted"; break;
        case HTTP_STATUS_NO_CONTENT: status_text = "No Content"; break;
        case HTTP_STATUS_BAD_REQUEST: status_text = "Bad Request"; break;
        case HTTP_STATUS_UNAUTHORIZED: status_text = "Unauthorized"; break;
        case HTTP_STATUS_FORBIDDEN: status_text = "Forbidden"; break;
        case HTTP_STATUS_NOT_FOUND: status_text = "Not Found"; break;
        case HTTP_STATUS_METHOD_NOT_ALLOWED: status_text = "Method Not Allowed"; break;
        case HTTP_STATUS_CONFLICT: status_text = "Conflict"; break;
        case HTTP_STATUS_INTERNAL_ERROR: status_text = "Internal Server Error"; break;
        case HTTP_STATUS_NOT_IMPLEMENTED: status_text = "Not Implemented"; break;
        case HTTP_STATUS_SERVICE_UNAVAILABLE: status_text = "Service Unavailable"; break;
        default: status_text = "Unknown"; break;
    }

    /* Write status line */
    written = (size_t)snprintf(buffer, buffer_size, "HTTP/1.1 %d %s\r\n", response->status, status_text);
    if (written >= buffer_size)
        return -1;

    /* Write headers */
    for (i = 0; i < response->header_count && written < buffer_size; i++) {
        size_t header_len = (size_t)snprintf(buffer + written, buffer_size - written, 
                                           "%s: %s\r\n", 
                                           response->headers[i].key, 
                                           response->headers[i].value);
        if (header_len >= buffer_size - written)
            return -1;
        written += header_len;
    }

    /* Write content-type if not already set */
    if (response->content_type && written < buffer_size) {
        size_t ct_len = (size_t)snprintf(buffer + written, buffer_size - written, 
                                       "Content-Type: %s\r\n", response->content_type);
        if (ct_len >= buffer_size - written)
            return -1;
        written += ct_len;
    }

    /* Write content-length if body exists */
    if (response->body && response->body_length > 0 && written < buffer_size) {
        size_t cl_len = (size_t)snprintf(buffer + written, buffer_size - written, 
                                       "Content-Length: %zu\r\n", response->body_length);
        if (cl_len >= buffer_size - written)
            return -1;
        written += cl_len;
    }

    /* End headers */
    if (written + 2 < buffer_size) {
        strcpy(buffer + written, "\r\n");
        written += 2;
    } else {
        return -1;
    }

    /* Write body */
    if (response->body && response->body_length > 0 && written < buffer_size) {
        size_t remaining = buffer_size - written - 1;
        size_t copy_len = (response->body_length < remaining) ? response->body_length : remaining;
        memcpy(buffer + written, response->body, copy_len);
        written += copy_len;
        buffer[written] = '\0';
    }

    return 0;
}

int
raled_http_set_json_body(http_response_t *response, const char *json)
{
    if (response == NULL || json == NULL)
        return -1;

    if (response->body) {
        free(response->body);
    }

    response->body = strdup(json);
    if (response->body == NULL)
        return -1;

    response->body_length = strlen(json);
    response->content_type = strdup("application/json");

    return 0;
}

bool
raled_http_check_auth(const http_request_t *request, const char *api_key)
{
    const char *auth_header;

    if (request == NULL || api_key == NULL)
        return false;

    /* Check Authorization header */
    auth_header = raled_http_get_header(request, "Authorization");
    if (auth_header == NULL)
        return false;

    /* Simple Bearer token check */
    if (strncmp(auth_header, "Bearer ", 7) == 0) {
        return strcmp(auth_header + 7, api_key) == 0;
    }

    return false;
}

const char *
raled_http_get_header(const http_request_t *request, const char *key)
{
    int i;

    if (request == NULL || key == NULL)
        return NULL;

    for (i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i].key, key) == 0) {
            return request->headers[i].value;
        }
    }

    return NULL;
}

void
raled_http_add_cors_headers(http_response_t *response)
{
    if (response == NULL)
        return;

    raled_http_set_header(response, "Access-Control-Allow-Origin", "*");
    raled_http_set_header(response, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    raled_http_set_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
}

int
raled_http_set_header(http_response_t *response, const char *key, const char *value)
{
    http_header_t *new_headers;

    if (response == NULL || key == NULL || value == NULL)
        return -1;

    /* Reallocate headers array */
    new_headers = realloc(response->headers, sizeof(http_header_t) * (response->header_count + 1));
    if (new_headers == NULL)
        return -1;

    response->headers = new_headers;
    response->headers[response->header_count].key = strdup(key);
    response->headers[response->header_count].value = strdup(value);
    response->header_count++;

    return 0;
}

bool
raled_path_matches(const char *path, const char *pattern)
{
    /* Simple exact match for now */
    if (path == NULL || pattern == NULL)
        return false;
    
    return strcmp(path, pattern) == 0;
}

/*-------------------------------------------------------------------------
 * Cleanup Functions
 *-------------------------------------------------------------------------*/

static void
raled_rest_cleanup_request(http_request_t *request)
{
    int i;

    if (request == NULL)
        return;

    if (request->path) {
        free(request->path);
        request->path = NULL;
    }
    if (request->query_string) {
        free(request->query_string);
        request->query_string = NULL;
    }
    if (request->body) {
        free(request->body);
        request->body = NULL;
    }
    
    for (i = 0; i < request->header_count; i++) {
        if (request->headers[i].key)
            free(request->headers[i].key);
        if (request->headers[i].value)
            free(request->headers[i].value);
    }
    if (request->headers) {
        free(request->headers);
        request->headers = NULL;
    }
    request->header_count = 0;
}

static void
raled_rest_cleanup_response(http_response_t *response)
{
    int i;

    if (response == NULL)
        return;

    if (response->body) {
        free(response->body);
        response->body = NULL;
    }
    if (response->content_type) {
        free(response->content_type);
        response->content_type = NULL;
    }

    for (i = 0; i < response->header_count; i++) {
        if (response->headers[i].key)
            free(response->headers[i].key);
        if (response->headers[i].value)
            free(response->headers[i].value);
    }
    if (response->headers) {
        free(response->headers);
        response->headers = NULL;
    }
    response->header_count = 0;
}

/*-------------------------------------------------------------------------
 * REST API Endpoint Handlers (Stubs)
 *-------------------------------------------------------------------------*/

int
raled_rest_handle_status(const http_request_t *request, http_response_t *response)
{
    cJSON *json;
    char *json_string;

    (void)request; /* Unused parameter */

    json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", "running");
    cJSON_AddStringToObject(json, "version", "1.0.0");
    cJSON_AddNumberToObject(json, "uptime", 0); /* TODO: Calculate actual uptime */
    
    json_string = cJSON_Print(json);
    response->status = HTTP_STATUS_OK;
    raled_http_set_json_body(response, json_string);
    
    free(json_string);
    cJSON_Delete(json);
    
    return 0;
}

int
raled_rest_handle_list_nodes(const http_request_t *request, http_response_t *response)
{
    cJSON *json;
    cJSON *nodes_array;
    char *json_string;

    (void)request; /* Unused parameter */

    json = cJSON_CreateObject();
    nodes_array = cJSON_CreateArray();
    
    /* TODO: Add actual nodes from cluster */
    cJSON_AddItemToObject(json, "nodes", nodes_array);
    
    json_string = cJSON_Print(json);
    response->status = HTTP_STATUS_OK;
    raled_http_set_json_body(response, json_string);
    
    free(json_string);
    cJSON_Delete(json);
    
    return 0;
}

int
raled_rest_handle_add_node(const http_request_t *request, http_response_t *response)
{
    (void)request; /* Unused parameter */
    
    response->status = HTTP_STATUS_NOT_IMPLEMENTED;
    raled_http_set_json_body(response, "{\"error\":\"Not Implemented\",\"message\":\"Add node endpoint not yet implemented\"}");
    
    return 0;
}

int
raled_rest_handle_trigger_election(const http_request_t *request, http_response_t *response)
{
    (void)request; /* Unused parameter */
    
    response->status = HTTP_STATUS_NOT_IMPLEMENTED;
    raled_http_set_json_body(response, "{\"error\":\"Not Implemented\",\"message\":\"Trigger election endpoint not yet implemented\"}");
    
    return 0;
}

int
raled_rest_handle_step_down(const http_request_t *request, http_response_t *response)
{
    (void)request; /* Unused parameter */
    
    response->status = HTTP_STATUS_NOT_IMPLEMENTED;
    raled_http_set_json_body(response, "{\"error\":\"Not Implemented\",\"message\":\"Step down endpoint not yet implemented\"}");
    
    return 0;
}

int
raled_rest_handle_health(const http_request_t *request, http_response_t *response)
{
    (void)request; /* Unused parameter */
    
    response->status = HTTP_STATUS_OK;
    raled_http_set_json_body(response, "{\"status\":\"healthy\",\"timestamp\":0}");
    
    return 0;
}

int
raled_rest_handle_metrics(const http_request_t *request, http_response_t *response)
{
    (void)request; /* Unused parameter */
    
    response->status = HTTP_STATUS_NOT_IMPLEMENTED;
    raled_http_set_json_body(response, "{\"error\":\"Not Implemented\",\"message\":\"Metrics endpoint not yet implemented\"}");
    
    return 0;
}

int
raled_rest_handle_shutdown(const http_request_t *request, http_response_t *response)
{
    (void)request; /* Unused parameter */
    
    response->status = HTTP_STATUS_ACCEPTED;
    raled_http_set_json_body(response, "{\"status\":\"shutdown_initiated\",\"message\":\"Graceful shutdown initiated\"}");
    
    /* TODO: Trigger actual shutdown */
    
    return 0;
}
