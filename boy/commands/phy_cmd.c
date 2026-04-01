#include "commands/phy_cmd.h"

#include "boy_log.h"
#include "collectors.h"
#include "common/table.h"
#include "vpp_client.h"

#include <vapi/vlib.api.vapi.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct boy_cli_inband_result {
	bool received_reply;
	int retval;
	char *reply;
} boy_cli_inband_result_t;

static boy_interface_entry_t *boy_find_interface_mut_by_name(boy_interface_table_t *table,
							      const char *name)
{
	size_t i;

	for (i = 0; i < table->count; i++) {
		if (strcmp(table->entries[i].interface_name, name) == 0) {
			return &table->entries[i];
		}
	}

	return NULL;
}

static vapi_error_e boy_cli_inband_cb(struct vapi_ctx_s *ctx, void *callback_ctx, vapi_error_e rv,
				      bool is_last, vapi_payload_cli_inband_reply *reply)
{
	boy_cli_inband_result_t *result = callback_ctx;
	size_t reply_len;
	char *buf;

	(void)ctx;
	(void)is_last;

	if (rv != VAPI_OK) {
		return rv;
	}
	if (reply == NULL) {
		return VAPI_OK;
	}

	reply_len = reply->reply.length;
	buf = malloc(reply_len + 1);
	if (buf == NULL) {
		return VAPI_ENOMEM;
	}
	memcpy(buf, reply->reply.buf, reply_len);
	buf[reply_len] = '\0';

	free(result->reply);
	result->reply = buf;
	result->retval = reply->retval;
	result->received_reply = true;
	return VAPI_OK;
}

static int boy_fetch_show_hardware_output(vapi_ctx_t ctx, char **output)
{
	static const char cmd[] = "show hardware-interfaces";
	vapi_msg_cli_inband *msg = NULL;
	boy_cli_inband_result_t result = {0};
	vapi_error_e err;

	*output = NULL;

	msg = vapi_alloc_cli_inband(ctx, sizeof(cmd) - 1);
	if (msg == NULL) {
		boy_log(APP_LOG_ERROR, "vapi_alloc_cli_inband failed");
		return 1;
	}

	memcpy(msg->payload.cmd.buf, cmd, sizeof(cmd) - 1);
	err = vapi_cli_inband(ctx, msg, boy_cli_inband_cb, &result);
	msg = NULL;
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_cli_inband failed: %s", boy_vapi_error_str(err));
		free(result.reply);
		return 1;
	}
	if (!result.received_reply) {
		boy_log(APP_LOG_ERROR, "cli_inband returned no reply");
		free(result.reply);
		return 1;
	}
	if (result.retval != 0) {
		boy_log(APP_LOG_WARNING, "cli_inband show hardware-interfaces retval=%d",
			result.retval);
		free(result.reply);
		return 1;
	}

	*output = result.reply;
	return 0;
}

static void boy_rtrim(char *line)
{
	size_t len = strlen(line);

	while (len > 0) {
		char ch = line[len - 1];
		if (ch != ' ' && ch != '\t' && ch != '\r') {
			break;
		}
		line[--len] = '\0';
	}
}

static void boy_override_card_family_from_cli_output(boy_interface_table_t *interfaces,
						      const char *cli_output)
{
	char *text;
	char *line;
	char *saveptr = NULL;
	boy_interface_entry_t *current = NULL;
	bool expect_family = false;

	if (cli_output == NULL || cli_output[0] == '\0') {
		return;
	}

	text = strdup(cli_output);
	if (text == NULL) {
		return;
	}

	for (line = strtok_r(text, "\n", &saveptr); line != NULL;
	     line = strtok_r(NULL, "\n", &saveptr)) {
		char if_name[65];
		unsigned idx;
		char link[16];
		char *trimmed = line;

		boy_rtrim(line);
		while (*trimmed == ' ' || *trimmed == '\t') {
			trimmed++;
		}
		if (*trimmed == '\0') {
			continue;
		}

		if (line[0] != ' ' && line[0] != '\t') {
			if (sscanf(line, "%64s %u %15s", if_name, &idx, link) == 3) {
				current = boy_find_interface_mut_by_name(interfaces, if_name);
				if (current != NULL && !boy_is_physical_interface(current)) {
					current = NULL;
				}
			} else {
				current = NULL;
			}
			expect_family = false;
			continue;
		}

		if (current == NULL) {
			continue;
		}
		if (strncmp(trimmed, "Ethernet address ", 17) == 0) {
			expect_family = true;
			continue;
		}
		if (!expect_family) {
			continue;
		}
		if (strchr(trimmed, ':') != NULL || strncmp(trimmed, "carrier ", 8) == 0) {
			expect_family = false;
			continue;
		}

		snprintf(current->card_family, sizeof(current->card_family), "%s", trimmed);
		expect_family = false;
	}

	free(text);
}

