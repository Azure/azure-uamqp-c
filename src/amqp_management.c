// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "azure_uamqp_c/amqp_management.h"
#include "azure_uamqp_c/link.h"
#include "azure_uamqp_c/amqpalloc.h"
#include "azure_uamqp_c/message_sender.h"
#include "azure_uamqp_c/message_receiver.h"
#include "azure_uamqp_c/messaging.h"
#include "azure_uamqp_c/amqpvalue_to_string.h"

typedef enum OPERATION_STATE_TAG
{
    OPERATION_STATE_NOT_SENT,
    OPERATION_STATE_AWAIT_REPLY
} OPERATION_STATE;

typedef struct OPERATION_MESSAGE_INSTANCE_TAG
{
    MESSAGE_HANDLE message;
    OPERATION_STATE operation_state;
    ON_OPERATION_COMPLETE on_operation_complete;
    void* callback_context;
    unsigned long message_id;
} OPERATION_MESSAGE_INSTANCE;

typedef struct AMQP_MANAGEMENT_INSTANCE_TAG
{
    SESSION_HANDLE session;
    LINK_HANDLE sender_link;
    LINK_HANDLE receiver_link;
    MESSAGE_SENDER_HANDLE message_sender;
    MESSAGE_RECEIVER_HANDLE message_receiver;
    OPERATION_MESSAGE_INSTANCE** operation_messages;
    size_t operation_message_count;
    unsigned long next_message_id;
    ON_AMQP_MANAGEMENT_STATE_CHANGED on_amqp_management_state_changed;
    void* callback_context;
    AMQP_MANAGEMENT_STATE amqp_management_state;
    AMQP_MANAGEMENT_STATE previous_amqp_management_state;
    int sender_connected : 1;
    int receiver_connected : 1;
} AMQP_MANAGEMENT_INSTANCE;

static void amqpmanagement_set_state(AMQP_MANAGEMENT_INSTANCE* amqp_management_instance, AMQP_MANAGEMENT_STATE amqp_management_state)
{
    amqp_management_instance->previous_amqp_management_state = amqp_management_instance->amqp_management_state;
    amqp_management_instance->amqp_management_state = amqp_management_state;

    if (amqp_management_instance->on_amqp_management_state_changed != NULL)
    {
        amqp_management_instance->on_amqp_management_state_changed(amqp_management_instance->callback_context, amqp_management_instance->amqp_management_state, amqp_management_instance->previous_amqp_management_state);
    }
}

static void remove_operation_message_by_index(AMQP_MANAGEMENT_INSTANCE* amqp_management_instance, size_t index)
{
    message_destroy(amqp_management_instance->operation_messages[index]->message);
    amqpalloc_free(amqp_management_instance->operation_messages[index]);

    if (amqp_management_instance->operation_message_count - index > 1)
    {
        memmove(&amqp_management_instance->operation_messages[index], &amqp_management_instance->operation_messages[index + 1], sizeof(OPERATION_MESSAGE_INSTANCE*) * (amqp_management_instance->operation_message_count - index - 1));
    }

    if (amqp_management_instance->operation_message_count == 1)
    {
        amqpalloc_free(amqp_management_instance->operation_messages);
        amqp_management_instance->operation_messages = NULL;
    }
    else
    {
        OPERATION_MESSAGE_INSTANCE** new_operation_messages = (OPERATION_MESSAGE_INSTANCE**)amqpalloc_realloc(amqp_management_instance->operation_messages, sizeof(OPERATION_MESSAGE_INSTANCE*) * (amqp_management_instance->operation_message_count - 1));
        if (new_operation_messages != NULL)
        {
            amqp_management_instance->operation_messages = new_operation_messages;
        }
    }

    amqp_management_instance->operation_message_count--;
}

