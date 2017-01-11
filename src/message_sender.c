// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stdbool.h>
#include <string.h>
#include "azure_c_shared_utility/xlogging.h"
#include "azure_uamqp_c/message_sender.h"
#include "azure_uamqp_c/amqpalloc.h"
#include "azure_uamqp_c/amqpvalue_to_string.h"

typedef enum MESSAGE_SEND_STATE_TAG
{
    MESSAGE_SEND_STATE_NOT_SENT,
    MESSAGE_SEND_STATE_PENDING
} MESSAGE_SEND_STATE;

typedef enum SEND_ONE_MESSAGE_RESULT_TAG
{
    SEND_ONE_MESSAGE_OK,
    SEND_ONE_MESSAGE_ERROR,
    SEND_ONE_MESSAGE_BUSY
} SEND_ONE_MESSAGE_RESULT;

typedef struct MESSAGE_WITH_CALLBACK_TAG
{
    MESSAGE_HANDLE message;
    ON_MESSAGE_SEND_COMPLETE on_message_send_complete;
    void* context;
    MESSAGE_SENDER_HANDLE message_sender;
    MESSAGE_SEND_STATE message_send_state;
} MESSAGE_WITH_CALLBACK;

typedef struct MESSAGE_SENDER_INSTANCE_TAG
{
    LINK_HANDLE link;
    size_t message_count;
    MESSAGE_WITH_CALLBACK** messages;
    MESSAGE_SENDER_STATE message_sender_state;
    ON_MESSAGE_SENDER_STATE_CHANGED on_message_sender_state_changed;
    void* on_message_sender_state_changed_context;
    unsigned int is_trace_on : 1;
} MESSAGE_SENDER_INSTANCE;

static void remove_pending_message_by_index(MESSAGE_SENDER_INSTANCE* message_sender_instance, size_t index)
{
    MESSAGE_WITH_CALLBACK** new_messages;

    if (message_sender_instance->messages[index]->message != NULL)
    {
        message_destroy(message_sender_instance->messages[index]->message);
        message_sender_instance->messages[index]->message = NULL;
    }

    amqpalloc_free(message_sender_instance->messages[index]);

    if (message_sender_instance->message_count - index > 1)
    {
        (void)memmove(&message_sender_instance->messages[index], &message_sender_instance->messages[index + 1], sizeof(MESSAGE_WITH_CALLBACK*) * (message_sender_instance->message_count - index - 1));
    }

    message_sender_instance->message_count--;

    if (message_sender_instance->message_count > 0)
    {
        new_messages = (MESSAGE_WITH_CALLBACK**)amqpalloc_realloc(message_sender_instance->messages, sizeof(MESSAGE_WITH_CALLBACK*) * (message_sender_instance->message_count));
        if (new_messages != NULL)
        {
            message_sender_instance->messages = new_messages;
        }
    }
    else
    {
        amqpalloc_free(message_sender_instance->messages);
        message_sender_instance->messages = NULL;
    }
}

static void remove_pending_message(MESSAGE_SENDER_INSTANCE* message_sender_instance, MESSAGE_WITH_CALLBACK* message_with_callback)
{
    size_t i;

    for (i = 0; i < message_sender_instance->message_count; i++)
    {
        if (message_sender_instance->messages[i] == message_with_callback)
        {
            remove_pending_message_by_index(message_sender_instance, i);
            break;
        }
    }
}

static void on_delivery_settled(void* context, delivery_number delivery_no, AMQP_VALUE delivery_state)
{
    MESSAGE_WITH_CALLBACK* message_with_callback = (MESSAGE_WITH_CALLBACK*)context;
    MESSAGE_SENDER_INSTANCE* message_sender_instance = (MESSAGE_SENDER_INSTANCE*)message_with_callback->message_sender;
    (void)delivery_no;

    if (message_with_callback->on_message_send_complete != NULL)
    {
        AMQP_VALUE descriptor = amqpvalue_get_inplace_descriptor(delivery_state);
        if ((descriptor == NULL) && (delivery_state != NULL))
        {
            LogError("Error getting descriptor for delivery state");
        }
        else
        {
            MESSAGE_SEND_RESULT message_send_result;

            if ((delivery_state == NULL) ||
                (is_accepted_type_by_descriptor(descriptor)))
            {
                message_send_result = MESSAGE_SEND_OK;
            }
            else
            {
                message_send_result = MESSAGE_SEND_ERROR;
            }

            message_with_callback->on_message_send_complete(message_with_callback->context, message_send_result);
        }
    }

    remove_pending_message(message_sender_instance, message_with_callback);
}

