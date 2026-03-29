#include "common.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char *app_log_level_name(app_log_level_t level)
{
	switch (level) {
	case APP_LOG_ERROR:
		return "ERROR";
	case APP_LOG_WARNING:
		return "WARNING";
	case APP_LOG_INFO:
		return "INFO";
	case APP_LOG_DEBUG:
		return "DEBUG";
	default:
		return "INFO";
	}
}

int app_parse_log_level(const char *value, app_log_level_t *level)
{
	if (strcmp(value, "error") == 0) {
		*level = APP_LOG_ERROR;
		return 0;
	}
	if (strcmp(value, "warning") == 0) {
		*level = APP_LOG_WARNING;
		return 0;
	}
	if (strcmp(value, "info") == 0) {
		*level = APP_LOG_INFO;
		return 0;
	}
	if (strcmp(value, "debug") == 0) {
		*level = APP_LOG_DEBUG;
		return 0;
	}

	return -1;
}

void app_log_message(FILE *stream, app_log_level_t current_level,
		     app_log_level_t message_level, const char *format, ...)
{
	va_list args;

	if (message_level > current_level) {
		return;
	}

	fprintf(stream, "[%s] ", app_log_level_name(message_level));
	va_start(args, format);
	vfprintf(stream, format, args);
	va_end(args);
	fputc('\n', stream);
}

const char *app_format_bytes_compact(uint64_t bytes, char *buffer, size_t buffer_size)
{
	static const char *units[] = {"bytes", "KiB", "MiB", "GiB", "TiB"};
	double value = (double)bytes;
	size_t unit_index = 0;

	while (value >= 1024.0 && unit_index < (sizeof(units) / sizeof(units[0])) - 1) {
		value /= 1024.0;
		unit_index++;
	}

	if (unit_index == 0) {
		snprintf(buffer, buffer_size, "%" PRIu64 " %s", bytes, units[unit_index]);
		return buffer;
	}

	if (value == (double)((uint64_t)value)) {
		snprintf(buffer, buffer_size, "%" PRIu64 " %s", (uint64_t)value,
			 units[unit_index]);
		return buffer;
	}

	snprintf(buffer, buffer_size, "%.2f %s", value, units[unit_index]);
	return buffer;
}
