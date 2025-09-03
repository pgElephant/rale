/*-------------------------------------------------------------------------
 *
 * ralectrl_http_client.c
 *		HTTP client implementation for ralectrl
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "ralectrl_http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/time.h>
#include <cjson/cJSON.h>

/*-------------------------------------------------------------------------
 * HTTP Client Configuration Functions
 *-------------------------------------------------------------------------*/

void
ralectrl_http_config_init(ralectrl_http_config_t *config)
{
    if (config == NULL)
        return;

    memset(config, 0, sizeof(ralectrl_http_config_t));
    config->server_host = strdup("localhost");
    config->server_port = 8080;
    config->timeout_seconds = RALECTRL_HTTP_DEFAULT_TIMEOUT;
    config->use_ssl = false;
    config->verify_ssl = true;
    config->max_redirects = RALECTRL_HTTP_MAX_REDIRECTS;
}

void
ralectrl_http_config_cleanup(ralectrl_http_config_t *config)
{
    if (config == NULL)
        return;

    if (config->server_host) {
        free(config->server_host);
        config->server_host = NULL;
    }
    if (config->api_key) {
        free(config->api_key);
        config->api_key = NULL;
    }
    if (config->ca_cert_file) {
        free(config->ca_cert_file);
        config->ca_cert_file = NULL;
    }
}

/*-------------------------------------------------------------------------
 * Low-level HTTP Functions
 *-------------------------------------------------------------------------*/

static int
ralectrl_http_connect(const char *host, uint16_t port, int timeout_seconds)
{
    struct sockaddr_in  server_addr;
    struct hostent      *server;
    int                 sockfd;
    struct timeval      tv;

    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error: Failed to create socket: %s\n", strerror(errno));
        return -1;
    }

    /* Set socket timeout */
    tv.tv_sec = timeout_seconds;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        fprintf(stderr, "Warning: Failed to set socket receive timeout\n");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        fprintf(stderr, "Warning: Failed to set socket send timeout\n");
    }

    /* Resolve hostname */
    server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr, "Error: No such host: %s\n", host);
        close(sockfd);
        return -1;
    }

    /* Setup server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], (size_t)server->h_length);
    server_addr.sin_port = htons(port);

    /* Connect to server */
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error: Failed to connect to %s:%d: %s\n", host, port, strerror(errno));
        close(sockfd);
        return -1;
    }

    return sockfd;
}

