# RALE Examples

## Overview

This document provides practical examples for using the RALE distributed consensus and key-value store system. Examples range from simple single-node setups to complex multi-node distributed applications.

## Basic Examples

### Hello World - Single Node

**File: `examples/hello_world.c`**
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <librale.h>

int main() {
    printf("RALE Hello World Example\n");
    
    /* Initialize the cluster subsystem */
    if (librale_cluster_init() != LIBRALE_SUCCESS) {
        fprintf(stderr, "Failed to initialize cluster\n");
        return 1;
    }
    
    /* Initialize RALE consensus */
    if (librale_rale_init(1, "./rale_data") != LIBRALE_SUCCESS) {
        fprintf(stderr, "Failed to initialize RALE\n");
        return 1;
    }
    
    /* Initialize distributed store */
    if (librale_dstore_init("./rale_data") != LIBRALE_SUCCESS) {
        fprintf(stderr, "Failed to initialize DStore\n");
        return 1;
    }
    
    /* Start services */
    librale_rale_start();
    librale_network_start(7400, 7401);
    
    printf("RALE services started\n");
    
    /* Store some data */
    const char *key = "greeting";
    const char *value = "Hello, RALE World!";
    
    if (librale_dstore_put(key, value, strlen(value)) == LIBRALE_SUCCESS) {
        printf("Stored: %s = %s\n", key, value);
    }
    
    /* Retrieve the data */
    char buffer[256];
    size_t len = sizeof(buffer);
    
    if (librale_dstore_get(key, buffer, &len) == LIBRALE_SUCCESS) {
        buffer[len] = '\0';
        printf("Retrieved: %s = %s\n", key, buffer);
    }
    
    /* Run for a few seconds */
    printf("Running for 5 seconds...\n");
    sleep(5);
    
    /* Cleanup */
    librale_cleanup();
    printf("RALE services stopped\n");
    
    return 0;
}
```

**Build and Run:**
```bash
# Compile
gcc -o hello_world hello_world.c -lrale

# Run
./hello_world
```

### Simple Key-Value Store

**File: `examples/kv_store.c`**
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <librale.h>

void print_help() {
    printf("Commands:\n");
    printf("  put <key> <value>  - Store a key-value pair\n");
    printf("  get <key>          - Retrieve value by key\n");
    printf("  del <key>          - Delete a key\n");
    printf("  list [prefix]      - List keys with optional prefix\n");
    printf("  status             - Show cluster status\n");
    printf("  help               - Show this help\n");
    printf("  quit               - Exit\n");
}

int main() {
    char *input;
    char *cmd, *key, *value;
    char buffer[1024];
    size_t len;
    
    /* Initialize RALE */
    if (librale_cluster_init() != LIBRALE_SUCCESS ||
        librale_rale_init(1, "./kv_data") != LIBRALE_SUCCESS ||
        librale_dstore_init("./kv_data") != LIBRALE_SUCCESS) {
        fprintf(stderr, "Failed to initialize RALE\n");
        return 1;
    }
    
    /* Start services */
    librale_rale_start();
    librale_network_start(7400, 7401);
    
    printf("RALE Key-Value Store\n");
    printf("Type 'help' for commands\n\n");
    
    while ((input = readline("rale> ")) != NULL) {
        if (strlen(input) > 0) {
            add_history(input);
        }
        
        /* Parse command */
        cmd = strtok(input, " ");
        if (cmd == NULL) {
            free(input);
            continue;
        }
        
        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            free(input);
            break;
        } else if (strcmp(cmd, "help") == 0) {
            print_help();
        } else if (strcmp(cmd, "put") == 0) {
            key = strtok(NULL, " ");
            value = strtok(NULL, "");
            
            if (key && value) {
                if (librale_dstore_put(key, value, strlen(value)) == LIBRALE_SUCCESS) {
                    printf("OK\n");
                } else {
                    printf("ERROR: Failed to store key\n");
                }
            } else {
                printf("Usage: put <key> <value>\n");
            }
        } else if (strcmp(cmd, "get") == 0) {
            key = strtok(NULL, " ");
            
            if (key) {
                len = sizeof(buffer);
                if (librale_dstore_get(key, buffer, &len) == LIBRALE_SUCCESS) {
                    buffer[len] = '\0';
                    printf("%s\n", buffer);
                } else {
                    printf("ERROR: Key not found\n");
                }
            } else {
                printf("Usage: get <key>\n");
            }
        } else if (strcmp(cmd, "del") == 0) {
            key = strtok(NULL, " ");
            
            if (key) {
                if (librale_dstore_delete(key) == LIBRALE_SUCCESS) {
                    printf("OK\n");
                } else {
                    printf("ERROR: Failed to delete key\n");
                }
            } else {
                printf("Usage: del <key>\n");
            }
        } else if (strcmp(cmd, "status") == 0) {
            if (librale_rale_is_leader()) {
                printf("Status: Leader\n");
            } else {
                int leader_id;
                if (librale_rale_get_leader(&leader_id) == LIBRALE_SUCCESS) {
                    printf("Status: Follower (Leader: Node %d)\n", leader_id);
                } else {
                    printf("Status: Unknown\n");
                }
            }
        } else {
            printf("Unknown command: %s\n", cmd);
            printf("Type 'help' for available commands\n");
        }
        
        free(input);
    }
    
    printf("\nShutting down...\n");
    librale_cleanup();
    return 0;
}
```

