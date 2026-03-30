#pragma once

#include "beastie.h"

typedef struct beastie_cli_options {
	beastie_mode_t mode;
	app_log_level_t log_level;
	beastie_capture_config_t capture;
} beastie_cli_options_t;

typedef enum beastie_cli_parse_status {
	BEASTIE_CLI_PARSE_OK = 0,
	BEASTIE_CLI_PARSE_EXIT_SUCCESS = 1,
	BEASTIE_CLI_PARSE_EXIT_FAILURE = 2,
} beastie_cli_parse_status_t;

beastie_cli_parse_status_t beastie_parse_cli_options(int argc, char *argv[],
						      beastie_cli_options_t *options);
