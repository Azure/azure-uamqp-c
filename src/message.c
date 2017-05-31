// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "azure_c_shared_utility/optimize_size.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/refcount.h"
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/amqpvalue.h"

typedef struct BODY_AMQP_DATA_TAG
{
	unsigned char* body_data_section_bytes;
	size_t body_data_section_length;
} BODY_AMQP_DATA;

typedef struct MESSAGE_INSTANCE_TAG
{
	BODY_AMQP_DATA* body_amqp_data_items;
	size_t body_amqp_data_count;
	AMQP_VALUE* body_amqp_sequence_items;
	size_t body_amqp_sequence_count;
	AMQP_VALUE body_amqp_value;
	HEADER_HANDLE header;
	annotations delivery_annotations;
	annotations message_annotations;
	PROPERTIES_HANDLE properties;
	application_properties application_properties;
	annotations footer;
    uint32_t message_format;
} MESSAGE_INSTANCE;

DEFINE_REFCOUNT_TYPE(MESSAGE_INSTANCE);

static void free_all_body_data_items(MESSAGE_HANDLE message)
{
	size_t i;

	for (i = 0; i < message->body_amqp_data_count; i++)
	{
		if (message->body_amqp_data_items[i].body_data_section_bytes != NULL)
		{
			free(message->body_amqp_data_items[i].body_data_section_bytes);
		}
	}

	free(message->body_amqp_data_items);
	message->body_amqp_data_count = 0;
	message->body_amqp_data_items = NULL;
}

static void free_all_body_sequence_items(MESSAGE_HANDLE message)
{
	size_t i;

	for (i = 0; i < message->body_amqp_sequence_count; i++)
	{
		if (message->body_amqp_sequence_items[i] != NULL)
		{
			amqpvalue_destroy(message->body_amqp_sequence_items[i]);
		}
	}

	free(message->body_amqp_sequence_items);
	message->body_amqp_sequence_count = 0;
	message->body_amqp_sequence_items = NULL;
}

MESSAGE_HANDLE message_create(void)
{
	MESSAGE_HANDLE result = (MESSAGE_HANDLE)REFCOUNT_TYPE_CREATE(MESSAGE_INSTANCE);
	if (result == NULL)
	{
		/* Codes_SRS_MESSAGE_01_002: [If allocating memory for the message fails, `message_create` shall fail and return NULL.] */
		LogError("Cannot allocate memory for message");
	}
	else
	{
		result->header = NULL;
		result->delivery_annotations = NULL;
		result->message_annotations = NULL;
		result->properties = NULL;
		result->application_properties = NULL;
		result->footer = NULL;
		result->body_amqp_data_items = NULL;
		result->body_amqp_data_count = 0;
		result->body_amqp_value = NULL;
		result->body_amqp_sequence_items = NULL;
		result->body_amqp_sequence_count = 0;
        result->message_format = 0;
	}

	/* Codes_SRS_MESSAGE_01_001: [`message_create` shall create a new AMQP message instance and on success it shall return a non-NULL handle for the newly created message instance.] */
	return result;
}

MESSAGE_HANDLE message_clone(MESSAGE_HANDLE source_message)
{
	MESSAGE_HANDLE result;

	/* Codes_SRS_MESSAGE_01_062: [If `source_message` is NULL, `message_clone` shall fail and return NULL.] */
	if (source_message == NULL)
	{
		LogError("NULL source_message");
		result = NULL;
	}
	else
	{
		/* Codes_SRS_MESSAGE_01_003: [`message_clone` shall clone a message entirely and on success return a non-NULL handle to the cloned message.] */
		INC_REF(MESSAGE_INSTANCE, source_message);
		result = source_message;
	}

	return result;
}

void message_destroy(MESSAGE_HANDLE message)
{
	if (message == NULL)
	{
		LogError("NULL message");
	}
	else
	{
		if (DEC_REF(MESSAGE_INSTANCE, message) == DEC_RETURN_ZERO)
		{
			if (message->header != NULL)
			{
				header_destroy(message->header);
			}
			if (message->properties != NULL)
			{
				properties_destroy(message->properties);
			}
			if (message->application_properties != NULL)
			{
				application_properties_destroy(message->application_properties);
			}
			if (message->footer != NULL)
			{
				annotations_destroy(message->footer);
			}
			if (message->body_amqp_value != NULL)
			{
				amqpvalue_destroy(message->body_amqp_value);
			}
			if (message->message_annotations != NULL)
			{
				application_properties_destroy(message->message_annotations);
			}

			free_all_body_data_items(message);
			free_all_body_sequence_items(message);
			free(message);
		}
	}
}

