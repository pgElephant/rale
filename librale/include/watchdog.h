/*-------------------------------------------------------------------------
 *
 * watchdog.h
 *		Librale Watchdog Device Integration
 *
 * Provides hardware/software watchdog support for split-brain prevention
 * in distributed systems. Integrates with Linux watchdog devices to
 * provide system-level failsafe mechanisms.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef LIBRALE_WATCHDOG_H
#define LIBRALE_WATCHDOG_H

#include "rale.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

/* Watchdog device paths */
#define WATCHDOG_DEFAULT_DEVICE		"/dev/watchdog"
#define WATCHDOG_FALLBACK_DEVICE	"/dev/watchdog0"
#define WATCHDOG_TEST_MODE_FILE		"/tmp/librale_watchdog_test"

/* Watchdog configuration constants */
#define WATCHDOG_MIN_TIMEOUT		5		/* Minimum timeout in seconds */
#define WATCHDOG_MAX_TIMEOUT		600		/* Maximum timeout in seconds */
#define WATCHDOG_DEFAULT_TIMEOUT	30		/* Default timeout in seconds */
#define WATCHDOG_SAFETY_MARGIN		5		/* Safety margin in seconds */
#define WATCHDOG_KEEPALIVE_INTERVAL	10		/* Keepalive interval in seconds */

/* Watchdog mode enumeration */
typedef enum {
    WATCHDOG_MODE_DISABLED = 0,
    WATCHDOG_MODE_OPTIONAL,		/* Watchdog failure doesn't prevent leadership */
    WATCHDOG_MODE_REQUIRED		/* Watchdog required for leadership */
} watchdog_mode_t;

/* Watchdog state enumeration */
typedef enum {
    WATCHDOG_STATE_UNINITIALIZED = 0,
    WATCHDOG_STATE_DISABLED,
    WATCHDOG_STATE_ENABLED,
    WATCHDOG_STATE_ACTIVE,
    WATCHDOG_STATE_FAILED,
    WATCHDOG_STATE_TEST_MODE
} watchdog_state_t;

/* Watchdog statistics structure */
typedef struct watchdog_stats_t {
    uint64_t		keepalives_sent;
    uint64_t		keepalives_failed;
    uint64_t		enable_count;
    uint64_t		disable_count;
    time_t		last_keepalive;
    time_t		last_enable;
    time_t		last_disable;
    uint32_t		current_timeout;
    bool		is_test_mode;
} watchdog_stats_t;

/* Watchdog configuration structure */
typedef struct watchdog_config_t {
    char			device_path[256];
    watchdog_mode_t		mode;
    uint32_t			timeout_seconds;
    uint32_t			safety_margin_seconds;
    uint32_t			keepalive_interval_seconds;
    bool			test_mode;
    bool			soft_noboot;		/* For testing - don't reboot */
} watchdog_config_t;

/* Watchdog context structure */
typedef struct watchdog_context_t {
    watchdog_config_t		config;
    watchdog_state_t		state;
    watchdog_stats_t		stats;
    int			device_fd;
    time_t			last_keepalive;
    time_t			enabled_at;
    bool			is_active;
    pthread_mutex_t		mutex;
} watchdog_context_t;

/* Core watchdog functions */
bool watchdog_init(watchdog_context_t *ctx, const watchdog_config_t *config);
void watchdog_cleanup(watchdog_context_t *ctx);
bool watchdog_enable(watchdog_context_t *ctx);
bool watchdog_disable(watchdog_context_t *ctx);
bool watchdog_keepalive(watchdog_context_t *ctx);
bool watchdog_is_active(const watchdog_context_t *ctx);

/* Watchdog configuration functions */
void watchdog_config_init_defaults(watchdog_config_t *config);
bool watchdog_config_validate(const watchdog_config_t *config);
bool watchdog_config_set_device(watchdog_config_t *config, const char *device_path);
bool watchdog_config_set_timeout(watchdog_config_t *config, uint32_t timeout_seconds);
bool watchdog_config_set_mode(watchdog_config_t *config, watchdog_mode_t mode);

/* Watchdog device functions */
bool watchdog_device_exists(const char *device_path);
bool watchdog_device_accessible(const char *device_path);
bool watchdog_device_test(const char *device_path);
int watchdog_device_get_timeout(const char *device_path);
bool watchdog_device_set_timeout(const char *device_path, uint32_t timeout_seconds);

/* Watchdog state functions */
watchdog_state_t watchdog_get_state(const watchdog_context_t *ctx);
const char *watchdog_state_to_string(watchdog_state_t state);
const char *watchdog_mode_to_string(watchdog_mode_t mode);
void watchdog_get_stats(const watchdog_context_t *ctx, watchdog_stats_t *stats);

/* Watchdog timing functions */
uint32_t watchdog_calculate_safe_timeout(uint32_t ttl, uint32_t loop_wait, int32_t safety_margin);
bool watchdog_should_enable_for_leadership(const watchdog_context_t *ctx, uint32_t ttl);
bool watchdog_time_to_keepalive(const watchdog_context_t *ctx);
uint32_t watchdog_time_until_expiry(const watchdog_context_t *ctx);

/* Watchdog test functions */
bool watchdog_enable_test_mode(watchdog_context_t *ctx);
bool watchdog_disable_test_mode(watchdog_context_t *ctx);
bool watchdog_simulate_failure(watchdog_context_t *ctx);
bool watchdog_verify_test_mode(const watchdog_context_t *ctx);

/* Watchdog integration functions for librale */
bool watchdog_can_become_leader(const watchdog_context_t *ctx);
bool watchdog_prepare_for_leadership(watchdog_context_t *ctx, uint32_t ttl);
bool watchdog_leadership_keepalive(watchdog_context_t *ctx);
bool watchdog_release_leadership(watchdog_context_t *ctx);

/* Watchdog logging functions */
void watchdog_log_state_change(const watchdog_context_t *ctx, watchdog_state_t old_state, 
                              watchdog_state_t new_state);
void watchdog_log_stats(const watchdog_context_t *ctx);
void watchdog_log_error(const watchdog_context_t *ctx, const char *operation, const char *error);

/* Utility functions */
bool watchdog_is_supported_platform(void);
const char *watchdog_get_platform_info(void);

#endif /* LIBRALE_WATCHDOG_H */
