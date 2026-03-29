#include "beastie.h"

#include <stdint.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
            app_format_bytes_compact(BEASTIE_DEFAULT_MAX_CAPTURE_BYTES, compact_bytes, sizeof(compact_bytes)),
            BEASTIE_DEFAULT_MEMIF_ID,
            BEASTIE_DEFAULT_MEMIF_SOCKET_PATH,
            progname, progname, progname, progname, progname);
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

int main(int argc, char *argv[]) {
    const char *output_filename = BEASTIE_DEFAULT_OUTPUT_FILENAME;
    uint64_t max_capture_bytes = BEASTIE_DEFAULT_MAX_CAPTURE_BYTES;
    uint32_t memif_id = BEASTIE_DEFAULT_MEMIF_ID;
    const char *memif_socket_path = BEASTIE_DEFAULT_MEMIF_SOCKET_PATH;
    app_log_level_t log_level = APP_LOG_INFO;
    int opt;
    static const struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"id", required_argument, NULL, 'i'},
        {"log-level", required_argument, NULL, 'l'},
        {"socket", required_argument, NULL, 's'},
        {"write", required_argument, NULL, 'w'},
        {"max-bytes", required_argument, NULL, 'm'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "hi:l:m:s:w:", long_options, NULL)) != -1) {
        switch (opt) {
        case 'i':
            if (parse_memif_id(optarg, &memif_id) != 0) {
                fprintf(stderr, "Invalid memif id: %s\n", optarg);
                return 1;
            }
            break;
        case 'l':
            if (app_parse_log_level(optarg, &log_level) != 0) {
                fprintf(stderr, "Invalid log level: %s\n", optarg);
                return 1;
            }
            break;
        case 'm':
            if (parse_max_capture_bytes(optarg, &max_capture_bytes) != 0) {
                fprintf(stderr, "Invalid max capture size: %s\n", optarg);
                return 1;
            }
            break;
        case 's':
            if (optarg[0] == '\0') {
                fprintf(stderr, "Invalid memif socket path: %s\n", optarg);
                return 1;
            }
            memif_socket_path = optarg;
            break;
        case 'w':
            output_filename = optarg;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    beastie_set_log_level(log_level);
    return beastie_run(output_filename, max_capture_bytes, memif_id, memif_socket_path);
}
