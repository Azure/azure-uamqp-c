

// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef AMQP_DEFINITIONS_H
#define AMQP_DEFINITIONS_H

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#include "azure_uamqp_c/amqpvalue.h"
#include "azure_c_shared_utility/umock_c_prod.h"

/* role */

/* role */

    typedef bool role;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_role, role, value);


    #define amqpvalue_get_role amqpvalue_get_boolean

    #define role_sender false
    #define role_receiver true

/* sender-settle-mode */

/* sender-settle-mode */

    typedef uint8_t sender_settle_mode;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_sender_settle_mode, sender_settle_mode, value);


    #define amqpvalue_get_sender_settle_mode amqpvalue_get_ubyte

    #define sender_settle_mode_unsettled 0
    #define sender_settle_mode_settled 1
    #define sender_settle_mode_mixed 2

/* receiver-settle-mode */

/* receiver-settle-mode */

    typedef uint8_t receiver_settle_mode;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_receiver_settle_mode, receiver_settle_mode, value);


    #define amqpvalue_get_receiver_settle_mode amqpvalue_get_ubyte

    #define receiver_settle_mode_first 0
    #define receiver_settle_mode_second 1

/* handle */

/* handle */

    typedef uint32_t handle;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_handle, handle, value);


    #define amqpvalue_get_handle amqpvalue_get_uint


/* seconds */

/* seconds */

    typedef uint32_t seconds;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_seconds, seconds, value);


    #define amqpvalue_get_seconds amqpvalue_get_uint


/* milliseconds */

/* milliseconds */

    typedef uint32_t milliseconds;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_milliseconds, milliseconds, value);


    #define amqpvalue_get_milliseconds amqpvalue_get_uint


/* delivery-tag */

/* delivery-tag */

    typedef amqp_binary delivery_tag;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_delivery_tag, delivery_tag, value);


    #define amqpvalue_get_delivery_tag amqpvalue_get_binary


/* sequence-no */

/* sequence-no */

    typedef uint32_t sequence_no;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_sequence_no, sequence_no, value);


    #define amqpvalue_get_sequence_no amqpvalue_get_uint


/* delivery-number */

/* delivery-number */

    typedef sequence_no delivery_number;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_delivery_number, delivery_number, value);


    #define amqpvalue_get_delivery_number amqpvalue_get_sequence_no


/* transfer-number */

/* transfer-number */

    typedef sequence_no transfer_number;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_transfer_number, transfer_number, value);


    #define amqpvalue_get_transfer_number amqpvalue_get_sequence_no


/* message-format */

/* message-format */

    typedef uint32_t message_format;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_message_format, message_format, value);


    #define amqpvalue_get_message_format amqpvalue_get_uint


/* ietf-language-tag */

/* ietf-language-tag */

    typedef const char* ietf_language_tag;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_ietf_language_tag, ietf_language_tag, value);


    #define amqpvalue_get_ietf_language_tag amqpvalue_get_symbol


/* fields */

/* fields */

    typedef AMQP_VALUE fields;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_fields, AMQP_VALUE, value);
    #define fields_clone amqpvalue_clone
    #define fields_destroy amqpvalue_destroy


    #define amqpvalue_get_fields amqpvalue_get_map


/* error */

    typedef struct ERROR_INSTANCE_TAG* ERROR_HANDLE;

    MOCKABLE_FUNCTION(, ERROR_HANDLE, error_create , const char*, condition_value);
    MOCKABLE_FUNCTION(, ERROR_HANDLE, error_clone, ERROR_HANDLE, value);
    MOCKABLE_FUNCTION(, void, error_destroy, ERROR_HANDLE, error);
    MOCKABLE_FUNCTION(, bool, is_error_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_error, AMQP_VALUE, value, ERROR_HANDLE*, ERROR_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_error, ERROR_HANDLE, error);

    MOCKABLE_FUNCTION(, int, error_get_condition, ERROR_HANDLE, error, const char**, condition_value);
    MOCKABLE_FUNCTION(, int, error_set_condition, ERROR_HANDLE, error, const char*, condition_value);
    MOCKABLE_FUNCTION(, int, error_get_description, ERROR_HANDLE, error, const char**, description_value);
    MOCKABLE_FUNCTION(, int, error_set_description, ERROR_HANDLE, error, const char*, description_value);
    MOCKABLE_FUNCTION(, int, error_get_info, ERROR_HANDLE, error, fields*, info_value);
    MOCKABLE_FUNCTION(, int, error_set_info, ERROR_HANDLE, error, fields, info_value);

/* amqp-error */

/* amqp-error */

    typedef const char* amqp_error;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_amqp_error, amqp_error, value);


    #define amqpvalue_get_amqp_error amqpvalue_get_symbol

    #define amqp_error_internal_error "amqp:internal-error"
    #define amqp_error_not_found "amqp:not-found"
    #define amqp_error_unauthorized_access "amqp:unauthorized-access"
    #define amqp_error_decode_error "amqp:decode-error"
    #define amqp_error_resource_limit_exceeded "amqp:resource-limit-exceeded"
    #define amqp_error_not_allowed "amqp:not-allowed"
    #define amqp_error_invalid_field "amqp:invalid-field"
    #define amqp_error_not_implemented "amqp:not-implemented"
    #define amqp_error_resource_locked "amqp:resource-locked"
    #define amqp_error_precondition_failed "amqp:precondition-failed"
    #define amqp_error_resource_deleted "amqp:resource-deleted"
    #define amqp_error_illegal_state "amqp:illegal-state"
    #define amqp_error_frame_size_too_small "amqp:frame-size-too-small"

/* connection-error */

/* connection-error */

    typedef const char* connection_error;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_connection_error, connection_error, value);


    #define amqpvalue_get_connection_error amqpvalue_get_symbol

    #define connection_error_connection_forced "amqp:connection:forced"
    #define connection_error_framing_error "amqp:connection:framing-error"
    #define connection_error_redirect "amqp:connection:redirect"

/* session-error */

/* session-error */

    typedef const char* session_error;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_session_error, session_error, value);


    #define amqpvalue_get_session_error amqpvalue_get_symbol

    #define session_error_window_violation "amqp:session:window-violation"
    #define session_error_errant_link "amqp:session:errant-link"
    #define session_error_handle_in_use "amqp:session:handle-in-use"
    #define session_error_unattached_handle "amqp:session:unattached-handle"

/* link-error */

/* link-error */

    typedef const char* link_error;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_link_error, link_error, value);


    #define amqpvalue_get_link_error amqpvalue_get_symbol

    #define link_error_detach_forced "amqp:link:detach-forced"
    #define link_error_transfer_limit_exceeded "amqp:link:transfer-limit-exceeded"
    #define link_error_message_size_exceeded "amqp:link:message-size-exceeded"
    #define link_error_redirect "amqp:link:redirect"
    #define link_error_stolen "amqp:link:stolen"

