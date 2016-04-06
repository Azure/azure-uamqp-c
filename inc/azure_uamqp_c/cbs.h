// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef CBS_H
#define CBS_H

#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/amqp_management.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

	typedef enum CBS_OPERATION_RESULT_TAG
	{
		CBS_OPERATION_RESULT_OK,
		CBS_OPERATION_RESULT_CBS_ERROR,
		CBS_OPERATION_RESULT_OPERATION_FAILED
	} CBS_OPERATION_RESULT;

	typedef struct CBS_INSTANCE_TAG* CBS_HANDLE;
	typedef void(*ON_CBS_OPERATION_COMPLETE)(void* context, CBS_OPERATION_RESULT cbs_operation_result, unsigned int status_code, const char* status_description);

	extern CBS_HANDLE cbs_create(SESSION_HANDLE session, ON_AMQP_MANAGEMENT_STATE_CHANGED on_amqp_management_state_changed, void* callback_context);
	extern void cbs_destroy(CBS_HANDLE cbs);
	extern int cbs_open(CBS_HANDLE amqp_management);
	extern int cbs_close(CBS_HANDLE amqp_management);
	extern int cbs_put_token(CBS_HANDLE cbs, const char* type, const char* audience, const char* token, ON_CBS_OPERATION_COMPLETE on_cbs_operation_complete, void* context);
	extern int cbs_delete_token(CBS_HANDLE cbs, const char* type, const char* audience, ON_CBS_OPERATION_COMPLETE on_cbs_operation_complete, void* context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CBS_H */
