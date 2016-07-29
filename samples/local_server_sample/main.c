// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stdio.h>
#include <stdbool.h>
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_uamqp_c/message_receiver.h"
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/messaging.h"
#include "azure_uamqp_c/amqpalloc.h"
#include "azure_uamqp_c/socket_listener.h"
#include "azure_uamqp_c/header_detect_io.h"
#include "azure_uamqp_c/connection.h"
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/link.h"

static unsigned int sent_messages = 0;
static const size_t msg_count = 1;
static CONNECTION_HANDLE connection;
static SESSION_HANDLE session;
static LINK_HANDLE link;
static MESSAGE_RECEIVER_HANDLE message_receiver;

static void on_message_receiver_state_changed(const void* context, MESSAGE_RECEIVER_STATE new_state, MESSAGE_RECEIVER_STATE previous_state)
{
    (void)context, new_state, previous_state;
}

static AMQP_VALUE on_message_received(const void* context, MESSAGE_HANDLE message)
{
	(void)message;
	(void)context;

	printf("Message received.\r\n");

	return messaging_delivery_accepted();
}

static bool on_new_link_attached(void* context, LINK_ENDPOINT_HANDLE new_link_endpoint, const char* name, role role, AMQP_VALUE source, AMQP_VALUE target)
{
    (void)context;
	link = link_create_from_endpoint(session, new_link_endpoint, name, role, source, target);
	link_set_rcv_settle_mode(link, receiver_settle_mode_first);
	message_receiver = messagereceiver_create(link, on_message_receiver_state_changed, NULL);
	messagereceiver_open(message_receiver, on_message_received, NULL);
	return true;
}

static bool on_new_session_endpoint(void* context, ENDPOINT_HANDLE new_endpoint)
{
    (void)context;
	session = session_create_from_endpoint(connection, new_endpoint, on_new_link_attached, NULL);
	session_set_incoming_window(session, 10000);
	session_begin(session);
	return true;
}

static void on_socket_accepted(void* context, XIO_HANDLE io)
{
	HEADERDETECTIO_CONFIG header_detect_io_config;
    (void)context;
    header_detect_io_config.underlying_io = io;
	XIO_HANDLE header_detect_io = xio_create(headerdetectio_get_interface_description(), &header_detect_io_config);
	connection = connection_create(header_detect_io, NULL, "1", on_new_session_endpoint, NULL);
	connection_listen(connection);
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
		size_t last_memory_used = 0;

		SOCKET_LISTENER_HANDLE socket_listener = socketlistener_create(5672);
		if (socketlistener_start(socket_listener, on_socket_accepted, NULL) != 0)
		{
			result = -1;
		}
		else
		{
			while (true)
			{
				size_t current_memory_used;
				size_t maximum_memory_used;
				socketlistener_dowork(socket_listener);

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

				if (connection != NULL)
				{
					connection_dowork(connection);
				}
			}

			result = 0;
		}

		socketlistener_destroy(socket_listener);
		platform_deinit();

		printf("Max memory usage:%lu\r\n", (unsigned long)amqpalloc_get_maximum_memory_used());
		printf("Current memory usage:%lu\r\n", (unsigned long)amqpalloc_get_current_memory_used());
	}

#ifdef _CRTDBG_MAP_ALLOC
	_CrtDumpMemoryLeaks();
#endif

	return result;
}
