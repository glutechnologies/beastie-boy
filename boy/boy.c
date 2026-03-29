#include "boy.h"

#include <vapi/interface.api.vapi.h>
#include <vapi/memif.api.vapi.h>
#include <vapi/span.api.vapi.h>
#include <vapi/vapi.h>
#include <vapi/vpe.api.vapi.h>

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DEFINE_VAPI_MSG_IDS_VPE_API_JSON;
DEFINE_VAPI_MSG_IDS_INTERFACE_API_JSON;
DEFINE_VAPI_MSG_IDS_MEMIF_API_JSON;
DEFINE_VAPI_MSG_IDS_SPAN_API_JSON;

static app_log_level_t current_log_level = APP_LOG_INFO;

static void boy_log(app_log_level_t level, const char *format, ...)
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

static const char *boy_vapi_error_str(vapi_error_e err)
{
	switch (err) {
	case VAPI_OK:
		return "success";
	case VAPI_EINVAL:
		return "invalid argument";
	case VAPI_EAGAIN:
		return "operation would block";
	case VAPI_ENOTSUP:
		return "operation not supported";
	case VAPI_ENOMEM:
		return "out of memory";
	case VAPI_ENORESP:
		return "no response";
	case VAPI_EMAP_FAIL:
		return "api mapping failure";
	case VAPI_ECON_FAIL:
		return "connection to VPP failed";
	case VAPI_EINCOMPATIBLE:
		return "incompatible VPP API";
	case VAPI_MUTEX_FAILURE:
		return "mutex failure";
	case VAPI_EUSER:
		return "user error";
	case VAPI_ENOTSOCK:
		return "socket is not valid";
	case VAPI_EACCES:
		return "socket permission denied";
	case VAPI_ECONNRESET:
		return "connection reset by peer";
	case VAPI_ESOCK_FAILURE:
		return "socket failure";
	default:
		return "unknown vapi error";
	}
}

typedef struct boy_vpp_version_result {
	bool received_reply;
	int retval;
	char program[33];
	char version[33];
	char build_date[33];
	char build_directory[257];
} boy_vpp_version_result_t;

typedef struct boy_interface_entry {
	u32 sw_if_index;
	char interface_name[65];
} boy_interface_entry_t;

typedef struct boy_interface_table {
	boy_interface_entry_t *entries;
	size_t count;
	size_t capacity;
} boy_interface_table_t;

typedef struct boy_span_entry {
	u32 sw_if_index_from;
	u32 sw_if_index_to;
	vapi_enum_span_state state;
	bool is_l2;
} boy_span_entry_t;

typedef struct boy_span_table {
	boy_span_entry_t *entries;
	size_t count;
	size_t capacity;
} boy_span_table_t;

typedef struct boy_memif_entry {
	char interface_name[65];
	u32 socket_id;
	char flags[32];
} boy_memif_entry_t;

typedef struct boy_memif_table {
	boy_memif_entry_t *entries;
	size_t count;
	size_t capacity;
} boy_memif_table_t;

static void boy_copy_api_string(char *dst, size_t dst_size, const u8 *src, size_t src_size)
{
	size_t len = strnlen((const char *)src, src_size);

	if (dst_size == 0) {
		return;
	}

	if (len >= dst_size) {
		len = dst_size - 1;
	}

	memcpy(dst, src, len);
	dst[len] = '\0';
}

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

static void boy_free_interface_table(boy_interface_table_t *table)
{
	free(table->entries);
	table->entries = NULL;
	table->count = 0;
	table->capacity = 0;
}

static void boy_free_span_table(boy_span_table_t *table)
{
	free(table->entries);
	table->entries = NULL;
	table->count = 0;
	table->capacity = 0;
}

static void boy_free_memif_table(boy_memif_table_t *table)
{
	free(table->entries);
	table->entries = NULL;
	table->count = 0;
	table->capacity = 0;
}

static const char *boy_find_interface_name(const boy_interface_table_t *table, u32 sw_if_index)
{
	size_t i;

	for (i = 0; i < table->count; i++) {
		if (table->entries[i].sw_if_index == sw_if_index) {
			return table->entries[i].interface_name;
		}
	}

	return "<unknown>";
}

static const char *boy_span_state_device(vapi_enum_span_state state)
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

