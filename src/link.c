// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/optimize_size.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_uamqp_c/link.h"
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/amqpvalue.h"
#include "azure_uamqp_c/amqp_definitions.h"
#include "azure_uamqp_c/amqp_frame_codec.h"
#include "azure_uamqp_c/async_operation.h"

#define DEFAULT_LINK_CREDIT 10000

typedef struct DELIVERY_INSTANCE_TAG
{
    delivery_number delivery_id;
    ON_DELIVERY_SETTLED on_delivery_settled;
    void* callback_context;
    void* link;
    tickcounter_ms_t start_tick;
    tickcounter_ms_t timeout;
} DELIVERY_INSTANCE;

typedef struct LINK_INSTANCE_TAG
{
    SESSION_HANDLE session;
    LINK_STATE link_state;
    LINK_STATE previous_link_state;
    AMQP_VALUE source;
    AMQP_VALUE target;
    handle handle;
    LINK_ENDPOINT_HANDLE link_endpoint;
    char* name;
    SINGLYLINKEDLIST_HANDLE pending_deliveries;
    sequence_no delivery_count;
    role role;
    ON_LINK_STATE_CHANGED on_link_state_changed;
    ON_LINK_FLOW_ON on_link_flow_on;
    ON_TRANSFER_RECEIVED on_transfer_received;
    void* callback_context;
    sender_settle_mode snd_settle_mode;
    receiver_settle_mode rcv_settle_mode;
    sequence_no initial_delivery_count;
    uint64_t max_message_size;
    uint64_t peer_max_message_size;
    uint32_t link_credit;
    uint32_t available;
    fields attach_properties;
    bool is_underlying_session_begun;
    bool is_closed;
    unsigned char* received_payload;
    uint32_t received_payload_size;
    delivery_number received_delivery_id;
    TICK_COUNTER_HANDLE tick_counter;
} LINK_INSTANCE;

DEFINE_ASYNC_OPERATION_CONTEXT(DELIVERY_INSTANCE);

static void set_link_state(LINK_INSTANCE* link_instance, LINK_STATE link_state)
{
    link_instance->previous_link_state = link_instance->link_state;
    link_instance->link_state = link_state;

    if (link_instance->on_link_state_changed != NULL)
    {
        link_instance->on_link_state_changed(link_instance->callback_context, link_state, link_instance->previous_link_state);
    }
}

static void remove_all_pending_deliveries(LINK_INSTANCE* link, bool indicate_settled)
{
    if (link->pending_deliveries != NULL)
    {
        LIST_ITEM_HANDLE item = singlylinkedlist_get_head_item(link->pending_deliveries);
        while (item != NULL)
        {
            LIST_ITEM_HANDLE next_item = singlylinkedlist_get_next_item(item);
            ASYNC_OPERATION_HANDLE pending_delivery_operation = (ASYNC_OPERATION_HANDLE)singlylinkedlist_item_get_value(item);
            if (pending_delivery_operation != NULL)
            {
                DELIVERY_INSTANCE* delivery_instance = (DELIVERY_INSTANCE*)GET_ASYNC_OPERATION_CONTEXT(DELIVERY_INSTANCE, pending_delivery_operation);
                if (indicate_settled && (delivery_instance->on_delivery_settled != NULL))
                {
                    delivery_instance->on_delivery_settled(delivery_instance->callback_context, delivery_instance->delivery_id, LINK_DELIVERY_SETTLE_REASON_NOT_DELIVERED, NULL);
                }

                async_operation_destroy(pending_delivery_operation);
            }

            item = next_item;
        }

        singlylinkedlist_destroy(link->pending_deliveries);
        link->pending_deliveries = NULL;
    }
}

static int send_flow(LINK_INSTANCE* link)
{
    int result;
    FLOW_HANDLE flow = flow_create(0, 0, 0);

    if (flow == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        if ((flow_set_link_credit(flow, link->link_credit) != 0) ||
            (flow_set_handle(flow, link->handle) != 0) ||
            (flow_set_delivery_count(flow, link->delivery_count) != 0))
        {
            result = __FAILURE__;
        }
        else
        {
            if (session_send_flow(link->link_endpoint, flow) != 0)
            {
                result = __FAILURE__;
            }
            else
            {
                result = 0;
            }
        }

        flow_destroy(flow);
    }

    return result;
}

static int send_disposition(LINK_INSTANCE* link_instance, delivery_number delivery_number, AMQP_VALUE delivery_state)
{
    int result;

    DISPOSITION_HANDLE disposition = disposition_create(link_instance->role, delivery_number);
    if (disposition == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        if ((disposition_set_last(disposition, delivery_number) != 0) ||
            (disposition_set_settled(disposition, true) != 0) ||
            ((delivery_state != NULL) && (disposition_set_state(disposition, delivery_state) != 0)))
        {
            result = __FAILURE__;
        }
        else
        {
            if (session_send_disposition(link_instance->link_endpoint, disposition) != 0)
            {
                result = __FAILURE__;
            }
            else
            {
                result = 0;
            }
        }

        disposition_destroy(disposition);
    }

    return result;
}

