#pragma once

#include "common/common.h"

#include <stdint.h>

#define BEASTIE_DEFAULT_OUTPUT_FILENAME "capture.pcap"
#define BEASTIE_DEFAULT_MEMIF_ID 0U
#define BEASTIE_DEFAULT_MEMIF_SOCKET_PATH "/run/vpp/memif.sock"
#define BEASTIE_DEFAULT_MAX_CAPTURE_BYTES (10ULL * 256ULL * 256ULL)

typedef enum beastie_mode {
	BEASTIE_MODE_CAPTURE = 0,
} beastie_mode_t;

typedef struct beastie_capture_config {
	const char *output_filename;
	uint64_t max_capture_bytes;
	uint32_t memif_id;
	const char *memif_socket_path;
} beastie_capture_config_t;

int beastie_run(beastie_mode_t mode, app_log_level_t log_level,
		const beastie_capture_config_t *capture_config);