static int encode_bytes(void* context, const unsigned char* bytes, size_t length)
{
    PAYLOAD* payload = (PAYLOAD*)context;
    (void)memcpy((unsigned char*)payload->bytes + payload->length, bytes, length);
    payload->length += length;
    return 0;
}

static void log_message_chunk(MESSAGE_SENDER_INSTANCE* message_sender_instance, const char* name, AMQP_VALUE value)
{
#ifdef NO_LOGGING
    UNUSED(message_sender_instance);
    UNUSED(name);
    UNUSED(value);
#else
    if (xlogging_get_log_function() != NULL && message_sender_instance->is_trace_on == 1)
    {
        char* value_as_string = NULL;
        LOG(AZ_LOG_TRACE, 0, "%s", name);
        LOG(AZ_LOG_TRACE, 0, "%s", (value_as_string = amqpvalue_to_string(value)));
        if (value_as_string != NULL)
        {
            amqpalloc_free(value_as_string);
        }
    }
#endif
}

static SEND_ONE_MESSAGE_RESULT send_one_message(MESSAGE_SENDER_INSTANCE* message_sender_instance, MESSAGE_WITH_CALLBACK* message_with_callback, MESSAGE_HANDLE message)
{
    SEND_ONE_MESSAGE_RESULT result;

    size_t encoded_size;
    size_t total_encoded_size = 0;
    MESSAGE_BODY_TYPE message_body_type;
    message_format message_format;

    if ((message_get_body_type(message, &message_body_type) != 0) ||
        (message_get_message_format(message, &message_format) != 0))
    {
        result = SEND_ONE_MESSAGE_ERROR;
    }
    else
    {
        // header
        HEADER_HANDLE header;
        AMQP_VALUE header_amqp_value;
        PROPERTIES_HANDLE properties;
        AMQP_VALUE properties_amqp_value;
        AMQP_VALUE application_properties;
        AMQP_VALUE application_properties_value;
        AMQP_VALUE body_amqp_value = NULL;
        size_t body_data_count = 0;
        AMQP_VALUE msg_annotations = NULL;

        message_get_header(message, &header);
        header_amqp_value = amqpvalue_create_header(header);
        if (header != NULL)
        {
            amqpvalue_get_encoded_size(header_amqp_value, &encoded_size);
            total_encoded_size += encoded_size;
        }

        // message annotations
        if ((message_get_message_annotations(message, &msg_annotations) == 0) &&
            (msg_annotations != NULL))
        {
            amqpvalue_get_encoded_size(msg_annotations, &encoded_size);
            total_encoded_size += encoded_size;
        }

        // properties
        message_get_properties(message, &properties);
        properties_amqp_value = amqpvalue_create_properties(properties);
        if (properties != NULL)
        {
            amqpvalue_get_encoded_size(properties_amqp_value, &encoded_size);
            total_encoded_size += encoded_size;
        }

        // application properties
        message_get_application_properties(message, &application_properties);
        application_properties_value = amqpvalue_create_application_properties(application_properties);
        if (application_properties != NULL)
        {
            amqpvalue_get_encoded_size(application_properties_value, &encoded_size);
            total_encoded_size += encoded_size;
        }

        result = SEND_ONE_MESSAGE_OK;

        // body - amqp data
        switch (message_body_type)
        {
            default:
                result = SEND_ONE_MESSAGE_ERROR;
                break;

            case MESSAGE_BODY_TYPE_VALUE:
            {
                AMQP_VALUE message_body_amqp_value;
                if (message_get_inplace_body_amqp_value(message, &message_body_amqp_value) != 0)
                {
                    result = SEND_ONE_MESSAGE_ERROR;
                }
                else
                {
                    body_amqp_value = amqpvalue_create_amqp_value(message_body_amqp_value);
                    if ((body_amqp_value == NULL) ||
                        (amqpvalue_get_encoded_size(body_amqp_value, &encoded_size) != 0))
                    {
                        result = SEND_ONE_MESSAGE_ERROR;
                    }
                    else
                    {
                        total_encoded_size += encoded_size;
                    }
                }

                break;
            }

            case MESSAGE_BODY_TYPE_DATA:
            {
                BINARY_DATA binary_data;
                size_t i;

                if (message_get_body_amqp_data_count(message, &body_data_count) != 0)
                {
                    result = SEND_ONE_MESSAGE_ERROR;
                }
                else
                {
                    for (i = 0; i < body_data_count; i++)
                    {
                        if (message_get_body_amqp_data(message, i, &binary_data) != 0)
                        {
                            result = SEND_ONE_MESSAGE_ERROR;
                        }
                        else
                        {
                            amqp_binary binary_value;
                            binary_value.bytes = binary_data.bytes;
                            binary_value.length = (uint32_t)binary_data.length;
                            AMQP_VALUE body_amqp_data = amqpvalue_create_data(binary_value);
                            if (body_amqp_data == NULL)
                            {
                                result = SEND_ONE_MESSAGE_ERROR;
                            }
                            else
                            {
                                if (amqpvalue_get_encoded_size(body_amqp_data, &encoded_size) != 0)
                                {
                                    result = SEND_ONE_MESSAGE_ERROR;
                                }
                                else
                                {
                                    total_encoded_size += encoded_size;
                                }

                                amqpvalue_destroy(body_amqp_data);
                            }
                        }
                    }
                }
                break;
            }
        }

        if (result == 0)
        {
            void* data_bytes = amqpalloc_malloc(total_encoded_size);
            PAYLOAD payload;
            payload.bytes = data_bytes;
            payload.length = 0;
            result = SEND_ONE_MESSAGE_OK;

            if (header != NULL)
            {
                if (amqpvalue_encode(header_amqp_value, encode_bytes, &payload) != 0)
                {
                    result = SEND_ONE_MESSAGE_ERROR;
                }

                log_message_chunk(message_sender_instance, "Header:", header_amqp_value);
            }

            if ((result == SEND_ONE_MESSAGE_OK) && (msg_annotations != NULL))
            {
                if (amqpvalue_encode(msg_annotations, encode_bytes, &payload) != 0)
                {
                    result = SEND_ONE_MESSAGE_ERROR;
                }

                log_message_chunk(message_sender_instance, "Message Annotations:", msg_annotations);
            }

            if ((result == SEND_ONE_MESSAGE_OK) && (properties != NULL))
            {
                if (amqpvalue_encode(properties_amqp_value, encode_bytes, &payload) != 0)
                {
                    result = SEND_ONE_MESSAGE_ERROR;
                }

                log_message_chunk(message_sender_instance, "Properties:", properties_amqp_value);
            }

            if ((result == SEND_ONE_MESSAGE_OK) && (application_properties != NULL))
            {
                if (amqpvalue_encode(application_properties_value, encode_bytes, &payload) != 0)
                {
                    result = SEND_ONE_MESSAGE_ERROR;
                }

                log_message_chunk(message_sender_instance, "Application properties:", application_properties_value);
            }

            if (result == SEND_ONE_MESSAGE_OK)
            {
                switch (message_body_type)
                {
                case MESSAGE_BODY_TYPE_VALUE:
                {
                    if (amqpvalue_encode(body_amqp_value, encode_bytes, &payload) != 0)
                    {
                        result = SEND_ONE_MESSAGE_ERROR;
                    }

                    log_message_chunk(message_sender_instance, "Body - amqp value:", body_amqp_value);
                    break;
                }
                case MESSAGE_BODY_TYPE_DATA:
                {
                    BINARY_DATA binary_data;
                    size_t i;

                    for (i = 0; i < body_data_count; i++)
                    {
                        if (message_get_body_amqp_data(message, i, &binary_data) != 0)
                        {
                            result = SEND_ONE_MESSAGE_ERROR;
                        }
                        else
                        {
                            amqp_binary binary_value;
                            binary_value.bytes = binary_data.bytes;
                            binary_value.length = (uint32_t)binary_data.length;
                            AMQP_VALUE body_amqp_data = amqpvalue_create_data(binary_value);
                            if (body_amqp_data == NULL)
                            {
                                result = SEND_ONE_MESSAGE_ERROR;
                            }
                            else
                            {
                                if (amqpvalue_encode(body_amqp_data, encode_bytes, &payload) != 0)
                                {
                                    result = SEND_ONE_MESSAGE_ERROR;
                                    break;
                                }

                                amqpvalue_destroy(body_amqp_data);
                            }
                        }
                    }
                    break;
                }
                }
            }

            if (result == SEND_ONE_MESSAGE_OK)
            {
                message_with_callback->message_send_state = MESSAGE_SEND_STATE_PENDING;
                switch (link_transfer(message_sender_instance->link, message_format, &payload, 1, on_delivery_settled, message_with_callback))
                {
                default:
                case LINK_TRANSFER_ERROR:
                    if (message_with_callback->on_message_send_complete != NULL)
                    {
                        message_with_callback->on_message_send_complete(message_with_callback->context, MESSAGE_SEND_ERROR);
                    }

                    result = SEND_ONE_MESSAGE_ERROR;
                    break;

                case LINK_TRANSFER_BUSY:
                    message_with_callback->message_send_state = MESSAGE_SEND_STATE_NOT_SENT;
                    result = SEND_ONE_MESSAGE_BUSY;
                    break;

                case LINK_TRANSFER_OK:
                    result = SEND_ONE_MESSAGE_OK;
                    break;
                }
            }

            amqpalloc_free(data_bytes);

            if (body_amqp_value != NULL)
            {
                amqpvalue_destroy(body_amqp_value);
            }

            amqpvalue_destroy(application_properties);
            amqpvalue_destroy(application_properties_value);
            amqpvalue_destroy(properties_amqp_value);
            properties_destroy(properties);
        }
    }

    return result;
}

