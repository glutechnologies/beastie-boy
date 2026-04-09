#include "commands/span_cmd.h"

#include "boy_log.h"
#include "collectors.h"
#include "common/table.h"
#include "vpp_client.h"

#include <vapi/span.api.vapi.h>

#include <stddef.h>
#include <string.h>

typedef struct boy_simple_reply_result {
	bool received_reply;
	int retval;
} boy_simple_reply_result_t;

static int boy_add_span_row(app_table_t *table, const boy_interface_entry_t *if_entry,
			    const char *destination, const char *device, const char *l2)
{
	char if_index_buf[16];
	const char *row[7];

	snprintf(if_index_buf, sizeof(if_index_buf), "%u", if_entry->sw_if_index);
	row[0] = if_entry->interface_name;
	row[1] = if_index_buf;
	row[2] = boy_if_type_to_string(if_entry->type);
	row[3] = if_entry->interface_dev_type;
	row[4] = destination;
	row[5] = device;
	row[6] = l2;
	return app_table_add_row(table, row);
}

static vapi_error_e boy_span_enable_disable_cb(
	struct vapi_ctx_s *ctx, void *callback_ctx, vapi_error_e rv, bool is_last,
	vapi_payload_sw_interface_span_enable_disable_reply *reply)
{
	boy_simple_reply_result_t *result = callback_ctx;

	(void)ctx;
	(void)is_last;

	if (rv != VAPI_OK) {
		return rv;
	}

	result->received_reply = true;
	result->retval = reply->retval;
	return VAPI_OK;
}

static int boy_set_span_entry(vapi_ctx_t ctx, u32 sw_if_index_from, u32 sw_if_index_to,
			      vapi_enum_span_state state, bool is_l2)
{
	vapi_msg_sw_interface_span_enable_disable *msg;
	boy_simple_reply_result_t result = {0};
	vapi_error_e err;

	msg = vapi_alloc_sw_interface_span_enable_disable(ctx);
	if (msg == NULL) {
		boy_log(APP_LOG_ERROR, "vapi_alloc_sw_interface_span_enable_disable failed");
		return 1;
	}

	msg->payload.sw_if_index_from = sw_if_index_from;
	msg->payload.sw_if_index_to = sw_if_index_to;
	msg->payload.state = state;
	msg->payload.is_l2 = is_l2;

	err = vapi_sw_interface_span_enable_disable(ctx, msg, boy_span_enable_disable_cb, &result);
	msg = NULL;
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_sw_interface_span_enable_disable failed: %s",
			boy_vapi_error_str(err));
		return 1;
	}

	if (!result.received_reply) {
		boy_log(APP_LOG_ERROR, "sw_interface_span_enable_disable returned no reply");
		return 1;
	}
	if (result.retval != 0) {
		boy_log(APP_LOG_ERROR, "sw_interface_span_enable_disable retval=%d", result.retval);
		return 1;
	}

	return 0;
}

static const boy_interface_entry_t *
boy_find_interface_entry_by_index(const boy_interface_table_t *table, uint32_t if_index)
{
	size_t i;

	for (i = 0; i < table->count; i++) {
		if (table->entries[i].sw_if_index == if_index) {
			return &table->entries[i];
		}
	}

	return NULL;
}

static const boy_interface_entry_t *
boy_find_interface_entry_by_name(const boy_interface_table_t *table, const char *name)
{
	size_t i;

	for (i = 0; i < table->count; i++) {
		if (strcmp(table->entries[i].interface_name, name) == 0) {
			return &table->entries[i];
		}
	}

	return NULL;
}

static vapi_enum_span_state boy_span_state_from_device_mode(boy_span_device_mode_t device_mode)
{
	switch (device_mode) {
	case BOY_SPAN_DEVICE_RX:
		return SPAN_STATE_API_RX;
	case BOY_SPAN_DEVICE_TX:
		return SPAN_STATE_API_TX;
	case BOY_SPAN_DEVICE_BOTH:
	default:
		return SPAN_STATE_API_RX_TX;
	}
}