static int send_detach(LINK_INSTANCE* link_instance, bool close, ERROR_HANDLE error_handle)
{
    int result;
    DETACH_HANDLE detach_performative;

    detach_performative = detach_create(0);
    if (detach_performative == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        if ((error_handle != NULL) &&
            (detach_set_error(detach_performative, error_handle) != 0))
        {
            result = __FAILURE__;
        }
        else if (close &&
            (detach_set_closed(detach_performative, true) != 0))
        {
            result = __FAILURE__;
        }
        else
        {
            if (session_send_detach(link_instance->link_endpoint, detach_performative) != 0)
            {
                result = __FAILURE__;
            }
            else
            {
                if (close)
                {
                    /* Declare link to be closed */
                    link_instance->is_closed = true;
                }

                result = 0;
            }
        }

        detach_destroy(detach_performative);
    }

    return result;
}

static int send_attach(LINK_INSTANCE* link, const char* name, handle handle, role role)
{
    int result;
    ATTACH_HANDLE attach = attach_create(name, handle, role);

    if (attach == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        result = 0;

        link->delivery_count = link->initial_delivery_count;

        attach_set_snd_settle_mode(attach, link->snd_settle_mode);
        attach_set_rcv_settle_mode(attach, link->rcv_settle_mode);
        attach_set_role(attach, role);
        attach_set_source(attach, link->source);
        attach_set_target(attach, link->target);
        if (link->attach_properties != NULL)
        {
            attach_set_properties(attach, link->attach_properties);
        }

        if (role == role_sender)
        {
            if (attach_set_initial_delivery_count(attach, link->delivery_count) != 0)
            {
                result = __FAILURE__;
            }
        }

        if (result == 0)
        {
            if ((attach_set_max_message_size(attach, link->max_message_size) != 0) ||
                (session_send_attach(link->link_endpoint, attach) != 0))
            {
                result = __FAILURE__;
            }
            else
            {
                result = 0;
            }
        }

        attach_destroy(attach);
    }

    return result;
}

