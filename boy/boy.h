#pragma once

#include "common/common.h"

#include <stdint.h>

// Declaracions de funcions i estructures per boy
typedef enum boy_mode {
	BOY_MODE_SHOW_VERSION = 0,
	BOY_MODE_SHOW_SPAN = 1,
	BOY_MODE_SHOW_MEMIF = 2,
	BOY_MODE_UNSET_SPAN = 3,
	BOY_MODE_SET_SPAN = 4,
	BOY_MODE_CREATE_MEMIF = 5,
	BOY_MODE_SHOW_PHY = 6,
	BOY_MODE_SHOW_CODE_VERSION = 7,
} boy_mode_t;

typedef enum boy_span_device_mode {
	BOY_SPAN_DEVICE_RX = 0,
	BOY_SPAN_DEVICE_TX = 1,
	BOY_SPAN_DEVICE_BOTH = 2,
} boy_span_device_mode_t;

typedef struct boy_memif_create_options {
	uint32_t id;
	uint32_t socket_id;
	uint32_t ring_size;
	uint32_t buffer_size;
} boy_memif_create_options_t;

int boy_run(boy_mode_t mode, app_log_level_t log_level, uint32_t span_unset_if_index,
	    uint32_t span_set_if_index, const char *span_set_memif_name,
	    boy_span_device_mode_t span_set_device_mode,
	    const boy_memif_create_options_t *memif_create_options);
