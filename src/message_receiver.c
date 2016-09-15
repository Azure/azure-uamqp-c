// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include "azure_uamqp_c/message_receiver.h"
#include "azure_uamqp_c/amqpalloc.h"
#include "azure_uamqp_c/amqpvalue.h"
#include "azure_uamqp_c/amqp_definitions.h"

typedef struct MESSAGE_RECEIVER_INSTANCE_TAG
{
	LINK_HANDLE link;
	ON_MESSAGE_RECEIVED on_message_received;
	ON_MESSAGE_RECEIVER_STATE_CHANGED on_message_receiver_state_changed;
	MESSAGE_RECEIVER_STATE message_receiver_state;
	const void* on_message_receiver_state_changed_context;
	const void* callback_context;
	MESSAGE_HANDLE decoded_message;
	bool decode_error;
} MESSAGE_RECEIVER_INSTANCE;

static void set_message_receiver_state(MESSAGE_RECEIVER_INSTANCE* message_receiver_instance, MESSAGE_RECEIVER_STATE new_state)
{
	MESSAGE_RECEIVER_STATE previous_state = message_receiver_instance->message_receiver_state;
	message_receiver_instance->message_receiver_state = new_state;
	if (message_receiver_instance->on_message_receiver_state_changed != NULL)
	{
		message_receiver_instance->on_message_receiver_state_changed(message_receiver_instance->on_message_receiver_state_changed_context, new_state, previous_state);
	}
}

static void decode_message_value_callback(void* context, AMQP_VALUE decoded_value)
{
	MESSAGE_RECEIVER_INSTANCE* message_receiver_instance = (MESSAGE_RECEIVER_INSTANCE*)context;
	MESSAGE_HANDLE decoded_message = message_receiver_instance->decoded_message;
	AMQP_VALUE descriptor = amqpvalue_get_inplace_descriptor(decoded_value);

	if (is_application_properties_type_by_descriptor(descriptor))
	{
		if (message_set_application_properties(decoded_message, decoded_value) != 0)
		{
			message_receiver_instance->decode_error = true;
		}
	}
	else if (is_properties_type_by_descriptor(descriptor))
	{
		PROPERTIES_HANDLE properties;
		if (amqpvalue_get_properties(decoded_value, &properties) != 0)
		{
			message_receiver_instance->decode_error = true;
		}
		else
		{
			if (message_set_properties(decoded_message, properties) != 0)
			{
				message_receiver_instance->decode_error = true;
			}

			properties_destroy(properties);
		}
	}
	else if (is_delivery_annotations_type_by_descriptor(descriptor))
	{
		annotations delivery_annotations = amqpvalue_get_inplace_described_value(decoded_value);
		if ((delivery_annotations == NULL) ||
			(message_set_delivery_annotations(decoded_message, delivery_annotations) != 0))
		{
			message_receiver_instance->decode_error = true;
		}
	}
	else if (is_message_annotations_type_by_descriptor(descriptor))
	{
		annotations message_annotations = amqpvalue_get_inplace_described_value(decoded_value);
		if ((message_annotations == NULL) ||
			(message_set_message_annotations(decoded_message, message_annotations) != 0))
		{
			message_receiver_instance->decode_error = true;
		}
	}
	else if (is_header_type_by_descriptor(descriptor))
	{
		HEADER_HANDLE header;
		if (amqpvalue_get_header(decoded_value, &header) != 0)
		{
			message_receiver_instance->decode_error = true;
		}
		else
		{
			if (message_set_header(decoded_message, header) != 0)
			{
				message_receiver_instance->decode_error = true;
			}

			header_destroy(header);
		}
	}
	else if (is_footer_type_by_descriptor(descriptor))
	{
		annotations footer = amqpvalue_get_inplace_described_value(decoded_value);
		if ((footer == NULL) ||
			(message_set_footer(decoded_message, footer) != 0))
		{
			message_receiver_instance->decode_error = true;
		}
	}
	else if (is_amqp_value_type_by_descriptor(descriptor))
	{
		MESSAGE_BODY_TYPE body_type;
		message_get_body_type(decoded_message, &body_type);
		if (body_type != MESSAGE_BODY_TYPE_NONE)
		{
			message_receiver_instance->decode_error = true;
		}
		else
		{
			AMQP_VALUE body_amqp_value = amqpvalue_get_inplace_described_value(decoded_value);
			if ((body_amqp_value == NULL) ||
				(message_set_body_amqp_value(decoded_message, body_amqp_value) != 0))
			{
				message_receiver_instance->decode_error = true;
			}
		}
	}
	else if (is_data_type_by_descriptor(descriptor))
	{
		MESSAGE_BODY_TYPE body_type;
		message_get_body_type(decoded_message, &body_type);
		if ((body_type != MESSAGE_BODY_TYPE_NONE) &&
			(body_type != MESSAGE_BODY_TYPE_DATA))
		{
			message_receiver_instance->decode_error = true;
		}
		else
		{
			AMQP_VALUE body_data_value = amqpvalue_get_inplace_described_value(decoded_value);
			data data_value;

			if ((body_data_value == NULL) ||
				(amqpvalue_get_data(body_data_value, &data_value) != 0))
			{
				message_receiver_instance->decode_error = true;
			}
			else
			{
                BINARY_DATA binary_data;
                binary_data.bytes = data_value.bytes;
                binary_data.length = data_value.length;
				if (message_add_body_amqp_data(decoded_message, binary_data) != 0)
				{
					message_receiver_instance->decode_error = true;
				}
			}
		}
	}
}