/* open */

    typedef struct OPEN_INSTANCE_TAG* OPEN_HANDLE;

    MOCKABLE_FUNCTION(, OPEN_HANDLE, open_create , const char*, container_id_value);
    MOCKABLE_FUNCTION(, OPEN_HANDLE, open_clone, OPEN_HANDLE, value);
    MOCKABLE_FUNCTION(, void, open_destroy, OPEN_HANDLE, open);
    MOCKABLE_FUNCTION(, bool, is_open_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_open, AMQP_VALUE, value, OPEN_HANDLE*, OPEN_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_open, OPEN_HANDLE, open);

    MOCKABLE_FUNCTION(, int, open_get_container_id, OPEN_HANDLE, open, const char**, container_id_value);
    MOCKABLE_FUNCTION(, int, open_set_container_id, OPEN_HANDLE, open, const char*, container_id_value);
    MOCKABLE_FUNCTION(, int, open_get_hostname, OPEN_HANDLE, open, const char**, hostname_value);
    MOCKABLE_FUNCTION(, int, open_set_hostname, OPEN_HANDLE, open, const char*, hostname_value);
    MOCKABLE_FUNCTION(, int, open_get_max_frame_size, OPEN_HANDLE, open, uint32_t*, max_frame_size_value);
    MOCKABLE_FUNCTION(, int, open_set_max_frame_size, OPEN_HANDLE, open, uint32_t, max_frame_size_value);
    MOCKABLE_FUNCTION(, int, open_get_channel_max, OPEN_HANDLE, open, uint16_t*, channel_max_value);
    MOCKABLE_FUNCTION(, int, open_set_channel_max, OPEN_HANDLE, open, uint16_t, channel_max_value);
    MOCKABLE_FUNCTION(, int, open_get_idle_time_out, OPEN_HANDLE, open, milliseconds*, idle_time_out_value);
    MOCKABLE_FUNCTION(, int, open_set_idle_time_out, OPEN_HANDLE, open, milliseconds, idle_time_out_value);
    MOCKABLE_FUNCTION(, int, open_get_outgoing_locales, OPEN_HANDLE, open, AMQP_VALUE*, outgoing_locales_value);
    MOCKABLE_FUNCTION(, int, open_set_outgoing_locales, OPEN_HANDLE, open, AMQP_VALUE, outgoing_locales_value);
    MOCKABLE_FUNCTION(, int, open_get_incoming_locales, OPEN_HANDLE, open, AMQP_VALUE*, incoming_locales_value);
    MOCKABLE_FUNCTION(, int, open_set_incoming_locales, OPEN_HANDLE, open, AMQP_VALUE, incoming_locales_value);
    MOCKABLE_FUNCTION(, int, open_get_offered_capabilities, OPEN_HANDLE, open, AMQP_VALUE*, offered_capabilities_value);
    MOCKABLE_FUNCTION(, int, open_set_offered_capabilities, OPEN_HANDLE, open, AMQP_VALUE, offered_capabilities_value);
    MOCKABLE_FUNCTION(, int, open_get_desired_capabilities, OPEN_HANDLE, open, AMQP_VALUE*, desired_capabilities_value);
    MOCKABLE_FUNCTION(, int, open_set_desired_capabilities, OPEN_HANDLE, open, AMQP_VALUE, desired_capabilities_value);
    MOCKABLE_FUNCTION(, int, open_get_properties, OPEN_HANDLE, open, fields*, properties_value);
    MOCKABLE_FUNCTION(, int, open_set_properties, OPEN_HANDLE, open, fields, properties_value);

/* begin */

    typedef struct BEGIN_INSTANCE_TAG* BEGIN_HANDLE;

    MOCKABLE_FUNCTION(, BEGIN_HANDLE, begin_create , transfer_number, next_outgoing_id_value, uint32_t, incoming_window_value, uint32_t, outgoing_window_value);
    MOCKABLE_FUNCTION(, BEGIN_HANDLE, begin_clone, BEGIN_HANDLE, value);
    MOCKABLE_FUNCTION(, void, begin_destroy, BEGIN_HANDLE, begin);
    MOCKABLE_FUNCTION(, bool, is_begin_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_begin, AMQP_VALUE, value, BEGIN_HANDLE*, BEGIN_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_begin, BEGIN_HANDLE, begin);

    MOCKABLE_FUNCTION(, int, begin_get_remote_channel, BEGIN_HANDLE, begin, uint16_t*, remote_channel_value);
    MOCKABLE_FUNCTION(, int, begin_set_remote_channel, BEGIN_HANDLE, begin, uint16_t, remote_channel_value);
    MOCKABLE_FUNCTION(, int, begin_get_next_outgoing_id, BEGIN_HANDLE, begin, transfer_number*, next_outgoing_id_value);
    MOCKABLE_FUNCTION(, int, begin_set_next_outgoing_id, BEGIN_HANDLE, begin, transfer_number, next_outgoing_id_value);
    MOCKABLE_FUNCTION(, int, begin_get_incoming_window, BEGIN_HANDLE, begin, uint32_t*, incoming_window_value);
    MOCKABLE_FUNCTION(, int, begin_set_incoming_window, BEGIN_HANDLE, begin, uint32_t, incoming_window_value);
    MOCKABLE_FUNCTION(, int, begin_get_outgoing_window, BEGIN_HANDLE, begin, uint32_t*, outgoing_window_value);
    MOCKABLE_FUNCTION(, int, begin_set_outgoing_window, BEGIN_HANDLE, begin, uint32_t, outgoing_window_value);
    MOCKABLE_FUNCTION(, int, begin_get_handle_max, BEGIN_HANDLE, begin, handle*, handle_max_value);
    MOCKABLE_FUNCTION(, int, begin_set_handle_max, BEGIN_HANDLE, begin, handle, handle_max_value);
    MOCKABLE_FUNCTION(, int, begin_get_offered_capabilities, BEGIN_HANDLE, begin, AMQP_VALUE*, offered_capabilities_value);
    MOCKABLE_FUNCTION(, int, begin_set_offered_capabilities, BEGIN_HANDLE, begin, AMQP_VALUE, offered_capabilities_value);
    MOCKABLE_FUNCTION(, int, begin_get_desired_capabilities, BEGIN_HANDLE, begin, AMQP_VALUE*, desired_capabilities_value);
    MOCKABLE_FUNCTION(, int, begin_set_desired_capabilities, BEGIN_HANDLE, begin, AMQP_VALUE, desired_capabilities_value);
    MOCKABLE_FUNCTION(, int, begin_get_properties, BEGIN_HANDLE, begin, fields*, properties_value);
    MOCKABLE_FUNCTION(, int, begin_set_properties, BEGIN_HANDLE, begin, fields, properties_value);

