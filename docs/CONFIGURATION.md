# Configuration Guide

## Overview

This guide covers configuration of the RALE distributed consensus and key-value store system. RALE uses file-based configuration with support for runtime updates and environment variable overrides.

## Configuration Files

### Primary Configuration

**File Locations** (in order of precedence):
1. Command-line specified: `--config /path/to/config`
2. User configuration: `~/.config/rale/raled.conf`
3. System configuration: `/etc/rale/raled.conf`
4. Local configuration: `./raled.conf`

### Configuration Format

RALE uses a simple key-value configuration format:

```conf
# Comments start with #
key = value

# Multi-line values use continuation
long_value = line1 \
           line2 \
           line3

# Environment variables are expanded
data_dir = ${RALE_DATA_DIR:/var/lib/rale}
```

## Node Configuration

### Basic Node Settings

```conf
# Node identification
node_id = 1
node_name = rale-node-01

# Network configuration
bind_ip = 0.0.0.0
rale_port = 7400
dstore_port = 7401

# External addresses (for multi-homed systems)
external_ip = 10.0.1.100
advertise_rale_port = 7400
advertise_dstore_port = 7401
```

**Parameters:**
- `node_id`: Unique integer identifier (0-9999)
- `node_name`: Human-readable node name
- `bind_ip`: Local IP address to bind services
- `rale_port`: Port for RALE consensus protocol
- `dstore_port`: Port for distributed store operations
- `external_ip`: Public IP for cluster communication
- `advertise_*`: Ports advertised to other nodes

### Data Storage Settings

```conf
# Data directories
data_dir = /var/lib/rale/data
log_dir = /var/lib/rale/logs
state_dir = /var/lib/rale/state

# Database settings
db_sync_mode = full
db_cache_size = 64MB
db_checkpoint_interval = 300

# Log rotation
log_max_size = 100MB
log_max_files = 10
log_compression = true
```

**Parameters:**
- `data_dir`: Directory for DStore data files
- `log_dir`: Directory for operation logs
- `state_dir`: Directory for RALE state persistence
- `db_sync_mode`: Database synchronization (`full`, `normal`, `off`)
- `db_cache_size`: Database cache size
- `db_checkpoint_interval`: Checkpoint interval in seconds

## Cluster Configuration

### Cluster Membership

```conf
# Cluster settings
cluster_name = production-cluster
cluster_size = 3

# Initial cluster members
cluster_members = node1:10.0.1.100:7400,node2:10.0.1.101:7400,node3:10.0.1.102:7400

# Join existing cluster
join_cluster = true
bootstrap_nodes = 10.0.1.100:7400,10.0.1.101:7400
```

**Parameters:**
- `cluster_name`: Unique cluster identifier
- `cluster_size`: Expected cluster size for quorum calculations
- `cluster_members`: Static member list (for initial bootstrap)
- `join_cluster`: Whether to join existing cluster
- `bootstrap_nodes`: Nodes to contact when joining

### Consensus Settings

```conf
# RALE consensus parameters
election_timeout_min = 150
election_timeout_max = 300
heartbeat_interval = 50
max_log_entries_per_request = 100

# Leadership settings
leadership_priority = 1
auto_stepdown = true
stepdown_on_partition = true
```

**Parameters:**
- `election_timeout_min/max`: Election timeout range (milliseconds)
- `heartbeat_interval`: Leader heartbeat interval (milliseconds)
- `max_log_entries_per_request`: Batch size for log replication
- `leadership_priority`: Node priority for leader election (1-10)
- `auto_stepdown`: Automatically step down when isolated

## Network Configuration

### TCP/UDP Settings

```conf
# Network tuning
tcp_keepalive = true
tcp_keepalive_idle = 60
tcp_keepalive_interval = 10
tcp_keepalive_probes = 3

# Connection management
max_connections = 100
connection_timeout = 30
reconnect_interval = 5
max_reconnect_attempts = 10

# Buffer sizes
send_buffer_size = 64KB
recv_buffer_size = 64KB
max_message_size = 1MB
```

**Parameters:**
- `tcp_keepalive`: Enable TCP keepalive
- `tcp_keepalive_*`: Keepalive timing parameters
- `max_connections`: Maximum concurrent connections
- `connection_timeout`: Connection timeout in seconds
- `reconnect_interval`: Reconnection retry interval

### Security Settings

```conf
# Network security
enable_tls = false
tls_cert_file = /etc/rale/certs/server.crt
tls_key_file = /etc/rale/certs/server.key
tls_ca_file = /etc/rale/certs/ca.crt

# Authentication
require_auth = false
auth_method = none
shared_secret = ""
```