static AMQP_VALUE on_transfer_received(void* context, TRANSFER_HANDLE transfer, uint32_t payload_size, const unsigned char* payload_bytes)
{
	AMQP_VALUE result = NULL;

	MESSAGE_RECEIVER_INSTANCE* message_receiver_instance = (MESSAGE_RECEIVER_INSTANCE*)context;
    (void)transfer;
	if (message_receiver_instance->on_message_received != NULL)
	{
		MESSAGE_HANDLE message = message_create();
		if (message == NULL)
		{
			set_message_receiver_state(message_receiver_instance, MESSAGE_RECEIVER_STATE_ERROR);
		}
		else
		{
			message_receiver_instance->decoded_message;
			AMQPVALUE_DECODER_HANDLE amqpvalue_decoder = amqpvalue_decoder_create(decode_message_value_callback, message_receiver_instance);
			if (amqpvalue_decoder == NULL)
			{
				set_message_receiver_state(message_receiver_instance, MESSAGE_RECEIVER_STATE_ERROR);
			}
			else
			{
				message_receiver_instance->decoded_message = message;
				message_receiver_instance->decode_error = false;
				if (amqpvalue_decode_bytes(amqpvalue_decoder, payload_bytes, payload_size) != 0)
				{
					set_message_receiver_state(message_receiver_instance, MESSAGE_RECEIVER_STATE_ERROR);
				}
				else
				{
					if (message_receiver_instance->decode_error)
					{
						set_message_receiver_state(message_receiver_instance, MESSAGE_RECEIVER_STATE_ERROR);
					}
					else
					{
						result = message_receiver_instance->on_message_received(message_receiver_instance->callback_context, message);
					}
				}

				amqpvalue_decoder_destroy(amqpvalue_decoder);
			}

			message_destroy(message);
		}
	}

	return result;
}