static void send_all_pending_messages(MESSAGE_SENDER_INSTANCE* message_sender_instance)
{
    size_t i;

    for (i = 0; i < message_sender_instance->message_count; i++)
    {
        if (message_sender_instance->messages[i]->message_send_state == MESSAGE_SEND_STATE_NOT_SENT)
        {
            switch (send_one_message(message_sender_instance, message_sender_instance->messages[i], message_sender_instance->messages[i]->message))
            {
            default:
            case SEND_ONE_MESSAGE_ERROR:
            {
                ON_MESSAGE_SEND_COMPLETE on_message_send_complete = message_sender_instance->messages[i]->on_message_send_complete;
                void* context = message_sender_instance->messages[i]->context;
                remove_pending_message_by_index(message_sender_instance, i);

                on_message_send_complete(context, MESSAGE_SEND_ERROR);
                i = message_sender_instance->message_count;
                break;
            }
            case SEND_ONE_MESSAGE_BUSY:
                i = message_sender_instance->message_count + 1;
                break;

            case SEND_ONE_MESSAGE_OK:
                break;
            }

            i--;
        }
    }
}

static void set_message_sender_state(MESSAGE_SENDER_INSTANCE* message_sender_instance, MESSAGE_SENDER_STATE new_state)
{
    MESSAGE_SENDER_STATE previous_state = message_sender_instance->message_sender_state;
    message_sender_instance->message_sender_state = new_state;
    if (message_sender_instance->on_message_sender_state_changed != NULL)
    {
        message_sender_instance->on_message_sender_state_changed(message_sender_instance->on_message_sender_state_changed_context, new_state, previous_state);
    }
}