**Build with readline support:**
```bash
gcc -o kv_store kv_store.c -lrale -lreadline
```

## Cluster Examples

### Three Node Cluster Setup

**File: `examples/cluster_node.c`**
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <librale.h>

static volatile int running = 1;

void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;
}

int main(int argc, char *argv[]) {
    int node_id;
    int rale_port, dstore_port;
    char data_dir[256];
    
    if (argc != 2) {
        printf("Usage: %s <node_id>\n", argv[0]);
        printf("Example: %s 1\n", argv[0]);
        return 1;
    }
    
    node_id = atoi(argv[1]);
    rale_port = 7400 + node_id - 1;
    dstore_port = 7500 + node_id - 1;
    snprintf(data_dir, sizeof(data_dir), "./node%d_data", node_id);
    
    printf("Starting RALE Node %d\n", node_id);
    printf("RALE Port: %d, DStore Port: %d\n", rale_port, dstore_port);
    printf("Data Directory: %s\n", data_dir);
    
    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize RALE */
    if (librale_cluster_init() != LIBRALE_SUCCESS) {
        fprintf(stderr, "Failed to initialize cluster\n");
        return 1;
    }
    
    /* Add cluster nodes */
    librale_cluster_add_node(1, "node1", "127.0.0.1", 7400, 7500);
    librale_cluster_add_node(2, "node2", "127.0.0.1", 7401, 7501);
    librale_cluster_add_node(3, "node3", "127.0.0.1", 7402, 7502);
    
    /* Initialize this node */
    if (librale_rale_init(node_id, data_dir) != LIBRALE_SUCCESS) {
        fprintf(stderr, "Failed to initialize RALE\n");
        return 1;
    }
    
    if (librale_dstore_init(data_dir) != LIBRALE_SUCCESS) {
        fprintf(stderr, "Failed to initialize DStore\n");
        return 1;
    }
    
    /* Start services */
    librale_rale_start();
    librale_network_start(rale_port, dstore_port);
    
    printf("Node %d started successfully\n", node_id);
    
    /* Main loop */
    while (running) {
        /* Check leadership status */
        static int last_leader = -1;
        int current_leader;
        
        if (librale_rale_get_leader(&current_leader) == LIBRALE_SUCCESS) {
            if (current_leader != last_leader) {
                if (current_leader == node_id) {
                    printf("Node %d: I am now the leader!\n", node_id);
                } else {
                    printf("Node %d: Node %d is the leader\n", node_id, current_leader);
                }
                last_leader = current_leader;
            }
        }
        
        sleep(5);
    }
    
    printf("Node %d shutting down...\n", node_id);
    librale_cleanup();
    return 0;
}
```

**Start a three-node cluster:**
```bash
# Terminal 1
./cluster_node 1

# Terminal 2
./cluster_node 2

