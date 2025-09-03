# RALE API Reference

## Overview

This document provides comprehensive API reference for the librale distributed consensus and key-value store library. All public APIs are thread-safe and designed for production use.

## Core Headers

### Primary Header
```c
#include <librale.h>
```

### Public API Header
```c
#include <public_api.h>
```

## Data Types

### Cluster Node
```c
typedef struct Node {
    int    id;                    /* Unique node identifier */
    char   name[MAX_NODE_NAME];   /* Human-readable node name */
    char   ip[MAX_IP_LENGTH];     /* IP address */
    int    rale_port;             /* RALE consensus port */
    int    dstore_port;           /* DStore service port */
    int    state;                 /* Current node state */
    int    priority;              /* Election priority */
    int    term;                  /* Current term number */
    int    last_log_index;        /* Last log index */
    int    last_log_term;         /* Last log term */
} Node;
```

### Context
```c
typedef struct rale_context_t {
    pthread_mutex_t mutex;        /* Thread synchronization */
    pthread_cond_t  cond;         /* Condition variable */
    int            initialized;   /* Initialization flag */
} rale_context_t;
```

### Log Levels
```c
typedef enum {
    LOG_ERROR = 0,               /* Error conditions */
    LOG_INFO = 1,                /* Informational messages */
    LOG_WARNING = 2,             /* Warning conditions */
    LOG_DEBUG = 3                /* Debug messages */
} LogLevel;
```

### Error Codes
```c
#define LIBRALE_SUCCESS          0    /* Operation successful */
#define LIBRALE_ERROR           -1    /* General error */
#define LIBRALE_ERROR_MEMORY    -2    /* Memory allocation error */
#define LIBRALE_ERROR_NETWORK   -3    /* Network error */
#define LIBRALE_ERROR_TIMEOUT   -4    /* Timeout error */
#define LIBRALE_ERROR_INVALID   -5    /* Invalid parameter */
```

## Initialization and Cleanup

### librale_context_create
Creates a new RALE context for thread-safe operations.

```c
rale_context_t *librale_context_create(void);
```

**Returns:**
- Pointer to initialized context on success
- `NULL` on memory allocation failure

**Example:**
```c
rale_context_t *ctx = librale_context_create();
if (ctx == NULL) {
    fprintf(stderr, "Failed to create RALE context\n");
    return -1;
}
```

### librale_context_destroy
Destroys a RALE context and releases resources.

```c
void librale_context_destroy(rale_context_t *ctx);
```

**Parameters:**
- `ctx`: Context to destroy (can be `NULL`)

**Example:**
```c
librale_context_destroy(ctx);
ctx = NULL;
```

## Cluster Management

### librale_cluster_init
Initializes the cluster subsystem.

```c
int librale_cluster_init(void);
```

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

**Example:**
```c
if (librale_cluster_init() != LIBRALE_SUCCESS) {
    fprintf(stderr, "Failed to initialize cluster\n");
    return -1;
}
```

### librale_cluster_add_node
Adds a node to the cluster configuration.

```c
int librale_cluster_add_node(int node_id, const char *name, const char *ip,
                             int rale_port, int dstore_port);
```

**Parameters:**
- `node_id`: Unique node identifier (0-9999)
- `name`: Human-readable node name
- `ip`: IP address string
- `rale_port`: Port for RALE consensus protocol
- `dstore_port`: Port for DStore operations

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

**Example:**
```c
int result = librale_cluster_add_node(1, "node1", "10.0.0.1", 7400, 7401);
if (result != LIBRALE_SUCCESS) {
    fprintf(stderr, "Failed to add node to cluster\n");
    return -1;
}
```

### librale_cluster_remove_node
Removes a node from the cluster configuration.

```c
int librale_cluster_remove_node(int node_id);
```

**Parameters:**
- `node_id`: Node identifier to remove

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

### librale_cluster_get_nodes
Retrieves current cluster node list.

```c
int librale_cluster_get_nodes(Node *nodes, int *count, int max_nodes);
```

**Parameters:**
- `nodes`: Array to store node information
- `count`: Pointer to store actual node count
- `max_nodes`: Maximum nodes array can hold

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

**Example:**
```c
Node nodes[10];
int count;
int result = librale_cluster_get_nodes(nodes, &count, 10);
if (result == LIBRALE_SUCCESS) {
    printf("Cluster has %d nodes\n", count);
    for (int i = 0; i < count; i++) {
        printf("Node %d: %s (%s:%d)\n", 
               nodes[i].id, nodes[i].name, nodes[i].ip, nodes[i].rale_port);
    }
}
```

## Consensus Operations

