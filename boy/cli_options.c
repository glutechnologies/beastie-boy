#include "cli_options.h"

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void boy_print_usage(const char *progname)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s [options]\n"
		"\n"
		"Options:\n"
		"  -h, --help\n"
		"      Show this help message and exit.\n"
		"\n"
		"  -l, --log-level <error|warning|info|debug>\n"
		"      Set the message verbosity.\n"
		"      Default: info\n"
		"\n"
		"  -v, --show-version\n"
		"      Query the current VPP version using libvapi.\n"
		"      This is the default action if no option is given.\n"
		"\n"
		"  -s, --show-span\n"
		"      List SPAN/mirror candidate interfaces and current state.\n"
		"\n"
		"  -m, --show-memif\n"
		"      Query the current memif configuration using libvapi.\n"
		"\n"
		"  -p, --show-phy\n"
		"      Query physical interface details via libvapi.\n"
		"\n"
		"  -c, --create-memif\n"
		"      Create a default memif interface only if no memif exists.\n"
		"\n"
		"  -u, --unset-span <if-index>\n"
		"      Remove all SPAN entries configured on source interface\n"
		"      with VPP internal sw_if_index=<if-index>.\n"
		"\n"
		"      --set-span --iface-idx <if-index> --memif <name> --device <both|rx|tx>\n"
		"      Set SPAN from source interface <if-index> to destination memif\n"
		"      interface <name> with selected device direction.\n"
		"\n"
		"Examples:\n"
		"  %s\n"
		"  %s --show-span\n"
		"  %s --show-memif\n"
		"  %s --show-phy\n"
		"  %s --create-memif\n"
		"  %s --unset-span 5\n"
		"  %s --set-span --iface-idx 1 --memif memif0/0 --device both\n"
		"  %s --log-level debug\n",
		progname, progname, progname, progname, progname, progname, progname, progname,
		progname);
}

static int boy_parse_if_index(const char *value, uint32_t *result)
{
	char *end = NULL;
	unsigned long parsed;

	parsed = strtoul(value, &end, 10);
	if (value[0] == '\0' || end == value || *end != '\0') {
		return -1;
	}

	*result = (uint32_t)parsed;
	return 0;
}

static int boy_parse_device_mode(const char *value, boy_span_device_mode_t *result)
{
	if (strcmp(value, "rx") == 0) {
		*result = BOY_SPAN_DEVICE_RX;
		return 0;
	}
	if (strcmp(value, "tx") == 0) {
		*result = BOY_SPAN_DEVICE_TX;
		return 0;
	}
	if (strcmp(value, "both") == 0) {
		*result = BOY_SPAN_DEVICE_BOTH;
		return 0;
	}

	return -1;
}

boy_cli_parse_status_t boy_parse_cli_options(int argc, char *argv[], boy_cli_options_t *options)
{
	int opt;
	bool has_set_span = false;
	bool has_set_span_iface = false;
	bool has_set_span_memif = false;
	bool has_set_span_device = false;
	static const struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"iface-idx", required_argument, NULL, 'I'},
		{"log-level", required_argument, NULL, 'l'},
		{"memif", required_argument, NULL, 'M'},
		{"device", required_argument, NULL, 'D'},
		{"create-memif", no_argument, NULL, 'c'},
		{"show-memif", no_argument, NULL, 'm'},
		{"show-phy", no_argument, NULL, 'p'},
		{"show-span", no_argument, NULL, 's'},
		{"set-span", no_argument, NULL, 'S'},
		{"unset-span", required_argument, NULL, 'u'},
		{"show-version", no_argument, NULL, 'v'},
		{0, 0, 0, 0},
	};

	options->mode = BOY_MODE_SHOW_VERSION;
	options->log_level = APP_LOG_INFO;
	options->unset_span_if_index = 0;
	options->set_span_if_index = 0;
	options->set_span_memif_name = NULL;
	options->set_span_device_mode = BOY_SPAN_DEVICE_BOTH;

	while ((opt = getopt_long(argc, argv, "chI:l:D:M:mpsSu:v", long_options, NULL)) != -1) {
		switch (opt) {
		case 'c':
			options->mode = BOY_MODE_CREATE_MEMIF;
			break;
		case 'I':
			if (boy_parse_if_index(optarg, &options->set_span_if_index) != 0) {
				fprintf(stderr, "Invalid source interface index: %s\n", optarg);
				return BOY_CLI_PARSE_EXIT_FAILURE;
			}
			has_set_span_iface = true;
			break;
		case 'l':
			if (app_parse_log_level(optarg, &options->log_level) != 0) {
				fprintf(stderr, "Invalid log level: %s\n", optarg);
				return BOY_CLI_PARSE_EXIT_FAILURE;
			}
			break;
		case 'D':
			if (boy_parse_device_mode(optarg, &options->set_span_device_mode) != 0) {
				fprintf(stderr, "Invalid device mode: %s (expected both|rx|tx)\n",
					optarg);
				return BOY_CLI_PARSE_EXIT_FAILURE;
			}
			has_set_span_device = true;
			break;
		case 'M':
			if (optarg[0] == '\0') {
				fprintf(stderr, "Invalid memif interface name: %s\n", optarg);
				return BOY_CLI_PARSE_EXIT_FAILURE;
			}
			options->set_span_memif_name = optarg;
			has_set_span_memif = true;
			break;
		case 'm':
			options->mode = BOY_MODE_SHOW_MEMIF;
			break;
		case 'p':
			options->mode = BOY_MODE_SHOW_PHY;
			break;
		case 's':
			options->mode = BOY_MODE_SHOW_SPAN;
			break;
		case 'S':
			options->mode = BOY_MODE_SET_SPAN;
			has_set_span = true;
			break;
		case 'u':
			if (boy_parse_if_index(optarg, &options->unset_span_if_index) != 0) {
				fprintf(stderr, "Invalid interface index: %s\n", optarg);
				return BOY_CLI_PARSE_EXIT_FAILURE;
			}
			options->mode = BOY_MODE_UNSET_SPAN;
			break;
		case 'v':
			options->mode = BOY_MODE_SHOW_VERSION;
			break;
		case 'h':
			boy_print_usage(argv[0]);
			return BOY_CLI_PARSE_EXIT_SUCCESS;
		default:
			boy_print_usage(argv[0]);
			return BOY_CLI_PARSE_EXIT_FAILURE;
		}
	}

	if (!has_set_span && (has_set_span_iface || has_set_span_memif || has_set_span_device)) {
		fprintf(stderr,
			"--iface-idx/--memif/--device require --set-span\n");
		return BOY_CLI_PARSE_EXIT_FAILURE;
	}

	if (has_set_span &&
	    (!has_set_span_iface || !has_set_span_memif || !has_set_span_device)) {
		fprintf(stderr,
			"--set-span requires --iface-idx, --memif and --device\n");
		return BOY_CLI_PARSE_EXIT_FAILURE;
	}

	return BOY_CLI_PARSE_OK;
}