static void link_frame_received(void* context, AMQP_VALUE performative, uint32_t payload_size, const unsigned char* payload_bytes)
{
    LINK_INSTANCE* link_instance = (LINK_INSTANCE*)context;
    AMQP_VALUE descriptor = amqpvalue_get_inplace_descriptor(performative);

    if (is_attach_type_by_descriptor(descriptor))
    {
        ATTACH_HANDLE attach_handle;

        if (amqpvalue_get_attach(performative, &attach_handle) == 0)
        {
            if ((link_instance->role == role_receiver) &&
                (attach_get_initial_delivery_count(attach_handle, &link_instance->delivery_count) != 0))
            {
                /* error */
                remove_all_pending_deliveries(link_instance, true);
                set_link_state(link_instance, LINK_STATE_DETACHED);
            }
            else
            {
                if (attach_get_max_message_size(attach_handle, &link_instance->peer_max_message_size) != 0)
                {
                    LogError("Could not retrieve peer_max_message_size from attach frame");
                }

                if ((link_instance->link_state == LINK_STATE_DETACHED) ||
                    (link_instance->link_state == LINK_STATE_HALF_ATTACHED_ATTACH_SENT))
                {
                    if (link_instance->role == role_receiver)
                    {
                        link_instance->link_credit = DEFAULT_LINK_CREDIT;
                        send_flow(link_instance);
                    }
                    else
                    {
                        link_instance->link_credit = 0;
                    }

                    if (link_instance->link_state == LINK_STATE_DETACHED)
                    {
                        set_link_state(link_instance, LINK_STATE_HALF_ATTACHED_ATTACH_RECEIVED);
                    }
                    else
                    {
                        set_link_state(link_instance, LINK_STATE_ATTACHED);
                    }
                }
            }

            attach_destroy(attach_handle);
        }
    }
    else if (is_flow_type_by_descriptor(descriptor))
    {
        FLOW_HANDLE flow_handle;
        if (amqpvalue_get_flow(performative, &flow_handle) == 0)
        {
            if (link_instance->role == role_sender)
            {
                delivery_number rcv_delivery_count;
                uint32_t rcv_link_credit;

                if ((flow_get_link_credit(flow_handle, &rcv_link_credit) != 0) ||
                    (flow_get_delivery_count(flow_handle, &rcv_delivery_count) != 0))
                {
                    /* error */
                    remove_all_pending_deliveries(link_instance, true);
                    set_link_state(link_instance, LINK_STATE_DETACHED);
                }
                else
                {
                    link_instance->link_credit = rcv_delivery_count + rcv_link_credit - link_instance->delivery_count;
                    if (link_instance->link_credit > 0)
                    {
                        link_instance->on_link_flow_on(link_instance->callback_context);
                    }
                }
            }
        }

        flow_destroy(flow_handle);
    }
    else if (is_transfer_type_by_descriptor(descriptor))
    {
        if (link_instance->on_transfer_received != NULL)
        {
            TRANSFER_HANDLE transfer_handle;
            if (amqpvalue_get_transfer(performative, &transfer_handle) == 0)
            {
                AMQP_VALUE delivery_state;
                bool more;
                bool is_error;

                link_instance->link_credit--;
                link_instance->delivery_count++;
                if (link_instance->link_credit == 0)
                {
                    link_instance->link_credit = DEFAULT_LINK_CREDIT;
                    send_flow(link_instance);
                }

                more = false;
                /* Attempt to get more flag, default to false */
                (void)transfer_get_more(transfer_handle, &more);
                is_error = false;

                if (transfer_get_delivery_id(transfer_handle, &link_instance->received_delivery_id) != 0)
                {
                    /* is this not a continuation transfer? */
                    if (link_instance->received_payload_size == 0)
                    {
                        LogError("Could not get the delivery Id from the transfer performative");
                        is_error = true;
                    }
                }
                    
                if (!is_error)
                {
                    /* If this is a continuation transfer or if this is the first chunk of a multi frame transfer */
                    if ((link_instance->received_payload_size > 0) || more)
                    {
                        unsigned char* new_received_payload = (unsigned char*)realloc(link_instance->received_payload, link_instance->received_payload_size + payload_size);
                        if (new_received_payload == NULL)
                        {
                            LogError("Could not allocate memory for the received payload");
                        }
                        else
                        {
                            link_instance->received_payload = new_received_payload;
                            (void)memcpy(link_instance->received_payload + link_instance->received_payload_size, payload_bytes, payload_size);
                            link_instance->received_payload_size += payload_size;
                        }
                    }

                    if (!more)
                    {
                        const unsigned char* indicate_payload_bytes;
                        uint32_t indicate_payload_size;

                        /* if no previously stored chunks then simply report the current payload */
                        if (link_instance->received_payload_size > 0)
                        {
                            indicate_payload_size = link_instance->received_payload_size;
                            indicate_payload_bytes = link_instance->received_payload;
                        }
                        else
                        {
                            indicate_payload_size = payload_size;
                            indicate_payload_bytes = payload_bytes;
                        }

                        delivery_state = link_instance->on_transfer_received(link_instance->callback_context, transfer_handle, indicate_payload_size, indicate_payload_bytes);

                        if (link_instance->received_payload_size > 0)
                        {
                            free(link_instance->received_payload);
                            link_instance->received_payload = NULL;
                            link_instance->received_payload_size = 0;
                        }

                        if (delivery_state != NULL)
                        {
                            if (send_disposition(link_instance, link_instance->received_delivery_id, delivery_state) != 0)
                            {
                                LogError("Cannot send disposition frame");
                            }
                            amqpvalue_destroy(delivery_state);
                        }
                    }
                }

                transfer_destroy(transfer_handle);
            }
        }
    }
    else if (is_disposition_type_by_descriptor(descriptor))
    {
        DISPOSITION_HANDLE disposition;
        if (amqpvalue_get_disposition(performative, &disposition) != 0)
        {
            /* error */
        }
        else
        {
            delivery_number first;
            delivery_number last;

            if (disposition_get_first(disposition, &first) != 0)
            {
                /* error */
            }
            else
            {
                bool settled;

                if (disposition_get_last(disposition, &last) != 0)
                {
                    last = first;
                }

                if (disposition_get_settled(disposition, &settled) != 0)
                {
                    /* Error */
                    settled = false;
                }

                if (settled)
                {
                    LIST_ITEM_HANDLE pending_delivery = singlylinkedlist_get_head_item(link_instance->pending_deliveries);
                    while (pending_delivery != NULL)
                    {
                        LIST_ITEM_HANDLE next_pending_delivery = singlylinkedlist_get_next_item(pending_delivery);
                        ASYNC_OPERATION_HANDLE pending_delivery_operation = (ASYNC_OPERATION_HANDLE)singlylinkedlist_item_get_value(pending_delivery);
                        if (pending_delivery_operation == NULL)
                        {
                            /* error */
                            break;
                        }
                        else
                        {
                            DELIVERY_INSTANCE* delivery_instance = (DELIVERY_INSTANCE*)GET_ASYNC_OPERATION_CONTEXT(DELIVERY_INSTANCE, pending_delivery_operation);

                            if ((delivery_instance->delivery_id >= first) && (delivery_instance->delivery_id <= last))
                            {
                                AMQP_VALUE delivery_state;
                                if (disposition_get_state(disposition, &delivery_state) != 0)
                                {
                                    /* error */
                                }
                                else
                                {
                                    delivery_instance->on_delivery_settled(delivery_instance->callback_context, delivery_instance->delivery_id, LINK_DELIVERY_SETTLE_REASON_DISPOSITION_RECEIVED, delivery_state);
                                    async_operation_destroy(pending_delivery_operation);
                                    if (singlylinkedlist_remove(link_instance->pending_deliveries, pending_delivery) != 0)
                                    {
                                        /* error */
                                        break;
                                    }
                                    else
                                    {
                                        pending_delivery = next_pending_delivery;
                                    }
                                }
                            }
                            else
                            {
                                pending_delivery = next_pending_delivery;
                            }
                        }
                    }
                }
            }

            disposition_destroy(disposition);
        }
    }
    else if (is_detach_type_by_descriptor(descriptor))
    {
        DETACH_HANDLE detach;

        /* Set link state appropriately based on whether we received detach condition */
        if (amqpvalue_get_detach(performative, &detach) == 0)
        {
            bool closed = false;
            ERROR_HANDLE error;

            /* Received a detach while attached */
            if (link_instance->link_state == LINK_STATE_ATTACHED)
            {
                /* Respond with ack */
                (void)send_detach(link_instance, closed, NULL);
            }

            /* Received a closing detach after we sent a non-closing detach. */
            else if (closed &&
                ((link_instance->link_state == LINK_STATE_HALF_ATTACHED_ATTACH_SENT) || (link_instance->link_state == LINK_STATE_HALF_ATTACHED_ATTACH_RECEIVED)) &&
                !link_instance->is_closed)
            {

                /* In this case, we MUST signal that we closed by reattaching and then sending a closing detach.*/
                (void)send_attach(link_instance, link_instance->name, 0, link_instance->role);
                (void)send_detach(link_instance, true, NULL);
            }

            if (detach_get_error(detach, &error) == 0)
            {
                error_destroy(error);

                remove_all_pending_deliveries(link_instance, true);
                set_link_state(link_instance, LINK_STATE_ERROR);
            }
            else 
            {
                (void)detach_get_closed(detach, &closed);

                remove_all_pending_deliveries(link_instance, true);
                set_link_state(link_instance, LINK_STATE_DETACHED);
            }

            detach_destroy(detach);
        }
    }
}