int message_set_header(MESSAGE_HANDLE message, HEADER_HANDLE header)
{
	int result;

	if ((message == NULL) ||
		(header == NULL))
	{
		LogError("Bad arguments: message = %p, header = %p",
			message, header);
		result = __FAILURE__;
	}
	else
	{
		HEADER_HANDLE new_header;

		new_header = header_clone(header);
		if (new_header == NULL)
		{
			LogError("Cannot clone message header");
			result = __FAILURE__;
		}
		else
		{
			if (message->header != NULL)
			{
				header_destroy(message->header);
			}

			message->header = new_header;
			result = 0;
		}
	}

	return result;
}

int message_get_header(MESSAGE_HANDLE message, HEADER_HANDLE* header)
{
	int result;

	if ((message == NULL) ||
		(header == NULL))
	{
		LogError("Bad arguments: message = %p, header = %p",
			message, header);
		result = __FAILURE__;
	}
	else
	{
		if (message->header == NULL)
		{
			*header = NULL;
			result = 0;
		}
		else
		{
			*header = header_clone(message->header);
			if (*header == NULL)
			{
				LogError("Cannot clone message header");
				result = __FAILURE__;
			}
			else
			{
				result = 0;
			}
		}
	}

	return result;
}

int message_set_delivery_annotations(MESSAGE_HANDLE message, annotations delivery_annotations)
{
	int result;

	if ((message == NULL) ||
		(delivery_annotations == NULL))
	{
		LogError("Bad arguments: message = %p, delivery_annotations = %p",
			message, delivery_annotations);
		result = __FAILURE__;
	}
	else
	{
		annotations new_delivery_annotations;

		new_delivery_annotations = annotations_clone(delivery_annotations);
		if (new_delivery_annotations == NULL)
		{
			LogError("Cannot clone delivery annotations");
			result = __FAILURE__;
		}
		else
		{
			if (message->delivery_annotations != NULL)
			{
				annotations_destroy(message->delivery_annotations);
			}

			message->delivery_annotations = new_delivery_annotations;
			result = 0;
		}
	}

	return result;
}

int message_get_delivery_annotations(MESSAGE_HANDLE message, annotations* delivery_annotations)
{
	int result;

	if ((message == NULL) ||
		(delivery_annotations == NULL))
	{
		LogError("Bad arguments: message = %p, delivery_annotations = %p",
			message, delivery_annotations);
		result = __FAILURE__;
	}
	else
	{
		if (message->delivery_annotations == NULL)
		{
			*delivery_annotations = NULL;
			result = 0;
		}
		else
		{
			*delivery_annotations = annotations_clone(message->delivery_annotations);
			if (*delivery_annotations == NULL)
			{
				LogError("Cannot clone delivery annotations");
				result = __FAILURE__;
			}
			else
			{
				result = 0;
			}
		}
	}

	return result;
}

int message_set_message_annotations(MESSAGE_HANDLE message, annotations message_annotations)
{
	int result;

	if ((message == NULL) ||
		(message_annotations == NULL))
	{
		LogError("Bad arguments: message = %p, message_annotations = %p",
			message, message_annotations);
		result = __FAILURE__;
	}
	else
	{
		annotations new_message_annotations;

		new_message_annotations = annotations_clone(message_annotations);
		if (new_message_annotations == NULL)
		{
			LogError("Cannot clone message annotations");
			result = __FAILURE__;
		}
		else
		{
			if (message->message_annotations != NULL)
			{
				annotations_destroy(message->message_annotations);
			}

			message->message_annotations = new_message_annotations;
			result = 0;
		}
	}

	return result;
}

int message_get_message_annotations(MESSAGE_HANDLE message, annotations* message_annotations)
{
	int result;

	if ((message == NULL) ||
		(message_annotations == NULL))
	{
		LogError("Bad arguments: message = %p, message_annotations = %p",
			message, message_annotations);
		result = __FAILURE__;
	}
	else
	{
		if (message->message_annotations == NULL)
		{
			*message_annotations = NULL;
			result = 0;
		}
		else
		{
			*message_annotations = annotations_clone(message->message_annotations);
			if (*message_annotations == NULL)
			{
				LogError("Cannot clone message annotations");
				result = __FAILURE__;
			}
			else
			{
				result = 0;
			}
		}
	}

	return result;
}

