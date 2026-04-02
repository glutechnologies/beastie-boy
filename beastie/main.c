#include "beastie.h"

#include "beastie_log.h"
#include "cli_options.h"
#include "commands/capture_cmd.h"

int beastie_run(beastie_mode_t mode, app_log_level_t log_level,
		const beastie_capture_config_t *capture_config)
{
	beastie_set_log_level(log_level);

	switch (mode) {
	case BEASTIE_MODE_SHOW_VERSION:
		app_print_version(stdout, "beastie");
		return 0;
	case BEASTIE_MODE_CAPTURE:
	default:
		return beastie_run_capture(capture_config);
	}
}

int main(int argc, char *argv[])
{
	beastie_cli_options_t options;
	beastie_cli_parse_status_t parse_status;

	parse_status = beastie_parse_cli_options(argc, argv, &options);
	if (parse_status == BEASTIE_CLI_PARSE_EXIT_SUCCESS) {
		return 0;
	}
	if (parse_status != BEASTIE_CLI_PARSE_OK) {
		return 1;
	}

	return beastie_run(options.mode, options.log_level, &options.capture);
}