static AMQP_VALUE on_message_received(const void* context, MESSAGE_HANDLE message)
{
    AMQP_MANAGEMENT_INSTANCE* amqp_management_instance = (AMQP_MANAGEMENT_INSTANCE*)context;

    AMQP_VALUE application_properties;
    if (message_get_application_properties(message, &application_properties) != 0)
    {
        /* error */
    }
    else
    {
        PROPERTIES_HANDLE response_properties;

        if (message_get_properties(message, &response_properties) != 0)
        {
            /* error */
        }
        else
        {
            AMQP_VALUE key;
            AMQP_VALUE value;
            AMQP_VALUE desc_key;
            AMQP_VALUE desc_value;
            AMQP_VALUE map;
            AMQP_VALUE correlation_id_value;

            if (properties_get_correlation_id(response_properties, &correlation_id_value) != 0)
            {
                /* error */
            }
            else
            {
                map = amqpvalue_get_inplace_described_value(application_properties);
                if (map == NULL)
                {
                    /* error */
                }
                else
                {
                    key = amqpvalue_create_string("status-code");
                    if (key == NULL)
                    {
                        /* error */
                    }
                    else
                    {
                        value = amqpvalue_get_map_value(map, key);
                        if (value == NULL)
                        {
                            /* error */
                        }
                        else
                        {
                            int32_t status_code;
                            if (amqpvalue_get_int(value, &status_code) != 0)
                            {
                                /* error */
                            }
                            else
                            {
                                desc_key = amqpvalue_create_string("status-description");
                                if (desc_key == NULL)
                                {
                                    /* error */
                                }
                                else
                                {
                                    const char* status_description = NULL;

                                    desc_value = amqpvalue_get_map_value(map, desc_key);
                                    if (desc_value != NULL)
                                    {
                                        amqpvalue_get_string(desc_value, &status_description);
                                    }

                                    size_t i = 0;
                                    while (i < amqp_management_instance->operation_message_count)
                                    {
                                        if (amqp_management_instance->operation_messages[i]->operation_state == OPERATION_STATE_AWAIT_REPLY)
                                        {
                                            AMQP_VALUE expected_message_id = amqpvalue_create_ulong(amqp_management_instance->operation_messages[i]->message_id);
                                            OPERATION_RESULT operation_result;

                                            if (expected_message_id == NULL)
                                            {
                                                break;
                                            }
                                            else
                                            {
                                                if (amqpvalue_are_equal(correlation_id_value, expected_message_id))
                                                {
                                                    /* 202 is not mentioned in the draft in any way, this is a workaround for an EH bug for now */
                                                    if ((status_code != 200) && (status_code != 202))
                                                    {
                                                        operation_result = OPERATION_RESULT_OPERATION_FAILED;
                                                    }
                                                    else
                                                    {
                                                        operation_result = OPERATION_RESULT_OK;
                                                    }

                                                    amqp_management_instance->operation_messages[i]->on_operation_complete(amqp_management_instance->operation_messages[i]->callback_context, operation_result, status_code, status_description);

                                                    remove_operation_message_by_index(amqp_management_instance, i);

                                                    amqpvalue_destroy(expected_message_id);

                                                    break;
                                                }
                                                else
                                                {
                                                    i++;
                                                }

                                                amqpvalue_destroy(expected_message_id);
                                            }
                                        }
                                        else
                                        {
                                            i++;
                                        }
                                    }

                                    if (desc_value != NULL)
                                    {
                                        amqpvalue_destroy(desc_value);
                                    }
                                    amqpvalue_destroy(desc_key);
                                }
                            }
                            amqpvalue_destroy(value);
                        }
                        amqpvalue_destroy(key);
                    }
                }
            }

            properties_destroy(response_properties);
        }

        application_properties_destroy(application_properties);
    }

    return messaging_delivery_accepted();
}