static const char *boy_span_state_l2(vapi_enum_span_state state, bool is_l2)
{
	if (!is_l2) {
		return "none";
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
	boy_copy_api_string(entry->interface_name, sizeof(entry->interface_name),
			    reply->interface_name, sizeof(reply->interface_name));
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
	boy_copy_api_string(entry->interface_name, sizeof(entry->interface_name),
			    reply->if_name, sizeof(reply->if_name));
	entry->socket_id = reply->socket_id;
	boy_memif_flags_to_string(reply->flags, entry->flags, sizeof(entry->flags));
	return VAPI_OK;
}

static int boy_collect_interfaces(vapi_ctx_t ctx, boy_interface_table_t *table)
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

static int boy_collect_span(vapi_ctx_t ctx, bool is_l2, boy_span_table_t *table)
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

static int boy_collect_memif(vapi_ctx_t ctx, boy_memif_table_t *table)
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

static vapi_error_e boy_show_version_cb(struct vapi_ctx_s *ctx, void *callback_ctx,
					vapi_error_e rv, bool is_last,
					vapi_payload_show_version_reply *reply)
{
	boy_vpp_version_result_t *result = callback_ctx;

	(void)ctx;
	(void)is_last;

	if (rv != VAPI_OK) {
		return rv;
	}

	result->received_reply = true;
	result->retval = reply->retval;
	boy_copy_api_string(result->program, sizeof(result->program), reply->program,
			    sizeof(reply->program));
	boy_copy_api_string(result->version, sizeof(result->version), reply->version,
			    sizeof(reply->version));
	boy_copy_api_string(result->build_date, sizeof(result->build_date), reply->build_date,
			    sizeof(reply->build_date));
	boy_copy_api_string(result->build_directory, sizeof(result->build_directory),
			    reply->build_directory, sizeof(reply->build_directory));

	return VAPI_OK;
}

static int boy_show_version(void)
{
	vapi_ctx_t ctx = NULL;
	vapi_msg_show_version *msg = NULL;
	boy_vpp_version_result_t result = {0};
	vapi_error_e err;
	int rc = 1;

	boy_log(APP_LOG_INFO, "boy starting");
	boy_log(APP_LOG_DEBUG, "connecting to VPP via libvapi");

	err = vapi_ctx_alloc(&ctx);
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_ctx_alloc failed: %s", boy_vapi_error_str(err));
		return 1;
	}

	err = vapi_connect(ctx, "boy", NULL, 16, 16, VAPI_MODE_BLOCKING, true);
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_connect failed: %s", boy_vapi_error_str(err));
		goto cleanup;
	}

	msg = vapi_alloc_show_version(ctx);
	if (msg == NULL) {
		boy_log(APP_LOG_ERROR, "vapi_alloc_show_version failed");
		goto cleanup;
	}

	err = vapi_show_version(ctx, msg, boy_show_version_cb, &result);
	msg = NULL;
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_show_version failed: %s", boy_vapi_error_str(err));
		goto cleanup;
	}

	if (!result.received_reply) {
		boy_log(APP_LOG_ERROR, "show_version returned no reply");
		goto cleanup;
	}

	if (result.retval != 0) {
		boy_log(APP_LOG_ERROR, "show_version reply returned retval=%d", result.retval);
		goto cleanup;
	}

	boy_log(APP_LOG_INFO, "program: %s", result.program);
	boy_log(APP_LOG_INFO, "version: %s", result.version);
	boy_log(APP_LOG_DEBUG, "build date: %s", result.build_date);
	boy_log(APP_LOG_DEBUG, "build directory: %s", result.build_directory);
	rc = 0;

cleanup:
	if (msg != NULL && ctx != NULL) {
		vapi_msg_free(ctx, msg);
	}
	if (ctx != NULL) {
		vapi_disconnect(ctx);
		vapi_ctx_free(ctx);
	}
	return rc;
}

static int boy_show_span(void)
{
	vapi_ctx_t ctx = NULL;
	boy_interface_table_t interfaces = {0};
	boy_span_table_t span_entries = {0};
	vapi_error_e err;
	int rc = 1;
	size_t i;

	boy_log(APP_LOG_INFO, "boy starting");
	boy_log(APP_LOG_DEBUG, "querying SPAN state via libvapi");

	err = vapi_ctx_alloc(&ctx);
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_ctx_alloc failed: %s", boy_vapi_error_str(err));
		return 1;
	}

	err = vapi_connect(ctx, "boy", NULL, 16, 16, VAPI_MODE_BLOCKING, true);
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_connect failed: %s", boy_vapi_error_str(err));
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

	printf("Source                           Destination                       Device       L2\n");
	for (i = 0; i < span_entries.count; i++) {
		const boy_span_entry_t *entry = &span_entries.entries[i];
		const char *src = boy_find_interface_name(&interfaces, entry->sw_if_index_from);
		const char *dst = boy_find_interface_name(&interfaces, entry->sw_if_index_to);

		printf("%-32s %-32s (%6s) (%6s)\n",
		       src, dst,
		       boy_span_state_device(entry->state),
		       boy_span_state_l2(entry->state, entry->is_l2));
	}
	rc = 0;

cleanup:
	boy_free_span_table(&span_entries);
	boy_free_interface_table(&interfaces);
	if (ctx != NULL) {
		vapi_disconnect(ctx);
		vapi_ctx_free(ctx);
	}
	return rc;
}

static int boy_show_memif(void)
{
	vapi_ctx_t ctx = NULL;
	boy_memif_table_t memif_entries = {0};
	vapi_error_e err;
	int rc = 1;
	size_t i;

	boy_log(APP_LOG_INFO, "boy starting");
	boy_log(APP_LOG_DEBUG, "querying memif state via libvapi");

	err = vapi_ctx_alloc(&ctx);
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_ctx_alloc failed: %s", boy_vapi_error_str(err));
		return 1;
	}

	err = vapi_connect(ctx, "boy", NULL, 16, 16, VAPI_MODE_BLOCKING, true);
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_connect failed: %s", boy_vapi_error_str(err));
		goto cleanup;
	}

	if (boy_collect_memif(ctx, &memif_entries) != 0) {
		goto cleanup;
	}

	printf("Interface Name                  Socket-ID  Flags\n");
	for (i = 0; i < memif_entries.count; i++) {
		const boy_memif_entry_t *entry = &memif_entries.entries[i];

		printf("%-31s %-10u %-s\n",
		       entry->interface_name, entry->socket_id, entry->flags);
	}
	rc = 0;

cleanup:
	boy_free_memif_table(&memif_entries);
	if (ctx != NULL) {
		vapi_disconnect(ctx);
		vapi_ctx_free(ctx);
	}
	return rc;
}

int boy_run(boy_mode_t mode, app_log_level_t log_level)
{
	current_log_level = log_level;

	switch (mode) {
	case BOY_MODE_SHOW_MEMIF:
		return boy_show_memif();
	case BOY_MODE_SHOW_SPAN:
		return boy_show_span();
	case BOY_MODE_SHOW_VERSION:
	default:
		return boy_show_version();
	}
}
