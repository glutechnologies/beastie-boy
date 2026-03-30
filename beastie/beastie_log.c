#include "beastie_log.h"

#include <stdarg.h>
#include <stdio.h>

static app_log_level_t current_log_level = APP_LOG_INFO;

void beastie_set_log_level(app_log_level_t level)
{
	current_log_level = level;
}

void beastie_log(app_log_level_t level, const char *format, ...)
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