/* attach */

    typedef struct ATTACH_INSTANCE_TAG* ATTACH_HANDLE;

    MOCKABLE_FUNCTION(, ATTACH_HANDLE, attach_create , const char*, name_value, handle, handle_value, role, role_value);
    MOCKABLE_FUNCTION(, ATTACH_HANDLE, attach_clone, ATTACH_HANDLE, value);
    MOCKABLE_FUNCTION(, void, attach_destroy, ATTACH_HANDLE, attach);
    MOCKABLE_FUNCTION(, bool, is_attach_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_attach, AMQP_VALUE, value, ATTACH_HANDLE*, ATTACH_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_attach, ATTACH_HANDLE, attach);

    MOCKABLE_FUNCTION(, int, attach_get_name, ATTACH_HANDLE, attach, const char**, name_value);
    MOCKABLE_FUNCTION(, int, attach_set_name, ATTACH_HANDLE, attach, const char*, name_value);
    MOCKABLE_FUNCTION(, int, attach_get_handle, ATTACH_HANDLE, attach, handle*, handle_value);
    MOCKABLE_FUNCTION(, int, attach_set_handle, ATTACH_HANDLE, attach, handle, handle_value);
    MOCKABLE_FUNCTION(, int, attach_get_role, ATTACH_HANDLE, attach, role*, role_value);
    MOCKABLE_FUNCTION(, int, attach_set_role, ATTACH_HANDLE, attach, role, role_value);
    MOCKABLE_FUNCTION(, int, attach_get_snd_settle_mode, ATTACH_HANDLE, attach, sender_settle_mode*, snd_settle_mode_value);
    MOCKABLE_FUNCTION(, int, attach_set_snd_settle_mode, ATTACH_HANDLE, attach, sender_settle_mode, snd_settle_mode_value);
    MOCKABLE_FUNCTION(, int, attach_get_rcv_settle_mode, ATTACH_HANDLE, attach, receiver_settle_mode*, rcv_settle_mode_value);
    MOCKABLE_FUNCTION(, int, attach_set_rcv_settle_mode, ATTACH_HANDLE, attach, receiver_settle_mode, rcv_settle_mode_value);
    MOCKABLE_FUNCTION(, int, attach_get_source, ATTACH_HANDLE, attach, AMQP_VALUE*, source_value);
    MOCKABLE_FUNCTION(, int, attach_set_source, ATTACH_HANDLE, attach, AMQP_VALUE, source_value);
    MOCKABLE_FUNCTION(, int, attach_get_target, ATTACH_HANDLE, attach, AMQP_VALUE*, target_value);
    MOCKABLE_FUNCTION(, int, attach_set_target, ATTACH_HANDLE, attach, AMQP_VALUE, target_value);
    MOCKABLE_FUNCTION(, int, attach_get_unsettled, ATTACH_HANDLE, attach, AMQP_VALUE*, unsettled_value);
    MOCKABLE_FUNCTION(, int, attach_set_unsettled, ATTACH_HANDLE, attach, AMQP_VALUE, unsettled_value);
    MOCKABLE_FUNCTION(, int, attach_get_incomplete_unsettled, ATTACH_HANDLE, attach, bool*, incomplete_unsettled_value);
    MOCKABLE_FUNCTION(, int, attach_set_incomplete_unsettled, ATTACH_HANDLE, attach, bool, incomplete_unsettled_value);
    MOCKABLE_FUNCTION(, int, attach_get_initial_delivery_count, ATTACH_HANDLE, attach, sequence_no*, initial_delivery_count_value);
    MOCKABLE_FUNCTION(, int, attach_set_initial_delivery_count, ATTACH_HANDLE, attach, sequence_no, initial_delivery_count_value);
    MOCKABLE_FUNCTION(, int, attach_get_max_message_size, ATTACH_HANDLE, attach, uint64_t*, max_message_size_value);
    MOCKABLE_FUNCTION(, int, attach_set_max_message_size, ATTACH_HANDLE, attach, uint64_t, max_message_size_value);
    MOCKABLE_FUNCTION(, int, attach_get_offered_capabilities, ATTACH_HANDLE, attach, AMQP_VALUE*, offered_capabilities_value);
    MOCKABLE_FUNCTION(, int, attach_set_offered_capabilities, ATTACH_HANDLE, attach, AMQP_VALUE, offered_capabilities_value);
    MOCKABLE_FUNCTION(, int, attach_get_desired_capabilities, ATTACH_HANDLE, attach, AMQP_VALUE*, desired_capabilities_value);
    MOCKABLE_FUNCTION(, int, attach_set_desired_capabilities, ATTACH_HANDLE, attach, AMQP_VALUE, desired_capabilities_value);
    MOCKABLE_FUNCTION(, int, attach_get_properties, ATTACH_HANDLE, attach, fields*, properties_value);
    MOCKABLE_FUNCTION(, int, attach_set_properties, ATTACH_HANDLE, attach, fields, properties_value);

/* flow */

    typedef struct FLOW_INSTANCE_TAG* FLOW_HANDLE;

    MOCKABLE_FUNCTION(, FLOW_HANDLE, flow_create , uint32_t, incoming_window_value, transfer_number, next_outgoing_id_value, uint32_t, outgoing_window_value);
    MOCKABLE_FUNCTION(, FLOW_HANDLE, flow_clone, FLOW_HANDLE, value);
    MOCKABLE_FUNCTION(, void, flow_destroy, FLOW_HANDLE, flow);
    MOCKABLE_FUNCTION(, bool, is_flow_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_flow, AMQP_VALUE, value, FLOW_HANDLE*, FLOW_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_flow, FLOW_HANDLE, flow);

    MOCKABLE_FUNCTION(, int, flow_get_next_incoming_id, FLOW_HANDLE, flow, transfer_number*, next_incoming_id_value);
    MOCKABLE_FUNCTION(, int, flow_set_next_incoming_id, FLOW_HANDLE, flow, transfer_number, next_incoming_id_value);
    MOCKABLE_FUNCTION(, int, flow_get_incoming_window, FLOW_HANDLE, flow, uint32_t*, incoming_window_value);
    MOCKABLE_FUNCTION(, int, flow_set_incoming_window, FLOW_HANDLE, flow, uint32_t, incoming_window_value);
    MOCKABLE_FUNCTION(, int, flow_get_next_outgoing_id, FLOW_HANDLE, flow, transfer_number*, next_outgoing_id_value);
    MOCKABLE_FUNCTION(, int, flow_set_next_outgoing_id, FLOW_HANDLE, flow, transfer_number, next_outgoing_id_value);
    MOCKABLE_FUNCTION(, int, flow_get_outgoing_window, FLOW_HANDLE, flow, uint32_t*, outgoing_window_value);
    MOCKABLE_FUNCTION(, int, flow_set_outgoing_window, FLOW_HANDLE, flow, uint32_t, outgoing_window_value);
    MOCKABLE_FUNCTION(, int, flow_get_handle, FLOW_HANDLE, flow, handle*, handle_value);
    MOCKABLE_FUNCTION(, int, flow_set_handle, FLOW_HANDLE, flow, handle, handle_value);
    MOCKABLE_FUNCTION(, int, flow_get_delivery_count, FLOW_HANDLE, flow, sequence_no*, delivery_count_value);
    MOCKABLE_FUNCTION(, int, flow_set_delivery_count, FLOW_HANDLE, flow, sequence_no, delivery_count_value);
    MOCKABLE_FUNCTION(, int, flow_get_link_credit, FLOW_HANDLE, flow, uint32_t*, link_credit_value);
    MOCKABLE_FUNCTION(, int, flow_set_link_credit, FLOW_HANDLE, flow, uint32_t, link_credit_value);
    MOCKABLE_FUNCTION(, int, flow_get_available, FLOW_HANDLE, flow, uint32_t*, available_value);
    MOCKABLE_FUNCTION(, int, flow_set_available, FLOW_HANDLE, flow, uint32_t, available_value);
    MOCKABLE_FUNCTION(, int, flow_get_drain, FLOW_HANDLE, flow, bool*, drain_value);
    MOCKABLE_FUNCTION(, int, flow_set_drain, FLOW_HANDLE, flow, bool, drain_value);
    MOCKABLE_FUNCTION(, int, flow_get_echo, FLOW_HANDLE, flow, bool*, echo_value);
    MOCKABLE_FUNCTION(, int, flow_set_echo, FLOW_HANDLE, flow, bool, echo_value);
    MOCKABLE_FUNCTION(, int, flow_get_properties, FLOW_HANDLE, flow, fields*, properties_value);
    MOCKABLE_FUNCTION(, int, flow_set_properties, FLOW_HANDLE, flow, fields, properties_value);

