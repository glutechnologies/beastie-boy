#pragma once

#include "common/common.h"

#include <stdint.h>

// Declaracions de funcions i estructures per beastie
void beastie_set_log_level(app_log_level_t level);
int beastie_run(const char *output_filename, uint64_t max_capture_bytes, uint32_t memif_id,
		const char *memif_socket_path);