int message_set_properties(MESSAGE_HANDLE message, PROPERTIES_HANDLE properties)
{
	int result;

	if ((message == NULL) ||
		(properties == NULL))
	{
		LogError("Bad arguments: message = %p, properties = %p",
			message, properties);
		result = __FAILURE__;
	}
	else
	{
		PROPERTIES_HANDLE new_properties;

		new_properties = properties_clone(properties);
		if (new_properties == NULL)
		{
			LogError("Cannot clone message properties");
			result = __FAILURE__;
		}
		else
		{
			if (message->properties != NULL)
			{
				properties_destroy(message->properties);
			}

			message->properties = new_properties;
			result = 0;
		}
	}

	return result;
}

int message_get_properties(MESSAGE_HANDLE message, PROPERTIES_HANDLE* properties)
{
	int result;

	if ((message == NULL) ||
		(properties == NULL))
	{
		LogError("Bad arguments: message = %p, properties = %p",
			message, properties);
		result = __FAILURE__;
	}
	else
	{
		if (message->properties == NULL)
		{
			*properties = NULL;
			result = 0;
		}
		else
		{
			*properties = properties_clone(message->properties);
			if (*properties == NULL)
			{
				LogError("Cannot clone message properties");
				result = __FAILURE__;
			}
			else
			{
				result = 0;
			}
		}
	}

	return result;
}

int message_set_application_properties(MESSAGE_HANDLE message, AMQP_VALUE application_properties)
{
	int result;

	if ((message == NULL) ||
		(application_properties == NULL))
	{
		LogError("Bad arguments: message = %p, application_properties = %p",
			message, application_properties);
		result = __FAILURE__;
	}
	else
	{
		AMQP_VALUE new_application_properties;

		new_application_properties = application_properties_clone(application_properties);
		if (new_application_properties == NULL)
		{
			LogError("Cannot clone application properties");
			result = __FAILURE__;
		}
		else
		{
			if (message->application_properties != NULL)
			{
				amqpvalue_destroy(message->application_properties);
			}

			message->application_properties = new_application_properties;
			result = 0;
		}
	}

	return result;
}

int message_get_application_properties(MESSAGE_HANDLE message, AMQP_VALUE* application_properties)
{
	int result;

	if ((message == NULL) ||
		(application_properties == NULL))
	{
		LogError("Bad arguments: message = %p, application_properties = %p",
			message, application_properties);
		result = __FAILURE__;
	}
	else
	{
		if (message->application_properties == NULL)
		{
			*application_properties = NULL;
			result = 0;
		}
		else
		{
			*application_properties = application_properties_clone(message->application_properties);
			if (*application_properties == NULL)
			{
				LogError("Cannot clone application properties");
				result = __FAILURE__;
			}
			else
			{
				result = 0;
			}
		}
	}

	return result;
}

int message_set_footer(MESSAGE_HANDLE message, annotations footer)
{
	int result;

	if ((message == NULL) ||
		(footer == NULL))
	{
		LogError("Bad arguments: message = %p, footer = %p",
			message, footer);
		result = __FAILURE__;
	}
	else
	{
		AMQP_VALUE new_footer;

		new_footer = annotations_clone(footer);
		if (new_footer == NULL)
		{
			LogError("Cannot clone message footer");
			result = __FAILURE__;
		}
		else
		{
			if (message->footer != NULL)
			{
				annotations_destroy(message->footer);
			}

			message->footer = new_footer;
			result = 0;
		}
	}

	return result;
}

int message_get_footer(MESSAGE_HANDLE message, annotations* footer)
{
	int result;

	if ((message == NULL) ||
		(footer == NULL))
	{
		LogError("Bad arguments: message = %p, footer = %p",
			message, footer);
		result = __FAILURE__;
	}
	else
	{
		if (message->footer == NULL)
		{
			*footer = NULL;
			result = 0;
		}
		else
		{
			*footer = annotations_clone(message->footer);
			if (*footer == NULL)
			{
				LogError("Cannot clone message footer");
				result = __FAILURE__;
			}
			else
			{
				result = 0;
			}
		}
	}

	return result;
}

