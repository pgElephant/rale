# RALE - Distributed Consensus and Key-Value Store System

## Overview

RALE is a high-performance distributed consensus and key-value store system written in C. It provides reliable distributed coordination and persistent storage for distributed applications with strong consistency guarantees.

## Quick Start

```bash
# Clone and build
git clone https://github.com/pgElephant/rale.git
cd rale && ./build.sh

# Start single node
raled --config conf/raled1.conf

# Use CLI
ralectrl ADD --node-id 1 --node-name "node1" --node-ip "127.0.0.1" --rale-port 7400 --dstore-port 7401
```

## Components

- **librale**: Core consensus and distributed store library
- **raled**: Daemon process for cluster management  
- **ralectrl**: Command-line interface for cluster operations

## Key Features

- **RALE Consensus**: Reliable leader election and log replication
- **Distributed Store**: High-performance replicated key-value storage
- **Thread Safety**: Full multi-threading support with proper synchronization
- **Network Layer**: TCP/UDP communication with automatic failover
- **Memory Safety**: Safe allocation/deallocation with leak prevention
- **Clean Logging**: Professional logging without colors or terminal dependencies

## Documentation

📚 **Complete documentation is available in the `docs/` directory:**

### Getting Started
- **[Installation Guide](docs/INSTALLATION.md)** - Platform-specific installation instructions
- **[Configuration Guide](docs/CONFIGURATION.md)** - Complete configuration reference
- **[Examples](docs/EXAMPLES.md)** - Practical usage examples and tutorials

### Reference
- **[API Reference](docs/API_REFERENCE.md)** - Complete API documentation with examples
- **[Architecture](docs/ARCHITECTURE.md)** - System design and technical architecture

### Quick Links
- [Single Node Setup](docs/EXAMPLES.md#hello-world---single-node)
- [Three Node Cluster](docs/EXAMPLES.md#three-node-cluster-setup)  
- [API Usage Examples](docs/EXAMPLES.md#basic-examples)
- [Performance Benchmarks](docs/EXAMPLES.md#benchmark-tool)

## Prerequisites

- **C Compiler**: GCC 4.9+ or Clang 3.5+
- **CMake**: Version 3.16 or higher
- **pthread**: POSIX threads library

## Building

**Always use the build script for compilation:**

```bash
./build.sh
```

The build script handles:
- Dependency installation for your platform
- Proper CMake configuration
- Building with optimal settings
- Copying binaries to the correct locations

**Note:** Do not run `make` directly. Always use `./build.sh` to ensure proper setup and binary placement.

## Testing

```bash
./regression.sh        # Core functionality tests
./test_integration.sh   # Integration tests
```

## Architecture

RALE implements a distributed consensus algorithm with the following key components:

- **Consensus Layer**: Leader election, log replication, and cluster membership
- **Storage Layer**: Distributed key-value store with strong consistency
- **Network Layer**: Fault-tolerant communication between cluster nodes
- **Management Layer**: Configuration, monitoring, and administrative tools

See [Architecture Documentation](docs/ARCHITECTURE.md) for detailed technical information.

## Example Usage

### Library Integration
```c
#include <librale.h>

// Initialize cluster
librale_cluster_init();
librale_rale_init(1, "./data");
librale_dstore_init("./data");

// Store and retrieve data
librale_dstore_put("key1", "value1", 6);
char buffer[64]; size_t len = sizeof(buffer);
librale_dstore_get("key1", buffer, &len);
```

### Cluster Management
```bash
# Start three-node cluster
raled --config conf/raled1.conf &
raled --config conf/raled2.conf &  
raled --config conf/raled3.conf &

# Monitor cluster
ralectrl STATUS
ralectrl LIST
```

See [Examples Documentation](docs/EXAMPLES.md) for complete tutorials and code samples.

## Performance

- **Consensus**: 1000+ operations/second per cluster
- **Storage**: 10,000+ operations/second per node  
- **Latency**: <10ms for cluster writes, <1ms for local reads
- **Scalability**: Optimized for 3-7 node clusters

## Coding Standards

Professional C coding standards:
- 80-character line length, tabs for indentation
- Block comments only (`/* */`), no C++ style comments
- Variables declared at function start
- `lowercase_with_underscores` naming convention

## License

MIT License - see LICENSE file for details.

## Contributing

Contributions welcome! Please review the [Architecture Guide](docs/ARCHITECTURE.md) and [API Reference](docs/API_REFERENCE.md) before contributing.

## Support

- **Documentation**: See `docs/` directory for comprehensive guides
- **Issues**: GitHub issue tracker for bugs and feature requests
- **Examples**: Working code samples in `docs/EXAMPLES.md`

---

**Copyright (c) 2024-2025, pgElephant, Inc**