/* transfer */

    typedef struct TRANSFER_INSTANCE_TAG* TRANSFER_HANDLE;

    MOCKABLE_FUNCTION(, TRANSFER_HANDLE, transfer_create , handle, handle_value);
    MOCKABLE_FUNCTION(, TRANSFER_HANDLE, transfer_clone, TRANSFER_HANDLE, value);
    MOCKABLE_FUNCTION(, void, transfer_destroy, TRANSFER_HANDLE, transfer);
    MOCKABLE_FUNCTION(, bool, is_transfer_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_transfer, AMQP_VALUE, value, TRANSFER_HANDLE*, TRANSFER_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_transfer, TRANSFER_HANDLE, transfer);

    MOCKABLE_FUNCTION(, int, transfer_get_handle, TRANSFER_HANDLE, transfer, handle*, handle_value);
    MOCKABLE_FUNCTION(, int, transfer_set_handle, TRANSFER_HANDLE, transfer, handle, handle_value);
    MOCKABLE_FUNCTION(, int, transfer_get_delivery_id, TRANSFER_HANDLE, transfer, delivery_number*, delivery_id_value);
    MOCKABLE_FUNCTION(, int, transfer_set_delivery_id, TRANSFER_HANDLE, transfer, delivery_number, delivery_id_value);
    MOCKABLE_FUNCTION(, int, transfer_get_delivery_tag, TRANSFER_HANDLE, transfer, delivery_tag*, delivery_tag_value);
    MOCKABLE_FUNCTION(, int, transfer_set_delivery_tag, TRANSFER_HANDLE, transfer, delivery_tag, delivery_tag_value);
    MOCKABLE_FUNCTION(, int, transfer_get_message_format, TRANSFER_HANDLE, transfer, message_format*, message_format_value);
    MOCKABLE_FUNCTION(, int, transfer_set_message_format, TRANSFER_HANDLE, transfer, message_format, message_format_value);
    MOCKABLE_FUNCTION(, int, transfer_get_settled, TRANSFER_HANDLE, transfer, bool*, settled_value);
    MOCKABLE_FUNCTION(, int, transfer_set_settled, TRANSFER_HANDLE, transfer, bool, settled_value);
    MOCKABLE_FUNCTION(, int, transfer_get_more, TRANSFER_HANDLE, transfer, bool*, more_value);
    MOCKABLE_FUNCTION(, int, transfer_set_more, TRANSFER_HANDLE, transfer, bool, more_value);
    MOCKABLE_FUNCTION(, int, transfer_get_rcv_settle_mode, TRANSFER_HANDLE, transfer, receiver_settle_mode*, rcv_settle_mode_value);
    MOCKABLE_FUNCTION(, int, transfer_set_rcv_settle_mode, TRANSFER_HANDLE, transfer, receiver_settle_mode, rcv_settle_mode_value);
    MOCKABLE_FUNCTION(, int, transfer_get_state, TRANSFER_HANDLE, transfer, AMQP_VALUE*, state_value);
    MOCKABLE_FUNCTION(, int, transfer_set_state, TRANSFER_HANDLE, transfer, AMQP_VALUE, state_value);
    MOCKABLE_FUNCTION(, int, transfer_get_resume, TRANSFER_HANDLE, transfer, bool*, resume_value);
    MOCKABLE_FUNCTION(, int, transfer_set_resume, TRANSFER_HANDLE, transfer, bool, resume_value);
    MOCKABLE_FUNCTION(, int, transfer_get_aborted, TRANSFER_HANDLE, transfer, bool*, aborted_value);
    MOCKABLE_FUNCTION(, int, transfer_set_aborted, TRANSFER_HANDLE, transfer, bool, aborted_value);
    MOCKABLE_FUNCTION(, int, transfer_get_batchable, TRANSFER_HANDLE, transfer, bool*, batchable_value);
    MOCKABLE_FUNCTION(, int, transfer_set_batchable, TRANSFER_HANDLE, transfer, bool, batchable_value);

/* disposition */

    typedef struct DISPOSITION_INSTANCE_TAG* DISPOSITION_HANDLE;

    MOCKABLE_FUNCTION(, DISPOSITION_HANDLE, disposition_create , role, role_value, delivery_number, first_value);
    MOCKABLE_FUNCTION(, DISPOSITION_HANDLE, disposition_clone, DISPOSITION_HANDLE, value);
    MOCKABLE_FUNCTION(, void, disposition_destroy, DISPOSITION_HANDLE, disposition);
    MOCKABLE_FUNCTION(, bool, is_disposition_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_disposition, AMQP_VALUE, value, DISPOSITION_HANDLE*, DISPOSITION_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_disposition, DISPOSITION_HANDLE, disposition);

    MOCKABLE_FUNCTION(, int, disposition_get_role, DISPOSITION_HANDLE, disposition, role*, role_value);
    MOCKABLE_FUNCTION(, int, disposition_set_role, DISPOSITION_HANDLE, disposition, role, role_value);
    MOCKABLE_FUNCTION(, int, disposition_get_first, DISPOSITION_HANDLE, disposition, delivery_number*, first_value);
    MOCKABLE_FUNCTION(, int, disposition_set_first, DISPOSITION_HANDLE, disposition, delivery_number, first_value);
    MOCKABLE_FUNCTION(, int, disposition_get_last, DISPOSITION_HANDLE, disposition, delivery_number*, last_value);
    MOCKABLE_FUNCTION(, int, disposition_set_last, DISPOSITION_HANDLE, disposition, delivery_number, last_value);
    MOCKABLE_FUNCTION(, int, disposition_get_settled, DISPOSITION_HANDLE, disposition, bool*, settled_value);
    MOCKABLE_FUNCTION(, int, disposition_set_settled, DISPOSITION_HANDLE, disposition, bool, settled_value);
    MOCKABLE_FUNCTION(, int, disposition_get_state, DISPOSITION_HANDLE, disposition, AMQP_VALUE*, state_value);
    MOCKABLE_FUNCTION(, int, disposition_set_state, DISPOSITION_HANDLE, disposition, AMQP_VALUE, state_value);
    MOCKABLE_FUNCTION(, int, disposition_get_batchable, DISPOSITION_HANDLE, disposition, bool*, batchable_value);
    MOCKABLE_FUNCTION(, int, disposition_set_batchable, DISPOSITION_HANDLE, disposition, bool, batchable_value);

/* detach */

    typedef struct DETACH_INSTANCE_TAG* DETACH_HANDLE;

    MOCKABLE_FUNCTION(, DETACH_HANDLE, detach_create , handle, handle_value);
    MOCKABLE_FUNCTION(, DETACH_HANDLE, detach_clone, DETACH_HANDLE, value);
    MOCKABLE_FUNCTION(, void, detach_destroy, DETACH_HANDLE, detach);
    MOCKABLE_FUNCTION(, bool, is_detach_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_detach, AMQP_VALUE, value, DETACH_HANDLE*, DETACH_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_detach, DETACH_HANDLE, detach);

    MOCKABLE_FUNCTION(, int, detach_get_handle, DETACH_HANDLE, detach, handle*, handle_value);
    MOCKABLE_FUNCTION(, int, detach_set_handle, DETACH_HANDLE, detach, handle, handle_value);
    MOCKABLE_FUNCTION(, int, detach_get_closed, DETACH_HANDLE, detach, bool*, closed_value);
    MOCKABLE_FUNCTION(, int, detach_set_closed, DETACH_HANDLE, detach, bool, closed_value);
    MOCKABLE_FUNCTION(, int, detach_get_error, DETACH_HANDLE, detach, ERROR_HANDLE*, error_value);
    MOCKABLE_FUNCTION(, int, detach_set_error, DETACH_HANDLE, detach, ERROR_HANDLE, error_value);

/* end */

    typedef struct END_INSTANCE_TAG* END_HANDLE;

    MOCKABLE_FUNCTION(, END_HANDLE, end_create );
    MOCKABLE_FUNCTION(, END_HANDLE, end_clone, END_HANDLE, value);
    MOCKABLE_FUNCTION(, void, end_destroy, END_HANDLE, end);
    MOCKABLE_FUNCTION(, bool, is_end_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_end, AMQP_VALUE, value, END_HANDLE*, END_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_end, END_HANDLE, end);

    MOCKABLE_FUNCTION(, int, end_get_error, END_HANDLE, end, ERROR_HANDLE*, error_value);
    MOCKABLE_FUNCTION(, int, end_set_error, END_HANDLE, end, ERROR_HANDLE, error_value);

/* close */

    typedef struct CLOSE_INSTANCE_TAG* CLOSE_HANDLE;

    MOCKABLE_FUNCTION(, CLOSE_HANDLE, close_create );
    MOCKABLE_FUNCTION(, CLOSE_HANDLE, close_clone, CLOSE_HANDLE, value);
    MOCKABLE_FUNCTION(, void, close_destroy, CLOSE_HANDLE, close);
    MOCKABLE_FUNCTION(, bool, is_close_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_close, AMQP_VALUE, value, CLOSE_HANDLE*, CLOSE_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_close, CLOSE_HANDLE, close);

    MOCKABLE_FUNCTION(, int, close_get_error, CLOSE_HANDLE, close, ERROR_HANDLE*, error_value);
    MOCKABLE_FUNCTION(, int, close_set_error, CLOSE_HANDLE, close, ERROR_HANDLE, error_value);

