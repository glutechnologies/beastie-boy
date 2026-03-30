#include "beastie.h"
#include "pcap_writer.h"
#include "common/table.h"

#include <libmemif.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MEMIF_LOCAL_IFNAME "btie0"
#define MAX_MEMIF_BUFS 256
#define PROGRESS_BAR_WIDTH 32
#define PCAP_FLUSH_INTERVAL_BYTES (1024ULL * 1024ULL)

static volatile sig_atomic_t keep_running = 1;
static const char *pcap_filename = BEASTIE_DEFAULT_OUTPUT_FILENAME;
static uint64_t packets_captured = 0;
static uint64_t bytes_captured = 0;
static uint64_t max_bytes_to_capture = 0;
static uint64_t bytes_since_last_flush = 0;
static uint32_t current_memif_id = BEASTIE_DEFAULT_MEMIF_ID;
static const char *current_memif_socket_path = BEASTIE_DEFAULT_MEMIF_SOCKET_PATH;
static app_log_level_t current_log_level = APP_LOG_INFO;

static void beastie_log(app_log_level_t level, const char *format, ...)
{
	va_list args;

	if (level > current_log_level) {
		return;
	}

	fprintf(stdout, "[%s] ", app_log_level_name(level));
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
	fputc('\n', stdout);
}

void beastie_set_log_level(app_log_level_t level)
{
	current_log_level = level;
}

static void beastie_reset_capture_state(const char *output_filename, uint64_t max_capture_bytes,
					uint32_t memif_id, const char *memif_socket_path)
{
	if (output_filename != NULL && output_filename[0] != '\0') {
		pcap_filename = output_filename;
	}
	packets_captured = 0;
	bytes_captured = 0;
	max_bytes_to_capture = max_capture_bytes;
	bytes_since_last_flush = 0;
	current_memif_id = memif_id;
	current_memif_socket_path =
		(memif_socket_path != NULL && memif_socket_path[0] != '\0') ?
		memif_socket_path : BEASTIE_DEFAULT_MEMIF_SOCKET_PATH;
	keep_running = 1;
}

static int beastie_table_add_metric(app_table_t *table, const char *metric, const char *value)
{
	const char *row[2];

	row[0] = metric;
	row[1] = value;
	return app_table_add_row(table, row);
}

static void print_capture_progress(void)
{
	char bar[PROGRESS_BAR_WIDTH + 1];
	uint64_t filled = 0;
	double percent = 0.0;

	if (max_bytes_to_capture > 0) {
		percent = (100.0 * (double)bytes_captured) / (double)max_bytes_to_capture;
		if (percent > 100.0) {
			percent = 100.0;
		}
		filled = (bytes_captured * PROGRESS_BAR_WIDTH) / max_bytes_to_capture;
		if (filled > PROGRESS_BAR_WIDTH) {
			filled = PROGRESS_BAR_WIDTH;
		}
	}

	for (uint64_t i = 0; i < PROGRESS_BAR_WIDTH; i++) {
		bar[i] = (i < filled) ? '#' : '.';
	}
	bar[PROGRESS_BAR_WIDTH] = '\0';

	if (max_bytes_to_capture > 0) {
		printf("\r[%s] %" PRIu64 "/%" PRIu64 " bytes (%.1f%%), packets=%" PRIu64,
		       bar, bytes_captured, max_bytes_to_capture, percent, packets_captured);
	} else {
		printf("\r[%s] %" PRIu64 " bytes, packets=%" PRIu64,
		       bar, bytes_captured, packets_captured);
	}
	fflush(stdout);
}

static void print_capture_summary(void)
{
	app_table_t table;
	const app_table_column_t columns[] = {
		{"Metric", 8, 18, 0, APP_TABLE_ALIGN_LEFT},
		{"Value", 5, 0, 1, APP_TABLE_ALIGN_LEFT},
	};
	char bytes_buffer[32];
	char max_bytes_buffer[32];
	char packets_buffer[32];
	bool table_ok;

	print_capture_progress();
	printf("\n");

	table_ok = (app_table_init(&table, columns, sizeof(columns) / sizeof(columns[0])) == 0);
	if (table_ok) {
		snprintf(packets_buffer, sizeof(packets_buffer), "%" PRIu64, packets_captured);
		table_ok = (beastie_table_add_metric(&table, "Packets", packets_buffer) == 0);
		if (table_ok) {
			table_ok = (beastie_table_add_metric(
					    &table, "Bytes",
					    app_format_bytes_compact(bytes_captured, bytes_buffer,
								   sizeof(bytes_buffer))) == 0);
		}
		if (table_ok) {
			table_ok = (beastie_table_add_metric(&table, "Output", pcap_filename) == 0);
		}
		if (table_ok && max_bytes_to_capture > 0) {
			table_ok = (beastie_table_add_metric(
					    &table, "Max Bytes",
					    app_format_bytes_compact(max_bytes_to_capture,
								   max_bytes_buffer,
								   sizeof(max_bytes_buffer))) == 0);
		}

		if (table_ok) {
			app_table_render(stdout, &table);
		}
		app_table_free(&table);
	}

	beastie_log(APP_LOG_DEBUG,
		    "capture summary: packets=%" PRIu64 " bytes=%" PRIu64 " output=%s",
		    packets_captured, bytes_captured, pcap_filename);
}

