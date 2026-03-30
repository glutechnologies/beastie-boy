#include "table.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ioctl.h>
#include <unistd.h>

#define APP_TABLE_DEFAULT_TERMINAL_WIDTH 120U
#define APP_TABLE_COLUMN_GAP 2U

static char *app_table_strdup(const char *src)
{
	size_t len;
	char *dst;

	if (src == NULL) {
		src = "";
	}

	len = strlen(src);
	dst = malloc(len + 1);
	if (dst == NULL) {
		return NULL;
	}
	memcpy(dst, src, len + 1);
	return dst;
}

static int app_table_reserve_rows(app_table_t *table, size_t required_count)
{
	size_t new_capacity;
	app_table_row_t *new_rows;

	if (table->row_capacity >= required_count) {
		return 0;
	}

	new_capacity = (table->row_capacity == 0) ? 8 : (table->row_capacity * 2);
	while (new_capacity < required_count) {
		new_capacity *= 2;
	}

	new_rows = realloc(table->rows, new_capacity * sizeof(*new_rows));
	if (new_rows == NULL) {
		return -1;
	}

	table->rows = new_rows;
	table->row_capacity = new_capacity;
	return 0;
}

static size_t app_table_detect_terminal_width(FILE *stream)
{
	const char *columns_env = getenv("COLUMNS");
	int fd;
	struct winsize ws;

	if (columns_env != NULL && columns_env[0] != '\0') {
		char *end = NULL;
		unsigned long parsed = strtoul(columns_env, &end, 10);
		if (end != columns_env && *end == '\0' && parsed > 0) {
			return (size_t)parsed;
		}
	}

	fd = fileno(stream);
	if (fd >= 0 && isatty(fd) && ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
		return (size_t)ws.ws_col;
	}

	return APP_TABLE_DEFAULT_TERMINAL_WIDTH;
}

static size_t app_table_clamp_width(size_t width, const app_table_column_t *column)
{
	if (width < column->min_width) {
		width = column->min_width;
	}
	if (column->max_width != 0 && width > column->max_width) {
		width = column->max_width;
	}
	return width;
}

static void app_table_print_repeated(FILE *stream, char ch, size_t count)
{
	size_t i;

	for (i = 0; i < count; i++) {
		fputc((int)ch, stream);
	}
}

static void app_table_print_cell(FILE *stream, const char *value, size_t width,
				 app_table_align_t align)
{
	size_t value_len = strlen(value);
	size_t shown_len = value_len;
	size_t left_pad = 0;
	size_t right_pad = 0;

	if (shown_len > width) {
		shown_len = width;
	}

	if (align == APP_TABLE_ALIGN_RIGHT && width > shown_len) {
		left_pad = width - shown_len;
	} else if (width > shown_len) {
		right_pad = width - shown_len;
	}

	app_table_print_repeated(stream, ' ', left_pad);

	if (value_len > width && width > 3) {
		fwrite(value, 1, width - 3, stream);
		fputs("...", stream);
	} else if (value_len > width) {
		fwrite(value, 1, width, stream);
	} else {
		fwrite(value, 1, shown_len, stream);
	}

	app_table_print_repeated(stream, ' ', right_pad);
}

int app_table_init(app_table_t *table, const app_table_column_t *columns, size_t column_count)
{
	size_t i;

	if (table == NULL || columns == NULL || column_count == 0) {
		return -1;
	}

	memset(table, 0, sizeof(*table));

	table->columns = calloc(column_count, sizeof(*table->columns));
	if (table->columns == NULL) {
		return -1;
	}

	for (i = 0; i < column_count; i++) {
		table->columns[i] = columns[i];
	}
	table->column_count = column_count;
	return 0;
}

void app_table_free(app_table_t *table)
{
	size_t i;
	size_t j;

	if (table == NULL) {
		return;
	}

	for (i = 0; i < table->row_count; i++) {
		for (j = 0; j < table->column_count; j++) {
			free(table->rows[i].cells[j]);
		}
		free(table->rows[i].cells);
	}

	free(table->rows);
	free(table->columns);
	memset(table, 0, sizeof(*table));
}

