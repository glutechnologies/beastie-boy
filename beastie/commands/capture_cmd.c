#include "commands/capture_cmd.h"

#include "beastie_log.h"
#include "pcap_writer.h"

#include <inttypes.h>
#include <libmemif.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MEMIF_LOCAL_IFNAME "btie0"
#define MAX_MEMIF_BUFS 256
#define PROGRESS_BAR_WIDTH 32
#define PCAP_FLUSH_INTERVAL_BYTES (1024ULL * 1024ULL)

typedef struct beastie_capture_state {
	volatile sig_atomic_t keep_running;
	const char *pcap_filename;
	uint64_t packets_captured;
	uint64_t bytes_captured;
	uint64_t max_bytes_to_capture;
	uint64_t bytes_since_last_flush;
	uint32_t current_memif_id;
	const char *current_memif_socket_path;
} beastie_capture_state_t;

static beastie_capture_state_t *active_capture_state = NULL;

static void print_capture_progress(const beastie_capture_state_t *state)
{
	char bar[PROGRESS_BAR_WIDTH + 1];
	uint64_t filled = 0;
	double percent = 0.0;
	uint64_t i;

	if (state->max_bytes_to_capture > 0) {
		percent = (100.0 * (double)state->bytes_captured) /
			  (double)state->max_bytes_to_capture;
		if (percent > 100.0) {
			percent = 100.0;
		}
		filled = (state->bytes_captured * PROGRESS_BAR_WIDTH) / state->max_bytes_to_capture;
		if (filled > PROGRESS_BAR_WIDTH) {
			filled = PROGRESS_BAR_WIDTH;
		}
	}

	for (i = 0; i < PROGRESS_BAR_WIDTH; i++) {
		bar[i] = (i < filled) ? '#' : '.';
	}
	bar[PROGRESS_BAR_WIDTH] = '\0';

	if (state->max_bytes_to_capture > 0) {
		printf("\r[%s] %" PRIu64 "/%" PRIu64 " bytes (%.1f%%), packets=%" PRIu64, bar,
		       state->bytes_captured, state->max_bytes_to_capture, percent,
		       state->packets_captured);
	} else {
		printf("\r[%s] %" PRIu64 " bytes, packets=%" PRIu64, bar, state->bytes_captured,
		       state->packets_captured);
	}
	fflush(stdout);
}

static void print_capture_summary(const beastie_capture_state_t *state)
{
	print_capture_progress(state);
	printf("\n");
	beastie_log(APP_LOG_INFO, "captured %" PRIu64 " bytes in %" PRIu64 " packets",
		    state->bytes_captured, state->packets_captured);
}

static void handle_signal(int signo)
{
	(void)signo;
	if (active_capture_state != NULL) {
		active_capture_state->keep_running = 0;
	}
}

static int on_connect(memif_conn_handle_t conn, void *private_ctx)
{
	beastie_capture_state_t *state = private_ctx;
	int err;

	beastie_log(APP_LOG_DEBUG,
		    "memif connected: role=slave mode=ethernet socket=%s interface=%s id=%u",
		    state->current_memif_socket_path, MEMIF_LOCAL_IFNAME, state->current_memif_id);

	err = memif_refill_queue(conn, 0, (uint16_t)-1, 0);
	if (err != MEMIF_ERR_SUCCESS) {
		beastie_log(APP_LOG_ERROR, "memif_refill_queue: %s", memif_strerror(err));
		return err;
	}

	return 0;
}

static int on_disconnect(memif_conn_handle_t conn, void *private_ctx)
{
	(void)private_ctx;
	beastie_log(APP_LOG_INFO, "memif disconnected");
	return memif_cancel_poll_event(memif_get_socket_handle(conn));
}

static int on_interrupt(memif_conn_handle_t conn, void *private_ctx, uint16_t qid)
{
	beastie_capture_state_t *state = private_ctx;
	memif_buffer_t bufs[MAX_MEMIF_BUFS];
	uint16_t rx = 0;
	uint16_t i;
	int err = memif_rx_burst(conn, qid, bufs, MAX_MEMIF_BUFS, &rx);

	if (err != MEMIF_ERR_SUCCESS) {
		beastie_log(APP_LOG_ERROR, "memif_rx_burst: %s", memif_strerror(err));
		return err;
	}

	for (i = 0; i < rx; i++) {
		if (state->max_bytes_to_capture > 0 &&
		    state->bytes_captured + bufs[i].len > state->max_bytes_to_capture) {
			state->keep_running = 0;
			break;
		}

		if (write_pcap_packet(state->pcap_filename, bufs[i].data, bufs[i].len) != 0) {
			beastie_log(APP_LOG_ERROR, "write_pcap_packet failed for output %s",
				    state->pcap_filename);
			state->keep_running = 0;
			return -1;
		}
		state->packets_captured++;
		state->bytes_captured += bufs[i].len;
		state->bytes_since_last_flush += bufs[i].len;

		if (state->max_bytes_to_capture > 0 &&
		    state->bytes_captured >= state->max_bytes_to_capture) {
			state->keep_running = 0;
			break;
		}
	}

	if (rx > 0) {
		print_capture_progress(state);
	}

	if (state->bytes_since_last_flush >= PCAP_FLUSH_INTERVAL_BYTES) {
		if (flush_pcap_writer() != 0) {
			beastie_log(APP_LOG_ERROR, "flush_pcap_writer failed for output %s",
				    state->pcap_filename);
			state->keep_running = 0;
			return -1;
		}
		state->bytes_since_last_flush = 0;
	}

	if (rx > 0) {
		err = memif_refill_queue(conn, qid, rx, 0);
		if (err != MEMIF_ERR_SUCCESS) {
			beastie_log(APP_LOG_ERROR, "memif_refill_queue: %s", memif_strerror(err));
			return err;
		}
	}

	return 0;
}