static void on_session_state_changed(void* context, SESSION_STATE new_session_state, SESSION_STATE previous_session_state)
{
    LINK_INSTANCE* link_instance = (LINK_INSTANCE*)context;
    (void)previous_session_state;

    if (new_session_state == SESSION_STATE_MAPPED)
    {
        if ((link_instance->link_state == LINK_STATE_DETACHED) && (!link_instance->is_closed))
        {
            if (send_attach(link_instance, link_instance->name, 0, link_instance->role) == 0)
            {
                set_link_state(link_instance, LINK_STATE_HALF_ATTACHED_ATTACH_SENT);
            }
        }
    }
    else if (new_session_state == SESSION_STATE_DISCARDING)
    {
        remove_all_pending_deliveries(link_instance, true);
        set_link_state(link_instance, LINK_STATE_DETACHED);
    }
    else if (new_session_state == SESSION_STATE_ERROR)
    {
        remove_all_pending_deliveries(link_instance, true);
        set_link_state(link_instance, LINK_STATE_ERROR);
    }
}

static void on_session_flow_on(void* context)
{
    LINK_INSTANCE* link_instance = (LINK_INSTANCE*)context;
    if (link_instance->role == role_sender)
    {
        link_instance->on_link_flow_on(link_instance->callback_context);
    }
}

static void on_send_complete(void* context, IO_SEND_RESULT send_result)
{
    LIST_ITEM_HANDLE delivery_instance_list_item = (LIST_ITEM_HANDLE)context;
    ASYNC_OPERATION_HANDLE pending_delivery_operation = (ASYNC_OPERATION_HANDLE)singlylinkedlist_item_get_value(delivery_instance_list_item);
    DELIVERY_INSTANCE* delivery_instance = (DELIVERY_INSTANCE*)GET_ASYNC_OPERATION_CONTEXT(DELIVERY_INSTANCE, pending_delivery_operation);
    LINK_HANDLE link = (LINK_HANDLE)delivery_instance->link;
    (void)send_result;
    if (link->snd_settle_mode == sender_settle_mode_settled)
    {
        delivery_instance->on_delivery_settled(delivery_instance->callback_context, delivery_instance->delivery_id, LINK_DELIVERY_SETTLE_REASON_SETTLED, NULL);
        async_operation_destroy(pending_delivery_operation);
        (void)singlylinkedlist_remove(link->pending_deliveries, delivery_instance_list_item);
    }
}