int app_table_add_row(app_table_t *table, const char *const *cells)
{
	app_table_row_t *row;
	size_t i;

	if (table == NULL || cells == NULL) {
		return -1;
	}

	if (app_table_reserve_rows(table, table->row_count + 1) != 0) {
		return -1;
	}

	row = &table->rows[table->row_count];
	row->cells = calloc(table->column_count, sizeof(*row->cells));
	if (row->cells == NULL) {
		return -1;
	}

	for (i = 0; i < table->column_count; i++) {
		row->cells[i] = app_table_strdup(cells[i]);
		if (row->cells[i] == NULL) {
			size_t k;
			for (k = 0; k < i; k++) {
				free(row->cells[k]);
			}
			free(row->cells);
			row->cells = NULL;
			return -1;
		}
	}

	table->row_count++;
	return 0;
}

void app_table_render(FILE *stream, const app_table_t *table)
{
	size_t i;
	size_t j;
	size_t total_width = 0;
	size_t available_width;
	size_t overflow;
	size_t *widths;
	size_t gap = APP_TABLE_COLUMN_GAP;

	if (stream == NULL || table == NULL || table->column_count == 0) {
		return;
	}

	widths = calloc(table->column_count, sizeof(*widths));
	if (widths == NULL) {
		return;
	}

	for (j = 0; j < table->column_count; j++) {
		widths[j] = strlen(table->columns[j].header ? table->columns[j].header : "");
	}

	for (i = 0; i < table->row_count; i++) {
		for (j = 0; j < table->column_count; j++) {
			size_t len = strlen(table->rows[i].cells[j] ? table->rows[i].cells[j] : "");
			if (len > widths[j]) {
				widths[j] = len;
			}
		}
	}

	for (j = 0; j < table->column_count; j++) {
		widths[j] = app_table_clamp_width(widths[j], &table->columns[j]);
		total_width += widths[j];
	}
	if (table->column_count > 1) {
		total_width += gap * (table->column_count - 1);
	}

	available_width = app_table_detect_terminal_width(stream);
	if (total_width > available_width) {
		overflow = total_width - available_width;
		while (overflow > 0) {
			bool reduced = false;

			for (j = 0; j < table->column_count && overflow > 0; j++) {
				if (!table->columns[j].is_flexible) {
					continue;
				}
				if (widths[j] > table->columns[j].min_width) {
					widths[j]--;
					overflow--;
					reduced = true;
				}
			}

			if (!reduced) {
				for (j = 0; j < table->column_count && overflow > 0; j++) {
					if (widths[j] > 3) {
						widths[j]--;
						overflow--;
						reduced = true;
					}
				}
			}

			if (!reduced) {
				break;
			}
		}
	}

	for (j = 0; j < table->column_count; j++) {
		const char *header = table->columns[j].header ? table->columns[j].header : "";
		app_table_print_cell(stream, header, widths[j], APP_TABLE_ALIGN_LEFT);
		if (j + 1 < table->column_count) {
			app_table_print_repeated(stream, ' ', gap);
		}
	}
	fputc('\n', stream);

	for (j = 0; j < table->column_count; j++) {
		app_table_print_repeated(stream, '-', widths[j]);
		if (j + 1 < table->column_count) {
			app_table_print_repeated(stream, ' ', gap);
		}
	}
	fputc('\n', stream);

	for (i = 0; i < table->row_count; i++) {
		for (j = 0; j < table->column_count; j++) {
			const char *value = table->rows[i].cells[j] ? table->rows[i].cells[j] : "";
			app_table_print_cell(stream, value, widths[j], table->columns[j].align);
			if (j + 1 < table->column_count) {
				app_table_print_repeated(stream, ' ', gap);
			}
		}
		fputc('\n', stream);
	}

	free(widths);
}
