#pragma once

#include <vapi/vapi.h>

#include <stddef.h>

const char *boy_vapi_error_str(vapi_error_e err);

int boy_open_vapi(vapi_ctx_t *ctx);
void boy_close_vapi(vapi_ctx_t *ctx);

void boy_copy_api_string(char *dst, size_t dst_size, const u8 *src, size_t src_size);

int boy_set_interface_admin_up(vapi_ctx_t ctx, u32 sw_if_index);