static void on_link_state_changed(void* context, LINK_STATE new_link_state, LINK_STATE previous_link_state)
{
	MESSAGE_RECEIVER_INSTANCE* message_receiver_instance = (MESSAGE_RECEIVER_INSTANCE*)context;
    (void)previous_link_state;

	switch (new_link_state)
	{
	case LINK_STATE_ATTACHED:
		if (message_receiver_instance->message_receiver_state == MESSAGE_RECEIVER_STATE_OPENING)
		{
			set_message_receiver_state(message_receiver_instance, MESSAGE_RECEIVER_STATE_OPEN);
		}
		break;
	case LINK_STATE_DETACHED:
        if ((message_receiver_instance->message_receiver_state == MESSAGE_RECEIVER_STATE_OPEN) ||
            (message_receiver_instance->message_receiver_state == MESSAGE_RECEIVER_STATE_CLOSING))
        {
            /* User initiated transition, we should be good */
            set_message_receiver_state(message_receiver_instance, MESSAGE_RECEIVER_STATE_IDLE);
        }
        else if (message_receiver_instance->message_receiver_state != MESSAGE_RECEIVER_STATE_IDLE)
        {
            /* Any other transition must be an error */
            set_message_receiver_state(message_receiver_instance, MESSAGE_RECEIVER_STATE_ERROR);
        }
        break;
    case LINK_STATE_ERROR:
        if (message_receiver_instance->message_receiver_state != MESSAGE_RECEIVER_STATE_ERROR)
        {
            set_message_receiver_state(message_receiver_instance, MESSAGE_RECEIVER_STATE_ERROR);
        }
        break;
	}
}

MESSAGE_RECEIVER_HANDLE messagereceiver_create(LINK_HANDLE link, ON_MESSAGE_RECEIVER_STATE_CHANGED on_message_receiver_state_changed, void* context)
{
	MESSAGE_RECEIVER_INSTANCE* result = (MESSAGE_RECEIVER_INSTANCE*)amqpalloc_malloc(sizeof(MESSAGE_RECEIVER_INSTANCE));
	if (result != NULL)
	{
		result->link = link;
		result->on_message_receiver_state_changed = on_message_receiver_state_changed;
		result->on_message_receiver_state_changed_context = context;
		result->message_receiver_state = MESSAGE_RECEIVER_STATE_IDLE;
	}

	return result;
}

void messagereceiver_destroy(MESSAGE_RECEIVER_HANDLE message_receiver)
{
	if (message_receiver != NULL)
	{
		(void)messagereceiver_close(message_receiver);
		amqpalloc_free(message_receiver);
	}
}

int messagereceiver_open(MESSAGE_RECEIVER_HANDLE message_receiver, ON_MESSAGE_RECEIVED on_message_received, const void* callback_context)
{
	int result;

	if (message_receiver == NULL)
	{
		result = __LINE__;
	}
	else
	{
		MESSAGE_RECEIVER_INSTANCE* message_receiver_instance = (MESSAGE_RECEIVER_INSTANCE*)message_receiver;

		if (message_receiver_instance->message_receiver_state == MESSAGE_RECEIVER_STATE_IDLE)
		{
			set_message_receiver_state(message_receiver_instance, MESSAGE_RECEIVER_STATE_OPENING);
			if (link_attach(message_receiver_instance->link, on_transfer_received, on_link_state_changed, NULL, message_receiver_instance) != 0)
			{
				result = __LINE__;
				set_message_receiver_state(message_receiver_instance, MESSAGE_RECEIVER_STATE_ERROR);
			}
			else
			{
				message_receiver_instance->on_message_received = on_message_received;
				message_receiver_instance->callback_context = callback_context;

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

int messagereceiver_close(MESSAGE_RECEIVER_HANDLE message_receiver)
{
	int result;

	if (message_receiver == NULL)
	{
		result = __LINE__;
	}
	else
	{
		MESSAGE_RECEIVER_INSTANCE* message_receiver_instance = (MESSAGE_RECEIVER_INSTANCE*)message_receiver;

		if ((message_receiver_instance->message_receiver_state == MESSAGE_RECEIVER_STATE_OPENING) ||
			(message_receiver_instance->message_receiver_state == MESSAGE_RECEIVER_STATE_OPEN))
		{
			set_message_receiver_state(message_receiver_instance, MESSAGE_RECEIVER_STATE_CLOSING);

			if (link_detach(message_receiver_instance->link, true) != 0)
			{
				result = __LINE__;
				set_message_receiver_state(message_receiver_instance, MESSAGE_RECEIVER_STATE_ERROR);
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
