/*-------------------------------------------------------------------------
 *
 * log.c
 *		Internal logging system implementation.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include "log.h"

#define MAX_LOG_MESSAGE 1024
#define MAX_TIMESTAMP 64

static log_callback_t log_callback = NULL;
static log_level_t current_level = LOG_INFO;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE *log_file = NULL;
static char log_file_path[512] = "";

static void write_to_file(const char *message);
static void write_to_stderr(const char *message);

void
log_set_level(log_level_t level)
{
	current_level = level;
}

log_callback_t
log_set_callback(log_callback_t callback)
{
	log_callback_t old_callback = log_callback;
	log_callback = callback;
	return old_callback;
}

void
log_set_file(const char *file_path)
{
	if (file_path == NULL)
	{
		return;
	}

	pthread_mutex_lock(&log_mutex);
	if (log_file != NULL)
	{
		fclose(log_file);
		log_file = NULL;
	}

	strlcpy(log_file_path, file_path, sizeof(log_file_path));

	log_file = fopen(file_path, "a");
	pthread_mutex_unlock(&log_mutex);
}

void
log_error(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	log_internal(LOG_ERROR, "ERROR", format, args);
	va_end(args);
}

void
log_warning(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	log_internal(LOG_WARNING, "WARNING", format, args);
	va_end(args);
}

void
log_info(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	log_internal(LOG_INFO, "INFO", format, args);
	va_end(args);
}

void
log_debug(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	log_internal(LOG_DEBUG, "DEBUG", format, args);
	va_end(args);
}

void
log_cleanup(void)
{
	pthread_mutex_lock(&log_mutex);
	if (log_file != NULL)
	{
		fclose(log_file);
		log_file = NULL;
	}
	pthread_mutex_unlock(&log_mutex);
}

void
log_internal(log_level_t level, const char *prefix, const char *format, ...)
{
	char message[MAX_LOG_MESSAGE];
	char timestamp[MAX_TIMESTAMP];
	time_t now;
	struct tm *tm_info;
	size_t written __attribute__((unused));
	va_list args;

	if (level > current_level)
	{
		return;
	}

	now = time(NULL);
	tm_info = localtime(&now);
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

        written = (size_t)snprintf(message, sizeof(message), "[%s] %s: ", timestamp, prefix);
	if (written < sizeof(message))
	{
		va_start(args, format);
		vsnprintf(message + written, sizeof(message) - written, format, args);
		va_end(args);
	}

	if (log_callback != NULL)
	{
		log_callback(level, message);
	}
	else
	{
		write_to_stderr(message);
		write_to_file(message);
	}
}

static void
write_to_file(const char *message)
{
	if (log_file == NULL)
	{
		return;
	}

	pthread_mutex_lock(&log_mutex);
	fputs(message, log_file);
	fputs("\n", log_file);
	fflush(log_file);
	pthread_mutex_unlock(&log_mutex);
}

static void
write_to_stderr(const char *message)
{
	fputs(message, stderr);
	fputs("\n", stderr);
}

void
report_hint(log_level_t level, const char *prefix, const char *hint, const char *format, ...)
{
	/* Convert to simple log call - ignore hint parameter */
	va_list args;
	va_start(args, format);
	log_internal(level, prefix, format, args);
	va_end(args);
}

void
report_internal(log_level_t level, const char *prefix, const char *detail, const char *hint, const char *format, ...)
{
	/* Convert to simple log call - ignore detail and hint parameters */
	va_list args;
	va_start(args, format);
	log_internal(level, prefix, format, args);
	va_end(args);
}
