#include "boy.h"

#include "boy_log.h"
#include "cli_options.h"
#include "commands/memif_cmd.h"
#include "commands/phy_cmd.h"
#include "commands/span_cmd.h"
#include "commands/version_cmd.h"

int boy_run(boy_mode_t mode, app_log_level_t log_level, uint32_t span_unset_if_index,
	    uint32_t span_set_if_index, const char *span_set_memif_name,
	    boy_span_device_mode_t span_set_device_mode,
	    const boy_memif_create_options_t *memif_create_options)
{
	boy_set_log_level(log_level);

	switch (mode) {
	case BOY_MODE_SHOW_CODE_VERSION:
		app_print_version(stdout, "boy");
		return 0;
	case BOY_MODE_SET_SPAN:
		return boy_set_span(span_set_if_index, span_set_memif_name, span_set_device_mode);
	case BOY_MODE_UNSET_SPAN:
		return boy_unset_span(span_unset_if_index);
	case BOY_MODE_SHOW_MEMIF:
		return boy_show_memif();
	case BOY_MODE_CREATE_MEMIF:
		return boy_create_memif(memif_create_options);
	case BOY_MODE_SHOW_PHY:
		return boy_show_phy();
	case BOY_MODE_SHOW_SPAN:
		return boy_show_span();
	case BOY_MODE_SHOW_VERSION:
	default:
		return boy_show_version();
	}
}

int main(int argc, char *argv[])
{
	boy_cli_options_t options;
	boy_cli_parse_status_t parse_status;

	parse_status = boy_parse_cli_options(argc, argv, &options);
	if (parse_status == BOY_CLI_PARSE_EXIT_SUCCESS) {
		return 0;
	}
	if (parse_status != BOY_CLI_PARSE_OK) {
		return 1;
	}

	return boy_run(options.mode, options.log_level, options.unset_span_if_index,
		       options.set_span_if_index, options.set_span_memif_name,
		       options.set_span_device_mode, &options.memif_create);
}
