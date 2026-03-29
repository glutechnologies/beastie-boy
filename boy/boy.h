#pragma once

#include "common/common.h"

// Declaracions de funcions i estructures per boy
typedef enum boy_mode {
	BOY_MODE_SHOW_VERSION = 0,
	BOY_MODE_SHOW_SPAN = 1,
	BOY_MODE_SHOW_MEMIF = 2,
} boy_mode_t;

int boy_run(boy_mode_t mode, app_log_level_t log_level);
