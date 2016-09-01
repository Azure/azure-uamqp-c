// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef LINK_H
#define LINK_H

#include <stddef.h>
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/amqpvalue.h"
#include "azure_uamqp_c/amqp_definitions.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "azure_c_shared_utility/umock_c_prod.h"

typedef struct LINK_INSTANCE_TAG* LINK_HANDLE;

typedef enum LINK_STATE_TAG
{
	LINK_STATE_DETACHED,
	LINK_STATE_HALF_ATTACHED,
	LINK_STATE_ATTACHED,
	LINK_STATE_ERROR
} LINK_STATE;

typedef enum LINK_TRANSFER_RESULT_TAG
{
	LINK_TRANSFER_OK,
	LINK_TRANSFER_ERROR,
	LINK_TRANSFER_BUSY
} LINK_TRANSFER_RESULT;

typedef void(*ON_DELIVERY_SETTLED)(void* context, delivery_number delivery_no, AMQP_VALUE delivery_state);
typedef AMQP_VALUE(*ON_TRANSFER_RECEIVED)(void* context, TRANSFER_HANDLE transfer, uint32_t payload_size, const unsigned char* payload_bytes);
typedef void(*ON_LINK_STATE_CHANGED)(void* context, LINK_STATE new_link_state, LINK_STATE previous_link_state);
typedef void(*ON_LINK_FLOW_ON)(void* context);

MOCKABLE_FUNCTION(, LINK_HANDLE, link_create, SESSION_HANDLE, session, const char*, name, role, role, AMQP_VALUE, source, AMQP_VALUE, target);
MOCKABLE_FUNCTION(, LINK_HANDLE, link_create_from_endpoint, SESSION_HANDLE, session, LINK_ENDPOINT_HANDLE, link_endpoint, const char*, name, role, role, AMQP_VALUE, source, AMQP_VALUE, target);
MOCKABLE_FUNCTION(, void, link_destroy, LINK_HANDLE, handle);
MOCKABLE_FUNCTION(, int,  link_set_snd_settle_mode, LINK_HANDLE, link, sender_settle_mode, snd_settle_mode);
MOCKABLE_FUNCTION(, int,  link_get_snd_settle_mode, LINK_HANDLE, link, sender_settle_mode*, snd_settle_mode);
MOCKABLE_FUNCTION(, int,  link_set_rcv_settle_mode, LINK_HANDLE, link, receiver_settle_mode, rcv_settle_mode);
MOCKABLE_FUNCTION(, int,  link_get_rcv_settle_mode, LINK_HANDLE, link, receiver_settle_mode*, rcv_settle_mode);
MOCKABLE_FUNCTION(, int,  link_set_initial_delivery_count, LINK_HANDLE, link, sequence_no, initial_delivery_count);
MOCKABLE_FUNCTION(, int,  link_get_initial_delivery_count, LINK_HANDLE, link, sequence_no*, initial_delivery_count);
MOCKABLE_FUNCTION(, int,  link_set_max_message_size, LINK_HANDLE, link, uint64_t, max_message_size);
MOCKABLE_FUNCTION(, int,  link_get_max_message_size, LINK_HANDLE, link, uint64_t*, max_message_size);
MOCKABLE_FUNCTION(, int,  link_set_attach_properties, LINK_HANDLE, link, fields, attach_properties);
MOCKABLE_FUNCTION(, int,  link_attach, LINK_HANDLE, link, ON_TRANSFER_RECEIVED, on_transfer_received, ON_LINK_STATE_CHANGED, on_link_state_changed, ON_LINK_FLOW_ON, on_link_flow_on, void*, callback_context);
MOCKABLE_FUNCTION(, int,  link_detach, LINK_HANDLE, link, bool, close);
MOCKABLE_FUNCTION(, LINK_TRANSFER_RESULT, link_transfer, LINK_HANDLE, handle, message_format, message_format, PAYLOAD*, payloads, size_t, payload_count, ON_DELIVERY_SETTLED, on_delivery_settled, void*, callback_context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LINK_H */
