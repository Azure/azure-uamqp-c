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
#include "azure_uamqp_c/message_receiver.h"
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/messaging.h"
#include "azure_uamqp_c/amqpalloc.h"
#include "azure_uamqp_c/saslclientio.h"
#include "azure_uamqp_c/sasl_plain.h"

/* This sample connects to an Event Hub, authenticates using SASL PLAIN (key name/key) and then it received all messages for partition 0 */
/* Replace the below settings with your own.*/

#define EH_HOST "<<<Replace with your own EH host (like myeventhub.servicebus.windows.net)>>>"
#define EH_KEY_NAME "<<<Replace with your own key name>>>"
#define EH_KEY "<<<Replace with your own key>>>"
#define EH_NAME "<<<Insert your event hub name here>>>"

static AMQP_VALUE on_message_received(const void* context, MESSAGE_HANDLE message)
{
	(void)message;
	(void)context;

	(void)printf("Message received.\r\n");

	return messaging_delivery_accepted();
}

int main(int argc, char** argv)
{
	int result;
    (void)argc, argv;

	XIO_HANDLE sasl_io = NULL;
	CONNECTION_HANDLE connection = NULL;
	SESSION_HANDLE session = NULL;
	LINK_HANDLE link = NULL;
	MESSAGE_RECEIVER_HANDLE message_receiver = NULL;

	amqpalloc_set_memory_tracing_enabled(true);

	if (platform_init() != 0)
	{
		result = -1;
	}
	else
	{
		size_t last_memory_used = 0;

		/* create SASL plain handler */
		SASL_PLAIN_CONFIG sasl_plain_config = { EH_KEY_NAME, EH_KEY, NULL };
		SASL_MECHANISM_HANDLE sasl_mechanism_handle = saslmechanism_create(saslplain_get_interface(), &sasl_plain_config);

		/* create the TLS IO */
        TLSIO_CONFIG tls_io_config = { EH_HOST, 5671 };
		const IO_INTERFACE_DESCRIPTION* tlsio_interface = platform_get_default_tlsio();
		XIO_HANDLE tls_io = xio_create(tlsio_interface, &tls_io_config);

		/* create the SASL client IO using the TLS IO */
		SASLCLIENTIO_CONFIG sasl_io_config;
        sasl_io_config.underlying_io = tls_io;
        sasl_io_config.sasl_mechanism = sasl_mechanism_handle;
		sasl_io = xio_create(saslclientio_get_interface_description(), &sasl_io_config);

		/* create the connection, session and link */
		connection = connection_create(sasl_io, EH_HOST, "whatever", NULL, NULL);
		session = session_create(connection, NULL, NULL);

		/* set incoming window to 100 for the session */
		session_set_incoming_window(session, 100);

		/* listen only on partition 0 */
		AMQP_VALUE source = messaging_create_source("amqps://" EH_HOST "/" EH_NAME "/ConsumerGroups/$Default/Partitions/0");
		AMQP_VALUE target = messaging_create_target("ingress-rx");
		link = link_create(session, "receiver-link", role_receiver, source, target);
		link_set_rcv_settle_mode(link, receiver_settle_mode_first);
		amqpvalue_destroy(source);
		amqpvalue_destroy(target);

		/* create a message receiver */
		message_receiver = messagereceiver_create(link, NULL, NULL);
		if ((message_receiver == NULL) ||
			(messagereceiver_open(message_receiver, on_message_received, message_receiver) != 0))
		{
			(void)printf("Cannot open the message receiver.");
			result = -1;
		}
		else
		{
			while (true)
			{
				size_t current_memory_used;
				size_t maximum_memory_used;
				connection_dowork(connection);

				current_memory_used = amqpalloc_get_current_memory_used();
				maximum_memory_used = amqpalloc_get_maximum_memory_used();

				if (current_memory_used != last_memory_used)
				{
					(void)printf("Current memory usage:%lu (max:%lu)\r\n", (unsigned long)current_memory_used, (unsigned long)maximum_memory_used);
					last_memory_used = current_memory_used;
				}
			}

			result = 0;
		}

		messagereceiver_destroy(message_receiver);
		link_destroy(link);
		session_destroy(session);
		connection_destroy(connection);
		platform_deinit();

		(void)printf("Max memory usage:%lu\r\n", (unsigned long)amqpalloc_get_maximum_memory_used());
		(void)printf("Current memory usage:%lu\r\n", (unsigned long)amqpalloc_get_current_memory_used());

#ifdef _CRTDBG_MAP_ALLOC
		_CrtDumpMemoryLeaks();
#endif
	}

	return result;
}
