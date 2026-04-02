#include "cli_options.h"

#include <getopt.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void print_usage(const char *progname)
{
	char compact_bytes[32];

	fprintf(stderr,
		"Usage:\n"
		"  %s [options]\n"
		"\n"
		"Options:\n"
		"  -h, --help\n"
		"      Show this help message and exit.\n"
		"\n"
		"  -V, --version\n"
		"      Show the Beastie code version derived from Git tags and commit.\n"
		"\n"
		"  -l, --log-level <error|warning|info|debug>\n"
		"      Set the message verbosity.\n"
		"      Default: info\n"
		"\n"
		"  -w, --write <file>\n"
		"      Write captured packets to <file> in PCAP format.\n"
		"      Default: capture.pcap\n"
		"\n"
		"  -m, --max-bytes <bytes>\n"
		"      Stop capturing when the output reaches <bytes> of packet data.\n"
		"      Default: %" PRIu64 " (%s)\n"
		"\n"
		"  -i, --id <memif-id>\n"
		"      Use <memif-id> as the memif peer identifier.\n"
		"      This value must match the memif id configured in VPP.\n"
		"      Default: %u\n"
		"\n"
		"  -s, --socket <path>\n"
		"      Use <path> as the memif socket path.\n"
		"      Default: %s\n"
		"\n"
		"Examples:\n"
		"  %s --log-level debug\n"
		"  %s --write trace.pcap\n"
		"  %s --write trace.pcap --max-bytes 1048576\n"
		"  %s --id 3 --write trace.pcap\n"
		"  %s --socket /run/vpp/custom.sock --write trace.pcap\n",
		progname, (uint64_t)BEASTIE_DEFAULT_MAX_CAPTURE_BYTES,
		app_format_bytes_compact(BEASTIE_DEFAULT_MAX_CAPTURE_BYTES, compact_bytes,
					 sizeof(compact_bytes)),
		BEASTIE_DEFAULT_MEMIF_ID, BEASTIE_DEFAULT_MEMIF_SOCKET_PATH, progname, progname,
		progname, progname, progname);
}

static int parse_max_capture_bytes(const char *value, uint64_t *result)
{
	char *end = NULL;
	unsigned long long parsed;

	parsed = strtoull(value, &end, 10);
	if (value[0] == '\0' || end == value || *end != '\0') {
		return -1;
	}

	*result = (uint64_t)parsed;
	return 0;
}

static int parse_memif_id(const char *value, uint32_t *result)
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

beastie_cli_parse_status_t beastie_parse_cli_options(int argc, char *argv[],
						      beastie_cli_options_t *options)
{
	int opt;
	static const struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"id", required_argument, NULL, 'i'},
		{"log-level", required_argument, NULL, 'l'},
		{"version", no_argument, NULL, 'V'},
		{"socket", required_argument, NULL, 's'},
		{"write", required_argument, NULL, 'w'},
		{"max-bytes", required_argument, NULL, 'm'},
		{0, 0, 0, 0},
	};

	options->mode = BEASTIE_MODE_CAPTURE;
	options->log_level = APP_LOG_INFO;
	options->capture.output_filename = BEASTIE_DEFAULT_OUTPUT_FILENAME;
	options->capture.max_capture_bytes = BEASTIE_DEFAULT_MAX_CAPTURE_BYTES;
	options->capture.memif_id = BEASTIE_DEFAULT_MEMIF_ID;
	options->capture.memif_socket_path = BEASTIE_DEFAULT_MEMIF_SOCKET_PATH;

	while ((opt = getopt_long(argc, argv, "Vhi:l:m:s:w:", long_options, NULL)) != -1) {
		switch (opt) {
		case 'V':
			options->mode = BEASTIE_MODE_SHOW_VERSION;
			break;
		case 'i':
			if (parse_memif_id(optarg, &options->capture.memif_id) != 0) {
				fprintf(stderr, "Invalid memif id: %s\n", optarg);
				return BEASTIE_CLI_PARSE_EXIT_FAILURE;
			}
			break;
		case 'l':
			if (app_parse_log_level(optarg, &options->log_level) != 0) {
				fprintf(stderr, "Invalid log level: %s\n", optarg);
				return BEASTIE_CLI_PARSE_EXIT_FAILURE;
			}
			break;
		case 'm':
			if (parse_max_capture_bytes(optarg, &options->capture.max_capture_bytes) != 0) {
				fprintf(stderr, "Invalid max capture size: %s\n", optarg);
				return BEASTIE_CLI_PARSE_EXIT_FAILURE;
			}
			break;
		case 's':
			if (optarg[0] == '\0') {
				fprintf(stderr, "Invalid memif socket path: %s\n", optarg);
				return BEASTIE_CLI_PARSE_EXIT_FAILURE;
			}
			options->capture.memif_socket_path = optarg;
			break;
		case 'w':
			options->capture.output_filename = optarg;
			break;
		case 'h':
			print_usage(argv[0]);
			return BEASTIE_CLI_PARSE_EXIT_SUCCESS;
		default:
			print_usage(argv[0]);
			return BEASTIE_CLI_PARSE_EXIT_FAILURE;
		}
	}

	return BEASTIE_CLI_PARSE_OK;
}