static void indicate_all_messages_as_error(MESSAGE_SENDER_INSTANCE* message_sender_instance)
{
    size_t i;

    for (i = 0; i < message_sender_instance->message_count; i++)
    {
        if (message_sender_instance->messages[i]->on_message_send_complete != NULL)
        {
            message_sender_instance->messages[i]->on_message_send_complete(message_sender_instance->messages[i]->context, MESSAGE_SEND_ERROR);
        }

        message_destroy(message_sender_instance->messages[i]->message);
        amqpalloc_free(message_sender_instance->messages[i]);
    }

    if (message_sender_instance->messages != NULL)
    {
        message_sender_instance->message_count = 0;

        amqpalloc_free(message_sender_instance->messages);
        message_sender_instance->messages = NULL;
    }
}

static void on_link_state_changed(void* context, LINK_STATE new_link_state, LINK_STATE previous_link_state)
{
    MESSAGE_SENDER_INSTANCE* message_sender_instance = (MESSAGE_SENDER_INSTANCE*)context;
    (void)previous_link_state;

    switch (new_link_state)
    {
    case LINK_STATE_ATTACHED:
        if (message_sender_instance->message_sender_state == MESSAGE_SENDER_STATE_OPENING)
        {
            set_message_sender_state(message_sender_instance, MESSAGE_SENDER_STATE_OPEN);
        }
        break;
    case LINK_STATE_DETACHED:
        if ((message_sender_instance->message_sender_state == MESSAGE_SENDER_STATE_OPEN) ||
            (message_sender_instance->message_sender_state == MESSAGE_SENDER_STATE_CLOSING))
        {
            /* User initiated transition, we should be good */
            indicate_all_messages_as_error(message_sender_instance);
            set_message_sender_state(message_sender_instance, MESSAGE_SENDER_STATE_IDLE);
        }
        else if (message_sender_instance->message_sender_state != MESSAGE_SENDER_STATE_IDLE)
        {
            /* Any other transition must be an error */
            set_message_sender_state(message_sender_instance, MESSAGE_SENDER_STATE_ERROR);
        }
        break;
    case LINK_STATE_ERROR:
        if (message_sender_instance->message_sender_state != MESSAGE_SENDER_STATE_ERROR)
        {
            indicate_all_messages_as_error(message_sender_instance);
            set_message_sender_state(message_sender_instance, MESSAGE_SENDER_STATE_ERROR);
        }
        break;
    }
}

