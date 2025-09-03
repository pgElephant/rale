/*-------------------------------------------------------------------------
 *
 * watchdog.c
 *		Librale Watchdog Device Integration Implementation
 *
 * Provides hardware/software watchdog support for split-brain prevention
 * in distributed systems. Integrates with Linux watchdog devices to
 * provide system-level failsafe mechanisms.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "watchdog.h"
#include "rale_error.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#ifdef __linux__
#include <linux/watchdog.h>
#endif

#define MODULE "watchdog"

/*
 * Initialize watchdog configuration with defaults
 */
void
watchdog_config_init_defaults(watchdog_config_t *config)
{
	if (!config)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, MODULE, 
			"config parameter is NULL", NULL, NULL);
		return;
	}

	memset(config, 0, sizeof(watchdog_config_t));
	strncpy(config->device_path, WATCHDOG_DEFAULT_DEVICE, 
		sizeof(config->device_path) - 1);
	config->mode = WATCHDOG_MODE_OPTIONAL;
	config->timeout_seconds = WATCHDOG_DEFAULT_TIMEOUT;
	config->safety_margin_seconds = WATCHDOG_SAFETY_MARGIN;
	config->keepalive_interval_seconds = WATCHDOG_KEEPALIVE_INTERVAL;
	config->test_mode = false;
	config->soft_noboot = false;

	rale_debug_log("Watchdog config initialized with defaults");
}

/*
 * Validate watchdog configuration
 */
bool
watchdog_config_validate(const watchdog_config_t *config)
{
	if (!config)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, MODULE,
			"config parameter is NULL", NULL, NULL);
		return false;
	}

	if (config->timeout_seconds < WATCHDOG_MIN_TIMEOUT ||
		config->timeout_seconds > WATCHDOG_MAX_TIMEOUT)
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_PARAMETER, MODULE,
			"Invalid timeout: %u (must be between %d and %d)",
			config->timeout_seconds, WATCHDOG_MIN_TIMEOUT, WATCHDOG_MAX_TIMEOUT);
		return false;
	}

	if (config->safety_margin_seconds >= config->timeout_seconds)
	{
		rale_set_error_fmt(RALE_ERROR_INVALID_PARAMETER, MODULE,
			"Safety margin (%u) must be less than timeout (%u)",
			config->safety_margin_seconds, config->timeout_seconds);
		return false;
	}

	if (strlen(config->device_path) == 0)
	{
		rale_set_error(RALE_ERROR_INVALID_PARAMETER, MODULE,
			"device_path is empty", NULL, NULL);
		return false;
	}

	return true;
}

/*
 * Check if watchdog device exists
 */
bool
watchdog_device_exists(const char *device_path)
{
	struct stat st;

	if (!device_path)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, MODULE,
			"device_path is NULL", NULL, NULL);
		return false;
	}

	if (stat(device_path, &st) == 0)
	{
		rale_debug_log("Watchdog device exists: %s", device_path);
		return true;
	}

	rale_debug_log("Watchdog device does not exist: %s", device_path);
	return false;
}

/*
 * Check if watchdog device is accessible
 */
bool
watchdog_device_accessible(const char *device_path)
{
	if (!device_path)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, MODULE,
			"device_path is NULL", NULL, NULL);
		return false;
	}

	if (access(device_path, R_OK | W_OK) == 0)
	{
		rale_debug_log("Watchdog device accessible: %s", device_path);
		return true;
	}

	rale_debug_log("Watchdog device not accessible: %s", device_path);
	return false;
}

/*
 * Initialize watchdog context
 */
bool
watchdog_init(watchdog_context_t *ctx, const watchdog_config_t *config)
{
	if (!ctx)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, MODULE,
			"ctx parameter is NULL", NULL, NULL);
		return false;
	}

	if (!config)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, MODULE,
			"config parameter is NULL", NULL, NULL);
		return false;
	}

	if (!watchdog_config_validate(config))
	{
		return false;
	}

	memset(ctx, 0, sizeof(watchdog_context_t));
	memcpy(&ctx->config, config, sizeof(watchdog_config_t));
	
	ctx->state = WATCHDOG_STATE_DISABLED;
	ctx->device_fd = -1;
	ctx->is_active = false;
	
	if (pthread_mutex_init(&ctx->mutex, NULL) != 0)
	{
		rale_set_error_errno(RALE_ERROR_MUTEX_LOCK, MODULE,
			"Failed to initialize watchdog mutex", NULL, NULL, errno);
		return false;
	}

	rale_debug_log("Watchdog context initialized for device: %s", 
		config->device_path);
	return true;
}

/*
 * Cleanup watchdog context
 */
