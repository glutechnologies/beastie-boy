#include "boy.h"

#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void print_usage(const char *progname)
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
            "  -u, --unset-span <if-index>\n"
            "      Remove all SPAN entries configured on source interface\n"
            "      with VPP internal sw_if_index=<if-index>.\n"
            "\n"
            "Examples:\n"
            "  %s\n"
            "  %s --show-span\n"
            "  %s --show-memif\n"
            "  %s --unset-span 5\n"
            "  %s --log-level debug\n",
            progname, progname, progname, progname, progname, progname);
}

static int parse_if_index(const char *value, uint32_t *result)
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
    boy_mode_t mode = BOY_MODE_SHOW_VERSION;
    app_log_level_t log_level = APP_LOG_INFO;
    uint32_t unset_span_if_index = 0;
    int opt;
    static const struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"log-level", required_argument, NULL, 'l'},
        {"show-memif", no_argument, NULL, 'm'},
        {"show-span", no_argument, NULL, 's'},
        {"unset-span", required_argument, NULL, 'u'},
        {"show-version", no_argument, NULL, 'v'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "hl:msu:v", long_options, NULL)) != -1) {
        switch (opt) {
        case 'l':
            if (app_parse_log_level(optarg, &log_level) != 0) {
                fprintf(stderr, "Invalid log level: %s\n", optarg);
                return 1;
            }
            break;
        case 'm':
            mode = BOY_MODE_SHOW_MEMIF;
            break;
        case 's':
            mode = BOY_MODE_SHOW_SPAN;
            break;
        case 'u':
            if (parse_if_index(optarg, &unset_span_if_index) != 0) {
                fprintf(stderr, "Invalid interface index: %s\n", optarg);
                return 1;
            }
            mode = BOY_MODE_UNSET_SPAN;
            break;
        case 'v':
            mode = BOY_MODE_SHOW_VERSION;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    return boy_run(mode, log_level, unset_span_if_index);
}
