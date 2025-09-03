# Installation Guide

## Overview

This guide covers installation of the RALE distributed consensus and key-value store system on various platforms. RALE consists of the core library (librale), daemon (raled), and CLI tool (ralectrl).

## System Requirements

### Minimum Requirements
- **OS**: Linux, macOS, or Unix-like system
- **CPU**: x86_64 or ARM64 architecture
- **Memory**: 256MB minimum, 1GB recommended
- **Disk**: 100MB for binaries, additional space for data storage
- **Network**: TCP/UDP connectivity between cluster nodes

### Software Dependencies
- **C Compiler**: GCC 4.9+ or Clang 3.5+
- **CMake**: Version 3.16 or higher
- **pthread**: POSIX threads library
- **Standard C Library**: glibc 2.17+ or musl

### Development Dependencies (Optional)
- **pkg-config**: For library detection
- **Valgrind**: For memory debugging
- **GDB**: For debugging
- **cppcheck**: For static analysis

## Quick Installation

### Using Installation Script (Recommended)

```bash
# Clone the repository
git clone https://github.com/pgElephant/rale.git
cd rale

# Run automated installation
sudo ./build.sh install

# Verify installation
rale --version
ralectrl --help
```

### Manual Build and Install

```bash
# Clone and build
git clone https://github.com/pgElephant/rale.git
cd rale

# Create build directory
mkdir build && cd build

# Configure build
cmake ..

# Build all components
make -j$(nproc)

# Install system-wide
sudo make install

# Update library cache (Linux)
sudo ldconfig
```

## Platform-Specific Installation

### Ubuntu/Debian

#### Install Dependencies
```bash
# Update package lists
sudo apt update

# Install build dependencies
sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libc6-dev \
    git

# Install development tools (optional)
sudo apt install -y \
    valgrind \
    gdb \
    cppcheck
```

#### Build and Install
```bash
# Clone repository
git clone https://github.com/pgElephant/rale.git
cd rale

# Build and install
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

#### Package Installation
```bash
# Build DEB package
./scripts/build_deb.sh

# Install package
sudo dpkg -i rale_1.0.0_amd64.deb

# Fix dependencies if needed
sudo apt-get install -f
```

### CentOS/RHEL/Fedora

#### Install Dependencies
```bash
# CentOS/RHEL
sudo yum groupinstall "Development Tools"
sudo yum install cmake pkg-config git

# Fedora
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake pkg-config git
```

#### Build and Install
```bash
# Clone and build
git clone https://github.com/pgElephant/rale.git
cd rale
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install

# Update library cache
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/librale.conf
sudo ldconfig
```

#### RPM Package
```bash
# Build RPM package
./scripts/build_rpm.sh

# Install package
sudo rpm -ivh rale-1.0.0-1.x86_64.rpm
```

### macOS

#### Install Dependencies
```bash
# Install Xcode command line tools
xcode-select --install

# Install Homebrew (if not installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install CMake
brew install cmake pkg-config
```

#### Build and Install
```bash
# Clone repository
git clone https://github.com/pgElephant/rale.git
cd rale

# Build and install
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)
sudo make install

# Update dynamic linker cache
sudo update_dyld_shared_cache
```

#### macOS Package
```bash
# Build macOS package
./scripts/build_macos_pkg.sh

# Install package
sudo installer -pkg rale-1.0.0.pkg -target /
```

### Docker Installation

#### Pre-built Container
```bash
# Pull official image
docker pull rale/rale:latest

# Run single node
docker run -d --name rale-node1 \
  -p 7400:7400 -p 7401:7401 \
  rale/rale:latest

# Run with custom configuration
docker run -d --name rale-node1 \
  -v /host/config:/etc/rale \
  -v /host/data:/var/lib/rale \
  -p 7400:7400 -p 7401:7401 \
  rale/rale:latest
```

#### Build Custom Container
```bash
# Clone repository
git clone https://github.com/pgElephant/rale.git
cd rale

# Build container
docker build -t rale:custom .

# Run container
docker run -d --name rale-custom \
  -p 7400:7400 -p 7401:7401 \
  rale:custom
```

## Library-Only Installation

If you only need the librale library for development:

### Static Library Installation
```bash
cd rale/librale
mkdir build && cd build
cmake -DBUILD_SHARED_LIBS=OFF ..
make -j$(nproc)
sudo make install
```

### Shared Library Installation
```bash
cd rale/librale
mkdir build && cd build
cmake -DBUILD_SHARED_LIBS=ON ..
make -j$(nproc)
sudo make install
sudo ldconfig  # Linux only
```

### Development Installation
```bash
cd rale/librale

# Install with development tools
./dev-setup.sh

# This creates:
# - Build environment
# - VS Code configuration
# - Development Makefile
# - Git pre-commit hooks
```

## Configuration

### System-wide Configuration
```bash
# Create configuration directory
sudo mkdir -p /etc/rale

# Copy sample configurations
sudo cp conf/raled*.conf /etc/rale/