static const char *boy_phy_link_state(vapi_enum_if_status_flags flags)
{
	return (flags & IF_STATUS_API_FLAG_LINK_UP) ? "up" : "down";
}

static void boy_phy_speed_to_string(u32 link_speed, char *buffer, size_t buffer_size)
{
	if (link_speed == 0) {
		snprintf(buffer, buffer_size, "unknown");
		return;
	}

	if ((link_speed % 1000U) == 0U) {
		snprintf(buffer, buffer_size, "%uGbps", link_speed / 1000U);
		return;
	}

	snprintf(buffer, buffer_size, "%uMbps", link_speed);
}

static int boy_add_phy_row(app_table_t *table, const boy_interface_entry_t *entry)
{
	char if_index_buf[16];
	char speed_buf[16];
	const char *row[6];

	snprintf(if_index_buf, sizeof(if_index_buf), "%u", entry->sw_if_index);
	boy_phy_speed_to_string(entry->link_speed, speed_buf, sizeof(speed_buf));
	row[0] = entry->interface_name;
	row[1] = if_index_buf;
	row[2] = boy_phy_link_state(entry->flags);
	row[3] = speed_buf;
	row[4] = entry->ethernet_address;
	row[5] = entry->card_family;
	return app_table_add_row(table, row);
}

int boy_show_phy(void)
{
	vapi_ctx_t ctx = NULL;
	boy_interface_table_t interfaces = {0};
	app_table_t table;
	const app_table_column_t columns[] = {
		{"Interface Name", 14, 0, 1, APP_TABLE_ALIGN_LEFT},
		{"If-Index", 8, 10, 0, APP_TABLE_ALIGN_RIGHT},
		{"Link", 4, 8, 0, APP_TABLE_ALIGN_LEFT},
		{"Speed", 5, 10, 0, APP_TABLE_ALIGN_LEFT},
		{"Ethernet Address", 16, 17, 0, APP_TABLE_ALIGN_LEFT},
		{"Card Family", 11, 0, 1, APP_TABLE_ALIGN_LEFT},
	};
	int rc = 1;
	size_t i;
	size_t shown = 0;
	bool table_initialized = false;
	char *hardware_output = NULL;

	boy_log_start("querying physical interface state via libvapi");

	if (boy_open_vapi(&ctx) != 0) {
		goto cleanup;
	}

	if (boy_collect_interfaces(ctx, &interfaces) != 0) {
		goto cleanup;
	}
	if (boy_fetch_show_hardware_output(ctx, &hardware_output) == 0) {
		boy_override_card_family_from_cli_output(&interfaces, hardware_output);
	} else {
		boy_log(APP_LOG_DEBUG, "using interface_dev_type as card family fallback");
	}

	if (app_table_init(&table, columns, sizeof(columns) / sizeof(columns[0])) != 0) {
		boy_log(APP_LOG_ERROR, "failed to initialize table for physical interface output");
		goto cleanup;
	}
	table_initialized = true;

	for (i = 0; i < interfaces.count; i++) {
		const boy_interface_entry_t *entry = &interfaces.entries[i];

		if (!boy_is_physical_interface(entry)) {
			continue;
		}

		if (boy_add_phy_row(&table, entry) != 0) {
			boy_log(APP_LOG_ERROR, "failed to append physical interface table row");
			goto cleanup;
		}
		shown++;
	}

	app_table_render(stdout, &table);
	if (shown == 0) {
		boy_log(APP_LOG_INFO, "no physical interfaces found");
	}
	table_initialized = false;
	app_table_free(&table);
	rc = 0;

cleanup:
	free(hardware_output);
	if (table_initialized) {
		app_table_free(&table);
	}
	boy_free_interface_table(&interfaces);
	boy_close_vapi(&ctx);
	return rc;
}