int message_add_body_amqp_data(MESSAGE_HANDLE message, BINARY_DATA amqp_data)
{
	int result;

	if ((message == NULL) ||
		((amqp_data.bytes == NULL) &&
		 (amqp_data.length != 0)))
	{
		LogError("Bad arguments: message = %p, bytes = %p, length = %u",
			message, amqp_data.bytes, (unsigned int)amqp_data.length);
		result = __FAILURE__;
	}
	else
	{
		BODY_AMQP_DATA* new_body_amqp_data_items = (BODY_AMQP_DATA*)realloc(message->body_amqp_data_items, sizeof(BODY_AMQP_DATA) * (message->body_amqp_data_count + 1));
		if (new_body_amqp_data_items == NULL)
		{
			LogError("Cannot allocate memory for body AMQP data items");
			result = __FAILURE__;
		}
		else
		{
			message->body_amqp_data_items = new_body_amqp_data_items;

			message->body_amqp_data_items[message->body_amqp_data_count].body_data_section_bytes = (unsigned char*)malloc(amqp_data.length);
			if (message->body_amqp_data_items[message->body_amqp_data_count].body_data_section_bytes == NULL)
			{
				LogError("Cannot allocate memory for body AMQP data to be added");
				result = __FAILURE__;
			}
			else
			{
				message->body_amqp_data_items[message->body_amqp_data_count].body_data_section_length = amqp_data.length;
				(void)memcpy(message->body_amqp_data_items[message->body_amqp_data_count].body_data_section_bytes, amqp_data.bytes, amqp_data.length);

				if (message->body_amqp_value != NULL)
				{
					amqpvalue_destroy(message->body_amqp_value);
					message->body_amqp_value = NULL;
				}
				free_all_body_sequence_items(message);

				message->body_amqp_data_count++;
				result = 0;
			}
		}
	}

	return result;
}

int message_get_body_amqp_data_in_place(MESSAGE_HANDLE message, size_t index, BINARY_DATA* binary_data)
{
	int result;

	if ((message == NULL) ||
		(binary_data == NULL))
	{
		LogError("Bad arguments: message = %p, binary_data = %p",
			message, binary_data);
		result = __FAILURE__;
	}
	else
	{
		if (index >= message->body_amqp_data_count)
		{
			LogError("Index too high for AMQP data (%u), number of AMQP data entries is %u",
				index, message->body_amqp_data_count);
			result = __FAILURE__;
		}
		else
		{
			binary_data->bytes = message->body_amqp_data_items[index].body_data_section_bytes;
			binary_data->length = message->body_amqp_data_items[index].body_data_section_length;

			result = 0;
		}
	}

	return result;
}

int message_get_body_amqp_data_count(MESSAGE_HANDLE message, size_t* count)
{
	int result;

	if ((message == NULL) ||
		(count == NULL))
	{
		LogError("Bad arguments: message = %p, count = %p",
			message, count);
		result = __FAILURE__;
	}
	else
	{
		*count = message->body_amqp_data_count;
		result = 0;
	}

	return result;
}

int message_add_body_amqp_sequence(MESSAGE_HANDLE message, AMQP_VALUE sequence_list)
{
	int result;
	size_t item_count;

	if ((message == NULL) ||
		(sequence_list == NULL))
	{
		LogError("Bad arguments: message = %p, sequence_list = %p",
			message, sequence_list);
		result = __FAILURE__;
	}
	else if (amqpvalue_get_list_item_count(sequence_list, (uint32_t*)&item_count) != 0)
	{
		LogError("Cannot retrieve message sequence list item count");
		result = __FAILURE__;
	}
	else
	{
		AMQP_VALUE* new_body_amqp_sequence_items = (AMQP_VALUE*)realloc(message->body_amqp_sequence_items, sizeof(AMQP_VALUE) * (message->body_amqp_sequence_count + 1));
		if (new_body_amqp_sequence_items == NULL)
		{
			LogError("Cannot allocate enough memory for sequence items");
			result = __FAILURE__;
		}
		else
		{
			message->body_amqp_sequence_items = new_body_amqp_sequence_items;

			message->body_amqp_sequence_items[message->body_amqp_sequence_count] = amqpvalue_clone(sequence_list);
			if (message->body_amqp_sequence_items[message->body_amqp_sequence_count] == NULL)
			{
				LogError("Cloning sequence failed");
				result = __FAILURE__;
			}
			else
			{
				if (message->body_amqp_value != NULL)
				{
					amqpvalue_destroy(message->body_amqp_value);
					message->body_amqp_value = NULL;
				}
				free_all_body_data_items(message);

				message->body_amqp_sequence_count++;
				result = 0;
			}
		}
	}

	return result;
}

