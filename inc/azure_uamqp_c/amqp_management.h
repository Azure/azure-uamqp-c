// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef AMQPMAN_H
#define AMQPMAN_H

#include <stdbool.h>
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/message.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "azure_c_shared_utility/umock_c_prod.h"

    typedef enum OPERATION_RESULT_TAG
    {
        OPERATION_RESULT_OK,
        OPERATION_RESULT_CBS_ERROR,
        OPERATION_RESULT_OPERATION_FAILED
    } OPERATION_RESULT;

    typedef enum AMQP_MANAGEMENT_STATE_TAG
    {
        AMQP_MANAGEMENT_STATE_IDLE,
        AMQP_MANAGEMENT_STATE_OPENING,
        AMQP_MANAGEMENT_STATE_OPEN,
        AMQP_MANAGEMENT_STATE_ERROR
    } AMQP_MANAGEMENT_STATE;

    typedef struct AMQP_MANAGEMENT_INSTANCE_TAG* AMQP_MANAGEMENT_HANDLE;
    typedef void(*ON_OPERATION_COMPLETE)(void* context, OPERATION_RESULT operation_result, unsigned int status_code, const char* status_description);
    typedef void(*ON_AMQP_MANAGEMENT_STATE_CHANGED)(void* context, AMQP_MANAGEMENT_STATE new_amqp_management_state, AMQP_MANAGEMENT_STATE previous_amqp_management_state);

    MOCKABLE_FUNCTION(, AMQP_MANAGEMENT_HANDLE, amqpmanagement_create, SESSION_HANDLE, session, const char*, management_node, ON_AMQP_MANAGEMENT_STATE_CHANGED, on_amqp_management_state_changed, void*, callback_context);
    MOCKABLE_FUNCTION(, void, amqpmanagement_destroy, AMQP_MANAGEMENT_HANDLE, amqp_management);
    MOCKABLE_FUNCTION(, int, amqpmanagement_open, AMQP_MANAGEMENT_HANDLE, amqp_management);
    MOCKABLE_FUNCTION(, int, amqpmanagement_close, AMQP_MANAGEMENT_HANDLE, amqp_management);
    MOCKABLE_FUNCTION(, int, amqpmanagement_start_operation, AMQP_MANAGEMENT_HANDLE, amqp_management, const char*, operation, const char*, type, const char*, locales, MESSAGE_HANDLE, message, ON_OPERATION_COMPLETE, on_operation_complete, void*, context);
    MOCKABLE_FUNCTION(, void, amqpmanagement_set_trace, AMQP_MANAGEMENT_HANDLE, amqp_management, bool, traceOn);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AMQPMAN_H */