# Terminal 3
./cluster_node 3
```

### Leader Election Monitor

**File: `examples/election_monitor.c`**
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <librale.h>

void print_timestamp() {
    time_t now = time(NULL);
    char *timestr = ctime(&now);
    timestr[strlen(timestr) - 1] = '\0';  /* Remove newline */
    printf("[%s] ", timestr);
}

int main() {
    int current_leader = -1;
    int election_count = 0;
    
    printf("RALE Leader Election Monitor\n");
    printf("Monitoring leadership changes...\n\n");
    
    /* Initialize cluster for monitoring */
    if (librale_cluster_init() != LIBRALE_SUCCESS) {
        fprintf(stderr, "Failed to initialize cluster\n");
        return 1;
    }
    
    /* Connect to cluster */
    librale_cluster_add_node(1, "node1", "127.0.0.1", 7400, 7500);
    librale_cluster_add_node(2, "node2", "127.0.0.1", 7401, 7501);
    librale_cluster_add_node(3, "node3", "127.0.0.1", 7402, 7502);
    
    while (1) {
        int leader_id;
        
        if (librale_rale_get_leader(&leader_id) == LIBRALE_SUCCESS) {
            if (leader_id != current_leader) {
                election_count++;
                print_timestamp();
                
                if (current_leader == -1) {
                    printf("Initial leader: Node %d\n", leader_id);
                } else {
                    printf("Leadership change #%d: Node %d -> Node %d\n", 
                           election_count, current_leader, leader_id);
                }
                
                current_leader = leader_id;
            }
        } else {
            if (current_leader != -1) {
                print_timestamp();
                printf("No leader available\n");
                current_leader = -1;
            }
        }
        
        sleep(1);
    }
    
    return 0;
}
```

## Advanced Examples

### Distributed Counter

**File: `examples/distributed_counter.c`**
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <librale.h>

typedef struct {
    const char *counter_name;
    int increment_count;
    int thread_id;
} thread_data_t;

void *increment_worker(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    char key[256];
    char value_str[32];
    char buffer[32];
    size_t len;
    int current_value;
    
    snprintf(key, sizeof(key), "counter:%s", data->counter_name);
    
    for (int i = 0; i < data->increment_count; i++) {
        /* Read current value */
        len = sizeof(buffer);
        if (librale_dstore_get(key, buffer, &len) == LIBRALE_SUCCESS) {
            buffer[len] = '\0';
            current_value = atoi(buffer);
        } else {
            current_value = 0;  /* Counter doesn't exist yet */
        }
        
        /* Increment */
        current_value++;
        
        /* Write back */
        snprintf(value_str, sizeof(value_str), "%d", current_value);
        if (librale_dstore_put(key, value_str, strlen(value_str)) == LIBRALE_SUCCESS) {
            printf("Thread %d: Counter '%s' = %d\n", 
                   data->thread_id, data->counter_name, current_value);
        } else {
            printf("Thread %d: Failed to update counter\n", data->thread_id);
        }
        
        /* Small delay to see interleaving */
        usleep(100000);  /* 100ms */
    }
    
    return NULL;
}

