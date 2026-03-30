#pragma once

#include "common/common.h"

void beastie_set_log_level(app_log_level_t level);
void beastie_log(app_log_level_t level, const char *format, ...);