static void on_link_flow_on(void* context)
{
    MESSAGE_SENDER_INSTANCE* message_sender_instance = (MESSAGE_SENDER_INSTANCE*)context;
    send_all_pending_messages(message_sender_instance);
}

MESSAGE_SENDER_HANDLE messagesender_create(LINK_HANDLE link, ON_MESSAGE_SENDER_STATE_CHANGED on_message_sender_state_changed, void* context)
{
    MESSAGE_SENDER_INSTANCE* result = amqpalloc_malloc(sizeof(MESSAGE_SENDER_INSTANCE));
    if (result != NULL)
    {
        result->messages = NULL;
        result->message_count = 0;
        result->link = link;
        result->on_message_sender_state_changed = on_message_sender_state_changed;
        result->on_message_sender_state_changed_context = context;
        result->message_sender_state = MESSAGE_SENDER_STATE_IDLE;
        result->is_trace_on = 0;
    }

    return result;
}

void messagesender_destroy(MESSAGE_SENDER_HANDLE message_sender)
{
    if (message_sender != NULL)
    {
        MESSAGE_SENDER_INSTANCE* message_sender_instance = (MESSAGE_SENDER_INSTANCE*)message_sender;

        messagesender_close(message_sender_instance);

        indicate_all_messages_as_error(message_sender_instance);

        amqpalloc_free(message_sender);
    }
}