/* sasl-code */

/* sasl-code */

    typedef uint8_t sasl_code;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_sasl_code, sasl_code, value);


    #define amqpvalue_get_sasl_code amqpvalue_get_ubyte

    #define sasl_code_ok 0
    #define sasl_code_auth 1
    #define sasl_code_sys 2
    #define sasl_code_sys_perm 3
    #define sasl_code_sys_temp 4

/* sasl-mechanisms */

    typedef struct SASL_MECHANISMS_INSTANCE_TAG* SASL_MECHANISMS_HANDLE;

    MOCKABLE_FUNCTION(, SASL_MECHANISMS_HANDLE, sasl_mechanisms_create , AMQP_VALUE, sasl_server_mechanisms_value);
    MOCKABLE_FUNCTION(, SASL_MECHANISMS_HANDLE, sasl_mechanisms_clone, SASL_MECHANISMS_HANDLE, value);
    MOCKABLE_FUNCTION(, void, sasl_mechanisms_destroy, SASL_MECHANISMS_HANDLE, sasl_mechanisms);
    MOCKABLE_FUNCTION(, bool, is_sasl_mechanisms_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_sasl_mechanisms, AMQP_VALUE, value, SASL_MECHANISMS_HANDLE*, SASL_MECHANISMS_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_sasl_mechanisms, SASL_MECHANISMS_HANDLE, sasl_mechanisms);

    MOCKABLE_FUNCTION(, int, sasl_mechanisms_get_sasl_server_mechanisms, SASL_MECHANISMS_HANDLE, sasl_mechanisms, AMQP_VALUE*, sasl_server_mechanisms_value);
    MOCKABLE_FUNCTION(, int, sasl_mechanisms_set_sasl_server_mechanisms, SASL_MECHANISMS_HANDLE, sasl_mechanisms, AMQP_VALUE, sasl_server_mechanisms_value);

/* sasl-init */

    typedef struct SASL_INIT_INSTANCE_TAG* SASL_INIT_HANDLE;

    MOCKABLE_FUNCTION(, SASL_INIT_HANDLE, sasl_init_create , const char*, mechanism_value);
    MOCKABLE_FUNCTION(, SASL_INIT_HANDLE, sasl_init_clone, SASL_INIT_HANDLE, value);
    MOCKABLE_FUNCTION(, void, sasl_init_destroy, SASL_INIT_HANDLE, sasl_init);
    MOCKABLE_FUNCTION(, bool, is_sasl_init_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_sasl_init, AMQP_VALUE, value, SASL_INIT_HANDLE*, SASL_INIT_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_sasl_init, SASL_INIT_HANDLE, sasl_init);

    MOCKABLE_FUNCTION(, int, sasl_init_get_mechanism, SASL_INIT_HANDLE, sasl_init, const char**, mechanism_value);
    MOCKABLE_FUNCTION(, int, sasl_init_set_mechanism, SASL_INIT_HANDLE, sasl_init, const char*, mechanism_value);
    MOCKABLE_FUNCTION(, int, sasl_init_get_initial_response, SASL_INIT_HANDLE, sasl_init, amqp_binary*, initial_response_value);
    MOCKABLE_FUNCTION(, int, sasl_init_set_initial_response, SASL_INIT_HANDLE, sasl_init, amqp_binary, initial_response_value);
    MOCKABLE_FUNCTION(, int, sasl_init_get_hostname, SASL_INIT_HANDLE, sasl_init, const char**, hostname_value);
    MOCKABLE_FUNCTION(, int, sasl_init_set_hostname, SASL_INIT_HANDLE, sasl_init, const char*, hostname_value);

/* sasl-challenge */

    typedef struct SASL_CHALLENGE_INSTANCE_TAG* SASL_CHALLENGE_HANDLE;

    MOCKABLE_FUNCTION(, SASL_CHALLENGE_HANDLE, sasl_challenge_create , amqp_binary, challenge_value);
    MOCKABLE_FUNCTION(, SASL_CHALLENGE_HANDLE, sasl_challenge_clone, SASL_CHALLENGE_HANDLE, value);
    MOCKABLE_FUNCTION(, void, sasl_challenge_destroy, SASL_CHALLENGE_HANDLE, sasl_challenge);
    MOCKABLE_FUNCTION(, bool, is_sasl_challenge_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_sasl_challenge, AMQP_VALUE, value, SASL_CHALLENGE_HANDLE*, SASL_CHALLENGE_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_sasl_challenge, SASL_CHALLENGE_HANDLE, sasl_challenge);

    MOCKABLE_FUNCTION(, int, sasl_challenge_get_challenge, SASL_CHALLENGE_HANDLE, sasl_challenge, amqp_binary*, challenge_value);
    MOCKABLE_FUNCTION(, int, sasl_challenge_set_challenge, SASL_CHALLENGE_HANDLE, sasl_challenge, amqp_binary, challenge_value);

/* sasl-response */

    typedef struct SASL_RESPONSE_INSTANCE_TAG* SASL_RESPONSE_HANDLE;

    MOCKABLE_FUNCTION(, SASL_RESPONSE_HANDLE, sasl_response_create , amqp_binary, response_value);
    MOCKABLE_FUNCTION(, SASL_RESPONSE_HANDLE, sasl_response_clone, SASL_RESPONSE_HANDLE, value);
    MOCKABLE_FUNCTION(, void, sasl_response_destroy, SASL_RESPONSE_HANDLE, sasl_response);
    MOCKABLE_FUNCTION(, bool, is_sasl_response_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_sasl_response, AMQP_VALUE, value, SASL_RESPONSE_HANDLE*, SASL_RESPONSE_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_sasl_response, SASL_RESPONSE_HANDLE, sasl_response);

    MOCKABLE_FUNCTION(, int, sasl_response_get_response, SASL_RESPONSE_HANDLE, sasl_response, amqp_binary*, response_value);
    MOCKABLE_FUNCTION(, int, sasl_response_set_response, SASL_RESPONSE_HANDLE, sasl_response, amqp_binary, response_value);

/* sasl-outcome */

    typedef struct SASL_OUTCOME_INSTANCE_TAG* SASL_OUTCOME_HANDLE;

    MOCKABLE_FUNCTION(, SASL_OUTCOME_HANDLE, sasl_outcome_create , sasl_code, code_value);
    MOCKABLE_FUNCTION(, SASL_OUTCOME_HANDLE, sasl_outcome_clone, SASL_OUTCOME_HANDLE, value);
    MOCKABLE_FUNCTION(, void, sasl_outcome_destroy, SASL_OUTCOME_HANDLE, sasl_outcome);
    MOCKABLE_FUNCTION(, bool, is_sasl_outcome_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_sasl_outcome, AMQP_VALUE, value, SASL_OUTCOME_HANDLE*, SASL_OUTCOME_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_sasl_outcome, SASL_OUTCOME_HANDLE, sasl_outcome);

    MOCKABLE_FUNCTION(, int, sasl_outcome_get_code, SASL_OUTCOME_HANDLE, sasl_outcome, sasl_code*, code_value);
    MOCKABLE_FUNCTION(, int, sasl_outcome_set_code, SASL_OUTCOME_HANDLE, sasl_outcome, sasl_code, code_value);
    MOCKABLE_FUNCTION(, int, sasl_outcome_get_additional_data, SASL_OUTCOME_HANDLE, sasl_outcome, amqp_binary*, additional_data_value);
    MOCKABLE_FUNCTION(, int, sasl_outcome_set_additional_data, SASL_OUTCOME_HANDLE, sasl_outcome, amqp_binary, additional_data_value);