### librale_rale_init
Initializes the RALE consensus subsystem.

```c
int librale_rale_init(int node_id, const char *data_dir);
```

**Parameters:**
- `node_id`: This node's identifier
- `data_dir`: Directory for persistent state

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

### librale_rale_start
Starts the RALE consensus protocol.

```c
int librale_rale_start(void);
```

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

### librale_rale_stop
Stops the RALE consensus protocol.

```c
int librale_rale_stop(void);
```

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

### librale_rale_is_leader
Checks if this node is the current leader.

```c
int librale_rale_is_leader(void);
```

**Returns:**
- `1` if this node is leader
- `0` if this node is not leader
- `LIBRALE_ERROR` on error

**Example:**
```c
int is_leader = librale_rale_is_leader();
if (is_leader == 1) {
    printf("This node is the leader\n");
} else if (is_leader == 0) {
    printf("This node is a follower\n");
} else {
    fprintf(stderr, "Error checking leadership status\n");
}
```

### librale_rale_get_leader
Gets the current cluster leader information.

```c
int librale_rale_get_leader(int *leader_id);
```

**Parameters:**
- `leader_id`: Pointer to store leader node ID

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

## Distributed Store Operations

### librale_dstore_init
Initializes the distributed store subsystem.

```c
int librale_dstore_init(const char *data_dir);
```

**Parameters:**
- `data_dir`: Directory for data storage

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

### librale_dstore_put
Stores a key-value pair in the distributed store.

```c
int librale_dstore_put(const char *key, const void *value, size_t value_len);
```

**Parameters:**
- `key`: Key string (null-terminated)
- `value`: Value data
- `value_len`: Length of value data

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

**Example:**
```c
const char *key = "user:123:name";
const char *value = "John Doe";
int result = librale_dstore_put(key, value, strlen(value));
if (result != LIBRALE_SUCCESS) {
    fprintf(stderr, "Failed to store key-value pair\n");
}
```

### librale_dstore_get
Retrieves a value by key from the distributed store.

```c
int librale_dstore_get(const char *key, void *value, size_t *value_len);
```

**Parameters:**
- `key`: Key string to lookup
- `value`: Buffer to store retrieved value
- `value_len`: Pointer to buffer size (input) and actual size (output)

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

**Example:**
```c
char buffer[1024];
size_t len = sizeof(buffer);
int result = librale_dstore_get("user:123:name", buffer, &len);
if (result == LIBRALE_SUCCESS) {
    buffer[len] = '\0';  /* Null-terminate if string */
    printf("Retrieved value: %s\n", buffer);
} else {
    fprintf(stderr, "Key not found or error occurred\n");
}
```

### librale_dstore_delete
Deletes a key from the distributed store.

```c
int librale_dstore_delete(const char *key);
```

**Parameters:**
- `key`: Key string to delete

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

### librale_dstore_list
Lists keys matching a prefix.

```c
int librale_dstore_list(const char *prefix, char **keys, int *count, int max_keys);
```

**Parameters:**
- `prefix`: Key prefix to match
- `keys`: Array of string pointers to store keys
- `count`: Pointer to store actual key count
- `max_keys`: Maximum keys array can hold

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

## Network Operations

### librale_network_init
Initializes network subsystem.

```c
int librale_network_init(void);
```

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

### librale_network_start
Starts network services (TCP/UDP servers).

```c
int librale_network_start(int rale_port, int dstore_port);
```

**Parameters:**
- `rale_port`: Port for RALE consensus protocol
- `dstore_port`: Port for DStore operations

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

### librale_network_stop
Stops network services.

```c
int librale_network_stop(void);
```

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

## Configuration Management

### librale_config_load
Loads configuration from file.

```c
int librale_config_load(const char *config_file);
```

**Parameters:**
- `config_file`: Path to configuration file

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

### librale_config_get
Gets a configuration value.

```c
int librale_config_get(const char *key, char *value, size_t value_len);
```

**Parameters:**
- `key`: Configuration key
- `value`: Buffer to store value
- `value_len`: Buffer size

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

### librale_config_set
Sets a configuration value.

```c
int librale_config_set(const char *key, const char *value);
```

**Parameters:**
- `key`: Configuration key
- `value`: Configuration value

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

## Logging and Diagnostics

### Log Callback Type
```c
typedef int (*librale_log_callback_t)(librale_log_level level, 
                                     const char *prefix,
                                     const char *message, 
                                     const char *detail,
                                     const char *hint);
```

### librale_log_set_callback
Sets a custom log callback function.

```c
int librale_log_set_callback(librale_log_callback_t callback);
```

**Parameters:**
- `callback`: Log callback function (or `NULL` to disable)