int messagesender_open(MESSAGE_SENDER_HANDLE message_sender)
{
    int result;

    if (message_sender == NULL)
    {
        result = __LINE__;
    }
    else
    {
        MESSAGE_SENDER_INSTANCE* message_sender_instance = (MESSAGE_SENDER_INSTANCE*)message_sender;

        if (message_sender_instance->message_sender_state == MESSAGE_SENDER_STATE_IDLE)
        {
            set_message_sender_state(message_sender_instance, MESSAGE_SENDER_STATE_OPENING);
            if (link_attach(message_sender_instance->link, NULL, on_link_state_changed, on_link_flow_on, message_sender_instance) != 0)
            {
                result = __LINE__;
                set_message_sender_state(message_sender_instance, MESSAGE_SENDER_STATE_ERROR);
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
    }

    return result;
}

int messagesender_close(MESSAGE_SENDER_HANDLE message_sender)
{
    int result;

    if (message_sender == NULL)
    {
        result = __LINE__;
    }
    else
    {
        MESSAGE_SENDER_INSTANCE* message_sender_instance = (MESSAGE_SENDER_INSTANCE*)message_sender;

        if ((message_sender_instance->message_sender_state == MESSAGE_SENDER_STATE_OPENING) ||
            (message_sender_instance->message_sender_state == MESSAGE_SENDER_STATE_OPEN))
        {
            set_message_sender_state(message_sender_instance, MESSAGE_SENDER_STATE_CLOSING);
            if (link_detach(message_sender_instance->link, true) != 0)
            {
                result = __LINE__;
                set_message_sender_state(message_sender_instance, MESSAGE_SENDER_STATE_ERROR);
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
    }

    return result;
}

int messagesender_send(MESSAGE_SENDER_HANDLE message_sender, MESSAGE_HANDLE message, ON_MESSAGE_SEND_COMPLETE on_message_send_complete, void* callback_context)
{
    int result;

    if ((message_sender == NULL) ||
        (message == NULL))
    {
        result = __LINE__;
    }
    else
    {
        MESSAGE_SENDER_INSTANCE* message_sender_instance = (MESSAGE_SENDER_INSTANCE*)message_sender;
        if (message_sender_instance->message_sender_state == MESSAGE_SENDER_STATE_ERROR)
        {
            result = __LINE__;
        }
        else
        {
            MESSAGE_WITH_CALLBACK* message_with_callback = (MESSAGE_WITH_CALLBACK*)amqpalloc_malloc(sizeof(MESSAGE_WITH_CALLBACK));
            if (message_with_callback == NULL)
            {
                result = __LINE__;
            }
            else
            {
                MESSAGE_WITH_CALLBACK** new_messages = (MESSAGE_WITH_CALLBACK**)amqpalloc_realloc(message_sender_instance->messages, sizeof(MESSAGE_WITH_CALLBACK*) * (message_sender_instance->message_count + 1));
                if (new_messages == NULL)
                {
                    amqpalloc_free(message_with_callback);
                    result = __LINE__;
                }
                else
                {
                    result = 0;

                    message_sender_instance->messages = new_messages;
                    if (message_sender_instance->message_sender_state != MESSAGE_SENDER_STATE_OPEN)
                    {
                        message_with_callback->message = message_clone(message);
                        if (message_with_callback->message == NULL)
                        {
                            amqpalloc_free(message_with_callback);
                            result = __LINE__;
                        }

                        message_with_callback->message_send_state = MESSAGE_SEND_STATE_NOT_SENT;
                    }
                    else
                    {
                        message_with_callback->message = NULL;
                        message_with_callback->message_send_state = MESSAGE_SEND_STATE_PENDING;
                    }

                    if (result == 0)
                    {
                        message_with_callback->on_message_send_complete = on_message_send_complete;
                        message_with_callback->context = callback_context;
                        message_with_callback->message_sender = message_sender_instance;

                        message_sender_instance->messages[message_sender_instance->message_count] = message_with_callback;
                        message_sender_instance->message_count++;

                        if (message_sender_instance->message_sender_state == MESSAGE_SENDER_STATE_OPEN)
                        {
                            switch (send_one_message(message_sender_instance, message_with_callback, message))
                            {
                            default:
                            case SEND_ONE_MESSAGE_ERROR:
                                remove_pending_message_by_index(message_sender_instance, message_sender_instance->message_count - 1);
                                result = __LINE__;
                                break;

                            case SEND_ONE_MESSAGE_BUSY:
                                message_with_callback->message = message_clone(message);
                                if (message_with_callback->message == NULL)
                                {
                                    amqpalloc_free(message_with_callback);
                                    result = __LINE__;
                                }
                                else
                                {
                                    message_with_callback->message_send_state = MESSAGE_SEND_STATE_NOT_SENT;
                                    result = 0;
                                }
                                break;

                            case SEND_ONE_MESSAGE_OK:
                                result = 0;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    return result;
}

void messagesender_set_trace(MESSAGE_SENDER_HANDLE message_sender, bool traceOn)
{
    MESSAGE_SENDER_INSTANCE* message_sender_instance = (MESSAGE_SENDER_INSTANCE*)message_sender;
    if (message_sender_instance != NULL)
    {
        message_sender_instance->is_trace_on = traceOn ? 1 : 0;
    }
}
