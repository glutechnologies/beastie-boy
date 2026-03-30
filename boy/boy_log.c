#include "boy_log.h"

#include <stdarg.h>
#include <stdio.h>

#ifndef BOY_VPP_API_SERIES
#define BOY_VPP_API_SERIES 0
#endif

#ifndef BOY_VPP_API_VERSION_STR
#define BOY_VPP_API_VERSION_STR "unknown"
#endif

static app_log_level_t current_log_level = APP_LOG_INFO;

void boy_set_log_level(app_log_level_t level)
{
	current_log_level = level;
}

void boy_log(app_log_level_t level, const char *format, ...)
{
	va_list args;

	if (level > current_log_level) {
		return;
	}

	fprintf(stdout, "[%s] ", app_log_level_name(level));
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
	fputc('\n', stdout);
}

void boy_log_start(const char *operation)
{
	boy_log(APP_LOG_INFO, "boy starting");
	boy_log(APP_LOG_DEBUG, "build VPP API target: series=%d version=%s",
		BOY_VPP_API_SERIES, BOY_VPP_API_VERSION_STR);
	boy_log(APP_LOG_DEBUG, "%s", operation);
}