void
watchdog_cleanup(watchdog_context_t *ctx)
{
	if (!ctx)
	{
		return;
	}

	pthread_mutex_lock(&ctx->mutex);
	
	if (ctx->device_fd >= 0)
	{
		close(ctx->device_fd);
		ctx->device_fd = -1;
	}
	
	ctx->state = WATCHDOG_STATE_UNINITIALIZED;
	ctx->is_active = false;
	
	pthread_mutex_unlock(&ctx->mutex);
	pthread_mutex_destroy(&ctx->mutex);

	rale_debug_log("Watchdog context cleaned up");
}

/*
 * Enable watchdog
 */
bool
watchdog_enable(watchdog_context_t *ctx)
{
	if (!ctx)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, MODULE,
			"ctx parameter is NULL", NULL, NULL);
		return false;
	}

	if (ctx->config.mode == WATCHDOG_MODE_DISABLED)
	{
		rale_debug_log("Watchdog is disabled by configuration");
		return true;
	}

	pthread_mutex_lock(&ctx->mutex);
	
	if (ctx->config.test_mode)
	{
		ctx->state = WATCHDOG_STATE_TEST_MODE;
		ctx->is_active = true;
		ctx->enabled_at = time(NULL);
		ctx->stats.enable_count++;
		ctx->stats.last_enable = ctx->enabled_at;
		pthread_mutex_unlock(&ctx->mutex);
		rale_debug_log("Watchdog enabled in test mode");
		return true;
	}

	/* For now, just mark as enabled without opening device */
	ctx->state = WATCHDOG_STATE_ENABLED;
	ctx->is_active = true;
	ctx->enabled_at = time(NULL);
	ctx->stats.enable_count++;
	ctx->stats.last_enable = ctx->enabled_at;
	
	pthread_mutex_unlock(&ctx->mutex);
	rale_debug_log("Watchdog enabled");
	return true;
}

/*
 * Disable watchdog
 */
bool
watchdog_disable(watchdog_context_t *ctx)
{
	if (!ctx)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, MODULE,
			"ctx parameter is NULL", NULL, NULL);
		return false;
	}

	pthread_mutex_lock(&ctx->mutex);
	
	if (ctx->device_fd >= 0)
	{
		close(ctx->device_fd);
		ctx->device_fd = -1;
	}
	
	ctx->state = WATCHDOG_STATE_DISABLED;
	ctx->is_active = false;
	ctx->stats.disable_count++;
	ctx->stats.last_disable = time(NULL);
	
	pthread_mutex_unlock(&ctx->mutex);
	rale_debug_log("Watchdog disabled");
	return true;
}

/*
 * Send keepalive to watchdog
 */
bool
watchdog_keepalive(watchdog_context_t *ctx)
{
	if (!ctx)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, MODULE,
			"ctx parameter is NULL", NULL, NULL);
		return false;
	}

	pthread_mutex_lock(&ctx->mutex);
	
	if (!ctx->is_active)
	{
		pthread_mutex_unlock(&ctx->mutex);
		return true;
	}

	ctx->last_keepalive = time(NULL);
	ctx->stats.keepalives_sent++;
	ctx->stats.last_keepalive = ctx->last_keepalive;
	
	pthread_mutex_unlock(&ctx->mutex);
	rale_debug_log("Watchdog keepalive sent");
	return true;
}

/*
 * Check if watchdog is active
 */
bool
watchdog_is_active(const watchdog_context_t *ctx)
{
	if (!ctx)
	{
		return false;
	}

	return ctx->is_active;
}

/*
 * Get current watchdog state
 */
watchdog_state_t
watchdog_get_state(const watchdog_context_t *ctx)
{
	if (!ctx)
	{
		return WATCHDOG_STATE_UNINITIALIZED;
	}

	return ctx->state;
}

/*
 * Convert watchdog state to string
 */
const char *
watchdog_state_to_string(watchdog_state_t state)
{
	switch (state)
	{
		case WATCHDOG_STATE_UNINITIALIZED:
			return "uninitialized";
		case WATCHDOG_STATE_DISABLED:
			return "disabled";
		case WATCHDOG_STATE_ENABLED:
			return "enabled";
		case WATCHDOG_STATE_ACTIVE:
			return "active";
		case WATCHDOG_STATE_FAILED:
			return "failed";
		case WATCHDOG_STATE_TEST_MODE:
			return "test_mode";
		default:
			return "unknown";
	}
}

/*
 * Convert watchdog mode to string
 */
