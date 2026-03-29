#include "boy.h"

#include <getopt.h>
#include <stdint.h>
#include <stdio.h>

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
            "      Query the current SPAN configuration using libvapi.\n"
            "\n"
            "  -m, --show-memif\n"
            "      Query the current memif configuration using libvapi.\n"
            "\n"
            "Examples:\n"
            "  %s\n"
            "  %s --show-span\n"
            "  %s --show-memif\n"
            "  %s --log-level debug\n",
            progname, progname, progname, progname, progname);
}

int main(int argc, char *argv[]) {
    boy_mode_t mode = BOY_MODE_SHOW_VERSION;
    app_log_level_t log_level = APP_LOG_INFO;
    int opt;
    static const struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"log-level", required_argument, NULL, 'l'},
        {"show-memif", no_argument, NULL, 'm'},
        {"show-span", no_argument, NULL, 's'},
        {"show-version", no_argument, NULL, 'v'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "hl:msv", long_options, NULL)) != -1) {
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

    return boy_run(mode, log_level);
}
