#include "collectors.h"

#include "boy_log.h"
#include "vpp_client.h"

#include <vapi/interface.api.vapi.h>
#include <vapi/memif.api.vapi.h>
#include <vapi/span.api.vapi.h>

#include <stdlib.h>
#include <string.h>

static int boy_reserve_entries(void **items, size_t *capacity, size_t item_size,
			       size_t required_count)
{
	size_t new_capacity;
	void *new_items;

	if (*capacity >= required_count) {
		return 0;
	}

	new_capacity = (*capacity == 0) ? 8 : (*capacity * 2);
	while (new_capacity < required_count) {
		new_capacity *= 2;
	}

	new_items = realloc(*items, new_capacity * item_size);
	if (new_items == NULL) {
		return -1;
	}

	*items = new_items;
	*capacity = new_capacity;
	return 0;
}

void boy_free_interface_table(boy_interface_table_t *table)
{
	free(table->entries);
	table->entries = NULL;
	table->count = 0;
	table->capacity = 0;
}

void boy_free_span_table(boy_span_table_t *table)
{
	free(table->entries);
	table->entries = NULL;
	table->count = 0;
	table->capacity = 0;
}

void boy_free_memif_table(boy_memif_table_t *table)
{
	free(table->entries);
	table->entries = NULL;
	table->count = 0;
	table->capacity = 0;
}

const char *boy_find_interface_name(const boy_interface_table_t *table, u32 sw_if_index)
{
	size_t i;

	for (i = 0; i < table->count; i++) {
		if (table->entries[i].sw_if_index == sw_if_index) {
			return table->entries[i].interface_name;
		}
	}

	return "<unknown>";
}

const char *boy_if_type_to_string(vapi_enum_if_type type)
{
	switch (type) {
	case IF_API_TYPE_HARDWARE:
		return "hardware";
	case IF_API_TYPE_SUB:
		return "sub";
	case IF_API_TYPE_P2P:
		return "p2p";
	case IF_API_TYPE_PIPE:
		return "pipe";
	default:
		return "unknown";
	}
}

bool boy_is_span_source_candidate(const boy_interface_entry_t *entry)
{
	const bool is_hardware = (entry->type == IF_API_TYPE_HARDWARE);
	const bool is_vlan_subif = (entry->type == IF_API_TYPE_SUB) &&
				   (strchr(entry->interface_name, '.') != NULL);

	if (!is_hardware && !is_vlan_subif) {
		return false;
	}
	if (strcmp(entry->interface_name, "local0") == 0) {
		return false;
	}
	if (strncmp(entry->interface_name, "memif", 5) == 0) {
		return false;
	}

	return true;
}

const char *boy_span_state_device(vapi_enum_span_state state)
{
	switch (state) {
	case SPAN_STATE_API_RX:
		return "rx";
	case SPAN_STATE_API_TX:
		return "tx";
	case SPAN_STATE_API_RX_TX:
		return "both";
	case SPAN_STATE_API_DISABLED:
	default:
		return "none";
	}
}

const char *boy_span_state_l2(vapi_enum_span_state state, bool is_l2)
{
	if (!is_l2) {
		return "*";
	}

	return boy_span_state_device(state);
}

static const char *boy_memif_flags_to_string(vapi_enum_if_status_flags flags, char *buffer,
					      size_t buffer_size)
{
	const bool admin_up = (flags & IF_STATUS_API_FLAG_ADMIN_UP) != 0;
	const bool link_up = (flags & IF_STATUS_API_FLAG_LINK_UP) != 0;

	if (admin_up && link_up) {
		snprintf(buffer, buffer_size, "admin-up,link-up");
	} else if (admin_up) {
		snprintf(buffer, buffer_size, "admin-up");
	} else if (link_up) {
		snprintf(buffer, buffer_size, "link-up");
	} else {
		snprintf(buffer, buffer_size, "none");
	}

	return buffer;
}

static vapi_error_e boy_sw_interface_dump_cb(struct vapi_ctx_s *ctx, void *callback_ctx,
					     vapi_error_e rv, bool is_last,
					     vapi_payload_sw_interface_details *reply)
{
	boy_interface_table_t *table = callback_ctx;
	boy_interface_entry_t *entry;

	(void)ctx;

	if (rv != VAPI_OK) {
		return rv;
	}
	if (is_last || reply == NULL) {
		return VAPI_OK;
	}

	if (boy_reserve_entries((void **)&table->entries, &table->capacity,
				sizeof(*table->entries), table->count + 1) != 0) {
		return VAPI_ENOMEM;
	}

	entry = &table->entries[table->count++];
	entry->sw_if_index = reply->sw_if_index;
	entry->type = reply->type;
	boy_copy_api_string(entry->interface_name, sizeof(entry->interface_name),
			    reply->interface_name, sizeof(reply->interface_name));
	boy_copy_api_string(entry->interface_dev_type, sizeof(entry->interface_dev_type),
			    reply->interface_dev_type, sizeof(reply->interface_dev_type));
	return VAPI_OK;
}

