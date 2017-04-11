// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef AMQP_MANAGEMENT_H
#define AMQP_MANAGEMENT_H

#include <stdbool.h>
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/message.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "azure_c_shared_utility/umock_c_prod.h"

    typedef enum AMQP_MANAGEMENT_EXECUTE_OPERATION_RESULT_TAG
    {
        AMQP_MANAGEMENT_EXECUTE_OPERATION_OK,
        AMQP_MANAGEMENT_EXECUTE_OPERATION_ERROR,
        AMQP_MANAGEMENT_EXECUTE_OPERATION_FAILED_BAD_STATUS,
        AMQP_MANAGEMENT_EXECUTE_OPERATION_INSTANCE_CLOSED
    } AMQP_MANAGEMENT_EXECUTE_OPERATION_RESULT;

    typedef enum AMQP_MANAGEMENT_OPEN_RESULT_TAG
    {
        AMQP_MANAGEMENT_OPEN_OK,
        AMQP_MANAGEMENT_OPEN_ERROR,
        AMQP_MANAGEMENT_OPEN_CANCELLED
    } AMQP_MANAGEMENT_OPEN_RESULT;

    typedef struct AMQP_MANAGEMENT_INSTANCE_TAG* AMQP_MANAGEMENT_HANDLE;
    typedef void(*ON_AMQP_MANAGEMENT_OPEN_COMPLETE)(void* context, AMQP_MANAGEMENT_OPEN_RESULT open_result);
    typedef void(*ON_AMQP_MANAGEMENT_ERROR)(void* context);
    typedef void(*ON_AMQP_MANAGEMENT_EXECUTE_OPERATION_COMPLETE)(void* context, AMQP_MANAGEMENT_EXECUTE_OPERATION_RESULT execute_operation_result, unsigned int status_code, const char* status_description);

    MOCKABLE_FUNCTION(, AMQP_MANAGEMENT_HANDLE, amqp_management_create, SESSION_HANDLE, session, const char*, management_node);
    MOCKABLE_FUNCTION(, void, amqp_management_destroy, AMQP_MANAGEMENT_HANDLE, amqp_management);
    MOCKABLE_FUNCTION(, int, amqp_management_open_async, AMQP_MANAGEMENT_HANDLE, amqp_management, ON_AMQP_MANAGEMENT_OPEN_COMPLETE, on_amqp_management_open_complete, void*, on_amqp_management_open_complete_context, ON_AMQP_MANAGEMENT_ERROR, on_amqp_management_error, void*, on_amqp_management_error_context);
    MOCKABLE_FUNCTION(, int, amqp_management_close, AMQP_MANAGEMENT_HANDLE, amqp_management);
    MOCKABLE_FUNCTION(, int, amqp_management_execute_operation_async, AMQP_MANAGEMENT_HANDLE, amqp_management, const char*, operation, const char*, type, const char*, locales, MESSAGE_HANDLE, message, ON_AMQP_MANAGEMENT_EXECUTE_OPERATION_COMPLETE, on_execute_operation_complete, void*, context);
    MOCKABLE_FUNCTION(, void, amqp_management_set_trace, AMQP_MANAGEMENT_HANDLE, amqp_management, bool, trace_on);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AMQP_MANAGEMENT_H */
