#pragma once

#include <vapi/interface.api.vapi.h>
#include <vapi/span.api.vapi.h>
#include <vapi/vapi.h>

#include <stdbool.h>
#include <stddef.h>

typedef struct boy_interface_entry {
	u32 sw_if_index;
	char interface_name[65];
	vapi_enum_if_type type;
	char interface_dev_type[65];
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

void boy_free_interface_table(boy_interface_table_t *table);
void boy_free_span_table(boy_span_table_t *table);
void boy_free_memif_table(boy_memif_table_t *table);

const char *boy_find_interface_name(const boy_interface_table_t *table, u32 sw_if_index);

const char *boy_if_type_to_string(vapi_enum_if_type type);
bool boy_is_span_source_candidate(const boy_interface_entry_t *entry);
const char *boy_span_state_device(vapi_enum_span_state state);
const char *boy_span_state_l2(vapi_enum_span_state state, bool is_l2);

int boy_collect_interfaces(vapi_ctx_t ctx, boy_interface_table_t *table);
int boy_collect_span(vapi_ctx_t ctx, bool is_l2, boy_span_table_t *table);
int boy_collect_memif(vapi_ctx_t ctx, boy_memif_table_t *table);
