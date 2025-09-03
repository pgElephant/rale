/*-------------------------------------------------------------------------
 *
 * system_detect.h
 *		Operating system detection and system-specific definitions
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef SYSTEM_DETECT_H
#define SYSTEM_DETECT_H

/*-------------------------------------------------------------------------
 * Operating System Detection
 *-------------------------------------------------------------------------*/

#if defined(__APPLE__) && defined(__MACH__)
    #define OS_MACOS 1
    #define OS_NAME "macOS"
    #define OS_FAMILY "darwin"
#elif defined(__linux__)
    #define OS_LINUX 1
    #define OS_NAME "Linux"
    #define OS_FAMILY "linux"
#elif defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #define OS_NAME "Windows"
    #define OS_FAMILY "windows"
#elif defined(__FreeBSD__)
    #define OS_FREEBSD 1
    #define OS_NAME "FreeBSD"
    #define OS_FAMILY "bsd"
#elif defined(__NetBSD__)
    #define OS_NETBSD 1
    #define OS_NAME "NetBSD"
    #define OS_FAMILY "bsd"
#elif defined(__OpenBSD__)
    #define OS_OPENBSD 1
    #define OS_NAME "OpenBSD"
    #define OS_FAMILY "bsd"
#else
    #define OS_UNKNOWN 1
    #define OS_NAME "Unknown"
    #define OS_FAMILY "unknown"
#endif

/*-------------------------------------------------------------------------
 * Architecture Detection
 *-------------------------------------------------------------------------*/

#if defined(__x86_64__) || defined(_M_X64)
    #define ARCH_X86_64 1
    #define ARCH_NAME "x86_64"
    #define ARCH_BITS 64
#elif defined(__i386__) || defined(_M_IX86)
    #define ARCH_X86 1
    #define ARCH_NAME "x86"
    #define ARCH_BITS 32
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ARCH_ARM64 1
    #define ARCH_NAME "ARM64"
    #define ARCH_BITS 64
#elif defined(__arm__) || defined(_M_ARM)
    #define ARCH_ARM 1
    #define ARCH_NAME "ARM"
    #define ARCH_BITS 32
#else
    #define ARCH_UNKNOWN 1
    #define ARCH_NAME "Unknown"
    #define ARCH_BITS 0
#endif

/*-------------------------------------------------------------------------
 * System-Specific Features
 *-------------------------------------------------------------------------*/

/* Unix domain sockets support */
#if defined(OS_MACOS) || defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_NETBSD) || defined(OS_OPENBSD)
    #define HAVE_UNIX_SOCKETS 1
#else
    #define HAVE_UNIX_SOCKETS 0
#endif

/* Systemd support (Linux only) */
#if defined(OS_LINUX)
    #define HAVE_SYSTEMD 1
#else
    #define HAVE_SYSTEMD 0
#endif

/* Launchd support (macOS only) */
#if defined(OS_MACOS)
    #define HAVE_LAUNCHD 1
#else
    #define HAVE_LAUNCHD 0
#endif

/* Watchdog support (Linux only) */
#if defined(OS_LINUX)
    #define HAVE_WATCHDOG 1
#else
    #define HAVE_WATCHDOG 0
#endif

/*-------------------------------------------------------------------------
 * System-Specific Paths
 *-------------------------------------------------------------------------*/

#if defined(OS_MACOS)
    #define DEFAULT_CONFIG_DIR "/usr/local/etc/ram"
    #define DEFAULT_LOG_DIR "/usr/local/var/log/ram"
    #define DEFAULT_DATA_DIR "/usr/local/var/lib/ram"
    #define DEFAULT_PID_DIR "/usr/local/var/run/ram"
    #define DEFAULT_SOCKET_DIR "/tmp"
#elif defined(OS_LINUX)
    #define DEFAULT_CONFIG_DIR "/etc/ram"
    #define DEFAULT_LOG_DIR "/var/log/ram"
    #define DEFAULT_DATA_DIR "/var/lib/ram"
    #define DEFAULT_PID_DIR "/var/run/ram"
    #define DEFAULT_SOCKET_DIR "/tmp"
#elif defined(OS_WINDOWS)
    #define DEFAULT_CONFIG_DIR "C:\\ProgramData\\ram"
    #define DEFAULT_LOG_DIR "C:\\ProgramData\\ram\\logs"
    #define DEFAULT_DATA_DIR "C:\\ProgramData\\ram\\data"
    #define DEFAULT_PID_DIR "C:\\ProgramData\\ram\\run"
    #define DEFAULT_SOCKET_DIR "C:\\ProgramData\\ram\\sockets"
#else
    #define DEFAULT_CONFIG_DIR "/etc/ram"
    #define DEFAULT_LOG_DIR "/var/log/ram"
    #define DEFAULT_DATA_DIR "/var/lib/ram"
    #define DEFAULT_PID_DIR "/var/run/ram"
    #define DEFAULT_SOCKET_DIR "/tmp"
#endif

/*-------------------------------------------------------------------------
 * System-Specific Compiler Flags
 *-------------------------------------------------------------------------*/

#if defined(OS_MACOS)
    #define PLATFORM_CFLAGS "-DMACOS"
    #define PLATFORM_LDFLAGS "-framework CoreFoundation -framework IOKit"
#elif defined(OS_LINUX)
    #define PLATFORM_CFLAGS "-DLINUX"
    #define PLATFORM_LDFLAGS "-ldl -lrt"
#elif defined(OS_WINDOWS)
    #define PLATFORM_CFLAGS "-DWINDOWS"
    #define PLATFORM_LDFLAGS "-lws2_32 -liphlpapi"
#else
    #define PLATFORM_CFLAGS ""
    #define PLATFORM_LDFLAGS ""
#endif

/*-------------------------------------------------------------------------
 * Utility Macros
 *-------------------------------------------------------------------------*/

#define IS_UNIX() (defined(OS_MACOS) || defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_NETBSD) || defined(OS_OPENBSD))
#define IS_WINDOWS() defined(OS_WINDOWS)
#define IS_MACOS() defined(OS_MACOS)
#define IS_LINUX() defined(OS_LINUX)

/*-------------------------------------------------------------------------
 * System Information Functions
 *-------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get operating system name as string
 */
const char *get_os_name(void);

/**
 * Get operating system family as string
 */
const char *get_os_family(void);

/**
 * Get architecture name as string
 */
const char *get_arch_name(void);

/**
 * Get architecture bitness
 */
int get_arch_bits(void);

/**
 * Check if a specific feature is available
 */
int has_feature(const char *feature);

/**
 * Get system-specific default path for a given type
 */
const char *get_default_path(const char *path_type);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_DETECT_H */