LINK_HANDLE link_create(SESSION_HANDLE session, const char* name, role role, AMQP_VALUE source, AMQP_VALUE target)
{
    LINK_INSTANCE* result = (LINK_INSTANCE*)malloc(sizeof(LINK_INSTANCE));
    if (result != NULL)
    {
        result->link_state = LINK_STATE_DETACHED;
        result->previous_link_state = LINK_STATE_DETACHED;
        result->role = role;
        result->source = amqpvalue_clone(source);
        result->target = amqpvalue_clone(target);
        result->session = session;
        result->handle = 0;
        result->snd_settle_mode = sender_settle_mode_unsettled;
        result->rcv_settle_mode = receiver_settle_mode_first;
        result->delivery_count = 0;
        result->initial_delivery_count = 0;
        result->max_message_size = 0;
        result->peer_max_message_size = 0;
        result->is_underlying_session_begun = false;
        result->is_closed = false;
        result->attach_properties = NULL;
        result->received_payload = NULL;
        result->received_payload_size = 0;
        result->received_delivery_id = 0;

        result->tick_counter = tickcounter_create();
        if (result->tick_counter == NULL)
        {
            free(result);
            result = NULL;
        }
        else
        {
            result->pending_deliveries = singlylinkedlist_create();
            if (result->pending_deliveries == NULL)
            {
                tickcounter_destroy(result->tick_counter);
                free(result);
                result = NULL;
            }
            else
            {
                size_t name_length = strlen(name);
                result->name = (char*)malloc(name_length + 1);
                if (result->name == NULL)
                {
                    tickcounter_destroy(result->tick_counter);
                    singlylinkedlist_destroy(result->pending_deliveries);
                    free(result);
                    result = NULL;
                }
                else
                {
                    result->on_link_state_changed = NULL;
                    result->callback_context = NULL;
                    set_link_state(result, LINK_STATE_DETACHED);

                    (void)memcpy(result->name, name, name_length + 1);
                    result->link_endpoint = session_create_link_endpoint(session, name);
                    if (result->link_endpoint == NULL)
                    {
                        tickcounter_destroy(result->tick_counter);
                        singlylinkedlist_destroy(result->pending_deliveries);
                        free(result->name);
                        free(result);
                        result = NULL;
                    }
                }
            }
        }
    }

    return result;
}

LINK_HANDLE link_create_from_endpoint(SESSION_HANDLE session, LINK_ENDPOINT_HANDLE link_endpoint, const char* name, role role, AMQP_VALUE source, AMQP_VALUE target)
{
    LINK_INSTANCE* result = (LINK_INSTANCE*)malloc(sizeof(LINK_INSTANCE));
    if (result != NULL)
    {
        result->link_state = LINK_STATE_DETACHED;
        result->previous_link_state = LINK_STATE_DETACHED;
        result->session = session;
        result->handle = 0;
        result->snd_settle_mode = sender_settle_mode_unsettled;
        result->rcv_settle_mode = receiver_settle_mode_first;
        result->delivery_count = 0;
        result->initial_delivery_count = 0;
        result->max_message_size = 0;
        result->peer_max_message_size = 0;
        result->is_underlying_session_begun = false;
        result->is_closed = false;
        result->attach_properties = NULL;
        result->received_payload = NULL;
        result->received_payload_size = 0;
        result->received_delivery_id = 0;
        result->source = amqpvalue_clone(target);
        result->target = amqpvalue_clone(source);
        if (role == role_sender)
        {
            result->role = role_receiver;
        }
        else
        {
            result->role = role_sender;
        }

        result->tick_counter = tickcounter_create();
        if (result->tick_counter == NULL)
        {
            free(result);
            result = NULL;
        }
        else
        {
            result->pending_deliveries = singlylinkedlist_create();
            if (result->pending_deliveries == NULL)
            {
                tickcounter_destroy(result->tick_counter);
                free(result);
                result = NULL;
            }
            else
            {
                size_t name_length = strlen(name);
                result->name = (char*)malloc(name_length + 1);
                if (result->name == NULL)
                {
                    tickcounter_destroy(result->tick_counter);
                    singlylinkedlist_destroy(result->pending_deliveries);
                    free(result);
                    result = NULL;
                }
                else
                {
                    (void)memcpy(result->name, name, name_length + 1);
                    result->on_link_state_changed = NULL;
                    result->callback_context = NULL;
                    result->link_endpoint = link_endpoint;
                }
            }
        }
    }

    return result;
}

void link_destroy(LINK_HANDLE link)
{
    if (link != NULL)
    {
        remove_all_pending_deliveries((LINK_INSTANCE*)link, false);
        tickcounter_destroy(link->tick_counter);

        link->on_link_state_changed = NULL;
        (void)link_detach(link, true);
        session_destroy_link_endpoint(link->link_endpoint);
        amqpvalue_destroy(link->source);
        amqpvalue_destroy(link->target);

        if (link->name != NULL)
        {
            free(link->name);
        }

        if (link->attach_properties != NULL)
        {
            amqpvalue_destroy(link->attach_properties);
        }

        if (link->received_payload != NULL)
        {
            free(link->received_payload);
        }

        free(link);
    }
}

