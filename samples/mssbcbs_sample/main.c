// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stdio.h>
#include <stdbool.h>
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/tlsio.h"
#include "azure_uamqp_c/message_sender.h"
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/messaging.h"
#include "azure_uamqp_c/amqpalloc.h"
#include "azure_uamqp_c/saslclientio.h"
#include "azure_uamqp_c/sasl_mssbcbs.h"
#include "azure_uamqp_c/cbs.h"

/* This sample connects to an IoTHub, authenticates using a CBS token and sends one message */
/* Replace the below settings with your own.*/

#define IOT_HUB_HOST "<<<Replace with your own IoTHub host (like myiothub.azure-devices.net)>>>"
#define IOT_HUB_DEVICE_NAME "<<<Replace with your device Id (like test_Device)>>>"
#define IOT_HUB_DEVICE_SAS_TOKEN "<<<Replace with your own device SAS token (needs to be generated)>>>"

static unsigned int sent_messages = 0;
static const size_t msg_count = 1;
static bool auth = false;

static void on_amqp_management_state_chaged(void* context, AMQP_MANAGEMENT_STATE new_amqp_management_state, AMQP_MANAGEMENT_STATE previous_amqp_management_state)
{
	(void)context;
	(void)previous_amqp_management_state;

	if (new_amqp_management_state == AMQP_MANAGEMENT_STATE_IDLE)
	{
		printf("Disconnected.\r\n");
	}
}

static void on_message_send_complete(void* context, MESSAGE_SEND_RESULT send_result)
{
	(void)send_result;
	(void)context;

	printf("Sent.\r\n");
	sent_messages++;
}

static void on_cbs_operation_complete(void* context, CBS_OPERATION_RESULT cbs_operation_result, unsigned int status_code, const char* status_description)
{
	(void)context, status_code, status_description;

	if (cbs_operation_result == CBS_OPERATION_RESULT_OK)
	{
		auth = true;
	}
}

int main(int argc, char** argv)
{
	int result;

    (void)argc, argv;
	amqpalloc_set_memory_tracing_enabled(true);

	if (platform_init() != 0)
	{
		result = -1;
	}
	else
	{
		XIO_HANDLE sasl_io;
		CONNECTION_HANDLE connection;
		SESSION_HANDLE session;
		LINK_HANDLE link;
		MESSAGE_SENDER_HANDLE message_sender;
		MESSAGE_HANDLE message;

		size_t last_memory_used = 0;

		/* create SASL MSSBCBS handler */
		SASL_MECHANISM_HANDLE sasl_mechanism_handle = saslmechanism_create(saslmssbcbs_get_interface(), NULL);
		XIO_HANDLE tls_io;

		/* create the TLS IO */
        TLSIO_CONFIG tls_io_config = { IOT_HUB_HOST, 5671 };
		const IO_INTERFACE_DESCRIPTION* tlsio_interface = platform_get_default_tlsio();
		tls_io = xio_create(tlsio_interface, &tls_io_config);

		/* create the SASL client IO using the TLS IO */
		SASLCLIENTIO_CONFIG sasl_io_config;
        sasl_io_config.underlying_io = tls_io;
        sasl_io_config.sasl_mechanism = sasl_mechanism_handle;
		sasl_io = xio_create(saslclientio_get_interface_description(), &sasl_io_config);

		/* create the connection, session and link */
		connection = connection_create(sasl_io, IOT_HUB_HOST, "some", NULL, NULL);
		session = session_create(connection, NULL, NULL);
		session_set_incoming_window(session, 2147483647);
		session_set_outgoing_window(session, 2);

		CBS_HANDLE cbs = cbs_create(session, NULL, NULL);
		if (cbs_open(cbs) == 0)
		{
			(void)cbs_put_token(cbs, "servicebus.windows.net:sastoken", IOT_HUB_HOST "/devices/" IOT_HUB_DEVICE_NAME, IOT_HUB_DEVICE_SAS_TOKEN, on_cbs_operation_complete, cbs);

			while (!auth)
			{
				size_t current_memory_used;
				size_t maximum_memory_used;
				connection_dowork(connection);

				current_memory_used = amqpalloc_get_current_memory_used();
				maximum_memory_used = amqpalloc_get_maximum_memory_used();

				if (current_memory_used != last_memory_used)
				{
					printf("Current memory usage:%lu (max:%lu)\r\n", (unsigned long)current_memory_used, (unsigned long)maximum_memory_used);
					last_memory_used = current_memory_used;
				}
			}
		}

		AMQP_VALUE source = messaging_create_source("ingress");
		AMQP_VALUE target = messaging_create_target("amqps://" IOT_HUB_HOST "/devices/" IOT_HUB_DEVICE_NAME "/messages/events");
		link = link_create(session, "sender-link", role_sender, source, target);
		(void)link_set_max_message_size(link, 65536);

		amqpvalue_destroy(source);
		amqpvalue_destroy(target);

		message = message_create();
		unsigned char hello[] = { 'H', 'e', 'l', 'l', 'o' };
		BINARY_DATA binary_data;
        binary_data.bytes = hello;
        binary_data.length = sizeof(hello);
		message_add_body_amqp_data(message, binary_data);

        fields attach_properties = amqpvalue_create_map();
        AMQP_VALUE test_attach_property_key = amqpvalue_create_string("test_attach_property_key");
        AMQP_VALUE test_attach_property_value = amqpvalue_create_string("a_test_property");
        (void)amqpvalue_set_map_value(attach_properties, test_attach_property_key, test_attach_property_value);

        link_set_attach_properties(link, attach_properties);

        amqpvalue_destroy(test_attach_property_key);
        amqpvalue_destroy(test_attach_property_value);
        amqpvalue_destroy(attach_properties);

		/* create a message sender */
		message_sender = messagesender_create(link, NULL, NULL);
		if (messagesender_open(message_sender) == 0)
		{
			uint32_t i;

			for (i = 0; i < msg_count; i++)
			{
				(void)messagesender_send(message_sender, message, on_message_send_complete, message);
			}

			message_destroy(message);

			while (true)
			{
				size_t current_memory_used;
				size_t maximum_memory_used;
				connection_dowork(connection);

				current_memory_used = amqpalloc_get_current_memory_used();
				maximum_memory_used = amqpalloc_get_maximum_memory_used();

				if (current_memory_used != last_memory_used)
				{
					printf("Current memory usage:%lu (max:%lu)\r\n", (unsigned long)current_memory_used, (unsigned long)maximum_memory_used);
					last_memory_used = current_memory_used;
				}

				if (sent_messages == msg_count)
				{
					break;
				}
			}
		}

		cbs_destroy(cbs);
		messagesender_destroy(message_sender);
		link_destroy(link);
		session_destroy(session);
		connection_destroy(connection);
		xio_destroy(sasl_io);
		xio_destroy(tls_io);
		saslmechanism_destroy(sasl_mechanism_handle);
		platform_deinit();

		printf("Max memory usage:%lu\r\n", (unsigned long)amqpalloc_get_maximum_memory_used());
		printf("Current memory usage:%lu\r\n", (unsigned long)amqpalloc_get_current_memory_used());
	}

#ifdef _CRTDBG_MAP_ALLOC
	_CrtDumpMemoryLeaks();
#endif

	return 0;
}
