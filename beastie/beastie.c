#include "beastie.h"
#include "pcap_writer.h"

#include <memif/libmemif.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PCAP_FILENAME "capture.pcap"
#define MEMIF_SOCKET_PATH "/run/vpp/memif.sock"
#define MEMIF_INTERFACE_NAME "beastie0"
#define MEMIF_ID 0
#define MAX_MEMIF_BUFS 256

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signo)
{
	(void)signo;
	keep_running = 0;
}

static int on_connect(memif_conn_handle_t conn, void *private_ctx)
{
	(void)private_ctx;
	printf("memif connected\n");

	int err = memif_refill_queue(conn, 0, (uint16_t)-1, 0);
	if (err != MEMIF_ERR_SUCCESS) {
		fprintf(stderr, "memif_refill_queue: %s\n", memif_strerror(err));
		return err;
	}

	return 0;
}

static int on_disconnect(memif_conn_handle_t conn, void *private_ctx)
{
	(void)private_ctx;
	printf("memif disconnected\n");
	return memif_cancel_poll_event(memif_get_socket_handle(conn));
}

static int on_interrupt(memif_conn_handle_t conn, void *private_ctx, uint16_t qid)
{
	(void)private_ctx;
	memif_buffer_t bufs[MAX_MEMIF_BUFS];
	uint16_t rx = 0;
	int err = memif_rx_burst(conn, qid, bufs, MAX_MEMIF_BUFS, &rx);

	if (err != MEMIF_ERR_SUCCESS) {
		fprintf(stderr, "memif_rx_burst: %s\n", memif_strerror(err));
		return err;
	}

	for (uint16_t i = 0; i < rx; i++) {
		write_pcap_packet(PCAP_FILENAME, bufs[i].data, bufs[i].len);
	}

	if (rx > 0) {
		err = memif_refill_queue(conn, qid, rx, 0);
		if (err != MEMIF_ERR_SUCCESS) {
			fprintf(stderr, "memif_refill_queue: %s\n", memif_strerror(err));
			return err;
		}
	}

	return 0;
}

int beastie_run(void)
{
	memif_socket_args_t socket_args = {0};
	memif_conn_args_t conn_args = {0};
	memif_socket_handle_t socket = NULL;
	memif_conn_handle_t conn = NULL;
	int err;
	int rc = 1;

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	snprintf(socket_args.path, sizeof(socket_args.path), "%s", MEMIF_SOCKET_PATH);
	snprintf(socket_args.app_name, sizeof(socket_args.app_name), "%s", "beastie");
	err = memif_create_socket(&socket, &socket_args, NULL);
	if (err != MEMIF_ERR_SUCCESS) {
		fprintf(stderr, "memif_create_socket: %s\n", memif_strerror(err));
		goto cleanup;
	}

	conn_args.socket = socket;
	conn_args.interface_id = MEMIF_ID;
	conn_args.is_master = 0;
	conn_args.mode = MEMIF_INTERFACE_MODE_ETHERNET;
	snprintf((char *)conn_args.interface_name, sizeof(conn_args.interface_name), "%s", MEMIF_INTERFACE_NAME);

	err = memif_create(&conn, &conn_args, on_connect, on_disconnect, on_interrupt, NULL);
	if (err != MEMIF_ERR_SUCCESS) {
		fprintf(stderr, "memif_create: %s\n", memif_strerror(err));
		goto cleanup;
	}

	printf("Waiting for packets on memif...\n");
	while (keep_running) {
		err = memif_poll_event(socket, -1);
		if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_POLL_CANCEL) {
			fprintf(stderr, "memif_poll_event: %s\n", memif_strerror(err));
			goto cleanup;
		}
	}

	rc = 0;

cleanup:
	if (conn != NULL) {
		memif_delete(&conn);
	}
	if (socket != NULL) {
		memif_delete_socket(&socket);
	}
	return rc;
}