static void handle_signal(int signo)
{
	(void)signo;
	keep_running = 0;
}

static int on_connect(memif_conn_handle_t conn, void *private_ctx)
{
	(void)private_ctx;
	beastie_log(APP_LOG_DEBUG,
		    "memif connected: role=slave mode=ethernet socket=%s interface=%s id=%u",
		    current_memif_socket_path, MEMIF_LOCAL_IFNAME, current_memif_id);

	int err = memif_refill_queue(conn, 0, (uint16_t)-1, 0);
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
	(void)private_ctx;
	memif_buffer_t bufs[MAX_MEMIF_BUFS];
	uint16_t rx = 0;
	int err = memif_rx_burst(conn, qid, bufs, MAX_MEMIF_BUFS, &rx);

	if (err != MEMIF_ERR_SUCCESS) {
		beastie_log(APP_LOG_ERROR, "memif_rx_burst: %s", memif_strerror(err));
		return err;
	}

	for (uint16_t i = 0; i < rx; i++) {
		if (max_bytes_to_capture > 0 &&
		    bytes_captured + bufs[i].len > max_bytes_to_capture) {
			keep_running = 0;
			break;
		}

		if (write_pcap_packet(pcap_filename, bufs[i].data, bufs[i].len) != 0) {
			beastie_log(APP_LOG_ERROR, "write_pcap_packet failed for output %s",
				   pcap_filename);
			keep_running = 0;
			return -1;
		}
		packets_captured++;
		bytes_captured += bufs[i].len;
		bytes_since_last_flush += bufs[i].len;

		if (max_bytes_to_capture > 0 && bytes_captured >= max_bytes_to_capture) {
			keep_running = 0;
			break;
		}
	}

	if (rx > 0) {
		print_capture_progress();
	}

	if (bytes_since_last_flush >= PCAP_FLUSH_INTERVAL_BYTES) {
		if (flush_pcap_writer() != 0) {
			beastie_log(APP_LOG_ERROR, "flush_pcap_writer failed for output %s",
				   pcap_filename);
			keep_running = 0;
			return -1;
		}
		bytes_since_last_flush = 0;
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

int beastie_run(const char *output_filename, uint64_t max_capture_bytes, uint32_t memif_id,
		const char *memif_socket_path)
{
	memif_socket_args_t socket_args = {0};
	memif_conn_args_t conn_args = {0};
	memif_socket_handle_t socket = NULL;
	memif_conn_handle_t conn = NULL;
	int err;
	int rc = 1;

	beastie_reset_capture_state(output_filename, max_capture_bytes, memif_id, memif_socket_path);

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	snprintf(socket_args.path, sizeof(socket_args.path), "%s", current_memif_socket_path);
	snprintf(socket_args.app_name, sizeof(socket_args.app_name), "%s", "beastie");
	err = memif_create_socket(&socket, &socket_args, NULL);
	if (err != MEMIF_ERR_SUCCESS) {
		beastie_log(APP_LOG_ERROR, "memif_create_socket: %s", memif_strerror(err));
		goto cleanup;
	}

	conn_args.socket = socket;
	conn_args.interface_id = current_memif_id;
	conn_args.is_master = 0;
	conn_args.mode = MEMIF_INTERFACE_MODE_ETHERNET;
	snprintf((char *)conn_args.interface_name, sizeof(conn_args.interface_name), "%s",
		 MEMIF_LOCAL_IFNAME);

	err = memif_create(&conn, &conn_args, on_connect, on_disconnect, on_interrupt, NULL);
	if (err != MEMIF_ERR_SUCCESS) {
		beastie_log(APP_LOG_ERROR, "memif_create: %s", memif_strerror(err));
		goto cleanup;
	}

	beastie_log(APP_LOG_INFO, "beastie starting");
	beastie_log(APP_LOG_DEBUG,
		    "memif config: role=slave mode=ethernet socket=%s interface=%s id=%u",
		    current_memif_socket_path, MEMIF_LOCAL_IFNAME, current_memif_id);
	beastie_log(APP_LOG_INFO, "capture output: %s", pcap_filename);
	if (max_bytes_to_capture > 0) {
		beastie_log(APP_LOG_DEBUG, "capture limit: %" PRIu64 " bytes", max_bytes_to_capture);
	}
	beastie_log(APP_LOG_DEBUG, "memif id: %u", current_memif_id);
	beastie_log(APP_LOG_DEBUG, "memif socket: %s", current_memif_socket_path);
	beastie_log(APP_LOG_INFO, "waiting for packets on memif...");
	while (keep_running) {
		err = memif_poll_event(socket, -1);
		if (!keep_running) {
			break;
		}
		if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_POLL_CANCEL) {
			beastie_log(APP_LOG_ERROR, "memif_poll_event: %s", memif_strerror(err));
			goto cleanup;
		}
	}

	rc = 0;
	if (flush_pcap_writer() != 0) {
		beastie_log(APP_LOG_ERROR, "flush_pcap_writer failed for output %s", pcap_filename);
		rc = 1;
	}
	print_capture_summary();

cleanup:
	close_pcap_writer();
	if (conn != NULL) {
		memif_delete(&conn);
	}
	if (socket != NULL) {
		memif_delete_socket(&socket);
	}
	return rc;
}