int boy_show_span(void)
{
	vapi_ctx_t ctx = NULL;
	boy_interface_table_t interfaces = {0};
	boy_span_table_t span_entries = {0};
	app_table_t table;
	const app_table_column_t columns[] = {
		{"Interface Name", 14, 0, 1, APP_TABLE_ALIGN_LEFT},
		{"If-Index", 8, 10, 0, APP_TABLE_ALIGN_RIGHT},
		{"If-Type", 7, 12, 0, APP_TABLE_ALIGN_LEFT},
		{"Dev-Type", 8, 0, 1, APP_TABLE_ALIGN_LEFT},
		{"Destination", 11, 0, 1, APP_TABLE_ALIGN_LEFT},
		{"Device", 6, 8, 0, APP_TABLE_ALIGN_LEFT},
		{"L2", 2, 8, 0, APP_TABLE_ALIGN_LEFT},
	};
	int rc = 1;
	size_t i;
	bool table_initialized = false;

	boy_log_start("querying SPAN candidates and state via libvapi");

	if (boy_open_vapi(&ctx) != 0) {
		goto cleanup;
	}

	if (boy_collect_interfaces(ctx, &interfaces) != 0) {
		goto cleanup;
	}
	if (boy_collect_span(ctx, false, &span_entries) != 0) {
		goto cleanup;
	}
	if (boy_collect_span(ctx, true, &span_entries) != 0) {
		goto cleanup;
	}

	if (app_table_init(&table, columns, sizeof(columns) / sizeof(columns[0])) != 0) {
		boy_log(APP_LOG_ERROR, "failed to initialize table for SPAN output");
		goto cleanup;
	}
	table_initialized = true;

	for (i = 0; i < interfaces.count; i++) {
		const boy_interface_entry_t *if_entry = &interfaces.entries[i];
		size_t j;
		bool found = false;

		if (!boy_is_span_source_candidate(if_entry)) {
			continue;
		}

		for (j = 0; j < span_entries.count; j++) {
			const boy_span_entry_t *span_entry = &span_entries.entries[j];
			const char *dst;
			const char *device_state;

			if (span_entry->sw_if_index_from != if_entry->sw_if_index) {
				continue;
			}

			dst = boy_find_interface_name(&interfaces, span_entry->sw_if_index_to);
			device_state =
				span_entry->is_l2 ? "*" : boy_span_state_device(span_entry->state);
			if (boy_add_span_row(&table, if_entry, dst, device_state,
					     boy_span_state_l2(span_entry->state,
							      span_entry->is_l2)) != 0) {
				boy_log(APP_LOG_ERROR, "failed to append SPAN table row");
				goto cleanup;
			}
			found = true;
		}

		if (!found) {
			if (boy_add_span_row(&table, if_entry, "*", "*", "*") != 0) {
				boy_log(APP_LOG_ERROR, "failed to append SPAN table row");
				goto cleanup;
			}
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
	boy_free_span_table(&span_entries);
	boy_free_interface_table(&interfaces);
	boy_close_vapi(&ctx);
	return rc;
}

int boy_unset_span(uint32_t source_if_index)
{
	vapi_ctx_t ctx = NULL;
	boy_interface_table_t interfaces = {0};
	boy_span_table_t span_entries = {0};
	const char *source_name = "<unknown>";
	int rc = 1;
	size_t i;
	size_t removed = 0;

	boy_log_start("removing SPAN entries via libvapi");

	if (boy_open_vapi(&ctx) != 0) {
		goto cleanup;
	}

	if (boy_collect_interfaces(ctx, &interfaces) != 0) {
		goto cleanup;
	}
	if (boy_collect_span(ctx, false, &span_entries) != 0) {
		goto cleanup;
	}
	if (boy_collect_span(ctx, true, &span_entries) != 0) {
		goto cleanup;
	}
	source_name = boy_find_interface_name(&interfaces, source_if_index);

	for (i = 0; i < span_entries.count; i++) {
		const boy_span_entry_t *entry = &span_entries.entries[i];
		const char *src;
		const char *dst;

		if (entry->sw_if_index_from != source_if_index) {
			continue;
		}

		src = boy_find_interface_name(&interfaces, entry->sw_if_index_from);
		dst = boy_find_interface_name(&interfaces, entry->sw_if_index_to);
		boy_log(APP_LOG_DEBUG, "disabling SPAN: src=%s(%u) dst=%s(%u) l2=%s", src,
			entry->sw_if_index_from, dst, entry->sw_if_index_to,
			entry->is_l2 ? "yes" : "no");

		if (boy_set_span_entry(ctx, entry->sw_if_index_from, entry->sw_if_index_to,
				       SPAN_STATE_API_DISABLED, entry->is_l2) != 0) {
			goto cleanup;
		}
		removed++;
	}

	if (removed == 0) {
		boy_log(APP_LOG_WARNING, "no SPAN entries found for source if-index=%u (%s)",
			source_if_index, source_name);
	} else {
		boy_log(APP_LOG_INFO, "removed %zu SPAN entr%s for source if-index=%u (%s)",
			removed, (removed == 1) ? "y" : "ies", source_if_index, source_name);
	}

	rc = 0;

cleanup:
	boy_free_span_table(&span_entries);
	boy_free_interface_table(&interfaces);
	boy_close_vapi(&ctx);
	return rc;
}

int boy_set_span(uint32_t source_if_index, const char *destination_memif_name,
		 boy_span_device_mode_t device_mode)
{
	vapi_ctx_t ctx = NULL;
	boy_interface_table_t interfaces = {0};
	const boy_interface_entry_t *source_entry;
	const boy_interface_entry_t *destination_entry;
	vapi_enum_span_state state;
	int rc = 1;

	boy_log_start("setting SPAN entry via libvapi");

	if (boy_open_vapi(&ctx) != 0) {
		goto cleanup;
	}
	if (boy_collect_interfaces(ctx, &interfaces) != 0) {
		goto cleanup;
	}

	source_entry = boy_find_interface_entry_by_index(&interfaces, source_if_index);
	if (source_entry == NULL) {
		boy_log(APP_LOG_ERROR, "source interface if-index=%u not found", source_if_index);
		goto cleanup;
	}

	destination_entry = boy_find_interface_entry_by_name(&interfaces, destination_memif_name);
	if (destination_entry == NULL) {
		boy_log(APP_LOG_ERROR, "destination interface '%s' not found", destination_memif_name);
		goto cleanup;
	}
	if (strncmp(destination_entry->interface_name, "memif", 5) != 0) {
		boy_log(APP_LOG_ERROR, "destination interface '%s' is not a memif",
			destination_entry->interface_name);
		goto cleanup;
	}
	if (source_entry->sw_if_index == destination_entry->sw_if_index) {
		boy_log(APP_LOG_ERROR, "source and destination interfaces must be different");
		goto cleanup;
	}

	if ((source_entry->flags & IF_STATUS_API_FLAG_ADMIN_UP) == 0) {
		boy_log(APP_LOG_INFO, "source interface %s(%u) is admin-down, setting it admin-up",
			source_entry->interface_name, source_entry->sw_if_index);
		if (boy_set_interface_admin_up(ctx, source_entry->sw_if_index) != 0) {
			goto cleanup;
		}
	}

	if ((destination_entry->flags & IF_STATUS_API_FLAG_ADMIN_UP) == 0) {
		boy_log(APP_LOG_INFO,
			"destination interface %s(%u) is admin-down, setting it admin-up",
			destination_entry->interface_name, destination_entry->sw_if_index);
		if (boy_set_interface_admin_up(ctx, destination_entry->sw_if_index) != 0) {
			goto cleanup;
		}
	}

	state = boy_span_state_from_device_mode(device_mode);
	boy_log(APP_LOG_INFO,
		"setting SPAN: src=%s(%u) dst=%s(%u) device=%s l2=no",
		source_entry->interface_name, source_entry->sw_if_index,
		destination_entry->interface_name, destination_entry->sw_if_index,
		boy_span_state_device(state));

	if (boy_set_span_entry(ctx, source_entry->sw_if_index, destination_entry->sw_if_index, state,
			       false) != 0) {
		goto cleanup;
	}

	boy_log(APP_LOG_INFO, "SPAN entry configured successfully");
	rc = 0;

cleanup:
	boy_free_interface_table(&interfaces);
	boy_close_vapi(&ctx);
	return rc;
}