int link_set_snd_settle_mode(LINK_HANDLE link, sender_settle_mode snd_settle_mode)
{
    int result;

    if (link == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        link->snd_settle_mode = snd_settle_mode;
        result = 0;
    }

    return result;
}

int link_get_snd_settle_mode(LINK_HANDLE link, sender_settle_mode* snd_settle_mode)
{
    int result;

    if ((link == NULL) ||
        (snd_settle_mode == NULL))
    {
        result = __FAILURE__;
    }
    else
    {
        *snd_settle_mode = link->snd_settle_mode;

        result = 0;
    }

    return result;
}

int link_set_rcv_settle_mode(LINK_HANDLE link, receiver_settle_mode rcv_settle_mode)
{
    int result;

    if (link == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        link->rcv_settle_mode = rcv_settle_mode;
        result = 0;
    }

    return result;
}

int link_get_rcv_settle_mode(LINK_HANDLE link, receiver_settle_mode* rcv_settle_mode)
{
    int result;

    if ((link == NULL) ||
        (rcv_settle_mode == NULL))
    {
        result = __FAILURE__;
    }
    else
    {
        *rcv_settle_mode = link->rcv_settle_mode;
        result = 0;
    }

    return result;
}

int link_set_initial_delivery_count(LINK_HANDLE link, sequence_no initial_delivery_count)
{
    int result;

    if (link == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        link->initial_delivery_count = initial_delivery_count;
        result = 0;
    }

    return result;
}

int link_get_initial_delivery_count(LINK_HANDLE link, sequence_no* initial_delivery_count)
{
    int result;

    if ((link == NULL) ||
        (initial_delivery_count == NULL))
    {
        result = __FAILURE__;
    }
    else
    {
        *initial_delivery_count = link->initial_delivery_count;
        result = 0;
    }

    return result;
}

int link_set_max_message_size(LINK_HANDLE link, uint64_t max_message_size)
{
    int result;

    if (link == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        link->max_message_size = max_message_size;
        result = 0;
    }

    return result;
}

int link_get_max_message_size(LINK_HANDLE link, uint64_t* max_message_size)
{
    int result;

    if ((link == NULL) ||
        (max_message_size == NULL))
    {
        result = __FAILURE__;
    }
    else
    {
        *max_message_size = link->max_message_size;
        result = 0;
    }

    return result;
}

int link_get_peer_max_message_size(LINK_HANDLE link, uint64_t* peer_max_message_size)
{
    int result;

    if ((link == NULL) ||
        (peer_max_message_size == NULL))
    {
        LogError("Bad arguments: link = %p, peer_max_message_size = %p",
            link, peer_max_message_size);
        result = __FAILURE__;
    }
    else if ((link->link_state != LINK_STATE_ATTACHED) &&
        (link->link_state != LINK_STATE_HALF_ATTACHED_ATTACH_RECEIVED))
    {
        LogError("Attempting to read peer max message size before it was received");
        result = __FAILURE__;
    }
    else
    {
        *peer_max_message_size = link->peer_max_message_size;
        result = 0;
    }

    return result;
}

