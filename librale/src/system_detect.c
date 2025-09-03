/*-------------------------------------------------------------------------
 *
 * system_detect.c
 *		Implementation of operating system detection and system-specific functions
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "system_detect.h"
#include <string.h>

/*-------------------------------------------------------------------------
 * System Information Functions
 *-------------------------------------------------------------------------*/

const char *
get_os_name(void)
{
#if defined(OS_MACOS)
    return "macOS";
#elif defined(OS_LINUX)
    return "Linux";
#elif defined(OS_WINDOWS)
    return "Windows";
#elif defined(OS_FREEBSD)
    return "FreeBSD";
#elif defined(OS_NETBSD)
    return "NetBSD";
#elif defined(OS_OPENBSD)
    return "OpenBSD";
#else
    return "Unknown";
#endif
}

const char *
get_os_family(void)
{
#if defined(OS_MACOS)
    return "darwin";
#elif defined(OS_LINUX)
    return "linux";
#elif defined(OS_WINDOWS)
    return "windows";
#elif defined(OS_FREEBSD) || defined(OS_NETBSD) || defined(OS_OPENBSD)
    return "bsd";
#else
    return "unknown";
#endif
}

const char *
get_arch_name(void)
{
#if defined(ARCH_X86_64)
    return "x86_64";
#elif defined(ARCH_X86)
    return "x86";
#elif defined(ARCH_ARM64)
    return "ARM64";
#elif defined(ARCH_ARM)
    return "ARM";
#else
    return "Unknown";
#endif
}

int
get_arch_bits(void)
{
#if defined(ARCH_X86_64) || defined(ARCH_ARM64)
    return 64;
#elif defined(ARCH_X86) || defined(ARCH_ARM)
    return 32;
#else
    return 0;
#endif
}

int
has_feature(const char *feature)
{
    if (feature == NULL)
        return 0;

    if (strcmp(feature, "unix_sockets") == 0)
        return HAVE_UNIX_SOCKETS;
    else if (strcmp(feature, "systemd") == 0)
        return HAVE_SYSTEMD;
    else if (strcmp(feature, "launchd") == 0)
        return HAVE_LAUNCHD;
    else if (strcmp(feature, "watchdog") == 0)
        return HAVE_WATCHDOG;
    else
        return 0;
}

const char *
get_default_path(const char *path_type)
{
    if (path_type == NULL)
        return NULL;

    if (strcmp(path_type, "config") == 0)
        return DEFAULT_CONFIG_DIR;
    else if (strcmp(path_type, "log") == 0)
        return DEFAULT_LOG_DIR;
    else if (strcmp(path_type, "data") == 0)
        return DEFAULT_DATA_DIR;
    else if (strcmp(path_type, "pid") == 0)
        return DEFAULT_PID_DIR;
    else if (strcmp(path_type, "socket") == 0)
        return DEFAULT_SOCKET_DIR;
    else
        return NULL;
}
