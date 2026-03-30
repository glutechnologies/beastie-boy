#pragma once

#include "common/common.h"

void boy_set_log_level(app_log_level_t level);
void boy_log(app_log_level_t level, const char *format, ...);
void boy_log_start(const char *operation);