int link_set_attach_properties(LINK_HANDLE link, fields attach_properties)
{
    int result;

    if (link == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        link->attach_properties = amqpvalue_clone(attach_properties);
        if (link->attach_properties == NULL)
        {
            result = __FAILURE__;
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

int link_attach(LINK_HANDLE link, ON_TRANSFER_RECEIVED on_transfer_received, ON_LINK_STATE_CHANGED on_link_state_changed, ON_LINK_FLOW_ON on_link_flow_on, void* callback_context)
{
    int result;

    if ((link == NULL) ||
        (link->is_closed))
    {
        result = __FAILURE__;
    }
    else
    {
        if (!link->is_underlying_session_begun)
        {
            link->on_link_state_changed = on_link_state_changed;
            link->on_transfer_received = on_transfer_received;
            link->on_link_flow_on = on_link_flow_on;
            link->callback_context = callback_context;

            if (session_begin(link->session) != 0)
            {
                result = __FAILURE__;
            }
            else
            {
                link->is_underlying_session_begun = true;

                if (session_start_link_endpoint(link->link_endpoint, link_frame_received, on_session_state_changed, on_session_flow_on, link) != 0)
                {
                    result = __FAILURE__;
                }
                else
                {
                    link->received_payload_size = 0;

                    result = 0;
                }
            }
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

int link_detach(LINK_HANDLE link, bool close)
{
    int result;

    if ((link == NULL) ||
        (link->is_closed))
    {
        result = __FAILURE__;
    }
    else
    {
        switch (link->link_state)
        {
        case LINK_STATE_HALF_ATTACHED_ATTACH_SENT:
        case LINK_STATE_HALF_ATTACHED_ATTACH_RECEIVED:
            /* Sending detach when remote is not yet attached */
            if (send_detach(link, close, NULL) != 0)
            {
                result = __FAILURE__;
            }
            else
            {
                set_link_state(link, LINK_STATE_DETACHED);
                result = 0;
            }
            break;

        case LINK_STATE_ATTACHED:
            /* Send detach and wait for remote to respond */
            if (send_detach(link, close, NULL) != 0)
            {
                result = __FAILURE__;
            }
            else
            {
                set_link_state(link, LINK_STATE_HALF_ATTACHED_ATTACH_SENT);
                result = 0;
            }
            break;

        case LINK_STATE_DETACHED:
            /* Already detached */
            result = 0;
            break;

        default:
        case LINK_STATE_ERROR:
            /* Already detached and in error state */
            result = __FAILURE__;
            break;
        }
    }

    return result;
}

static bool remove_pending_delivery_condition_function(const void* item, const void* match_context, bool* continue_processing)
{
    bool result;

    if (item == match_context)
    {
        result = true;
        *continue_processing = false;
    }
    else
    {
        result = false;
        *continue_processing = true;
    }

    return result;
}

static void link_transfer_cancel_handler(ASYNC_OPERATION_HANDLE link_transfer_operation)
{
    DELIVERY_INSTANCE* pending_delivery = GET_ASYNC_OPERATION_CONTEXT(DELIVERY_INSTANCE, link_transfer_operation);
    if (pending_delivery->on_delivery_settled != NULL)
    {
        pending_delivery->on_delivery_settled(pending_delivery->callback_context, pending_delivery->delivery_id, LINK_DELIVERY_SETTLE_REASON_CANCELLED, NULL);
    }
    
    (void)singlylinkedlist_remove_if(((LINK_HANDLE)pending_delivery->link)->pending_deliveries, remove_pending_delivery_condition_function, pending_delivery);

    async_operation_destroy(link_transfer_operation);
}

ASYNC_OPERATION_HANDLE link_transfer_async(LINK_HANDLE link, message_format message_format, PAYLOAD* payloads, size_t payload_count, ON_DELIVERY_SETTLED on_delivery_settled, void* callback_context, LINK_TRANSFER_RESULT* link_transfer_error, tickcounter_ms_t timeout)
{
    ASYNC_OPERATION_HANDLE result;

    if ((link == NULL) ||
        (link_transfer_error == NULL))
    {
        if (link_transfer_error != NULL)
        {
            *link_transfer_error = LINK_TRANSFER_ERROR;
        }

        result = NULL;
    }
    else
    {
        if ((link->role != role_sender) ||
            (link->link_state != LINK_STATE_ATTACHED))
        {
            *link_transfer_error = LINK_TRANSFER_ERROR;
            result = NULL;
        }
        else if (link->link_credit == 0)
        {
            *link_transfer_error = LINK_TRANSFER_BUSY;
            result = NULL;
        }
        else
        {
            result = CREATE_ASYNC_OPERATION(DELIVERY_INSTANCE, link_transfer_cancel_handler);
            if (result != NULL)
            {
                TRANSFER_HANDLE transfer = transfer_create(0);
                if (transfer == NULL)
                {
                    *link_transfer_error = LINK_TRANSFER_ERROR;
                    async_operation_destroy(result);
                    result = NULL;
                }
                else
                {
                    sequence_no delivery_count = link->delivery_count + 1;
                    unsigned char delivery_tag_bytes[sizeof(delivery_count)];
                    delivery_tag delivery_tag;
                    bool settled;

                    (void)memcpy(delivery_tag_bytes, &delivery_count, sizeof(delivery_count));

                    delivery_tag.bytes = &delivery_tag_bytes;
                    delivery_tag.length = sizeof(delivery_tag_bytes);

                    if (link->snd_settle_mode == sender_settle_mode_unsettled)
                    {
                        settled = false;
                    }
                    else
                    {
                        settled = true;
                    }

                    if ((transfer_set_delivery_tag(transfer, delivery_tag) != 0) ||
                        (transfer_set_message_format(transfer, message_format) != 0) ||
                        (transfer_set_settled(transfer, settled) != 0))
                    {
                        *link_transfer_error = LINK_TRANSFER_ERROR;
                        async_operation_destroy(result);
                        result = NULL;
                    }
                    else
                    {
                        AMQP_VALUE transfer_value = amqpvalue_create_transfer(transfer);

                        if (transfer_value == NULL)
                        {
                            *link_transfer_error = LINK_TRANSFER_ERROR;
                            async_operation_destroy(result);
                            result = NULL;
                        }
                        else
                        {
                            DELIVERY_INSTANCE* pending_delivery = GET_ASYNC_OPERATION_CONTEXT(DELIVERY_INSTANCE, result);
                            if (pending_delivery == NULL)
                            {
                                *link_transfer_error = LINK_TRANSFER_ERROR;
                                async_operation_destroy(result);
                                result = NULL;
                            }
                            else
                            {
                                if (tickcounter_get_current_ms(link->tick_counter, &pending_delivery->start_tick) != 0)
                                {
                                    *link_transfer_error = LINK_TRANSFER_ERROR;
                                    async_operation_destroy(result);
                                    result = NULL;
                                }
                                else
                                {
                                    LIST_ITEM_HANDLE delivery_instance_list_item;
                                    pending_delivery->timeout = timeout;
                                    pending_delivery->on_delivery_settled = on_delivery_settled;
                                    pending_delivery->callback_context = callback_context;
                                    pending_delivery->link = link;
                                    delivery_instance_list_item = singlylinkedlist_add(link->pending_deliveries, result);

                                    if (delivery_instance_list_item == NULL)
                                    {
                                        *link_transfer_error = LINK_TRANSFER_ERROR;
                                        async_operation_destroy(result);
                                        result = NULL;
                                    }
                                    else
                                    {
                                        /* here we should feed data to the transfer frame */
                                        switch (session_send_transfer(link->link_endpoint, transfer, payloads, payload_count, &pending_delivery->delivery_id, (settled) ? on_send_complete : NULL, delivery_instance_list_item))
                                        {
                                        default:
                                        case SESSION_SEND_TRANSFER_ERROR:
                                            singlylinkedlist_remove(link->pending_deliveries, delivery_instance_list_item);
                                            *link_transfer_error = LINK_TRANSFER_ERROR;
                                            async_operation_destroy(result);
                                            result = NULL;
                                            break;

                                        case SESSION_SEND_TRANSFER_BUSY:
                                            /* Ensure we remove from list again since sender will attempt to transfer again on flow on */
                                            singlylinkedlist_remove(link->pending_deliveries, delivery_instance_list_item);
                                            *link_transfer_error = LINK_TRANSFER_BUSY;
                                            async_operation_destroy(result);
                                            result = NULL;
                                            break;

                                        case SESSION_SEND_TRANSFER_OK:
                                            link->delivery_count = delivery_count;
                                            link->link_credit--;
                                            break;
                                        }
                                    }
                                }
                            }

                            amqpvalue_destroy(transfer_value);
                        }
                    }

                    transfer_destroy(transfer);
                }
            }
        }
    }

    return result;
}

int link_get_name(LINK_HANDLE link, const char** link_name)
{
    int result;

    if (link == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        *link_name = link->name;
        result = 0;
    }

    return result;
}

int link_get_received_message_id(LINK_HANDLE link, delivery_number* message_id)
{
    int result;

    if (link == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        *message_id = link->received_delivery_id;
        result = 0;
    }

    return result;
}

int link_send_disposition(LINK_HANDLE link, delivery_number message_id, AMQP_VALUE delivery_state)
{
    int result;
    if (delivery_state == NULL)
    {
        result = 0;
    }
    else
    {
        result = send_disposition(link, message_id, delivery_state);
        if ( result != 0)
        {
            LogError("Cannot send disposition frame");
            result = __FAILURE__;
        }
    }
    return result;
}

void link_dowork(LINK_HANDLE link)
{
    if (link != NULL)
    {
        tickcounter_ms_t current_tick;

        if (tickcounter_get_current_ms(link->tick_counter, &current_tick) != 0)
        {
            /* error */
        }
        else
        {
            // go through all and find timed out deliveries
            LIST_ITEM_HANDLE item = singlylinkedlist_get_head_item(link->pending_deliveries);
            while (item != NULL)
            {
                LIST_ITEM_HANDLE next_item = singlylinkedlist_get_next_item(item);
                ASYNC_OPERATION_HANDLE delivery_instance_async_operation = (ASYNC_OPERATION_HANDLE)singlylinkedlist_item_get_value(item);
                DELIVERY_INSTANCE* delivery_instance = (DELIVERY_INSTANCE*)GET_ASYNC_OPERATION_CONTEXT(DELIVERY_INSTANCE, delivery_instance_async_operation);

                if ((delivery_instance->timeout != 0) &&
                    (current_tick - delivery_instance->start_tick >= delivery_instance->timeout))
                {
                    if (delivery_instance->on_delivery_settled != NULL)
                    {
                        delivery_instance->on_delivery_settled(delivery_instance->callback_context, delivery_instance->delivery_id, LINK_DELIVERY_SETTLE_REASON_TIMEOUT, NULL);
                    }

                    (void)singlylinkedlist_remove(link->pending_deliveries, item);
                    async_operation_destroy(delivery_instance_async_operation);
                }

                item = next_item;
            }
        }
    }
}