int main() {
    const int NUM_THREADS = 3;
    const int INCREMENTS_PER_THREAD = 5;
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    printf("Distributed Counter Example\n");
    printf("Running %d threads, %d increments each\n\n", 
           NUM_THREADS, INCREMENTS_PER_THREAD);
    
    /* Initialize RALE */
    if (librale_cluster_init() != LIBRALE_SUCCESS ||
        librale_rale_init(1, "./counter_data") != LIBRALE_SUCCESS ||
        librale_dstore_init("./counter_data") != LIBRALE_SUCCESS) {
        fprintf(stderr, "Failed to initialize RALE\n");
        return 1;
    }
    
    librale_rale_start();
    librale_network_start(7400, 7401);
    
    /* Create worker threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].counter_name = "test_counter";
        thread_data[i].increment_count = INCREMENTS_PER_THREAD;
        thread_data[i].thread_id = i + 1;
        
        if (pthread_create(&threads[i], NULL, increment_worker, &thread_data[i]) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            return 1;
        }
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Read final counter value */
    char buffer[32];
    size_t len = sizeof(buffer);
    if (librale_dstore_get("counter:test_counter", buffer, &len) == LIBRALE_SUCCESS) {
        buffer[len] = '\0';
        printf("\nFinal counter value: %s\n", buffer);
        printf("Expected value: %d\n", NUM_THREADS * INCREMENTS_PER_THREAD);
    }
    
    librale_cleanup();
    return 0;
}
```

### Configuration Management System

**File: `examples/config_manager.c`**
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <librale.h>

typedef struct {
    char app_name[64];
    char environment[32];
    char version[16];
} app_config_t;

int store_config(const char *app_name, const app_config_t *config) {
    char key[256];
    json_object *json_obj, *json_app, *json_env, *json_ver;
    const char *json_string;
    
    /* Create JSON object */
    json_obj = json_object_new_object();
    json_app = json_object_new_string(config->app_name);
    json_env = json_object_new_string(config->environment);
    json_ver = json_object_new_string(config->version);
    
    json_object_object_add(json_obj, "app_name", json_app);
    json_object_object_add(json_obj, "environment", json_env);
    json_object_object_add(json_obj, "version", json_ver);
    
    /* Store in distributed store */
    json_string = json_object_to_json_string(json_obj);
    snprintf(key, sizeof(key), "config:app:%s", app_name);
    
    int result = librale_dstore_put(key, json_string, strlen(json_string));
    
    json_object_put(json_obj);
    return result;
}

int load_config(const char *app_name, app_config_t *config) {
    char key[256];
    char buffer[1024];
    size_t len = sizeof(buffer);
    json_object *json_obj, *json_field;
    const char *str_value;
    
    snprintf(key, sizeof(key), "config:app:%s", app_name);
    
    if (librale_dstore_get(key, buffer, &len) != LIBRALE_SUCCESS) {
        return -1;
    }
    
    buffer[len] = '\0';
    json_obj = json_tokener_parse(buffer);
    if (json_obj == NULL) {
        return -1;
    }
    
    /* Extract fields */
    if (json_object_object_get_ex(json_obj, "app_name", &json_field)) {
        str_value = json_object_get_string(json_field);
        strncpy(config->app_name, str_value, sizeof(config->app_name) - 1);
    }
    
    if (json_object_object_get_ex(json_obj, "environment", &json_field)) {
        str_value = json_object_get_string(json_field);
        strncpy(config->environment, str_value, sizeof(config->environment) - 1);
    }
    
    if (json_object_object_get_ex(json_obj, "version", &json_field)) {
        str_value = json_object_get_string(json_field);
        strncpy(config->version, str_value, sizeof(config->version) - 1);
    }
    
    json_object_put(json_obj);
    return 0;
}

int main() {
    app_config_t config;
    
    printf("Distributed Configuration Manager\n\n");
    
    /* Initialize RALE */
    if (librale_cluster_init() != LIBRALE_SUCCESS ||
        librale_rale_init(1, "./config_data") != LIBRALE_SUCCESS ||
        librale_dstore_init("./config_data") != LIBRALE_SUCCESS) {
        fprintf(stderr, "Failed to initialize RALE\n");
        return 1;
    }
    
    librale_rale_start();
    librale_network_start(7400, 7401);
    
    /* Store some configurations */
    strcpy(config.app_name, "web_server");
    strcpy(config.environment, "production");
    strcpy(config.version, "1.2.3");
    
    if (store_config("web_server", &config) == LIBRALE_SUCCESS) {
        printf("Stored configuration for web_server\n");
    }
    
    strcpy(config.app_name, "database");
    strcpy(config.environment, "staging");
    strcpy(config.version, "2.1.0");
    
    if (store_config("database", &config) == LIBRALE_SUCCESS) {
        printf("Stored configuration for database\n");
    }
    
    /* Load and display configurations */
    printf("\nStored configurations:\n");
    
    if (load_config("web_server", &config) == 0) {
        printf("web_server: %s v%s (%s)\n", 
               config.app_name, config.version, config.environment);
    }
    
    if (load_config("database", &config) == 0) {
        printf("database: %s v%s (%s)\n", 
               config.app_name, config.version, config.environment);
    }
    
    librale_cleanup();
    return 0;
}
```

## Testing Examples

### Failover Testing

**File: `examples/failover_test.c`**
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <librale.h>