**Returns:**
- `LIBRALE_SUCCESS` on success

### librale_log_set_level
Sets the minimum log level.

```c
int librale_log_set_level(librale_log_level level);
```

**Parameters:**
- `level`: Minimum log level to output

**Returns:**
- `LIBRALE_SUCCESS` on success

### elog
Outputs a log message.

```c
void elog(librale_log_level level, const char *prefix, const char *format, ...);
```

**Parameters:**
- `level`: Log level
- `prefix`: Module prefix (e.g., "RALE", "DSTORE")
- `format`: Printf-style format string
- `...`: Format arguments

**Example:**
```c
elog(LOG_INFO, "RALE", "Node %d elected as leader", node_id);
elog(LOG_ERROR, "DSTORE", "Failed to open database: %s", strerror(errno));
```

### ereport
Reports an error with detail and hint.

```c
void ereport(librale_log_level level, const char *prefix, 
            const char *message, const char *detail, const char *hint);
```

**Parameters:**
- `level`: Log level
- `prefix`: Module prefix
- `message`: Main error message
- `detail`: Detailed error information
- `hint`: Suggestion for resolution

**Example:**
```c
ereport(LOG_ERROR, "RALE", 
       "Failed to connect to peer node",
       "Connection timeout after 30 seconds",
       "Check network connectivity and firewall settings");
```

## Status and Health

### librale_status_get
Gets current system status.

```c
int librale_status_get(char *status_json, size_t buffer_len);
```

**Parameters:**
- `status_json`: Buffer to store JSON status
- `buffer_len`: Buffer size

**Returns:**
- `LIBRALE_SUCCESS` on success
- `LIBRALE_ERROR` on failure

**Example:**
```c
char status[4096];
int result = librale_status_get(status, sizeof(status));
if (result == LIBRALE_SUCCESS) {
    printf("System Status: %s\n", status);
}
```

### librale_health_check
Performs a health check.

```c
int librale_health_check(void);
```

**Returns:**
- `LIBRALE_SUCCESS` if healthy
- `LIBRALE_ERROR` if unhealthy

## Memory Management

### librale_cleanup
Cleans up all librale resources.

```c
void librale_cleanup(void);
```

**Note:** Call this before program exit to ensure clean shutdown.

**Example:**
```c
/* At program exit */
librale_cleanup();
```

## Error Handling Best Practices

### Return Value Checking
Always check return values from librale functions:

```c
int result = librale_cluster_init();
if (result != LIBRALE_SUCCESS) {
    elog(LOG_ERROR, "APP", "Failed to initialize cluster");
    return -1;
}
```

### Resource Management
Use proper resource management patterns:

```c
rale_context_t *ctx = librale_context_create();
if (ctx == NULL) {
    return -1;
}

/* Use context... */

librale_context_destroy(ctx);
ctx = NULL;
```

### Thread Safety
All public APIs are thread-safe, but contexts should not be shared:

```c
/* Create separate contexts for different threads */
rale_context_t *ctx1 = librale_context_create();  /* Thread 1 */
rale_context_t *ctx2 = librale_context_create();  /* Thread 2 */
```

## Integration Examples

### Simple Key-Value Store
```c
#include <librale.h>

int main() {
    /* Initialize */
    if (librale_cluster_init() != LIBRALE_SUCCESS) {
        return -1;
    }
    
    if (librale_rale_init(1, "/var/lib/rale") != LIBRALE_SUCCESS) {
        return -1;
    }
    
    if (librale_dstore_init("/var/lib/rale/data") != LIBRALE_SUCCESS) {
        return -1;
    }
    
    /* Start services */
    librale_rale_start();
    librale_network_start(7400, 7401);
    
    /* Store data */
    librale_dstore_put("key1", "value1", 6);
    
    /* Retrieve data */
    char buffer[1024];
    size_t len = sizeof(buffer);
    if (librale_dstore_get("key1", buffer, &len) == LIBRALE_SUCCESS) {
        printf("Retrieved: %.*s\n", (int)len, buffer);
    }
    
    /* Cleanup */
    librale_cleanup();
    return 0;
}
```

### Leader Election Example
```c
#include <librale.h>

void monitor_leadership() {
    int current_leader = -1;
    
    while (1) {
        int leader_id;
        if (librale_rale_get_leader(&leader_id) == LIBRALE_SUCCESS) {
            if (leader_id != current_leader) {
                printf("Leadership changed: Node %d is now leader\n", leader_id);
                current_leader = leader_id;
            }
        }
        sleep(1);
    }
}
```

This API reference provides comprehensive coverage of all public librale APIs with examples and best practices for integration into distributed applications.