/* terminus-durability */

/* terminus-durability */

    typedef uint32_t terminus_durability;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_terminus_durability, terminus_durability, value);


    #define amqpvalue_get_terminus_durability amqpvalue_get_uint

    #define terminus_durability_none 0
    #define terminus_durability_configuration 1
    #define terminus_durability_unsettled_state 2

/* terminus-expiry-policy */

/* terminus-expiry-policy */

    typedef const char* terminus_expiry_policy;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_terminus_expiry_policy, terminus_expiry_policy, value);


    #define amqpvalue_get_terminus_expiry_policy amqpvalue_get_symbol

    #define terminus_expiry_policy_link_detach "link-detach"
    #define terminus_expiry_policy_session_end "session-end"
    #define terminus_expiry_policy_connection_close "connection-close"
    #define terminus_expiry_policy_never "never"

/* node-properties */

/* node-properties */

    typedef fields node_properties;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_node_properties, node_properties, value);


    #define amqpvalue_get_node_properties amqpvalue_get_fields


/* filter-set */

/* filter-set */

    typedef AMQP_VALUE filter_set;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_filter_set, AMQP_VALUE, value);
    #define filter_set_clone amqpvalue_clone
    #define filter_set_destroy amqpvalue_destroy


    #define amqpvalue_get_filter_set amqpvalue_get_map


/* source */

    typedef struct SOURCE_INSTANCE_TAG* SOURCE_HANDLE;

    MOCKABLE_FUNCTION(, SOURCE_HANDLE, source_create );
    MOCKABLE_FUNCTION(, SOURCE_HANDLE, source_clone, SOURCE_HANDLE, value);
    MOCKABLE_FUNCTION(, void, source_destroy, SOURCE_HANDLE, source);
    MOCKABLE_FUNCTION(, bool, is_source_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_source, AMQP_VALUE, value, SOURCE_HANDLE*, SOURCE_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_source, SOURCE_HANDLE, source);

    MOCKABLE_FUNCTION(, int, source_get_address, SOURCE_HANDLE, source, AMQP_VALUE*, address_value);
    MOCKABLE_FUNCTION(, int, source_set_address, SOURCE_HANDLE, source, AMQP_VALUE, address_value);
    MOCKABLE_FUNCTION(, int, source_get_durable, SOURCE_HANDLE, source, terminus_durability*, durable_value);
    MOCKABLE_FUNCTION(, int, source_set_durable, SOURCE_HANDLE, source, terminus_durability, durable_value);
    MOCKABLE_FUNCTION(, int, source_get_expiry_policy, SOURCE_HANDLE, source, terminus_expiry_policy*, expiry_policy_value);
    MOCKABLE_FUNCTION(, int, source_set_expiry_policy, SOURCE_HANDLE, source, terminus_expiry_policy, expiry_policy_value);
    MOCKABLE_FUNCTION(, int, source_get_timeout, SOURCE_HANDLE, source, seconds*, timeout_value);
    MOCKABLE_FUNCTION(, int, source_set_timeout, SOURCE_HANDLE, source, seconds, timeout_value);
    MOCKABLE_FUNCTION(, int, source_get_dynamic, SOURCE_HANDLE, source, bool*, dynamic_value);
    MOCKABLE_FUNCTION(, int, source_set_dynamic, SOURCE_HANDLE, source, bool, dynamic_value);
    MOCKABLE_FUNCTION(, int, source_get_dynamic_node_properties, SOURCE_HANDLE, source, node_properties*, dynamic_node_properties_value);
    MOCKABLE_FUNCTION(, int, source_set_dynamic_node_properties, SOURCE_HANDLE, source, node_properties, dynamic_node_properties_value);
    MOCKABLE_FUNCTION(, int, source_get_distribution_mode, SOURCE_HANDLE, source, const char**, distribution_mode_value);
    MOCKABLE_FUNCTION(, int, source_set_distribution_mode, SOURCE_HANDLE, source, const char*, distribution_mode_value);
    MOCKABLE_FUNCTION(, int, source_get_filter, SOURCE_HANDLE, source, filter_set*, filter_value);
    MOCKABLE_FUNCTION(, int, source_set_filter, SOURCE_HANDLE, source, filter_set, filter_value);
    MOCKABLE_FUNCTION(, int, source_get_default_outcome, SOURCE_HANDLE, source, AMQP_VALUE*, default_outcome_value);
    MOCKABLE_FUNCTION(, int, source_set_default_outcome, SOURCE_HANDLE, source, AMQP_VALUE, default_outcome_value);
    MOCKABLE_FUNCTION(, int, source_get_outcomes, SOURCE_HANDLE, source, AMQP_VALUE*, outcomes_value);
    MOCKABLE_FUNCTION(, int, source_set_outcomes, SOURCE_HANDLE, source, AMQP_VALUE, outcomes_value);
    MOCKABLE_FUNCTION(, int, source_get_capabilities, SOURCE_HANDLE, source, AMQP_VALUE*, capabilities_value);
    MOCKABLE_FUNCTION(, int, source_set_capabilities, SOURCE_HANDLE, source, AMQP_VALUE, capabilities_value);

/* target */

    typedef struct TARGET_INSTANCE_TAG* TARGET_HANDLE;

    MOCKABLE_FUNCTION(, TARGET_HANDLE, target_create );
    MOCKABLE_FUNCTION(, TARGET_HANDLE, target_clone, TARGET_HANDLE, value);
    MOCKABLE_FUNCTION(, void, target_destroy, TARGET_HANDLE, target);
    MOCKABLE_FUNCTION(, bool, is_target_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_target, AMQP_VALUE, value, TARGET_HANDLE*, TARGET_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_target, TARGET_HANDLE, target);

    MOCKABLE_FUNCTION(, int, target_get_address, TARGET_HANDLE, target, AMQP_VALUE*, address_value);
    MOCKABLE_FUNCTION(, int, target_set_address, TARGET_HANDLE, target, AMQP_VALUE, address_value);
    MOCKABLE_FUNCTION(, int, target_get_durable, TARGET_HANDLE, target, terminus_durability*, durable_value);
    MOCKABLE_FUNCTION(, int, target_set_durable, TARGET_HANDLE, target, terminus_durability, durable_value);
    MOCKABLE_FUNCTION(, int, target_get_expiry_policy, TARGET_HANDLE, target, terminus_expiry_policy*, expiry_policy_value);
    MOCKABLE_FUNCTION(, int, target_set_expiry_policy, TARGET_HANDLE, target, terminus_expiry_policy, expiry_policy_value);
    MOCKABLE_FUNCTION(, int, target_get_timeout, TARGET_HANDLE, target, seconds*, timeout_value);
    MOCKABLE_FUNCTION(, int, target_set_timeout, TARGET_HANDLE, target, seconds, timeout_value);
    MOCKABLE_FUNCTION(, int, target_get_dynamic, TARGET_HANDLE, target, bool*, dynamic_value);
    MOCKABLE_FUNCTION(, int, target_set_dynamic, TARGET_HANDLE, target, bool, dynamic_value);
    MOCKABLE_FUNCTION(, int, target_get_dynamic_node_properties, TARGET_HANDLE, target, node_properties*, dynamic_node_properties_value);
    MOCKABLE_FUNCTION(, int, target_set_dynamic_node_properties, TARGET_HANDLE, target, node_properties, dynamic_node_properties_value);
    MOCKABLE_FUNCTION(, int, target_get_capabilities, TARGET_HANDLE, target, AMQP_VALUE*, capabilities_value);
    MOCKABLE_FUNCTION(, int, target_set_capabilities, TARGET_HANDLE, target, AMQP_VALUE, capabilities_value);

/* annotations */