static int
ralectrl_http_send_request(int sockfd, const char *method, const char *path, 
                          const char *host, const char *headers, const char *body)
{
    char    request[RALECTRL_HTTP_BUFFER_SIZE];
    int     request_len;

    /* Build HTTP request */
    if (body && strlen(body) > 0) {
        request_len = snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "%s"
            "\r\n"
            "%s",
            method, path, host, strlen(body), headers ? headers : "", body);
    } else {
        request_len = snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "%s"
            "\r\n",
            method, path, host, headers ? headers : "");
    }

    if (request_len >= (int)sizeof(request)) {
        fprintf(stderr, "Error: HTTP request too large\n");
        return -1;
    }

    /* Send request */
    if (send(sockfd, request, (size_t)request_len, 0) < 0) {
        fprintf(stderr, "Error: Failed to send HTTP request: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int
ralectrl_http_receive_response(int sockfd, ralectrl_http_response_t *response)
{
    char        buffer[RALECTRL_HTTP_BUFFER_SIZE];
    ssize_t     bytes_received;
    char        *response_data = NULL;
    size_t      response_size = 0;
    char        *header_end;
    char        *status_line;
    char        *body_start;
    char        *content_length_str;
    int         content_length = 0;

    /* Receive response data */
    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        
        /* Reallocate response buffer */
        response_data = realloc(response_data, response_size + (size_t)bytes_received + 1);
        if (response_data == NULL) {
            fprintf(stderr, "Error: Failed to allocate memory for response\n");
            return -1;
        }
        
        memcpy(response_data + response_size, buffer, (size_t)bytes_received);
        response_size += (size_t)bytes_received;
        response_data[response_size] = '\0';
    }

    if (response_data == NULL) {
        fprintf(stderr, "Error: No response received\n");
        return -1;
    }

    /* Parse status line */
    status_line = response_data;
    if (strncmp(status_line, "HTTP/1.", 7) == 0) {
        char *space = strchr(status_line + 7, ' ');
        if (space) {
            response->status_code = atoi(space + 1);
        }
    }

    /* Find header/body separator */
    header_end = strstr(response_data, "\r\n\r\n");
    if (header_end == NULL) {
        header_end = strstr(response_data, "\n\n");
        if (header_end) {
            body_start = header_end + 2;
        } else {
            body_start = response_data + response_size;
        }
    } else {
        body_start = header_end + 4;
    }

    /* Extract Content-Length */
    content_length_str = strstr(response_data, "Content-Length:");
    if (content_length_str && content_length_str < body_start) {
        content_length = atoi(content_length_str + 15);
    }

    /* Extract Content-Type */
    char *content_type_str = strstr(response_data, "Content-Type:");
    if (content_type_str && content_type_str < body_start) {
        char *ct_start = content_type_str + 13;
        while (*ct_start == ' ') ct_start++;
        char *ct_end = strstr(ct_start, "\r\n");
        if (ct_end == NULL) ct_end = strstr(ct_start, "\n");
        if (ct_end) {
            size_t ct_len = (size_t)(ct_end - ct_start);
            response->content_type = malloc(ct_len + 1);
            if (response->content_type) {
                strncpy(response->content_type, ct_start, ct_len);
                response->content_type[ct_len] = '\0';
            }
        }
    }

    /* Extract body */
    if (body_start < response_data + response_size) {
        size_t body_len = response_size - (size_t)(body_start - response_data);
        if (content_length > 0 && body_len > (size_t)content_length) {
            body_len = (size_t)content_length;
        }
        
        response->body = malloc(body_len + 1);
        if (response->body) {
            memcpy(response->body, body_start, body_len);
            response->body[body_len] = '\0';
            response->body_length = body_len;
        }
    }

    free(response_data);
    return 0;
}

/*-------------------------------------------------------------------------
 * High-level HTTP Functions
 *-------------------------------------------------------------------------*/

int
ralectrl_http_get(const ralectrl_http_config_t *config, const char *path, ralectrl_http_response_t *response)
{
    int         sockfd;
    char        host_header[256];
    struct timeval start_time, end_time;

    if (config == NULL || path == NULL || response == NULL)
        return -1;

    memset(response, 0, sizeof(ralectrl_http_response_t));
    gettimeofday(&start_time, NULL);

    /* Connect to server */
    sockfd = ralectrl_http_connect(config->server_host, config->server_port, config->timeout_seconds);
    if (sockfd < 0)
        return -1;

    /* Prepare headers */
    snprintf(host_header, sizeof(host_header), "%s:%d", config->server_host, config->server_port);
    
    char headers[512] = "";
    if (config->api_key) {
        snprintf(headers, sizeof(headers), "Authorization: Bearer %s\r\n", config->api_key);
    }

    /* Send GET request */
    if (ralectrl_http_send_request(sockfd, "GET", path, host_header, headers, NULL) < 0) {
        close(sockfd);
        return -1;
    }

    /* Receive response */
    if (ralectrl_http_receive_response(sockfd, response) < 0) {
        close(sockfd);
        return -1;
    }

    close(sockfd);

    /* Calculate response time */
    gettimeofday(&end_time, NULL);
    response->response_time_ms = ((end_time.tv_sec - start_time.tv_sec) * 1000) + 
                                ((end_time.tv_usec - start_time.tv_usec) / 1000);

    return 0;
}

int
ralectrl_http_post_json(const ralectrl_http_config_t *config, const char *path, 
                        const char *json_body, ralectrl_http_response_t *response)
{
    int         sockfd;
    char        host_header[256];
    char        headers[512];
    struct timeval start_time, end_time;

    if (config == NULL || path == NULL || response == NULL)
        return -1;

    memset(response, 0, sizeof(ralectrl_http_response_t));
    gettimeofday(&start_time, NULL);

    /* Connect to server */
    sockfd = ralectrl_http_connect(config->server_host, config->server_port, config->timeout_seconds);
    if (sockfd < 0)
        return -1;

    /* Prepare headers */
    snprintf(host_header, sizeof(host_header), "%s:%d", config->server_host, config->server_port);
    
    if (config->api_key) {
        snprintf(headers, sizeof(headers), 
                "Content-Type: application/json\r\n"
                "Authorization: Bearer %s\r\n", config->api_key);
    } else {
        strcpy(headers, "Content-Type: application/json\r\n");
    }

    /* Send POST request */
    if (ralectrl_http_send_request(sockfd, "POST", path, host_header, headers, json_body) < 0) {
        close(sockfd);
        return -1;
    }

    /* Receive response */
    if (ralectrl_http_receive_response(sockfd, response) < 0) {
        close(sockfd);
        return -1;
    }

    close(sockfd);

    /* Calculate response time */
    gettimeofday(&end_time, NULL);
    response->response_time_ms = ((end_time.tv_sec - start_time.tv_sec) * 1000) + 
                                ((end_time.tv_usec - start_time.tv_usec) / 1000);

    return 0;
}

int
ralectrl_http_delete(const ralectrl_http_config_t *config, const char *path, ralectrl_http_response_t *response)
{
    int         sockfd;
    char        host_header[256];
    char        headers[512];
    struct timeval start_time, end_time;

    if (config == NULL || path == NULL || response == NULL)
        return -1;

    memset(response, 0, sizeof(ralectrl_http_response_t));
    gettimeofday(&start_time, NULL);

    /* Connect to server */
    sockfd = ralectrl_http_connect(config->server_host, config->server_port, config->timeout_seconds);
    if (sockfd < 0)
        return -1;

    /* Prepare headers */
    snprintf(host_header, sizeof(host_header), "%s:%d", config->server_host, config->server_port);
    
    if (config->api_key) {
        snprintf(headers, sizeof(headers), "Authorization: Bearer %s\r\n", config->api_key);
    } else {
        strcpy(headers, "");
    }

    /* Send DELETE request */
    if (ralectrl_http_send_request(sockfd, "DELETE", path, host_header, headers, NULL) < 0) {
        close(sockfd);
        return -1;
    }

    /* Receive response */
    if (ralectrl_http_receive_response(sockfd, response) < 0) {
        close(sockfd);
        return -1;
    }

    close(sockfd);

    /* Calculate response time */
    gettimeofday(&end_time, NULL);
    response->response_time_ms = ((end_time.tv_sec - start_time.tv_sec) * 1000) + 
                                ((end_time.tv_usec - start_time.tv_usec) / 1000);

    return 0;
}

void
ralectrl_http_response_cleanup(ralectrl_http_response_t *response)
{
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
    response->body_length = 0;
    response->status_code = 0;
    response->response_time_ms = 0;
}

/*-------------------------------------------------------------------------
 * REST API Wrapper Functions
 *-------------------------------------------------------------------------*/

int
ralectrl_api_get_status(const ralectrl_http_config_t *config, char *status_json, size_t buffer_size)
{
    ralectrl_http_response_t response;
    int result = -1;

    if (ralectrl_http_get(config, "/api/v1/status", &response) == 0) {
        if (response.status_code == 200 && response.body) {
            if (strlen(response.body) < buffer_size) {
                strcpy(status_json, response.body);
                result = 0;
            }
        }
    }

    ralectrl_http_response_cleanup(&response);
    return result;
}

int
ralectrl_api_list_nodes(const ralectrl_http_config_t *config, char *nodes_json, size_t buffer_size)
{
    ralectrl_http_response_t response;
    int result = -1;

    if (ralectrl_http_get(config, "/api/v1/nodes", &response) == 0) {
        if (response.status_code == 200 && response.body) {
            if (strlen(response.body) < buffer_size) {
                strcpy(nodes_json, response.body);
                result = 0;
            }
        }
    }

    ralectrl_http_response_cleanup(&response);
    return result;
}

int
ralectrl_api_trigger_election(const ralectrl_http_config_t *config, char *response_json, size_t buffer_size)
{
    ralectrl_http_response_t response;
    int result = -1;

    if (ralectrl_http_post_json(config, "/api/v1/election/trigger", "{}", &response) == 0) {
        if (response.body && strlen(response.body) < buffer_size) {
            strcpy(response_json, response.body);
            result = (response.status_code == 200 || response.status_code == 202) ? 0 : -1;
        }
    }

    ralectrl_http_response_cleanup(&response);
    return result;
}

int
ralectrl_api_get_health(const ralectrl_http_config_t *config, char *health_json, size_t buffer_size)
{
    ralectrl_http_response_t response;
    int result = -1;

    if (ralectrl_http_get(config, "/api/v1/health", &response) == 0) {
        if (response.status_code == 200 && response.body) {
            if (strlen(response.body) < buffer_size) {
                strcpy(health_json, response.body);
                result = 0;
            }
        }
    }

    ralectrl_http_response_cleanup(&response);
    return result;
}

/*-------------------------------------------------------------------------
 * Utility Functions
 *-------------------------------------------------------------------------*/

int
ralectrl_parse_server_url(const char *url, char **host, uint16_t *port, bool *use_ssl)
{
    const char *proto_end;
    const char *host_start;
    const char *port_start;
    char *host_end;
    size_t host_len;

    if (url == NULL || host == NULL || port == NULL || use_ssl == NULL)
        return -1;

    /* Check protocol */
    if (strncmp(url, "http://", 7) == 0) {
        *use_ssl = false;
        *port = 80;
        host_start = url + 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        *use_ssl = true;
        *port = 443;
        host_start = url + 8;
    } else {
        /* No protocol specified, assume http */
        *use_ssl = false;
        *port = 8080; /* Default for raled */
        host_start = url;
    }

    /* Find port separator */
    port_start = strchr(host_start, ':');
    if (port_start) {
        host_len = (size_t)(port_start - host_start);
        *port = (uint16_t)atoi(port_start + 1);
    } else {
        /* Find path separator */
        host_end = strchr(host_start, '/');
        if (host_end) {
            host_len = (size_t)(host_end - host_start);
        } else {
            host_len = strlen(host_start);
        }
    }

    /* Extract hostname */
    *host = malloc(host_len + 1);
    if (*host == NULL)
        return -1;
    
    strncpy(*host, host_start, host_len);
    (*host)[host_len] = '\0';

    return 0;
}

bool
ralectrl_http_is_success(const ralectrl_http_response_t *response)
{
    return response && response->status_code >= 200 && response->status_code < 300;
}

int
ralectrl_extract_error_message(const char *json_response, char *error_message, size_t buffer_size)
{
    cJSON *json;
    cJSON *error_obj;
    cJSON *message_obj;
    const char *message;

    if (json_response == NULL || error_message == NULL)
        return -1;

    json = cJSON_Parse(json_response);
    if (json == NULL)
        return -1;

    /* Try to get error.message */
    error_obj = cJSON_GetObjectItem(json, "error");
    if (error_obj && cJSON_IsString(error_obj)) {
        message = cJSON_GetStringValue(error_obj);
    } else {
        /* Try to get message */
        message_obj = cJSON_GetObjectItem(json, "message");
        if (message_obj && cJSON_IsString(message_obj)) {
            message = cJSON_GetStringValue(message_obj);
        } else {
            message = "Unknown error";
        }
    }

    if (strlen(message) < buffer_size) {
        strcpy(error_message, message);
    } else {
        strncpy(error_message, message, buffer_size - 1);
        error_message[buffer_size - 1] = '\0';
    }

    cJSON_Delete(json);
    return 0;
}

void
ralectrl_print_api_error(const ralectrl_http_response_t *response)
{
    char error_message[256];

    if (response == NULL) {
        fprintf(stderr, "Error: No response received\n");
        return;
    }

    fprintf(stderr, "Error: HTTP %d", response->status_code);

    if (response->body && ralectrl_extract_error_message(response->body, error_message, sizeof(error_message)) == 0) {
        fprintf(stderr, " - %s\n", error_message);
    } else {
        fprintf(stderr, "\n");
    }
}

int
ralectrl_test_connection(const ralectrl_http_config_t *config)
{
    char health_json[1024];
    return ralectrl_api_get_health(config, health_json, sizeof(health_json));
}