static void beastie_capture_state_init(beastie_capture_state_t *state,
				       const beastie_capture_config_t *config)
{
	state->keep_running = 1;
	state->pcap_filename = (config->output_filename != NULL && config->output_filename[0] != '\0')
				       ? config->output_filename
				       : BEASTIE_DEFAULT_OUTPUT_FILENAME;
	state->packets_captured = 0;
	state->bytes_captured = 0;
	state->max_bytes_to_capture = config->max_capture_bytes;
	state->bytes_since_last_flush = 0;
	state->current_memif_id = config->memif_id;
	state->current_memif_socket_path = (config->memif_socket_path != NULL &&
					    config->memif_socket_path[0] != '\0')
						   ? config->memif_socket_path
						   : BEASTIE_DEFAULT_MEMIF_SOCKET_PATH;
}

int beastie_run_capture(const beastie_capture_config_t *config)
{
	memif_socket_args_t socket_args = {0};
	memif_conn_args_t conn_args = {0};
	memif_socket_handle_t socket = NULL;
	memif_conn_handle_t conn = NULL;
	beastie_capture_state_t state;
	int err;
	int rc = 1;

	beastie_capture_state_init(&state, config);
	active_capture_state = &state;

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	snprintf(socket_args.path, sizeof(socket_args.path), "%s", state.current_memif_socket_path);
	snprintf(socket_args.app_name, sizeof(socket_args.app_name), "%s", "beastie");
	err = memif_create_socket(&socket, &socket_args, NULL);
	if (err != MEMIF_ERR_SUCCESS) {
		beastie_log(APP_LOG_ERROR, "memif_create_socket: %s", memif_strerror(err));
		goto cleanup;
	}

	conn_args.socket = socket;
	conn_args.interface_id = state.current_memif_id;
	conn_args.is_master = 0;
	conn_args.mode = MEMIF_INTERFACE_MODE_ETHERNET;
	snprintf((char *)conn_args.interface_name, sizeof(conn_args.interface_name), "%s",
		 MEMIF_LOCAL_IFNAME);

	err = memif_create(&conn, &conn_args, on_connect, on_disconnect, on_interrupt, &state);
	if (err != MEMIF_ERR_SUCCESS) {
		beastie_log(APP_LOG_ERROR, "memif_create: %s", memif_strerror(err));
		goto cleanup;
	}

	beastie_log(APP_LOG_INFO, "beastie starting");
	beastie_log(APP_LOG_DEBUG,
		    "memif config: role=slave mode=ethernet socket=%s interface=%s id=%u",
		    state.current_memif_socket_path, MEMIF_LOCAL_IFNAME, state.current_memif_id);
	beastie_log(APP_LOG_INFO, "capture output: %s", state.pcap_filename);
	if (state.max_bytes_to_capture > 0) {
		beastie_log(APP_LOG_DEBUG, "capture limit: %" PRIu64 " bytes",
			    state.max_bytes_to_capture);
	}
	beastie_log(APP_LOG_DEBUG, "memif id: %u", state.current_memif_id);
	beastie_log(APP_LOG_DEBUG, "memif socket: %s", state.current_memif_socket_path);
	beastie_log(APP_LOG_INFO, "waiting for packets on memif...");

	while (state.keep_running) {
		err = memif_poll_event(socket, -1);
		if (!state.keep_running) {
			break;
		}
		if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_POLL_CANCEL) {
			beastie_log(APP_LOG_ERROR, "memif_poll_event: %s", memif_strerror(err));
			goto cleanup;
		}
	}

	rc = 0;
	if (flush_pcap_writer() != 0) {
		beastie_log(APP_LOG_ERROR, "flush_pcap_writer failed for output %s",
			    state.pcap_filename);
		rc = 1;
	}
	print_capture_summary(&state);

cleanup:
	active_capture_state = NULL;
	close_pcap_writer();
	if (conn != NULL) {
		memif_delete(&conn);
	}
	if (socket != NULL) {
		memif_delete_socket(&socket);
	}
	return rc;
}
