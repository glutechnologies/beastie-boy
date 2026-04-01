#include "vpp_client.h"

#include "boy_log.h"

#include <vapi/interface.api.vapi.h>
#include <vapi/memif.api.vapi.h>
#include <vapi/span.api.vapi.h>
#include <vapi/vlib.api.vapi.h>
#include <vapi/vpe.api.vapi.h>

#include <string.h>

DEFINE_VAPI_MSG_IDS_VPE_API_JSON;
DEFINE_VAPI_MSG_IDS_VLIB_API_JSON;
DEFINE_VAPI_MSG_IDS_INTERFACE_API_JSON;
DEFINE_VAPI_MSG_IDS_MEMIF_API_JSON;
DEFINE_VAPI_MSG_IDS_SPAN_API_JSON;

const char *boy_vapi_error_str(vapi_error_e err)
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

int boy_open_vapi(vapi_ctx_t *ctx)
{
	vapi_error_e err;

	*ctx = NULL;

	err = vapi_ctx_alloc(ctx);
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_ctx_alloc failed: %s", boy_vapi_error_str(err));
		return 1;
	}

	err = vapi_connect(*ctx, "boy", NULL, 16, 16, VAPI_MODE_BLOCKING, true);
	if (err != VAPI_OK) {
		boy_log(APP_LOG_ERROR, "vapi_connect failed: %s", boy_vapi_error_str(err));
		vapi_ctx_free(*ctx);
		*ctx = NULL;
		return 1;
	}

	return 0;
}

void boy_close_vapi(vapi_ctx_t *ctx)
{
	if (ctx == NULL || *ctx == NULL) {
		return;
	}

	vapi_disconnect(*ctx);
	vapi_ctx_free(*ctx);
	*ctx = NULL;
}

void boy_copy_api_string(char *dst, size_t dst_size, const u8 *src, size_t src_size)
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