void start_node(int node_id) {
    char data_dir[256];
    int rale_port = 7400 + node_id - 1;
    int dstore_port = 7500 + node_id - 1;
    
    snprintf(data_dir, sizeof(data_dir), "./test_node%d", node_id);
    
    printf("Child: Starting node %d (PID %d)\n", node_id, getpid());
    
    librale_cluster_init();
    
    /* Add all cluster nodes */
    for (int i = 1; i <= 3; i++) {
        librale_cluster_add_node(i, 
                               (i == 1) ? "node1" : (i == 2) ? "node2" : "node3",
                               "127.0.0.1", 
                               7400 + i - 1, 
                               7500 + i - 1);
    }
    
    librale_rale_init(node_id, data_dir);
    librale_dstore_init(data_dir);
    librale_rale_start();
    librale_network_start(rale_port, dstore_port);
    
    /* Keep running until killed */
    while (1) {
        sleep(1);
    }
}

int main() {
    pid_t node_pids[3];
    int status;
    
    printf("RALE Failover Test\n");
    printf("Starting 3-node cluster...\n\n");
    
    /* Start three nodes */
    for (int i = 1; i <= 3; i++) {
        node_pids[i-1] = fork();
        
        if (node_pids[i-1] == 0) {
            /* Child process - run node */
            start_node(i);
            exit(0);
        } else if (node_pids[i-1] < 0) {
            fprintf(stderr, "Failed to fork node %d\n", i);
            return 1;
        }
    }
    
    printf("Parent: All nodes started\n");
    printf("Waiting 10 seconds for cluster formation...\n");
    sleep(10);
    
    printf("Killing node 1 to test failover...\n");
    kill(node_pids[0], SIGTERM);
    waitpid(node_pids[0], &status, 0);
    printf("Node 1 terminated\n");
    
    printf("Waiting 15 seconds for failover...\n");
    sleep(15);
    
    printf("Restarting node 1...\n");
    node_pids[0] = fork();
    if (node_pids[0] == 0) {
        start_node(1);
        exit(0);
    }
    
    printf("Waiting 10 more seconds...\n");
    sleep(10);
    
    printf("Terminating all nodes...\n");
    for (int i = 0; i < 3; i++) {
        if (node_pids[i] > 0) {
            kill(node_pids[i], SIGTERM);
            waitpid(node_pids[i], &status, 0);
        }
    }
    
    printf("Failover test completed\n");
    return 0;
}
```

## Performance Examples

### Benchmark Tool

**File: `examples/benchmark.c`**
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <librale.h>

double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void benchmark_writes(int count) {
    char key[64], value[64];
    double start, end;
    int successful = 0;
    
    printf("Benchmarking %d write operations...\n", count);
    start = get_time();
    
    for (int i = 0; i < count; i++) {
        snprintf(key, sizeof(key), "bench_key_%d", i);
        snprintf(value, sizeof(value), "benchmark_value_%d", i);
        
        if (librale_dstore_put(key, value, strlen(value)) == LIBRALE_SUCCESS) {
            successful++;
        }
    }
    
    end = get_time();
    
    printf("Write Results:\n");
    printf("  Operations: %d\n", count);
    printf("  Successful: %d\n", successful);
    printf("  Failed: %d\n", count - successful);
    printf("  Time: %.3f seconds\n", end - start);
    printf("  Throughput: %.1f ops/sec\n", successful / (end - start));
    printf("  Average latency: %.3f ms\n", (end - start) * 1000 / count);
}

void benchmark_reads(int count) {
    char key[64], buffer[64];
    size_t len;
    double start, end;
    int successful = 0;
    
    printf("\nBenchmarking %d read operations...\n", count);
    start = get_time();
    
    for (int i = 0; i < count; i++) {
        snprintf(key, sizeof(key), "bench_key_%d", i % 1000);  /* Read existing keys */
        len = sizeof(buffer);
        
        if (librale_dstore_get(key, buffer, &len) == LIBRALE_SUCCESS) {
            successful++;
        }
    }
    
    end = get_time();
    
    printf("Read Results:\n");
    printf("  Operations: %d\n", count);
    printf("  Successful: %d\n", successful);
    printf("  Failed: %d\n", count - successful);
    printf("  Time: %.3f seconds\n", end - start);
    printf("  Throughput: %.1f ops/sec\n", successful / (end - start));
    printf("  Average latency: %.3f ms\n", (end - start) * 1000 / count);
}

int main(int argc, char *argv[]) {
    int write_count = 1000;
    int read_count = 5000;
    
    if (argc >= 2) write_count = atoi(argv[1]);
    if (argc >= 3) read_count = atoi(argv[2]);
    
    printf("RALE Benchmark Tool\n");
    printf("Write operations: %d\n", write_count);
    printf("Read operations: %d\n\n", read_count);
    
    /* Initialize RALE */
    if (librale_cluster_init() != LIBRALE_SUCCESS ||
        librale_rale_init(1, "./bench_data") != LIBRALE_SUCCESS ||
        librale_dstore_init("./bench_data") != LIBRALE_SUCCESS) {
        fprintf(stderr, "Failed to initialize RALE\n");
        return 1;
    }
    
    librale_rale_start();
    librale_network_start(7400, 7401);
    
    /* Wait for initialization */
    sleep(2);
    
    /* Run benchmarks */
    benchmark_writes(write_count);
    benchmark_reads(read_count);
    
    printf("\nBenchmark completed\n");
    librale_cleanup();
    return 0;
}
```