/* annotations */

    typedef AMQP_VALUE annotations;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_annotations, AMQP_VALUE, value);
    #define annotations_clone amqpvalue_clone
    #define annotations_destroy amqpvalue_destroy


    #define amqpvalue_get_annotations amqpvalue_get_map


/* message-id-ulong */

/* message-id-ulong */

    typedef uint64_t message_id_ulong;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_message_id_ulong, message_id_ulong, value);


    #define amqpvalue_get_message_id_ulong amqpvalue_get_ulong


/* message-id-uuid */

/* message-id-uuid */

    typedef uuid message_id_uuid;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_message_id_uuid, message_id_uuid, value);


    #define amqpvalue_get_message_id_uuid amqpvalue_get_uuid


/* message-id-binary */

/* message-id-binary */

    typedef amqp_binary message_id_binary;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_message_id_binary, message_id_binary, value);


    #define amqpvalue_get_message_id_binary amqpvalue_get_binary


/* message-id-string */

/* message-id-string */

    typedef const char* message_id_string;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_message_id_string, message_id_string, value);


    #define amqpvalue_get_message_id_string amqpvalue_get_string


/* address-string */

/* address-string */

    typedef const char* address_string;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_address_string, address_string, value);


    #define amqpvalue_get_address_string amqpvalue_get_string


/* header */

    typedef struct HEADER_INSTANCE_TAG* HEADER_HANDLE;

    MOCKABLE_FUNCTION(, HEADER_HANDLE, header_create );
    MOCKABLE_FUNCTION(, HEADER_HANDLE, header_clone, HEADER_HANDLE, value);
    MOCKABLE_FUNCTION(, void, header_destroy, HEADER_HANDLE, header);
    MOCKABLE_FUNCTION(, bool, is_header_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_header, AMQP_VALUE, value, HEADER_HANDLE*, HEADER_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_header, HEADER_HANDLE, header);

    MOCKABLE_FUNCTION(, int, header_get_durable, HEADER_HANDLE, header, bool*, durable_value);
    MOCKABLE_FUNCTION(, int, header_set_durable, HEADER_HANDLE, header, bool, durable_value);
    MOCKABLE_FUNCTION(, int, header_get_priority, HEADER_HANDLE, header, uint8_t*, priority_value);
    MOCKABLE_FUNCTION(, int, header_set_priority, HEADER_HANDLE, header, uint8_t, priority_value);
    MOCKABLE_FUNCTION(, int, header_get_ttl, HEADER_HANDLE, header, milliseconds*, ttl_value);
    MOCKABLE_FUNCTION(, int, header_set_ttl, HEADER_HANDLE, header, milliseconds, ttl_value);
    MOCKABLE_FUNCTION(, int, header_get_first_acquirer, HEADER_HANDLE, header, bool*, first_acquirer_value);
    MOCKABLE_FUNCTION(, int, header_set_first_acquirer, HEADER_HANDLE, header, bool, first_acquirer_value);
    MOCKABLE_FUNCTION(, int, header_get_delivery_count, HEADER_HANDLE, header, uint32_t*, delivery_count_value);
    MOCKABLE_FUNCTION(, int, header_set_delivery_count, HEADER_HANDLE, header, uint32_t, delivery_count_value);

/* delivery-annotations */

/* delivery-annotations */

    typedef annotations delivery_annotations;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_delivery_annotations, delivery_annotations, value);

    MOCKABLE_FUNCTION(, bool, is_delivery_annotations_type_by_descriptor, AMQP_VALUE, descriptor);

    #define amqpvalue_get_delivery_annotations amqpvalue_get_annotations


/* message-annotations */

/* message-annotations */

    typedef annotations message_annotations;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_message_annotations, message_annotations, value);

    MOCKABLE_FUNCTION(, bool, is_message_annotations_type_by_descriptor, AMQP_VALUE, descriptor);

    #define amqpvalue_get_message_annotations amqpvalue_get_annotations


/* application-properties */

/* application-properties */

    typedef AMQP_VALUE application_properties;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_application_properties, AMQP_VALUE, value);
    #define application_properties_clone amqpvalue_clone
    #define application_properties_destroy amqpvalue_destroy

    MOCKABLE_FUNCTION(, bool, is_application_properties_type_by_descriptor, AMQP_VALUE, descriptor);

    #define amqpvalue_get_application_properties amqpvalue_get_map


/* data */

/* data */

    typedef amqp_binary data;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_data, data, value);

    MOCKABLE_FUNCTION(, bool, is_data_type_by_descriptor, AMQP_VALUE, descriptor);

    #define amqpvalue_get_data amqpvalue_get_binary


/* amqp-sequence */

/* amqp-sequence */

    typedef AMQP_VALUE amqp_sequence;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_amqp_sequence, AMQP_VALUE, value);
    #define amqp_sequence_clone amqpvalue_clone
    #define amqp_sequence_destroy amqpvalue_destroy

    MOCKABLE_FUNCTION(, bool, is_amqp_sequence_type_by_descriptor, AMQP_VALUE, descriptor);

    #define amqpvalue_get_amqp_sequence amqpvalue_get_list


/* amqp-value */

/* amqp-value */

    typedef AMQP_VALUE amqp_value;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_amqp_value, AMQP_VALUE, value);
    #define amqp_value_clone amqpvalue_clone
    #define amqp_value_destroy amqpvalue_destroy

    MOCKABLE_FUNCTION(, bool, is_amqp_value_type_by_descriptor, AMQP_VALUE, descriptor);

    #define amqpvalue_get_amqp_value amqpvalue_get_*


/* footer */

/* footer */

    typedef annotations footer;

    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_footer, footer, value);

    MOCKABLE_FUNCTION(, bool, is_footer_type_by_descriptor, AMQP_VALUE, descriptor);

    #define amqpvalue_get_footer amqpvalue_get_annotations


