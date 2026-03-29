#pragma once

#include "common/common.h"

#include <stdint.h>

// Declaracions de funcions i estructures per beastie
#define BEASTIE_DEFAULT_OUTPUT_FILENAME "capture.pcap"
#define BEASTIE_DEFAULT_MEMIF_ID 0U
#define BEASTIE_DEFAULT_MEMIF_SOCKET_PATH "/run/vpp/memif.sock"
#define BEASTIE_DEFAULT_MAX_CAPTURE_BYTES (10ULL * 256ULL * 256ULL)

void beastie_set_log_level(app_log_level_t level);
int beastie_run(const char *output_filename, uint64_t max_capture_bytes, uint32_t memif_id,
		const char *memif_socket_path);
