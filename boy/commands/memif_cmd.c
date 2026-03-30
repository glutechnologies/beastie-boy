#include "commands/memif_cmd.h"

#include "boy_log.h"
#include "collectors.h"
#include "common/table.h"
#include "vpp_client.h"

#include <stddef.h>

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