/* properties */

    typedef struct PROPERTIES_INSTANCE_TAG* PROPERTIES_HANDLE;

    MOCKABLE_FUNCTION(, PROPERTIES_HANDLE, properties_create );
    MOCKABLE_FUNCTION(, PROPERTIES_HANDLE, properties_clone, PROPERTIES_HANDLE, value);
    MOCKABLE_FUNCTION(, void, properties_destroy, PROPERTIES_HANDLE, properties);
    MOCKABLE_FUNCTION(, bool, is_properties_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_properties, AMQP_VALUE, value, PROPERTIES_HANDLE*, PROPERTIES_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_properties, PROPERTIES_HANDLE, properties);

    MOCKABLE_FUNCTION(, int, properties_get_message_id, PROPERTIES_HANDLE, properties, AMQP_VALUE*, message_id_value);
    MOCKABLE_FUNCTION(, int, properties_set_message_id, PROPERTIES_HANDLE, properties, AMQP_VALUE, message_id_value);
    MOCKABLE_FUNCTION(, int, properties_get_user_id, PROPERTIES_HANDLE, properties, amqp_binary*, user_id_value);
    MOCKABLE_FUNCTION(, int, properties_set_user_id, PROPERTIES_HANDLE, properties, amqp_binary, user_id_value);
    MOCKABLE_FUNCTION(, int, properties_get_to, PROPERTIES_HANDLE, properties, AMQP_VALUE*, to_value);
    MOCKABLE_FUNCTION(, int, properties_set_to, PROPERTIES_HANDLE, properties, AMQP_VALUE, to_value);
    MOCKABLE_FUNCTION(, int, properties_get_subject, PROPERTIES_HANDLE, properties, const char**, subject_value);
    MOCKABLE_FUNCTION(, int, properties_set_subject, PROPERTIES_HANDLE, properties, const char*, subject_value);
    MOCKABLE_FUNCTION(, int, properties_get_reply_to, PROPERTIES_HANDLE, properties, AMQP_VALUE*, reply_to_value);
    MOCKABLE_FUNCTION(, int, properties_set_reply_to, PROPERTIES_HANDLE, properties, AMQP_VALUE, reply_to_value);
    MOCKABLE_FUNCTION(, int, properties_get_correlation_id, PROPERTIES_HANDLE, properties, AMQP_VALUE*, correlation_id_value);
    MOCKABLE_FUNCTION(, int, properties_set_correlation_id, PROPERTIES_HANDLE, properties, AMQP_VALUE, correlation_id_value);
    MOCKABLE_FUNCTION(, int, properties_get_content_type, PROPERTIES_HANDLE, properties, const char**, content_type_value);
    MOCKABLE_FUNCTION(, int, properties_set_content_type, PROPERTIES_HANDLE, properties, const char*, content_type_value);
    MOCKABLE_FUNCTION(, int, properties_get_content_encoding, PROPERTIES_HANDLE, properties, const char**, content_encoding_value);
    MOCKABLE_FUNCTION(, int, properties_set_content_encoding, PROPERTIES_HANDLE, properties, const char*, content_encoding_value);
    MOCKABLE_FUNCTION(, int, properties_get_absolute_expiry_time, PROPERTIES_HANDLE, properties, timestamp*, absolute_expiry_time_value);
    MOCKABLE_FUNCTION(, int, properties_set_absolute_expiry_time, PROPERTIES_HANDLE, properties, timestamp, absolute_expiry_time_value);
    MOCKABLE_FUNCTION(, int, properties_get_creation_time, PROPERTIES_HANDLE, properties, timestamp*, creation_time_value);
    MOCKABLE_FUNCTION(, int, properties_set_creation_time, PROPERTIES_HANDLE, properties, timestamp, creation_time_value);
    MOCKABLE_FUNCTION(, int, properties_get_group_id, PROPERTIES_HANDLE, properties, const char**, group_id_value);
    MOCKABLE_FUNCTION(, int, properties_set_group_id, PROPERTIES_HANDLE, properties, const char*, group_id_value);
    MOCKABLE_FUNCTION(, int, properties_get_group_sequence, PROPERTIES_HANDLE, properties, sequence_no*, group_sequence_value);
    MOCKABLE_FUNCTION(, int, properties_set_group_sequence, PROPERTIES_HANDLE, properties, sequence_no, group_sequence_value);
    MOCKABLE_FUNCTION(, int, properties_get_reply_to_group_id, PROPERTIES_HANDLE, properties, const char**, reply_to_group_id_value);
    MOCKABLE_FUNCTION(, int, properties_set_reply_to_group_id, PROPERTIES_HANDLE, properties, const char*, reply_to_group_id_value);

/* received */

    typedef struct RECEIVED_INSTANCE_TAG* RECEIVED_HANDLE;

    MOCKABLE_FUNCTION(, RECEIVED_HANDLE, received_create , uint32_t, section_number_value, uint64_t, section_offset_value);
    MOCKABLE_FUNCTION(, RECEIVED_HANDLE, received_clone, RECEIVED_HANDLE, value);
    MOCKABLE_FUNCTION(, void, received_destroy, RECEIVED_HANDLE, received);
    MOCKABLE_FUNCTION(, bool, is_received_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_received, AMQP_VALUE, value, RECEIVED_HANDLE*, RECEIVED_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_received, RECEIVED_HANDLE, received);

    MOCKABLE_FUNCTION(, int, received_get_section_number, RECEIVED_HANDLE, received, uint32_t*, section_number_value);
    MOCKABLE_FUNCTION(, int, received_set_section_number, RECEIVED_HANDLE, received, uint32_t, section_number_value);
    MOCKABLE_FUNCTION(, int, received_get_section_offset, RECEIVED_HANDLE, received, uint64_t*, section_offset_value);
    MOCKABLE_FUNCTION(, int, received_set_section_offset, RECEIVED_HANDLE, received, uint64_t, section_offset_value);

/* accepted */

    typedef struct ACCEPTED_INSTANCE_TAG* ACCEPTED_HANDLE;

    MOCKABLE_FUNCTION(, ACCEPTED_HANDLE, accepted_create );
    MOCKABLE_FUNCTION(, ACCEPTED_HANDLE, accepted_clone, ACCEPTED_HANDLE, value);
    MOCKABLE_FUNCTION(, void, accepted_destroy, ACCEPTED_HANDLE, accepted);
    MOCKABLE_FUNCTION(, bool, is_accepted_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_accepted, AMQP_VALUE, value, ACCEPTED_HANDLE*, ACCEPTED_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_accepted, ACCEPTED_HANDLE, accepted);


/* rejected */

    typedef struct REJECTED_INSTANCE_TAG* REJECTED_HANDLE;

    MOCKABLE_FUNCTION(, REJECTED_HANDLE, rejected_create );
    MOCKABLE_FUNCTION(, REJECTED_HANDLE, rejected_clone, REJECTED_HANDLE, value);
    MOCKABLE_FUNCTION(, void, rejected_destroy, REJECTED_HANDLE, rejected);
    MOCKABLE_FUNCTION(, bool, is_rejected_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_rejected, AMQP_VALUE, value, REJECTED_HANDLE*, REJECTED_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_rejected, REJECTED_HANDLE, rejected);

    MOCKABLE_FUNCTION(, int, rejected_get_error, REJECTED_HANDLE, rejected, ERROR_HANDLE*, error_value);
    MOCKABLE_FUNCTION(, int, rejected_set_error, REJECTED_HANDLE, rejected, ERROR_HANDLE, error_value);

/* released */

    typedef struct RELEASED_INSTANCE_TAG* RELEASED_HANDLE;

    MOCKABLE_FUNCTION(, RELEASED_HANDLE, released_create );
    MOCKABLE_FUNCTION(, RELEASED_HANDLE, released_clone, RELEASED_HANDLE, value);
    MOCKABLE_FUNCTION(, void, released_destroy, RELEASED_HANDLE, released);
    MOCKABLE_FUNCTION(, bool, is_released_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_released, AMQP_VALUE, value, RELEASED_HANDLE*, RELEASED_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_released, RELEASED_HANDLE, released);


/* modified */

    typedef struct MODIFIED_INSTANCE_TAG* MODIFIED_HANDLE;

    MOCKABLE_FUNCTION(, MODIFIED_HANDLE, modified_create );
    MOCKABLE_FUNCTION(, MODIFIED_HANDLE, modified_clone, MODIFIED_HANDLE, value);
    MOCKABLE_FUNCTION(, void, modified_destroy, MODIFIED_HANDLE, modified);
    MOCKABLE_FUNCTION(, bool, is_modified_type_by_descriptor, AMQP_VALUE, descriptor);
    MOCKABLE_FUNCTION(, int, amqpvalue_get_modified, AMQP_VALUE, value, MODIFIED_HANDLE*, MODIFIED_handle);
    MOCKABLE_FUNCTION(, AMQP_VALUE, amqpvalue_create_modified, MODIFIED_HANDLE, modified);

    MOCKABLE_FUNCTION(, int, modified_get_delivery_failed, MODIFIED_HANDLE, modified, bool*, delivery_failed_value);
    MOCKABLE_FUNCTION(, int, modified_set_delivery_failed, MODIFIED_HANDLE, modified, bool, delivery_failed_value);
    MOCKABLE_FUNCTION(, int, modified_get_undeliverable_here, MODIFIED_HANDLE, modified, bool*, undeliverable_here_value);
    MOCKABLE_FUNCTION(, int, modified_set_undeliverable_here, MODIFIED_HANDLE, modified, bool, undeliverable_here_value);
    MOCKABLE_FUNCTION(, int, modified_get_message_annotations, MODIFIED_HANDLE, modified, fields*, message_annotations_value);
    MOCKABLE_FUNCTION(, int, modified_set_message_annotations, MODIFIED_HANDLE, modified, fields, message_annotations_value);


#ifdef __cplusplus
}
#endif

#endif /* AMQP_DEFINITIONS_H */
