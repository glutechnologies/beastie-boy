#include "commands/memif_cmd.h"

#include "boy_log.h"
#include "collectors.h"
#include "common/table.h"
#include "vpp_client.h"

#include <vapi/memif.api.vapi.h>

#include <stddef.h>
#include <string.h>

#define BOY_DEFAULT_MEMIF_ID 0U
#define BOY_DEFAULT_MEMIF_SOCKET_ID 0U
#define BOY_DEFAULT_MEMIF_RING_SIZE 1024U
#define BOY_DEFAULT_MEMIF_BUFFER_SIZE 2048U

typedef struct boy_memif_create_result {
	bool received_reply;
	int retval;
	uint32_t sw_if_index;
} boy_memif_create_result_t;

static vapi_error_e boy_memif_create_cb(struct vapi_ctx_s *ctx, void *callback_ctx, vapi_error_e rv,
					bool is_last, vapi_payload_memif_create_reply *reply)
{
	boy_memif_create_result_t *result = callback_ctx;

	(void)ctx;
	(void)is_last;

	if (rv != VAPI_OK) {
		return rv;
	}

	result->received_reply = true;
	result->retval = reply->retval;
	result->sw_if_index = reply->sw_if_index;
	return VAPI_OK;
}

static int boy_add_memif_row(app_table_t *table, const boy_memif_entry_t *entry)
{
	char socket_id_buf[16];
	const char *row[3];

	snprintf(socket_id_buf, sizeof(socket_id_buf), "%u", entry->socket_id);
	row[0] = entry->interface_name;
	row[1] = socket_id_buf;
	row[2] = entry->flags;
	return app_table_add_row(table, row);
}

int boy_show_memif(void)
{
	vapi_ctx_t ctx = NULL;
	boy_memif_table_t memif_entries = {0};
	app_table_t table;
	const app_table_column_t columns[] = {
		{"Interface Name", 14, 0, 1, APP_TABLE_ALIGN_LEFT},
		{"Socket-ID", 9, 12, 0, APP_TABLE_ALIGN_RIGHT},
		{"Flags", 5, 0, 1, APP_TABLE_ALIGN_LEFT},
	};
	int rc = 1;
	size_t i;
	bool table_initialized = false;

	boy_log_start("querying memif state via libvapi");

	if (boy_open_vapi(&ctx) != 0) {
		goto cleanup;
	}

	if (boy_collect_memif(ctx, &memif_entries) != 0) {
		goto cleanup;
	}

	if (app_table_init(&table, columns, sizeof(columns) / sizeof(columns[0])) != 0) {
		boy_log(APP_LOG_ERROR, "failed to initialize table for memif output");
		goto cleanup;
	}
	table_initialized = true;

	for (i = 0; i < memif_entries.count; i++) {
		const boy_memif_entry_t *entry = &memif_entries.entries[i];

		if (boy_add_memif_row(&table, entry) != 0) {
			boy_log(APP_LOG_ERROR, "failed to append memif table row");
			goto cleanup;
		}
	}
	app_table_render(stdout, &table);
	table_initialized = false;
	app_table_free(&table);
	rc = 0;

cleanup:
	if (table_initialized) {
		app_table_free(&table);
	}
	boy_free_memif_table(&memif_entries);
	boy_close_vapi(&ctx);
	return rc;
}

int boy_create_memif(const boy_memif_create_options_t *options)
{
	vapi_ctx_t ctx = NULL;
	vapi_msg_memif_create *msg = NULL;
	boy_memif_create_result_t result = {0};
	vapi_error_e err;
	int rc = 1;

	boy_log_start("creating memif via libvapi");

	if (boy_open_vapi(&ctx) != 0) {
		goto cleanup;
	}

	msg = vapi_alloc_memif_create(ctx);
	if (msg == NULL) {
		boy_log(APP_LOG_ERROR, "vapi_alloc_memif_create failed");
		goto cleanup;
	}

	memset(&msg->payload, 0, sizeof(msg->payload));
	msg->payload.role = MEMIF_ROLE_API_MASTER;
	msg->payload.mode = MEMIF_MODE_API_ETHERNET;
	msg->payload.rx_queues = 1;
	msg->payload.tx_queues = 1;
	msg->payload.id = options->id;
	msg->payload.socket_id = options->socket_id;
	msg->payload.ring_size = options->ring_size;
	msg->payload.buffer_size = options->buffer_size;
	msg->payload.no_zero_copy = false;

	err = vapi_memif_create(ctx, msg, boy_memif_create_cb, &result);
	msg = NULL;
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_memif_create failed: %s", boy_vapi_error_str(err));
		goto cleanup;
	}
	if (!result.received_reply) {
		boy_log(APP_LOG_ERROR, "memif_create returned no reply");
		goto cleanup;
	}
	if (result.retval != 0) {
		boy_log(APP_LOG_ERROR, "memif_create retval=%d", result.retval);
		goto cleanup;
	}

	if (boy_set_interface_admin_up(ctx, result.sw_if_index) != 0) {
		boy_log(APP_LOG_ERROR,
			"memif created but failed to set admin-up: sw_if_index=%u",
			result.sw_if_index);
		goto cleanup;
	}

	boy_log(APP_LOG_INFO,
		"created memif and set admin-up: id=%u socket-id=%u ring-size=%u buffer-size=%u sw_if_index=%u",
		options->id, options->socket_id, options->ring_size, options->buffer_size,
		result.sw_if_index);
	rc = 0;

cleanup:
	boy_close_vapi(&ctx);
	return rc;
}
