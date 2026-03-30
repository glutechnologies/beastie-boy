#pragma once

#include "boy.h"

#include <stdint.h>

int boy_show_span(void);
int boy_unset_span(uint32_t source_if_index);
int boy_set_span(uint32_t source_if_index, const char *destination_memif_name,
		 boy_span_device_mode_t device_mode);