**Parameters:**
- `enable_tls`: Enable TLS encryption
- `tls_cert_file`: Server certificate file
- `tls_key_file`: Server private key file
- `tls_ca_file`: Certificate authority file
- `require_auth`: Require client authentication

## Logging Configuration

### Log Levels and Output

```conf
# Logging configuration
log_level = INFO
log_format = structured
log_output = file

# File logging
log_file = /var/log/rale/raled.log
log_file_rotation = daily
log_file_max_size = 100MB
log_file_max_age = 30d

# Syslog integration
syslog_enabled = false
syslog_facility = daemon
syslog_tag = raled
```

**Log Levels:**
- `ERROR`: Error conditions only
- `WARNING`: Warnings and errors
- `INFO`: Informational messages and above
- `DEBUG`: All messages including debug

**Log Formats:**
- `structured`: JSON-formatted logs
- `plain`: Human-readable plain text
- `compact`: Compact single-line format

### Module-Specific Logging

```conf
# Per-module log levels
log_level_rale = INFO
log_level_dstore = WARNING
log_level_network = ERROR
log_level_cluster = DEBUG

# Log filtering
log_filter_exclude = heartbeat,ping
log_filter_include = election,leadership
```

## Performance Tuning

### Memory Settings

```conf
# Memory configuration
max_memory_usage = 1GB
cache_size = 256MB
buffer_pool_size = 128MB

# Garbage collection
gc_interval = 300
gc_threshold = 0.8
auto_compact = true
compact_interval = 3600
```

**Parameters:**
- `max_memory_usage`: Maximum memory limit
- `cache_size`: General cache size
- `buffer_pool_size`: Network buffer pool size
- `gc_interval`: Cleanup interval in seconds

### I/O Settings

```conf
# I/O configuration
io_threads = 4
disk_sync_mode = normal
write_batch_size = 1000
read_ahead_size = 64KB

# Performance monitoring
enable_metrics = true
metrics_interval = 60
metrics_retention = 24h
```

**Parameters:**
- `io_threads`: Number of I/O worker threads
- `disk_sync_mode`: Disk synchronization mode
- `write_batch_size`: Write batching size
- `enable_metrics`: Enable performance metrics collection

## Environment Variables

### System Environment

```bash
# Data locations
export RALE_DATA_DIR=/var/lib/rale
export RALE_CONFIG_DIR=/etc/rale
export RALE_LOG_DIR=/var/log/rale

# Network settings
export RALE_BIND_IP=0.0.0.0
export RALE_PORT=7400
export RALE_DSTORE_PORT=7401

# Runtime options
export RALE_LOG_LEVEL=INFO
export RALE_DEBUG=false
```

### Configuration Overrides

Environment variables override configuration file values:

```bash
# Override any configuration parameter
export RALE_NODE_ID=1
export RALE_CLUSTER_NAME=my-cluster
export RALE_ELECTION_TIMEOUT_MIN=200

# Boolean values
export RALE_ENABLE_TLS=true
export RALE_AUTO_STEPDOWN=false

# List values (comma-separated)
export RALE_BOOTSTRAP_NODES=10.0.1.100:7400,10.0.1.101:7400
```

## Sample Configurations

### Single Node (Development)

**File: `conf/single-node.conf`**
```conf
# Single node development configuration
node_id = 1
node_name = dev-node
bind_ip = 127.0.0.1
rale_port = 7400
dstore_port = 7401

# Local data storage
data_dir = ./data
log_dir = ./logs
state_dir = ./state

# Development settings
log_level = DEBUG
log_output = console
cluster_size = 1
bootstrap_cluster = true

# Fast timeouts for development
election_timeout_min = 50
election_timeout_max = 100
heartbeat_interval = 20
```

### Three Node Cluster

**File: `conf/cluster-node1.conf`**
```conf
# Node 1 configuration
node_id = 1
node_name = cluster-node-01
bind_ip = 0.0.0.0
external_ip = 10.0.1.100
rale_port = 7400
dstore_port = 7401

# Cluster membership
cluster_name = production
cluster_size = 3
cluster_members = node1:10.0.1.100:7400,node2:10.0.1.101:7400,node3:10.0.1.102:7400

# Production data storage
data_dir = /var/lib/rale/data
log_dir = /var/log/rale
state_dir = /var/lib/rale/state

# Production logging
log_level = INFO
log_file = /var/log/rale/raled.log
log_file_rotation = daily

# Leadership priority (highest)
leadership_priority = 3
```

**File: `conf/cluster-node2.conf`**
```conf
# Node 2 configuration
node_id = 2
node_name = cluster-node-02
bind_ip = 0.0.0.0
external_ip = 10.0.1.101
rale_port = 7400
dstore_port = 7401

# Join existing cluster
cluster_name = production
join_cluster = true
bootstrap_nodes = 10.0.1.100:7400

# Production settings
data_dir = /var/lib/rale/data
log_dir = /var/log/rale
state_dir = /var/lib/rale/state
log_level = INFO
log_file = /var/log/rale/raled.log

# Medium leadership priority
leadership_priority = 2
```

