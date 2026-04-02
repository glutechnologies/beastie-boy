#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

typedef enum app_log_level {
	APP_LOG_ERROR = 0,
	APP_LOG_WARNING = 1,
	APP_LOG_INFO = 2,
	APP_LOG_DEBUG = 3,
} app_log_level_t;

const char *app_log_level_name(app_log_level_t level);
int app_parse_log_level(const char *value, app_log_level_t *level);
void app_log_message(FILE *stream, app_log_level_t current_level,
		     app_log_level_t message_level, const char *format, ...);
const char *app_format_bytes_compact(uint64_t bytes, char *buffer, size_t buffer_size);
const char *app_git_version(void);
void app_print_version(FILE *stream, const char *app_name);