static vapi_error_e boy_span_dump_cb(struct vapi_ctx_s *ctx, void *callback_ctx,
				     vapi_error_e rv, bool is_last,
				     vapi_payload_sw_interface_span_details *reply)
{
	boy_span_table_t *table = callback_ctx;
	boy_span_entry_t *entry;

	(void)ctx;

	if (rv != VAPI_OK) {
		return rv;
	}
	if (is_last || reply == NULL) {
		return VAPI_OK;
	}

	if (boy_reserve_entries((void **)&table->entries, &table->capacity,
				sizeof(*table->entries), table->count + 1) != 0) {
		return VAPI_ENOMEM;
	}

	entry = &table->entries[table->count++];
	entry->sw_if_index_from = reply->sw_if_index_from;
	entry->sw_if_index_to = reply->sw_if_index_to;
	entry->state = reply->state;
	entry->is_l2 = reply->is_l2;
	return VAPI_OK;
}

static vapi_error_e boy_memif_dump_cb(struct vapi_ctx_s *ctx, void *callback_ctx,
				      vapi_error_e rv, bool is_last,
				      vapi_payload_memif_details *reply)
{
	boy_memif_table_t *table = callback_ctx;
	boy_memif_entry_t *entry;

	(void)ctx;

	if (rv != VAPI_OK) {
		return rv;
	}
	if (is_last || reply == NULL) {
		return VAPI_OK;
	}

	if (boy_reserve_entries((void **)&table->entries, &table->capacity,
				sizeof(*table->entries), table->count + 1) != 0) {
		return VAPI_ENOMEM;
	}

	entry = &table->entries[table->count++];
	boy_copy_api_string(entry->interface_name, sizeof(entry->interface_name), reply->if_name,
			    sizeof(reply->if_name));
	entry->socket_id = reply->socket_id;
	boy_memif_flags_to_string(reply->flags, entry->flags, sizeof(entry->flags));
	return VAPI_OK;
}

int boy_collect_interfaces(vapi_ctx_t ctx, boy_interface_table_t *table)
{
	vapi_msg_sw_interface_dump *msg;
	vapi_error_e err;

	msg = vapi_alloc_sw_interface_dump(ctx, 0);
	if (msg == NULL) {
		boy_log(APP_LOG_ERROR, "vapi_alloc_sw_interface_dump failed");
		return 1;
	}

	msg->payload.sw_if_index = ~0;
	msg->payload.name_filter_valid = false;
	err = vapi_sw_interface_dump(ctx, msg, boy_sw_interface_dump_cb, table);
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_sw_interface_dump failed: %s", boy_vapi_error_str(err));
		return 1;
	}

	return 0;
}

int boy_collect_span(vapi_ctx_t ctx, bool is_l2, boy_span_table_t *table)
{
	vapi_msg_sw_interface_span_dump *msg;
	vapi_error_e err;

	msg = vapi_alloc_sw_interface_span_dump(ctx);
	if (msg == NULL) {
		boy_log(APP_LOG_ERROR, "vapi_alloc_sw_interface_span_dump failed");
		return 1;
	}

	msg->payload.is_l2 = is_l2;
	err = vapi_sw_interface_span_dump(ctx, msg, boy_span_dump_cb, table);
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_sw_interface_span_dump failed: %s",
			boy_vapi_error_str(err));
		return 1;
	}

	return 0;
}

int boy_collect_memif(vapi_ctx_t ctx, boy_memif_table_t *table)
{
	vapi_msg_memif_dump *msg;
	vapi_error_e err;

	msg = vapi_alloc_memif_dump(ctx);
	if (msg == NULL) {
		boy_log(APP_LOG_ERROR, "vapi_alloc_memif_dump failed");
		return 1;
	}

	err = vapi_memif_dump(ctx, msg, boy_memif_dump_cb, table);
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_memif_dump failed: %s", boy_vapi_error_str(err));
		return 1;
	}

	return 0;
}