### High Performance Configuration

**File: `conf/high-performance.conf`**
```conf
# High performance cluster node
node_id = 1
node_name = perf-node-01

# Network optimization
tcp_keepalive = true
max_connections = 1000
send_buffer_size = 256KB
recv_buffer_size = 256KB
max_message_size = 4MB

# Memory optimization
max_memory_usage = 4GB
cache_size = 1GB
buffer_pool_size = 512MB

# I/O optimization
io_threads = 8
disk_sync_mode = normal
write_batch_size = 5000
read_ahead_size = 256KB

# Aggressive timeouts
election_timeout_min = 100
election_timeout_max = 200
heartbeat_interval = 25
max_log_entries_per_request = 500

# Performance monitoring
enable_metrics = true
metrics_interval = 30
log_level = WARNING
```

## Runtime Configuration

### CLI Configuration Updates

```bash
# View current configuration
ralectrl --socket /tmp/raled.sock GET --key log_level

# Update configuration
ralectrl --socket /tmp/raled.sock SET --key log_level --value DEBUG

# List all configuration
ralectrl --socket /tmp/raled.sock CONFIG
```

### Programmatic Updates

```c
#include <librale.h>

/* Update configuration via API */
int result = librale_config_set("log_level", "DEBUG");
if (result != LIBRALE_SUCCESS) {
    fprintf(stderr, "Failed to update configuration\n");
}

/* Get configuration value */
char value[256];
result = librale_config_get("log_level", value, sizeof(value));
if (result == LIBRALE_SUCCESS) {
    printf("Current log level: %s\n", value);
}
```

## Configuration Validation

### Syntax Validation

```bash
# Validate configuration file
raled --config raled.conf --validate

# Check for syntax errors
raled --config raled.conf --check-config
```

### Parameter Validation

The system validates configuration parameters at startup:

- **Node ID**: Must be unique integer (0-9999)
- **Ports**: Must be valid port numbers (1-65535)
- **IP Addresses**: Must be valid IPv4/IPv6 addresses
- **File Paths**: Must be accessible directories
- **Memory Sizes**: Must be valid size specifications (KB, MB, GB)
- **Time Values**: Must be positive integers with optional units (s, ms)

### Common Validation Errors

```
ERROR: Invalid node_id '999999' - must be 0-9999
ERROR: Invalid port '70000' - must be 1-65535
ERROR: Invalid IP address '300.0.0.1'
ERROR: Directory '/nonexistent' does not exist
ERROR: Invalid memory size 'XYZ' - use format like '64MB'
```

## Security Considerations

### File Permissions

```bash
# Secure configuration files
chmod 640 /etc/rale/*.conf
chown root:rale /etc/rale/*.conf

# Secure data directories
chmod 750 /var/lib/rale
chown rale:rale /var/lib/rale

# Secure log files
chmod 640 /var/log/rale/*.log
chown rale:rale /var/log/rale
```

### Network Security

```conf
# Restrict binding (production)
bind_ip = 10.0.1.100  # Specific interface only

# Enable TLS (recommended for production)
enable_tls = true
tls_cert_file = /etc/rale/certs/server.crt
tls_key_file = /etc/rale/certs/server.key
tls_ca_file = /etc/rale/certs/ca.crt

# Authentication
require_auth = true
shared_secret = "your-secure-shared-secret"
```

### Firewall Configuration

```bash
# Open required ports
sudo ufw allow 7400/tcp  # RALE consensus
sudo ufw allow 7401/tcp  # DStore operations

# Restrict access to specific networks
sudo ufw allow from 10.0.1.0/24 to any port 7400
sudo ufw allow from 10.0.1.0/24 to any port 7401
```

## Troubleshooting Configuration

### Common Issues

**Configuration Not Found:**
```bash
# Check file permissions
ls -la /etc/rale/raled.conf

# Check search paths
strace raled --config test.conf 2>&1 | grep conf
```

**Invalid Parameters:**
```bash
# Run validation
raled --config raled.conf --validate

# Check logs for validation errors
journalctl -u rale | grep ERROR
```

**Network Binding Issues:**
```bash
# Check port availability
netstat -tulpn | grep 7400

# Test binding
nc -l 7400  # Should succeed if port is free
```

### Debug Configuration Loading

```bash
# Enable debug output
export RALE_DEBUG=true
raled --config raled.conf --foreground

# Trace configuration loading
strace -e trace=openat raled --config raled.conf 2>&1 | grep conf
```

For additional configuration help, see the API reference and troubleshooting guides.