static int send_operation_messages(AMQP_MANAGEMENT_INSTANCE* amqp_management_instance)
{
    int result;

    if ((amqp_management_instance->sender_connected != 0) &&
        (amqp_management_instance->receiver_connected != 0))
    {
        size_t i;

        for (i = 0; i < amqp_management_instance->operation_message_count; i++)
        {
            if (amqp_management_instance->operation_messages[i]->operation_state == OPERATION_STATE_NOT_SENT)
            {
                if (messagesender_send(amqp_management_instance->message_sender, amqp_management_instance->operation_messages[i]->message, NULL, NULL) != 0)
                {
                    /* error */
                    break;
                }

                amqp_management_instance->operation_messages[i]->operation_state = OPERATION_STATE_AWAIT_REPLY;
            }
        }

        if (i < amqp_management_instance->operation_message_count)
        {
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }
    else
    {
        result = 0;
    }

    return result;
}

static void on_message_sender_state_changed(void* context, MESSAGE_SENDER_STATE new_state, MESSAGE_SENDER_STATE previous_state)
{
    AMQP_MANAGEMENT_INSTANCE* amqp_management_instance = (AMQP_MANAGEMENT_INSTANCE*)context;
    (void)previous_state;
    switch (new_state)
    {
    default:
        break;

    case MESSAGE_SENDER_STATE_OPEN:
        amqp_management_instance->sender_connected = 1;
        (void)send_operation_messages(amqp_management_instance);
        break;

    case MESSAGE_SENDER_STATE_CLOSING:
    case MESSAGE_SENDER_STATE_ERROR:
        amqp_management_instance->sender_connected = 0;
        amqpmanagement_set_state(amqp_management_instance, AMQP_MANAGEMENT_STATE_ERROR);
        break;
    }
}

static void on_message_receiver_state_changed(const void* context, MESSAGE_RECEIVER_STATE new_state, MESSAGE_RECEIVER_STATE previous_state)
{
    AMQP_MANAGEMENT_INSTANCE* amqp_management_instance = (AMQP_MANAGEMENT_INSTANCE*)context;
    (void)previous_state;
    switch (new_state)
    {
    default:
        break;

    case MESSAGE_RECEIVER_STATE_OPEN:
        amqp_management_instance->receiver_connected = 1;
        (void)send_operation_messages(amqp_management_instance);
        break;

    case MESSAGE_RECEIVER_STATE_CLOSING:
    case MESSAGE_RECEIVER_STATE_ERROR:
        amqp_management_instance->receiver_connected = 0;
        amqpmanagement_set_state(amqp_management_instance, AMQP_MANAGEMENT_STATE_ERROR);
        break;
    }
}

static int set_message_id(MESSAGE_HANDLE message, unsigned long next_message_id)
{
    int result = 0;

    PROPERTIES_HANDLE properties;
    if (message_get_properties(message, &properties) != 0)
    {
        result = __LINE__;
    }
    else
    {
        AMQP_VALUE message_id = amqpvalue_create_message_id_ulong(next_message_id);
        if (message_id == NULL)
        {
            result = __LINE__;
        }
        else
        {
            if (properties_set_message_id(properties, message_id) != 0)
            {
                result = __LINE__;
            }

            amqpvalue_destroy(message_id);
        }

        if (message_set_properties(message, properties) != 0)
        {
            result = __LINE__;
        }

        properties_destroy(properties);
    }

    return result;
}

static int add_string_key_value_pair_to_map(AMQP_VALUE map, const char* key, const char* value)
{
    int result;

    AMQP_VALUE key_value = amqpvalue_create_string(key);
    if (key == NULL)
    {
        result = __LINE__;
    }
    else
    {
        AMQP_VALUE value_value = amqpvalue_create_string(value);
        if (value_value == NULL)
        {
            result = __LINE__;
        }
        else
        {
            if (amqpvalue_set_map_value(map, key_value, value_value) != 0)
            {
                result = __LINE__;
            }
            else
            {
                result = 0;
            }

            amqpvalue_destroy(key_value);
        }

        amqpvalue_destroy(value_value);
    }

    return result;
}

AMQP_MANAGEMENT_HANDLE amqpmanagement_create(SESSION_HANDLE session, const char* management_node, ON_AMQP_MANAGEMENT_STATE_CHANGED on_amqp_management_state_changed, void* callback_context)
{
    AMQP_MANAGEMENT_INSTANCE* result;

    if (session == NULL)
    {
        result = NULL;
    }
    else
    {
        result = (AMQP_MANAGEMENT_INSTANCE*)amqpalloc_malloc(sizeof(AMQP_MANAGEMENT_INSTANCE));
        if (result != NULL)
        {
            result->session = session;
            result->sender_connected = 0;
            result->receiver_connected = 0;
            result->operation_message_count = 0;
            result->operation_messages = NULL;
            result->on_amqp_management_state_changed = on_amqp_management_state_changed;
            result->callback_context = callback_context;

            AMQP_VALUE source = messaging_create_source(management_node);
            if (source == NULL)
            {
                amqpalloc_free(result);
                result = NULL;
            }
            else
            {
                AMQP_VALUE target = messaging_create_target(management_node);
                if (target == NULL)
                {
                    amqpalloc_free(result);
                    result = NULL;
                }
                else
                {
                    static const char* sender_suffix = "-sender";

                    char* sender_link_name = (char*)amqpalloc_malloc(strlen(management_node) + strlen(sender_suffix) + 1);
                    if (sender_link_name == NULL)
                    {
                        result = NULL;
                    }
                    else
                    {
                        static const char* receiver_suffix = "-receiver";

                        (void)strcpy(sender_link_name, management_node);
                        (void)strcat(sender_link_name, sender_suffix);

                        char* receiver_link_name = (char*)amqpalloc_malloc(strlen(management_node) + strlen(receiver_suffix) + 1);
                        if (receiver_link_name == NULL)
                        {
                            result = NULL;
                        }
                        else
                        {
                            (void)strcpy(receiver_link_name, management_node);
                            (void)strcat(receiver_link_name, receiver_suffix);

                            result->sender_link = link_create(session, "cbs-sender", role_sender, source, target);
                            if (result->sender_link == NULL)
                            {
                                amqpalloc_free(result);
                                result = NULL;
                            }
                            else
                            {
                                result->receiver_link = link_create(session, "cbs-receiver", role_receiver, source, target);
                                if (result->receiver_link == NULL)
                                {
                                    link_destroy(result->sender_link);
                                    amqpalloc_free(result);
                                    result = NULL;
                                }
                                else
                                {
                                    if ((link_set_max_message_size(result->sender_link, 65535) != 0) ||
                                        (link_set_max_message_size(result->receiver_link, 65535) != 0))
                                    {
                                        link_destroy(result->sender_link);
                                        link_destroy(result->receiver_link);
                                        amqpalloc_free(result);
                                        result = NULL;
                                    }
                                    else
                                    {
                                        result->message_sender = messagesender_create(result->sender_link, on_message_sender_state_changed, result);
                                        if (result->message_sender == NULL)
                                        {
                                            link_destroy(result->sender_link);
                                            link_destroy(result->receiver_link);
                                            amqpalloc_free(result);
                                            result = NULL;
                                        }
                                        else
                                        {
                                            result->message_receiver = messagereceiver_create(result->receiver_link, on_message_receiver_state_changed, result);
                                            if (result->message_receiver == NULL)
                                            {
                                                messagesender_destroy(result->message_sender);
                                                link_destroy(result->sender_link);
                                                link_destroy(result->receiver_link);
                                                amqpalloc_free(result);
                                                result = NULL;
                                            }
                                            else
                                            {
                                                result->next_message_id = 0;
                                            }
                                        }
                                    }
                                }
                            }

                            amqpalloc_free(receiver_link_name);
                        }

                        amqpalloc_free(sender_link_name);
                    }

                    amqpvalue_destroy(target);
                }

                amqpvalue_destroy(source);
            }
        }
    }

    return result;
}

void amqpmanagement_destroy(AMQP_MANAGEMENT_HANDLE amqp_management)
{
    if (amqp_management != NULL)
    {
        (void)amqpmanagement_close(amqp_management);

        if (amqp_management->operation_message_count > 0)
        {
            size_t i;
            for (i = 0; i < amqp_management->operation_message_count; i++)
            {
                message_destroy(amqp_management->operation_messages[i]->message);
                amqpalloc_free(amqp_management->operation_messages[i]);
            }

            amqpalloc_free(amqp_management->operation_messages);
        }

        link_destroy(amqp_management->sender_link);
        link_destroy(amqp_management->receiver_link);
        messagesender_destroy(amqp_management->message_sender);
        messagereceiver_destroy(amqp_management->message_receiver);
        amqpalloc_free(amqp_management);
    }
}

int amqpmanagement_open(AMQP_MANAGEMENT_HANDLE amqp_management)
{
    int result;

    if (amqp_management == NULL)
    {
        result = __LINE__;
    }
    else
    {
        if (messagereceiver_open(amqp_management->message_receiver, on_message_received, amqp_management) != 0)
        {
            result = __LINE__;
        }
        else
        {
            if (messagesender_open(amqp_management->message_sender) != 0)
            {
                messagereceiver_close(amqp_management->message_receiver);
                result = __LINE__;
            }
            else
            {
                result = 0;
            }
        }
    }

    return result;
}

int amqpmanagement_close(AMQP_MANAGEMENT_HANDLE amqp_management)
{
    int result;

    if (amqp_management == NULL)
    {
        result = __LINE__;
    }
    else
    {
        if ((messagesender_close(amqp_management->message_sender) != 0) ||
            (messagereceiver_close(amqp_management->message_receiver) != 0))
        {
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

int amqpmanagement_start_operation(AMQP_MANAGEMENT_HANDLE amqp_management, const char* operation, const char* type, const char* locales, MESSAGE_HANDLE message, ON_OPERATION_COMPLETE on_operation_complete, void* context)
{
    int result;

    if ((amqp_management == NULL) ||
        (operation == NULL))
    {
        result = __LINE__;
    }
    else
    {
        AMQP_VALUE application_properties;
        if (message_get_application_properties(message, &application_properties) != 0)
        {
            result = __LINE__;
        }
        else
        {
            if ((add_string_key_value_pair_to_map(application_properties, "operation", operation) != 0) ||
                (add_string_key_value_pair_to_map(application_properties, "type", type) != 0) ||
                ((locales != NULL) && (add_string_key_value_pair_to_map(application_properties, "locales", locales) != 0)))
            {
                result = __LINE__;
            }
            else
            {
                if ((message_set_application_properties(message, application_properties) != 0) ||
                    (set_message_id(message, amqp_management->next_message_id) != 0))
                {
                    result = __LINE__;
                }
                else
                {
                    OPERATION_MESSAGE_INSTANCE* pending_operation_message = amqpalloc_malloc(sizeof(OPERATION_MESSAGE_INSTANCE));
                    if (pending_operation_message == NULL)
                    {
                        result = __LINE__;
                    }
                    else
                    {
                        pending_operation_message->message = message_clone(message);
                        pending_operation_message->callback_context = context;
                        pending_operation_message->on_operation_complete = on_operation_complete;
                        pending_operation_message->operation_state = OPERATION_STATE_NOT_SENT;
                        pending_operation_message->message_id = amqp_management->next_message_id;

                        amqp_management->next_message_id++;

                        OPERATION_MESSAGE_INSTANCE** new_operation_messages = amqpalloc_realloc(amqp_management->operation_messages, (amqp_management->operation_message_count + 1) * sizeof(OPERATION_MESSAGE_INSTANCE*));
                        if (new_operation_messages == NULL)
                        {
                            message_destroy(message);
                            amqpalloc_free(pending_operation_message);
                            result = __LINE__;
                        }
                        else
                        {
                            amqp_management->operation_messages = new_operation_messages;
                            amqp_management->operation_messages[amqp_management->operation_message_count] = pending_operation_message;
                            amqp_management->operation_message_count++;

                            if (send_operation_messages(amqp_management) != 0)
                            {
                                if (on_operation_complete != NULL)
                                {
                                    on_operation_complete(context, OPERATION_RESULT_CBS_ERROR, 0, NULL);
                                }

                                result = __LINE__;
                            }
                            else
                            {
                                result = 0;
                            }
                        }
                    }
                }
            }

            amqpvalue_destroy(application_properties);
        }
    }
    return result;
}

void amqpmanagement_set_trace(AMQP_MANAGEMENT_HANDLE amqp_management, bool traceOn)
{
    if (amqp_management != NULL)
    {
        messagesender_set_trace(amqp_management->message_sender, traceOn);
    }
}
