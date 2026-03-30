#pragma once

#include <stddef.h>
#include <stdio.h>

typedef enum app_table_align {
	APP_TABLE_ALIGN_LEFT = 0,
	APP_TABLE_ALIGN_RIGHT = 1,
} app_table_align_t;

typedef struct app_table_column {
	const char *header;
	size_t min_width;
	size_t max_width; /* 0 means unlimited */
	int is_flexible; /* column can shrink when terminal width is limited */
	app_table_align_t align;
} app_table_column_t;

typedef struct app_table_row {
	char **cells;
} app_table_row_t;

typedef struct app_table {
	app_table_column_t *columns;
	size_t column_count;
	app_table_row_t *rows;
	size_t row_count;
	size_t row_capacity;
} app_table_t;

int app_table_init(app_table_t *table, const app_table_column_t *columns, size_t column_count);
void app_table_free(app_table_t *table);
int app_table_add_row(app_table_t *table, const char *const *cells);
void app_table_render(FILE *stream, const app_table_t *table);
