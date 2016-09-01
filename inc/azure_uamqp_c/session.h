// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>
#include "azure_uamqp_c/amqpvalue.h"
#include "azure_uamqp_c/amqp_frame_codec.h"
#include "azure_uamqp_c/connection.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "azure_c_shared_utility/umock_c_prod.h"

	typedef struct SESSION_INSTANCE_TAG* SESSION_HANDLE;
	typedef struct LINK_ENDPOINT_INSTANCE_TAG* LINK_ENDPOINT_HANDLE;

	typedef enum SESION_STATE_TAG
	{
		SESSION_STATE_UNMAPPED,
		SESSION_STATE_BEGIN_SENT,
		SESSION_STATE_BEGIN_RCVD,
		SESSION_STATE_MAPPED,
		SESSION_STATE_END_SENT,
		SESSION_STATE_END_RCVD,
		SESSION_STATE_DISCARDING,
		SESSION_STATE_ERROR
	} SESSION_STATE;

	typedef enum SESSION_SEND_TRANSFER_RESULT_TAG
	{
		SESSION_SEND_TRANSFER_OK,
		SESSION_SEND_TRANSFER_ERROR,
		SESSION_SEND_TRANSFER_BUSY
	} SESSION_SEND_TRANSFER_RESULT;

	typedef void(*LINK_ENDPOINT_FRAME_RECEIVED_CALLBACK)(void* context, AMQP_VALUE performative, uint32_t frame_payload_size, const unsigned char* payload_bytes);
	typedef void(*ON_SESSION_STATE_CHANGED)(void* context, SESSION_STATE new_session_state, SESSION_STATE previous_session_state);
	typedef void(*ON_SESSION_FLOW_ON)(void* context);
	typedef bool(*ON_LINK_ATTACHED)(void* context, LINK_ENDPOINT_HANDLE new_link_endpoint, const char* name, role role, AMQP_VALUE source, AMQP_VALUE target);

	MOCKABLE_FUNCTION(, SESSION_HANDLE, session_create, CONNECTION_HANDLE, connection, ON_LINK_ATTACHED, on_link_attached, void*, callback_context);
	MOCKABLE_FUNCTION(, SESSION_HANDLE, session_create_from_endpoint, CONNECTION_HANDLE, connection, ENDPOINT_HANDLE, connection_endpoint, ON_LINK_ATTACHED, on_link_attached, void*, callback_context);
	MOCKABLE_FUNCTION(, int, session_set_incoming_window, SESSION_HANDLE, session, uint32_t, incoming_window);
	MOCKABLE_FUNCTION(, int, session_get_incoming_window, SESSION_HANDLE, session, uint32_t*, incoming_window);
	MOCKABLE_FUNCTION(, int, session_set_outgoing_window, SESSION_HANDLE, session, uint32_t, outgoing_window);
	MOCKABLE_FUNCTION(, int, session_get_outgoing_window, SESSION_HANDLE, session, uint32_t*, outgoing_window);
	MOCKABLE_FUNCTION(, int, session_set_handle_max, SESSION_HANDLE, session, handle, handle_max);
	MOCKABLE_FUNCTION(, int, session_get_handle_max, SESSION_HANDLE, session, handle*, handle_max);
	MOCKABLE_FUNCTION(, void, session_destroy, SESSION_HANDLE, session);
	MOCKABLE_FUNCTION(, int, session_begin, SESSION_HANDLE, session);
	MOCKABLE_FUNCTION(, int, session_end, SESSION_HANDLE, session, const char*, condition_value, const char*, description);
	MOCKABLE_FUNCTION(, LINK_ENDPOINT_HANDLE, session_create_link_endpoint, SESSION_HANDLE, session, const char*, name);
	MOCKABLE_FUNCTION(, void, session_destroy_link_endpoint, LINK_ENDPOINT_HANDLE, link_endpoint);
	MOCKABLE_FUNCTION(, int, session_start_link_endpoint, LINK_ENDPOINT_HANDLE, link_endpoint, ON_ENDPOINT_FRAME_RECEIVED, frame_received_callback, ON_SESSION_STATE_CHANGED, on_session_state_changed, ON_SESSION_FLOW_ON, on_session_flow_on, void*, context);
	MOCKABLE_FUNCTION(, int, session_send_flow, LINK_ENDPOINT_HANDLE, link_endpoint, FLOW_HANDLE, flow);
	MOCKABLE_FUNCTION(, int, session_send_attach, LINK_ENDPOINT_HANDLE, link_endpoint, ATTACH_HANDLE, attach);
	MOCKABLE_FUNCTION(, int, session_send_disposition, LINK_ENDPOINT_HANDLE, link_endpoint, DISPOSITION_HANDLE, disposition);
	MOCKABLE_FUNCTION(, int, session_send_detach, LINK_ENDPOINT_HANDLE, link_endpoint, DETACH_HANDLE, detach);
	MOCKABLE_FUNCTION(, SESSION_SEND_TRANSFER_RESULT, session_send_transfer, LINK_ENDPOINT_HANDLE, link_endpoint, TRANSFER_HANDLE, transfer, PAYLOAD*, payloads, size_t, payload_count, delivery_number*, delivery_id, ON_SEND_COMPLETE, on_send_complete, void*, callback_context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SESSION_H */
