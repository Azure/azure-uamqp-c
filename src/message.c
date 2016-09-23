// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <string.h>
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/amqpvalue.h"
#include "azure_uamqp_c/amqpalloc.h"

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

static void free_all_body_data_items(MESSAGE_INSTANCE* message_instance)
{
	size_t i;

	for (i = 0; i < message_instance->body_amqp_data_count; i++)
	{
		if (message_instance->body_amqp_data_items[i].body_data_section_bytes != NULL)
		{
			amqpalloc_free(message_instance->body_amqp_data_items[i].body_data_section_bytes);
		}
	}

	amqpalloc_free(message_instance->body_amqp_data_items);
	message_instance->body_amqp_data_count = 0;
	message_instance->body_amqp_data_items = NULL;
}

static void free_all_body_sequence_items(MESSAGE_INSTANCE* message_instance)
{
	size_t i;

	for (i = 0; i < message_instance->body_amqp_sequence_count; i++)
	{
		if (message_instance->body_amqp_sequence_items[i] != NULL)
		{
			amqpvalue_destroy(message_instance->body_amqp_sequence_items[i]);
		}
	}

	amqpalloc_free(message_instance->body_amqp_sequence_items);
	message_instance->body_amqp_sequence_count = 0;
	message_instance->body_amqp_sequence_items = NULL;
}

MESSAGE_HANDLE message_create(void)
{
	MESSAGE_INSTANCE* result = (MESSAGE_INSTANCE*)amqpalloc_malloc(sizeof(MESSAGE_INSTANCE));
	/* Codes_SRS_MESSAGE_01_002: [If allocating memory for the message fails, message_create shall fail and return NULL.] */
	if (result != NULL)
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

	/* Codes_SRS_MESSAGE_01_001: [message_create shall create a new AMQP message instance and on success it shall return a non-NULL handle for the newly created message instance.] */
	return result;
}