int message_get_body_amqp_sequence(MESSAGE_HANDLE message, size_t index, AMQP_VALUE* sequence_list)
{
	int result;

	if ((message == NULL) ||
		(sequence_list == NULL))
	{
		LogError("Bad arguments: message = %p, sequence_list = %p",
			message, sequence_list);
		result = __FAILURE__;
	}
	else
	{
		if (index >= message->body_amqp_sequence_count)
		{
			LogError("Index too high for AMQP sequence (%u), maximum is %u",
				index, message->body_amqp_sequence_count);
			result = __FAILURE__;
		}
		else
		{
			*sequence_list = amqpvalue_clone(message->body_amqp_sequence_items[index]);
			if (*sequence_list == NULL)
			{
				LogError("Cannot clone AMQP sequence");
				result = __FAILURE__;
			}
			else
			{
				result = 0;
			}
		}
	}

	return result;
}

int message_get_body_amqp_sequence_count(MESSAGE_HANDLE message, size_t* count)
{
	int result;

	if ((message == NULL) ||
		(count == NULL))
	{
		LogError("Bad arguments: message = %p, count = %p",
			message, count);
		result = __FAILURE__;
	}
	else
	{
		*count = message->body_amqp_sequence_count;
		result = 0;
	}

	return result;
}

int message_set_body_amqp_value(MESSAGE_HANDLE message, AMQP_VALUE body_amqp_value)
{
	int result;

	if ((message == NULL) ||
		(body_amqp_value == NULL))
	{
		LogError("Bad arguments: message = %p, body_amqp_value = %p",
			message, body_amqp_value);
		result = __FAILURE__;
	}
	else
	{
		message->body_amqp_value = amqpvalue_clone(body_amqp_value);
		if (message->body_amqp_value == NULL)
		{
			LogError("Cannot clone body AMQP value",
				message, body_amqp_value);
			result = __FAILURE__;
		}
		else
		{
			free_all_body_data_items(message);
			free_all_body_sequence_items(message);
			result = 0;
		}
	}

	return result;
}

int message_get_body_amqp_value_in_place(MESSAGE_HANDLE message, AMQP_VALUE* body_amqp_value)
{
	int result;

	if ((message == NULL) ||
		(body_amqp_value == NULL))
	{
		LogError("Bad arguments: message = %p, body_amqp_value = %p",
			message, body_amqp_value);
		result = __FAILURE__;
	}
	else
	{
		*body_amqp_value = message->body_amqp_value;

		result = 0;
	}

	return result;
}

int message_get_body_type(MESSAGE_HANDLE message, MESSAGE_BODY_TYPE* body_type)
{
	int result;

	if ((message == NULL) ||
		(body_type == NULL))
	{
		LogError("Bad arguments: message = %p, body_type = %p",
			message, body_type);
		result = __FAILURE__;
	}
	else
	{
		if (message->body_amqp_value != NULL)
		{
			*body_type = MESSAGE_BODY_TYPE_VALUE;
		}
		else if (message->body_amqp_data_count > 0)
		{
			*body_type = MESSAGE_BODY_TYPE_DATA;
		}
		else if (message->body_amqp_sequence_count > 0)
		{
			*body_type = MESSAGE_BODY_TYPE_SEQUENCE;
		}
		else
		{
			*body_type = MESSAGE_BODY_TYPE_NONE;
		}

		result = 0;
	}

	return result;
}

int message_set_message_format(MESSAGE_HANDLE message, uint32_t message_format)
{
    int result;

    if (message == NULL)
    {
		LogError("NULL message");
		result = __FAILURE__;
    }
    else
    {
        message->message_format = message_format;
        result = 0;
    }

    return result;
}

int message_get_message_format(MESSAGE_HANDLE message, uint32_t *message_format)
{
    int result;

    if ((message == NULL) ||
        (message_format == NULL))
    {
		LogError("Bad arguments: message = %p, message_format = %p",
			message, message_format);
		result = __FAILURE__;
    }
    else
    {
        *message_format = message->message_format;
        result = 0;
    }

    return result;
}
