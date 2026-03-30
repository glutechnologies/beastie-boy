#include "commands/version_cmd.h"

#include "boy_log.h"
#include "vpp_client.h"

#include <vapi/vpe.api.vapi.h>

typedef struct boy_vpp_version_result {
	bool received_reply;
	int retval;
	char program[33];
	char version[33];
	char build_date[33];
	char build_directory[257];
} boy_vpp_version_result_t;

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

int boy_show_version(void)
{
	vapi_ctx_t ctx = NULL;
	vapi_msg_show_version *msg = NULL;
	boy_vpp_version_result_t result = {0};
	vapi_error_e err;
	int rc = 1;

	boy_log_start("connecting to VPP via libvapi");

	if (boy_open_vapi(&ctx) != 0) {
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
	boy_close_vapi(&ctx);
	return rc;
}
