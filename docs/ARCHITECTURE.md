# RALE System Architecture

## Overview

RALE is a distributed consensus and key-value store system built on a solid foundation of proven algorithms and modern C engineering practices. The system provides reliable distributed coordination and persistent storage for distributed applications.

## System Components

### Core Library (librale)

**librale** is the foundation component providing distributed consensus and key-value storage capabilities.

**Key Features:**
- **Consensus Protocol**: RALE consensus algorithm for reliable leader election
- **Distributed Store**: High-performance key-value storage with replication
- **Network Layer**: TCP/UDP communication with automatic failover
- **Thread Safety**: Full multi-threading support with proper synchronization
- **Memory Management**: Safe allocation/deallocation with leak prevention
- **Logging**: Clean, production-ready logging without colors or icons

**API Design:**
- **Public API**: Clean, well-documented interface in `librale.h`
- **Thread Safe**: All public functions are thread-safe
- **Error Handling**: Consistent error codes and detailed reporting
- **Configuration**: Flexible runtime configuration system

### Daemon (raled)

**raled** is the daemon process that manages cluster membership and coordination.

**Responsibilities:**
- **Cluster Management**: Node registration, health monitoring, membership changes
- **Leadership**: Leader election and failover coordination
- **State Management**: Persistent cluster state and configuration
- **Communication**: Inter-node communication and client API endpoints
- **Monitoring**: Health checks, metrics collection, and status reporting

**Architecture:**
- **Event-Driven**: Asynchronous processing with event loops
- **Modular Design**: Clean separation of concerns across modules
- **Configuration**: File-based configuration with runtime updates
- **Logging**: Structured logging with configurable levels

### CLI Tool (ralectrl)

**ralectrl** is the command-line interface for cluster management and operations.

**Commands:**
- **Node Management**: ADD, REMOVE, LIST cluster nodes
- **Status Queries**: Cluster status, leader information, node health
- **Configuration**: Runtime configuration updates
- **Diagnostics**: Debug information and troubleshooting tools

**Design:**
- **Unix Philosophy**: Simple, composable commands
- **Scriptable**: JSON output for automation and integration
- **Error Handling**: Clear error messages and exit codes
- **Help System**: Comprehensive help and usage information