# Set permissions
sudo chown -R rale:rale /etc/rale
sudo chmod 640 /etc/rale/*.conf
```

### User Configuration
```bash
# Create user configuration
mkdir -p ~/.config/rale

# Copy and customize configuration
cp conf/raled1.conf ~/.config/rale/raled.conf
$EDITOR ~/.config/rale/raled.conf
```

### Data Directories
```bash
# Create system data directories
sudo mkdir -p /var/lib/rale/{data,logs,state}
sudo chown -R rale:rale /var/lib/rale
sudo chmod 750 /var/lib/rale

# Create user data directories
mkdir -p ~/.local/share/rale/{data,logs,state}
```

## Service Installation

### systemd Service (Linux)

#### Install Service Files
```bash
# Copy service file
sudo cp scripts/rale.service /etc/systemd/system/

# Reload systemd
sudo systemctl daemon-reload

# Enable service
sudo systemctl enable rale

# Start service
sudo systemctl start rale

# Check status
sudo systemctl status rale
```

#### Service Configuration
```bash
# Edit service configuration
sudo systemctl edit rale

# Add custom configuration
[Service]
Environment=RALE_CONFIG=/etc/rale/raled.conf
Environment=RALE_DATA_DIR=/var/lib/rale
User=rale
Group=rale
```

### launchd Service (macOS)

#### Install Launch Agent
```bash
# Copy plist file
cp scripts/com.rale.raled.plist ~/Library/LaunchAgents/

# Load service
launchctl load ~/Library/LaunchAgents/com.rale.raled.plist

# Start service
launchctl start com.rale.raled

# Check status
launchctl list | grep rale
```

## Verification

### Installation Verification
```bash
# Check installed binaries
which raled ralectrl
raled --version
ralectrl --version

# Check library installation
pkg-config --modversion librale
pkg-config --cflags --libs librale

# Check library linkage
ldd $(which raled)  # Linux
otool -L $(which raled)  # macOS
```

### Functional Testing
```bash
# Start single node for testing
raled --config conf/raled1.conf --foreground &
RALED_PID=$!

# Test CLI connectivity
ralectrl --socket /tmp/raled1.sock LIST

# Test basic operations
ralectrl --socket /tmp/raled1.sock ADD \
  --node-id 1 --node-name "test1" \
  --node-ip "127.0.0.1" --rale-port 7400 --dstore-port 7401

# Cleanup
kill $RALED_PID
```

### Performance Testing
```bash
# Run built-in benchmarks
./scripts/benchmark.sh

# Run load testing
./scripts/load_test.sh --nodes 3 --duration 60

# Monitor resource usage
top -p $(pgrep raled)
```

## Troubleshooting

### Common Issues

#### Build Failures
```bash
# Clean build directory
rm -rf build && mkdir build && cd build

# Verbose build output
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
make VERBOSE=1

# Check for missing dependencies
ldd bin/raled  # Linux
otool -L bin/raled  # macOS
```

#### Library Not Found
```bash
# Update library cache (Linux)
sudo ldconfig

# Check library path
echo $LD_LIBRARY_PATH

# Add library path
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# For permanent fix, add to /etc/ld.so.conf.d/
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/librale.conf
sudo ldconfig
```

#### Permission Issues
```bash
# Create rale user
sudo useradd -r -s /bin/false rale

# Fix ownership
sudo chown -R rale:rale /var/lib/rale /etc/rale

# Fix permissions
sudo chmod 755 /var/lib/rale
sudo chmod 640 /etc/rale/*.conf
```

#### Port Conflicts
```bash
# Check port usage
netstat -tulpn | grep :7400
lsof -i :7400

# Use alternative ports
ralectrl --socket /tmp/raled1.sock SET \
  --key rale_port --value 7500
```

### Debug Installation
```bash
# Enable debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run with debugging
gdb --args raled --config conf/raled1.conf --foreground

# Memory debugging
valgrind --leak-check=full raled --config conf/raled1.conf --foreground

# Static analysis
cppcheck --enable=all librale/src/
```

### Log Analysis
```bash
# Check system logs
journalctl -u rale -f  # systemd
tail -f /var/log/rale/raled.log

# Enable debug logging
ralectrl --socket /tmp/raled1.sock SET \
  --key log_level --value DEBUG

# Analyze logs
grep ERROR /var/log/rale/raled.log
grep -E "(WARN|ERROR)" /var/log/rale/raled.log | tail -20
```

## Uninstallation

### Manual Uninstall
```bash
# Stop services
sudo systemctl stop rale
sudo systemctl disable rale

# Remove binaries
sudo rm -f /usr/local/bin/{raled,ralectrl}

# Remove library
sudo rm -f /usr/local/lib/librale.*
sudo rm -rf /usr/local/include/rale

# Remove configuration
sudo rm -rf /etc/rale

# Remove data (CAUTION: This deletes all data!)
sudo rm -rf /var/lib/rale

# Update library cache
sudo ldconfig
```

### Package Uninstall
```bash
# Ubuntu/Debian
sudo apt remove rale

# CentOS/RHEL/Fedora
sudo rpm -e rale

# macOS (if installed via package)
sudo pkgutil --forget com.rale.rale
```

### Library Uninstall Script
```bash
# Use provided uninstall script
cd rale/librale
sudo ./uninstall.sh
```

## Next Steps

After successful installation:

1. **Read Configuration Guide**: See `docs/CONFIGURATION.md`
2. **Review API Documentation**: See `docs/API_REFERENCE.md`
3. **Set up Cluster**: See `docs/CLUSTER_SETUP.md`
4. **Integration Examples**: See `docs/EXAMPLES.md`

For additional help, consult the troubleshooting section or submit an issue to the project repository.