## Building Examples

### Simple Makefile

**File: `examples/Makefile`**
```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE
LIBS = -lrale -lpthread
JSON_LIBS = -ljson-c
READLINE_LIBS = -lreadline

# Default target
all: hello_world kv_store cluster_node election_monitor \
     distributed_counter benchmark failover_test

# Basic examples
hello_world: hello_world.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

cluster_node: cluster_node.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

election_monitor: election_monitor.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

benchmark: benchmark.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

failover_test: failover_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

# Examples with additional dependencies
kv_store: kv_store.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS) $(READLINE_LIBS)

distributed_counter: distributed_counter.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

config_manager: config_manager.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS) $(JSON_LIBS)

# Clean up
clean:
	rm -f hello_world kv_store cluster_node election_monitor \
	      distributed_counter config_manager benchmark failover_test
	rm -rf *_data *.log

# Test targets
test-single: hello_world
	./hello_world

test-cluster: cluster_node
	@echo "Start each node in a separate terminal:"
	@echo "./cluster_node 1"
	@echo "./cluster_node 2"  
	@echo "./cluster_node 3"

.PHONY: all clean test-single test-cluster
```

### CMake Configuration

**File: `examples/CMakeLists.txt`**
```cmake
cmake_minimum_required(VERSION 3.16)
project(rale_examples)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBRALE REQUIRED librale)

# Find optional packages
find_package(PkgConfig)
pkg_check_modules(JSON_C json-c)

# Set compiler flags
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -D_GNU_SOURCE")

# Include directories
include_directories(${LIBRALE_INCLUDE_DIRS})

# Basic examples
add_executable(hello_world hello_world.c)
target_link_libraries(hello_world ${LIBRALE_LIBRARIES} pthread)

add_executable(cluster_node cluster_node.c)
target_link_libraries(cluster_node ${LIBRALE_LIBRARIES} pthread)

add_executable(benchmark benchmark.c)
target_link_libraries(benchmark ${LIBRALE_LIBRARIES} pthread)

# Examples with readline
find_path(READLINE_INCLUDE_DIR readline/readline.h)
find_library(READLINE_LIBRARY readline)

if(READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
    add_executable(kv_store kv_store.c)
    target_include_directories(kv_store PRIVATE ${READLINE_INCLUDE_DIR})
    target_link_libraries(kv_store ${LIBRALE_LIBRARIES} ${READLINE_LIBRARY} pthread)
endif()

# Examples with JSON-C
if(JSON_C_FOUND)
    add_executable(config_manager config_manager.c)
    target_include_directories(config_manager PRIVATE ${JSON_C_INCLUDE_DIRS})
    target_link_libraries(config_manager ${LIBRALE_LIBRARIES} ${JSON_C_LIBRARIES} pthread)
endif()
```

**Build examples:**
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Running Examples

### Quick Start
```bash
# Single node example
./hello_world

# Interactive key-value store
./kv_store

# Three-node cluster (run in separate terminals)
./cluster_node 1
./cluster_node 2
./cluster_node 3

# Monitor leadership changes
./election_monitor

# Performance testing
./benchmark 5000 10000
```

### Production Testing
```bash
# Failover testing
./failover_test

# Long-running stability test
timeout 3600 ./cluster_node 1  # Run for 1 hour

# Performance under load
./benchmark 50000 100000
```

These examples demonstrate the flexibility and power of the RALE system for building distributed applications with strong consistency guarantees and automatic failover capabilities.