## Data Flow Architecture

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Client    │    │   Client    │    │   Client    │
│ Application │    │ Application │    │ Application │
└──────┬──────┘    └──────┬──────┘    └──────┬──────┘
       │                  │                  │
       └──────────────────┼──────────────────┘
                          │
                   ┌─────────────┐
                   │   librale   │
                   │  (Library)  │
                   └──────┬──────┘
                          │
       ┌──────────────────┼──────────────────┐
       │                  │                  │
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│    raled    │    │    raled    │    │    raled    │
│   (Node 1)  │◄──►│   (Node 2)  │◄──►│   (Node 3)  │
└─────────────┘    └─────────────┘    └─────────────┘
       ▲                  ▲                  ▲
       │                  │                  │
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  ralectrl   │    │  ralectrl   │    │  ralectrl   │
│    (CLI)    │    │    (CLI)    │    │    (CLI)    │
└─────────────┘    └─────────────┘    └─────────────┘
```

## Consensus Algorithm (RALE)

The RALE consensus protocol ensures distributed agreement and consistency across cluster nodes.

### Leader Election

**Process:**
1. **Candidate Selection**: Nodes transition to candidate state during leader timeout
2. **Vote Collection**: Candidates request votes from cluster members
3. **Majority Decision**: Node with majority votes becomes leader
4. **Heartbeat Maintenance**: Leader sends regular heartbeats to maintain authority

**Safety Properties:**
- **Election Safety**: At most one leader per term
- **Leader Append-Only**: Leaders never overwrite log entries
- **Log Matching**: Consistent log replication across nodes
- **Leader Completeness**: Leader contains all committed entries

### Log Replication

**Mechanism:**
1. **Client Request**: Application submits operation to leader
2. **Log Append**: Leader appends entry to local log
3. **Replication**: Leader replicates entry to follower nodes
4. **Commit**: Entry committed when majority acknowledges
5. **Application**: State machine applies committed entries

## Distributed Store (DStore)

The DStore provides a distributed key-value storage layer with strong consistency guarantees.

### Storage Architecture

**Components:**
- **Hash Table**: In-memory hash table for fast key lookups
- **Persistence**: File-based storage for durability
- **Replication**: Automatic replication across cluster nodes
- **Compaction**: Background compaction for space efficiency

**Operations:**
- **PUT**: Store key-value pairs with versioning
- **GET**: Retrieve values by key with consistency guarantees
- **DELETE**: Remove keys with tombstone handling
- **LIST**: Enumerate keys with prefix filtering

### Consistency Model

**Strong Consistency**: All reads return the most recent write
- **Read Operations**: Always served from committed state
- **Write Operations**: Require majority consensus before commit
- **Linearizability**: Operations appear atomic and instantaneous

**Durability**: All committed operations survive node failures
- **Write-Ahead Logging**: Operations logged before application
- **Periodic Snapshots**: State checkpoints for fast recovery
- **Replication**: Multiple copies across cluster nodes

## Network Architecture

### Communication Protocols

**RALE Protocol** (Consensus):
- **Transport**: TCP with heartbeat keepalives
- **Message Types**: Vote requests, append entries, heartbeats
- **Flow Control**: Backpressure and congestion management
- **Security**: Optional TLS encryption and authentication

**DStore Protocol** (Storage):
- **Transport**: TCP with connection pooling
- **Message Types**: PUT, GET, DELETE, LIST operations
- **Batching**: Operation batching for efficiency
- **Compression**: Optional message compression

**Management Protocol** (Control):
- **Transport**: Unix domain sockets for local control
- **Message Types**: Admin commands, status queries
- **Authentication**: Process-based security model
- **Format**: JSON for human readability

### Network Topology

**Fully Connected**: Every node can communicate with every other node
- **Redundancy**: Multiple communication paths for fault tolerance
- **Load Distribution**: Requests distributed across available nodes
- **Partition Handling**: Graceful degradation during network splits

## Error Handling and Recovery

### Failure Detection

**Node Failure Detection:**
- **Heartbeat Monitoring**: Regular health checks between nodes
- **Timeout Management**: Configurable timeout values for different scenarios
- **Split-Brain Prevention**: Quorum-based decisions during partitions

**Network Failure Handling:**
- **Connection Retry**: Automatic reconnection with exponential backoff
- **Message Queuing**: Buffering of messages during temporary outages
- **Graceful Degradation**: Reduced functionality during partial failures

### Recovery Mechanisms

**Leader Recovery:**
- **Fast Election**: Quick leader election during failures
- **Log Repair**: Automatic log consistency restoration
- **State Synchronization**: Catch-up mechanisms for lagging nodes

**Data Recovery:**
- **Log Replay**: Recovery from persistent logs
- **Snapshot Restoration**: Fast recovery from state snapshots
- **Incremental Sync**: Efficient synchronization of missing data

## Performance Characteristics

### Scalability

**Cluster Size**: Optimized for 3-7 node clusters
- **Read Performance**: Scales with cluster size
- **Write Performance**: Limited by consensus requirements
- **Memory Usage**: Efficient memory utilization with bounded growth

### Throughput

**Consensus Operations**: 1000+ operations/second per cluster
**Storage Operations**: 10,000+ operations/second per node
**Network Bandwidth**: Efficient protocol with minimal overhead

### Latency

**Leadership Election**: Sub-second election times
**Write Latency**: <10ms for local cluster writes
**Read Latency**: <1ms for local reads

## Security Model

### Authentication

**Process-Based**: Local control via Unix domain sockets
**Optional TLS**: Encrypted inter-node communication
**No Built-in Auth**: Designed for trusted network environments

### Authorization

**Admin Operations**: Full cluster control via ralectrl
**Application Access**: Direct library integration
**Read-Only Access**: Future support for read-only clients

### Data Protection

**At Rest**: Optional encryption for persistent storage
**In Transit**: Optional TLS for network communication
**Memory**: Secure memory handling with proper cleanup

## Configuration Management

### Static Configuration

**Cluster Definition**: Node addresses, ports, and roles
**Network Settings**: Timeout values, retry policies
**Storage Settings**: File paths, compaction policies

### Dynamic Configuration

**Runtime Updates**: Live configuration changes
**Cluster Membership**: Dynamic node addition/removal
**Performance Tuning**: Runtime optimization parameters

## Monitoring and Observability

### Metrics

**Consensus Metrics**: Term numbers, vote counts, election frequency
**Storage Metrics**: Operation latencies, storage utilization
**Network Metrics**: Connection status, message rates

### Logging

**Structured Logging**: Clean, parseable log format
**Log Levels**: ERROR, WARNING, INFO, DEBUG levels
**Module Identification**: Clear component identification

### Health Checks

**Node Health**: CPU, memory, disk utilization
**Cluster Health**: Leadership status, node connectivity
**Application Health**: Operation success rates, latencies

## Integration Patterns

### Library Integration

**Direct Linking**: Link librale directly into applications
**API Usage**: Use public API for consensus and storage operations
**Thread Safety**: Safe for multi-threaded applications

### Service Integration

**Daemon Mode**: Use raled as a standalone service
**IPC Communication**: Communicate via Unix sockets or network
**Language Bindings**: Future support for multiple languages

### Deployment Patterns

**Embedded**: Library embedded in application processes
**Standalone**: Separate daemon processes per node
**Containerized**: Docker-ready deployment configuration
**Cloud Native**: Kubernetes-ready with proper health checks