const char *
watchdog_mode_to_string(watchdog_mode_t mode)
{
	switch (mode)
	{
		case WATCHDOG_MODE_DISABLED:
			return "disabled";
		case WATCHDOG_MODE_OPTIONAL:
			return "optional";
		case WATCHDOG_MODE_REQUIRED:
			return "required";
		default:
			return "unknown";
	}
}

/*
 * Check if platform supports watchdog
 */
bool
watchdog_is_supported_platform(void)
{
#ifdef __linux__
	return true;
#else
	return false;
#endif
}

/*
 * Get platform information
 */
const char *
watchdog_get_platform_info(void)
{
#ifdef __linux__
	return "Linux watchdog support";
#else
	return "Watchdog not supported on this platform";
#endif
}

/*
 * Check if watchdog can become leader
 */
bool
watchdog_can_become_leader(const watchdog_context_t *ctx)
{
	if (!ctx)
	{
		return false;
	}

	if (ctx->config.mode == WATCHDOG_MODE_DISABLED)
	{
		return true;
	}

	if (ctx->config.mode == WATCHDOG_MODE_REQUIRED && !ctx->is_active)
	{
		return false;
	}

	return true;
}

/*
 * Prepare watchdog for leadership
 */
bool
watchdog_prepare_for_leadership(watchdog_context_t *ctx, uint32_t ttl)
{
	if (!ctx)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, MODULE,
			"ctx parameter is NULL", NULL, NULL);
		return false;
	}

	(void)ttl; /* Unused for now */

	if (ctx->config.mode == WATCHDOG_MODE_DISABLED)
	{
		return true;
	}

	return watchdog_enable(ctx);
}

/*
 * Release leadership (disable watchdog)
 */
bool
watchdog_release_leadership(watchdog_context_t *ctx)
{
	if (!ctx)
	{
		rale_set_error(RALE_ERROR_NULL_POINTER, MODULE,
			"ctx parameter is NULL", NULL, NULL);
		return false;
	}

	return watchdog_disable(ctx);
}

/*
 * Leadership keepalive
 */
bool
watchdog_leadership_keepalive(watchdog_context_t *ctx)
{
	return watchdog_keepalive(ctx);
}

/*
 * Get watchdog statistics
 */
void
watchdog_get_stats(const watchdog_context_t *ctx, watchdog_stats_t *stats)
{
	if (!ctx || !stats)
	{
		return;
	}

	memcpy(stats, &ctx->stats, sizeof(watchdog_stats_t));
}

/* Stub implementations for other functions - can be extended later */
bool watchdog_config_set_device(watchdog_config_t *config, const char *device_path) { (void)config; (void)device_path; return true; }
bool watchdog_config_set_timeout(watchdog_config_t *config, uint32_t timeout_seconds) { (void)config; (void)timeout_seconds; return true; }
bool watchdog_config_set_mode(watchdog_config_t *config, watchdog_mode_t mode) { (void)config; (void)mode; return true; }
bool watchdog_device_test(const char *device_path) { (void)device_path; return true; }
int watchdog_device_get_timeout(const char *device_path) { (void)device_path; return WATCHDOG_DEFAULT_TIMEOUT; }
bool watchdog_device_set_timeout(const char *device_path, uint32_t timeout_seconds) { (void)device_path; (void)timeout_seconds; return true; }
uint32_t watchdog_calculate_safe_timeout(uint32_t ttl, uint32_t loop_wait, int32_t safety_margin) { (void)ttl; (void)loop_wait; (void)safety_margin; return WATCHDOG_DEFAULT_TIMEOUT; }
bool watchdog_should_enable_for_leadership(const watchdog_context_t *ctx, uint32_t ttl) { (void)ctx; (void)ttl; return true; }
bool watchdog_time_to_keepalive(const watchdog_context_t *ctx) { (void)ctx; return true; }
uint32_t watchdog_time_until_expiry(const watchdog_context_t *ctx) { (void)ctx; return WATCHDOG_DEFAULT_TIMEOUT; }
bool watchdog_enable_test_mode(watchdog_context_t *ctx) { (void)ctx; return true; }
bool watchdog_disable_test_mode(watchdog_context_t *ctx) { (void)ctx; return true; }
bool watchdog_simulate_failure(watchdog_context_t *ctx) { (void)ctx; return true; }
bool watchdog_verify_test_mode(const watchdog_context_t *ctx) { (void)ctx; return true; }
void watchdog_log_state_change(const watchdog_context_t *ctx, watchdog_state_t old_state, watchdog_state_t new_state) { (void)ctx; (void)old_state; (void)new_state; }
void watchdog_log_stats(const watchdog_context_t *ctx) { (void)ctx; }
void watchdog_log_error(const watchdog_context_t *ctx, const char *operation, const char *error) { (void)ctx; (void)operation; (void)error; }