MESSAGE_HANDLE message_clone(MESSAGE_HANDLE source_message)
{
	MESSAGE_INSTANCE* result;

	/* Codes_SRS_MESSAGE_01_062: [If source_message is NULL, message_clone shall fail and return NULL.] */
	if (source_message == NULL)
	{
		result = NULL;
	}
	else
	{
		MESSAGE_INSTANCE* source_message_instance = (MESSAGE_INSTANCE*)source_message;
		result = (MESSAGE_INSTANCE*)message_create();

		/* Codes_SRS_MESSAGE_01_003: [message_clone shall clone a message entirely and on success return a non-NULL handle to the cloned message.] */
		/* Codes_SRS_MESSAGE_01_004: [If allocating memory for the new cloned message fails, message_clone shall fail and return NULL.] */
		if (result != NULL)
		{
            result->message_format = source_message_instance->message_format;

			if (source_message_instance->header != NULL)
			{
				/* Codes_SRS_MESSAGE_01_005: [If a header exists on the source message it shall be cloned by using header_clone.] */
				result->header = header_clone(source_message_instance->header);
				if (result->header == NULL)
				{
					message_destroy(result);
					result = NULL;
				}
			}

			if ((result != NULL) && (source_message_instance->delivery_annotations != NULL))
			{
				/* Codes_SRS_MESSAGE_01_006: [If delivery annotations exist on the source message they shall be cloned by using annotations_clone.] */
				result->delivery_annotations = annotations_clone(source_message_instance->delivery_annotations);
				if (result->delivery_annotations == NULL)
				{
					message_destroy(result);
					result = NULL;
				}
			}

			if ((result != NULL) && (source_message_instance->message_annotations != NULL))
			{
				/* Codes_SRS_MESSAGE_01_007: [If message annotations exist on the source message they shall be cloned by using annotations_clone.] */
				result->message_annotations = annotations_clone(source_message_instance->message_annotations);
				if (result->message_annotations == NULL)
				{
					message_destroy(result);
					result = NULL;
				}
			}

			if ((result != NULL) && (source_message_instance->properties != NULL))
			{
				/* Codes_SRS_MESSAGE_01_008: [If message properties exist on the source message they shall be cloned by using properties_clone.] */
				result->properties = properties_clone(source_message_instance->properties);
				if (result->properties == NULL)
				{
					message_destroy(result);
					result = NULL;
				}
			}

			if ((result != NULL) && (source_message_instance->application_properties != NULL))
			{
				/* Codes_SRS_MESSAGE_01_009: [If application properties exist on the source message they shall be cloned by using amqpvalue_clone.] */
				result->application_properties = amqpvalue_clone(source_message_instance->application_properties);
				if (result->application_properties == NULL)
				{
					message_destroy(result);
					result = NULL;
				}
			}

			if ((result != NULL) && (source_message_instance->footer != NULL))
			{
				/* Codes_SRS_MESSAGE_01_010: [If a footer exists on the source message it shall be cloned by using annotations_clone.] */
				result->footer = amqpvalue_clone(source_message_instance->footer);
				if (result->footer == NULL)
				{
					message_destroy(result);
					result = NULL;
				}
			}

			if ((result != NULL) && (source_message_instance->body_amqp_data_count > 0))
			{
				size_t i;

				result->body_amqp_data_items = (BODY_AMQP_DATA*)amqpalloc_malloc(source_message_instance->body_amqp_data_count * sizeof(BODY_AMQP_DATA));
				if (result->body_amqp_data_items == NULL)
				{
					message_destroy(result);
					result = NULL;
				}
				else
				{
					for (i = 0; i < source_message_instance->body_amqp_data_count; i++)
					{
						result->body_amqp_data_items[i].body_data_section_length = source_message_instance->body_amqp_data_items[i].body_data_section_length;

						/* Codes_SRS_MESSAGE_01_011: [If an AMQP data has been set as message body on the source message it shall be cloned by allocating memory for the binary payload.] */
						result->body_amqp_data_items[i].body_data_section_bytes = amqpalloc_malloc(source_message_instance->body_amqp_data_items[i].body_data_section_length);
						if (result->body_amqp_data_items[i].body_data_section_bytes == NULL)
						{
							break;
						}
						else
						{
							(void)memcpy(result->body_amqp_data_items[i].body_data_section_bytes, source_message_instance->body_amqp_data_items[i].body_data_section_bytes, result->body_amqp_data_items[i].body_data_section_length);
						}
					}

					result->body_amqp_data_count = i;
					if (i < source_message_instance->body_amqp_data_count)
					{
						message_destroy(result);
						result = NULL;
					}
				}
			}

			if ((result != NULL) && (source_message_instance->body_amqp_sequence_count > 0))
			{
				size_t i;

				result->body_amqp_sequence_items = (AMQP_VALUE*)amqpalloc_malloc(source_message_instance->body_amqp_sequence_count * sizeof(AMQP_VALUE));
				if (result->body_amqp_sequence_items == NULL)
				{
					message_destroy(result);
					result = NULL;
				}
				else
				{
					for (i = 0; i < source_message_instance->body_amqp_sequence_count; i++)
					{
						/* Codes_SRS_MESSAGE_01_011: [If an AMQP data has been set as message body on the source message it shall be cloned by allocating memory for the binary payload.] */
						result->body_amqp_sequence_items[i] = amqpvalue_clone(source_message_instance->body_amqp_sequence_items[i]);
						if (result->body_amqp_sequence_items[i] == NULL)
						{
							break;
						}
					}

					result->body_amqp_sequence_count = i;
					if (i < source_message_instance->body_amqp_sequence_count)
					{
						message_destroy(result);
						result = NULL;
					}
				}
			}

			if ((result != NULL) && (source_message_instance->body_amqp_value != NULL))
			{
				result->body_amqp_value = amqpvalue_clone(source_message_instance->body_amqp_value);
				if (result->body_amqp_value == NULL)
				{
					message_destroy(result);
					result = NULL;
				}
			}
		}
	}

	return result;
}

