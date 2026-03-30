#pragma once

#include "boy.h"

typedef struct boy_cli_options {
	boy_mode_t mode;
	app_log_level_t log_level;
	uint32_t unset_span_if_index;
	uint32_t set_span_if_index;
	const char *set_span_memif_name;
	boy_span_device_mode_t set_span_device_mode;
} boy_cli_options_t;

typedef enum boy_cli_parse_status {
	BOY_CLI_PARSE_OK = 0,
	BOY_CLI_PARSE_EXIT_SUCCESS = 1,
	BOY_CLI_PARSE_EXIT_FAILURE = 2,
} boy_cli_parse_status_t;

boy_cli_parse_status_t boy_parse_cli_options(int argc, char *argv[],
					      boy_cli_options_t *options);
