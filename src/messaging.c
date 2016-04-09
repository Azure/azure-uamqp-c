// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stdbool.h>
#include "azure_uamqp_c/amqpvalue.h"
#include "azure_uamqp_c/amqp_definitions.h"

AMQP_VALUE messaging_create_source(const char* address)
{
	AMQP_VALUE result;
	SOURCE_HANDLE source = source_create();
	if (source == NULL)
	{
		result = NULL;
	}
	else
	{
		AMQP_VALUE address_value = amqpvalue_create_string(address);
		if (address_value == NULL)
		{
			result = NULL;
		}
		else
		{
			if (source_set_address(source, address_value) != 0)
			{
				result = NULL;
			}
			else
			{
				result = amqpvalue_create_source(source);
			}

			amqpvalue_destroy(address_value);
		}

		source_destroy(source);
	}

	return result;
}

AMQP_VALUE messaging_create_target(const char* address)
{
	AMQP_VALUE result;
	TARGET_HANDLE target = target_create();
	if (target == NULL)
	{
		result = NULL;
	}
	else
	{
		AMQP_VALUE address_value = amqpvalue_create_string(address);
		if (address_value == NULL)
		{
			result = NULL;
		}
		else
		{
			if (target_set_address(target, address_value) != 0)
			{
				result = NULL;
			}
			else
			{
				result = amqpvalue_create_target(target);
			}

			amqpvalue_destroy(address_value);
		}

		target_destroy(target);
	}

	return result;
}

AMQP_VALUE messaging_delivery_received(uint32_t section_number, uint64_t section_offset)
{
	AMQP_VALUE result;
	RECEIVED_HANDLE received = received_create(section_number, section_offset);
	if (received == NULL)
	{
		result = NULL;
	}
	else
	{
		result = amqpvalue_create_received(received);
		received_destroy(received);
	}

	return result;
}

AMQP_VALUE messaging_delivery_accepted(void)
{
	AMQP_VALUE result;
	ACCEPTED_HANDLE accepted = accepted_create();
	if (accepted == NULL)
	{
		result = NULL;
	}
	else
	{
		result = amqpvalue_create_accepted(accepted);
		accepted_destroy(accepted);
	}

	return result;
}

AMQP_VALUE messaging_delivery_rejected(const char* error_condition, const char* error_description)
{
	AMQP_VALUE result;
	REJECTED_HANDLE rejected = rejected_create();
	if (rejected == NULL)
	{
		result = NULL;
	}
	else
	{
		ERROR_HANDLE error_handle = NULL;
		bool error_constructing = false;

		if (error_condition != NULL)
		{
			error_handle = error_create(error_condition);
			if (error_handle == NULL)
			{
				error_constructing = true;
			}
			else
			{
				if ((error_description != NULL) &&
					(error_set_description(error_handle, error_description) != 0))
				{
					error_constructing = true;
				}
				else
				{
					if (rejected_set_error(rejected, error_handle) != 0)
					{
						error_constructing = true;
					}
				}

				error_destroy(error_handle);
			}
		}

		if (error_constructing)
		{
			result = NULL;
		}
		else
		{
			result = amqpvalue_create_rejected(rejected);
		}

		rejected_destroy(rejected);
	}

	return result;
}

AMQP_VALUE messaging_delivery_released(void)
{
	AMQP_VALUE result;
	RELEASED_HANDLE released = released_create();
	if (released == NULL)
	{
		result = NULL;
	}
	else
	{
		result = amqpvalue_create_released(released);
		released_destroy(released);
	}

	return result;
}

AMQP_VALUE messaging_delivery_modified(bool delivery_failed, bool undeliverable_here, fields message_annotations)
{
	AMQP_VALUE result;
	MODIFIED_HANDLE modified = modified_create();
	if (modified == NULL)
	{
		result = NULL;
	}
	else
	{
		if ((modified_set_delivery_failed(modified, delivery_failed) != 0) ||
			(modified_set_undeliverable_here(modified, undeliverable_here) != 0) ||
			((message_annotations != NULL) && (modified_set_message_annotations(modified, message_annotations) != 0)))
		{
			result = NULL;
		}
		else
		{
			result = amqpvalue_create_modified(modified);
		}

		modified_destroy(modified);
	}

	return result;
}