void message_destroy(MESSAGE_HANDLE message)
{
	if (message != NULL)
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;

		if (message_instance->header != NULL)
		{
			header_destroy(message_instance->header);
		}
		if (message_instance->properties != NULL)
		{
			properties_destroy(message_instance->properties);
		}
		if (message_instance->application_properties != NULL)
		{
			application_properties_destroy(message_instance->application_properties);
		}
		if (message_instance->footer != NULL)
		{
			annotations_destroy(message_instance->footer);
		}
		if (message_instance->body_amqp_value != NULL)
		{
			amqpvalue_destroy(message_instance->body_amqp_value);
		}
        if (message_instance->message_annotations != NULL)
        {
            application_properties_destroy(message_instance->message_annotations);
        }

		free_all_body_data_items(message_instance);
		free_all_body_sequence_items(message_instance);
		amqpalloc_free(message_instance);
	}
}

int message_set_header(MESSAGE_HANDLE message, HEADER_HANDLE header)
{
	int result;

	if ((message == NULL) ||
		(header == NULL))
	{
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
		HEADER_HANDLE new_header;

		new_header = header_clone(header);
		if (new_header == NULL)
		{
			result = __LINE__;
		}
		else
		{
			if (message_instance->header != NULL)
			{
				header_destroy(message_instance->header);
			}

			message_instance->header = new_header;
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
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;

		if (message_instance->header == NULL)
		{
			*header = NULL;
			result = 0;
		}
		else
		{
			*header = header_clone(message_instance->header);
			if (*header == NULL)
			{
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

int message_set_delivery_annotations(MESSAGE_HANDLE message, annotations delivery_annotations)
{
	int result;

	if ((message == NULL) ||
		(delivery_annotations == NULL))
	{
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
		annotations new_delivery_annotations;

		new_delivery_annotations = annotations_clone(delivery_annotations);
		if (new_delivery_annotations == NULL)
		{
			result = __LINE__;
		}
		else
		{
			if (message_instance->delivery_annotations != NULL)
			{
				annotations_destroy(message_instance->delivery_annotations);
			}
			message_instance->delivery_annotations = new_delivery_annotations;
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
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;

		if (message_instance->delivery_annotations == NULL)
		{
			*delivery_annotations = NULL;
			result = 0;
		}
		else
		{
			*delivery_annotations = annotations_clone(message_instance->delivery_annotations);
			if (*delivery_annotations == NULL)
			{
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

int message_set_message_annotations(MESSAGE_HANDLE message, annotations message_annotations)
{
	int result;

	if ((message == NULL) ||
		(message_annotations == NULL))
	{
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
		annotations new_message_annotations;

		new_message_annotations = annotations_clone(message_annotations);
		if (new_message_annotations == NULL)
		{
			result = __LINE__;
		}
		else
		{
			if (message_instance->message_annotations != NULL)
			{
				annotations_destroy(message_instance->message_annotations);
			}

			message_instance->message_annotations = new_message_annotations;
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
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;

		if (message_instance->message_annotations == NULL)
		{
			*message_annotations = NULL;
			result = 0;
		}
		else
		{
			*message_annotations = annotations_clone(message_instance->message_annotations);
			if (*message_annotations == NULL)
			{
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

int message_set_properties(MESSAGE_HANDLE message, PROPERTIES_HANDLE properties)
{
	int result;

	if ((message == NULL) ||
		(properties == NULL))
	{
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
		PROPERTIES_HANDLE new_properties;

		new_properties = properties_clone(properties);
		if (new_properties == NULL)
		{
			result = __LINE__;
		}
		else
		{
			if (message_instance->properties != NULL)
			{
				properties_destroy(message_instance->properties);
			}

			message_instance->properties = new_properties;
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
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;

		if (message_instance->properties == NULL)
		{
			*properties = NULL;
			result = 0;
		}
		else
		{
			*properties = properties_clone(message_instance->properties);
			if (*properties == NULL)
			{
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

int message_set_application_properties(MESSAGE_HANDLE message, AMQP_VALUE application_properties)
{
	int result;

	if ((message == NULL) ||
		(application_properties == NULL))
	{
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
		AMQP_VALUE new_application_properties;

		new_application_properties = application_properties_clone(application_properties);
		if (new_application_properties == NULL)
		{
			result = __LINE__;
		}
		else
		{
			if (message_instance->application_properties != NULL)
			{
				amqpvalue_destroy(message_instance->application_properties);
			}

			message_instance->application_properties = new_application_properties;
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
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;

		if (message_instance->application_properties == NULL)
		{
			*application_properties = NULL;
			result = 0;
		}
		else
		{
			*application_properties = application_properties_clone(message_instance->application_properties);
			if (*application_properties == NULL)
			{
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

int message_set_footer(MESSAGE_HANDLE message, annotations footer)
{
	int result;

	if ((message == NULL) ||
		(footer == NULL))
	{
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
		AMQP_VALUE new_footer;

		new_footer = annotations_clone(footer);
		if (new_footer == NULL)
		{
			result = __LINE__;
		}
		else
		{
			if (message_instance->footer != NULL)
			{
				annotations_destroy(message_instance->footer);
			}

			message_instance->footer = new_footer;
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
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;

		if (message_instance->footer == NULL)
		{
			*footer = NULL;
			result = 0;
		}
		else
		{
			*footer = annotations_clone(message_instance->footer);
			if (*footer == NULL)
			{
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

int message_add_body_amqp_data(MESSAGE_HANDLE message, BINARY_DATA binary_data)
{
	int result;

	MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
	if ((message == NULL) ||
		(binary_data.bytes == NULL) &&
		(binary_data.length != 0))
	{
		result = __LINE__;
	}
	else
	{
		BODY_AMQP_DATA* new_body_amqp_data_items = (BODY_AMQP_DATA*)amqpalloc_realloc(message_instance->body_amqp_data_items, sizeof(BODY_AMQP_DATA) * (message_instance->body_amqp_data_count + 1));
		if (new_body_amqp_data_items == NULL)
		{
			result = __LINE__;
		}
		else
		{
			message_instance->body_amqp_data_items = new_body_amqp_data_items;

			message_instance->body_amqp_data_items[message_instance->body_amqp_data_count].body_data_section_bytes = (unsigned char*)amqpalloc_malloc(binary_data.length);
			if (message_instance->body_amqp_data_items[message_instance->body_amqp_data_count].body_data_section_bytes == NULL)
			{
				result = __LINE__;
			}
			else
			{
				message_instance->body_amqp_data_items[message_instance->body_amqp_data_count].body_data_section_length = binary_data.length;
				(void)memcpy(message_instance->body_amqp_data_items[message_instance->body_amqp_data_count].body_data_section_bytes, binary_data.bytes, binary_data.length);

				if (message_instance->body_amqp_value != NULL)
				{
					amqpvalue_destroy(message_instance->body_amqp_value);
					message_instance->body_amqp_value = NULL;
				}
				free_all_body_sequence_items(message_instance);

				message_instance->body_amqp_data_count++;
				result = 0;
			}
		}
	}

	return result;
}

int message_get_body_amqp_data(MESSAGE_HANDLE message, size_t index, BINARY_DATA* binary_data)
{
	int result;

	if ((message == NULL) ||
		(binary_data == NULL))
	{
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;

		if (index >= message_instance->body_amqp_data_count)
		{
			result = __LINE__;
		}
		else
		{
			binary_data->bytes = message_instance->body_amqp_data_items[index].body_data_section_bytes;
			binary_data->length = message_instance->body_amqp_data_items[index].body_data_section_length;

			result = 0;
		}
	}

	return result;
}

int message_get_body_amqp_data_count(MESSAGE_HANDLE message, size_t* count)
{
	int result;

	MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
	if ((message == NULL) ||
		(count == NULL))
	{
		result = __LINE__;
	}
	else
	{
		*count = message_instance->body_amqp_data_count;
		result = 0;
	}

	return result;
}

int message_add_body_amqp_sequence(MESSAGE_HANDLE message, AMQP_VALUE sequence_list)
{
	int result;
	size_t item_count;

	MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
	if ((message == NULL) ||
		(sequence_list == NULL) ||
		(amqpvalue_get_list_item_count(sequence_list, (uint32_t*)&item_count) != 0))
	{
		result = __LINE__;
	}
	else
	{
		AMQP_VALUE* new_body_amqp_sequence_items = (AMQP_VALUE*)amqpalloc_realloc(message_instance->body_amqp_sequence_items, sizeof(AMQP_VALUE) * (message_instance->body_amqp_sequence_count + 1));
		if (new_body_amqp_sequence_items == NULL)
		{
			result = __LINE__;
		}
		else
		{
			message_instance->body_amqp_sequence_items = new_body_amqp_sequence_items;

			message_instance->body_amqp_sequence_items[message_instance->body_amqp_sequence_count] = amqpvalue_clone(sequence_list);
			if (message_instance->body_amqp_sequence_items[message_instance->body_amqp_sequence_count] == NULL)
			{
				result = __LINE__;
			}
			else
			{
				if (message_instance->body_amqp_value != NULL)
				{
					amqpvalue_destroy(message_instance->body_amqp_value);
					message_instance->body_amqp_value = NULL;
				}
				free_all_body_data_items(message_instance);

				message_instance->body_amqp_sequence_count++;
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
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;

		if (index >= message_instance->body_amqp_sequence_count)
		{
			result = __LINE__;
		}
		else
		{
			*sequence_list = amqpvalue_clone(message_instance->body_amqp_sequence_items[index]);
			if (*sequence_list == NULL)
			{
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

int message_get_body_amqp_sequence_count(MESSAGE_HANDLE message, size_t* count)
{
	int result;

	MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
	if ((message == NULL) ||
		(count == NULL))
	{
		result = __LINE__;
	}
	else
	{
		*count = message_instance->body_amqp_sequence_count;
		result = 0;
	}

	return result;
}

int message_set_body_amqp_value(MESSAGE_HANDLE message, AMQP_VALUE body_amqp_value)
{
	int result;

	MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
	if ((message == NULL) ||
		(body_amqp_value == NULL))
	{
		result = __LINE__;
	}
	else
	{
		message_instance->body_amqp_value = amqpvalue_clone(body_amqp_value);

		free_all_body_data_items(message_instance);
		free_all_body_sequence_items(message_instance);
		result = 0;
	}

	return result;
}

int message_get_inplace_body_amqp_value(MESSAGE_HANDLE message, AMQP_VALUE* body_amqp_value)
{
	int result;

	if ((message == NULL) ||
		(body_amqp_value == NULL))
	{
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;

		*body_amqp_value = message_instance->body_amqp_value;

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
		result = __LINE__;
	}
	else
	{
		MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;

		if (message_instance->body_amqp_value != NULL)
		{
			*body_type = MESSAGE_BODY_TYPE_VALUE;
		}
		else if (message_instance->body_amqp_data_count > 0)
		{
			*body_type = MESSAGE_BODY_TYPE_DATA;
		}
		else if (message_instance->body_amqp_sequence_count > 0)
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
        result = __LINE__;
    }
    else
    {
        MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
        message_instance->message_format = message_format;
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
        result = __LINE__;
    }
    else
    {
        MESSAGE_INSTANCE* message_instance = (MESSAGE_INSTANCE*)message;
        *message_format = message_instance->message_format;
        result = 0;
    }

    return result;
}
