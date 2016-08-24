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

#ifdef USE_OPENSSL
#include "azure_c_shared_utility/tlsio_openssl.h"
#include <openssl\ssl.h>
#include <openssl\x509.h>
#include <openssl\err.h>
#include <openssl\engine.h>
#include <openssl\conf.h>
#endif

/* This sample connects to an Event Hub, authenticates using SASL PLAIN (key name/key) and then it received all messages for partition 0 */
/* Replace the below settings with your own.*/

#define EH_HOST "<<<Replace with your own EH host (like myeventhub.servicebus.windows.net)>>>"
#define EH_KEY_NAME "<<<Replace with your own key name>>>"
#define EH_KEY "<<<Replace with your own key>>>"

static AMQP_VALUE on_message_received(const void* context, MESSAGE_HANDLE message)
{
	(void)message;
	(void)context;

	printf("Message received.\r\n");
	message_count++;

	return messaging_delivery_accepted();
}

#ifdef USE_OPENSSL
static int verify_remote_certificate(X509_STORE_CTX *x509_store_ctx, void *arg)
{
	char name[4096];
	X509_NAME_oneline(X509_get_subject_name(x509_store_ctx->cert), name, XN_FLAG_ONELINE);

	char issuer[4096];
	X509_NAME_oneline(X509_get_issuer_name(x509_store_ctx->cert), issuer, XN_FLAG_ONELINE);

	TLSIO_CONFIG* pConfig = (TLSIO_CONFIG*)arg;

	int length = strlen(name);

	for (int ii = 0; ii < length; ii++)
	{
		if (strncmp("CN=", name + ii, 3) == 0)
		{
			int expected = strlen(pConfig->hostname);
			int actual = strlen(name + ii + 3);

			if (expected >= actual && strncmp(name + ii + 3, pConfig->hostname + expected - actual, strlen(pConfig->hostname) + expected - actual) == 0)
			{
				printf("\r\nAccepting Certificate with correct Domain Name: %s IssuedBy: %s\r\n", name, issuer);
				return 1;
			}
		}
	}

	printf("\r\nRejecting Certificate with incorrect Domain Name: %s IssuedBy: %s\r\n", name, issuer);
	return 0;
}
#endif

int main(int argc, char** argv)
{
	int result;
	(void)argc, argv;

#ifdef _CRTDBG_MAP_ALLOC
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	//_CrtSetBreakAlloc(96);
#endif

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
		XIO_HANDLE tls_io;

		/* create the TLS IO */
        TLSIO_CONFIG tls_io_config = { EH_HOST, 5671 };
		const IO_INTERFACE_DESCRIPTION* tlsio_interface = platform_get_default_tlsio();
		tls_io = xio_create(tlsio_interface, &tls_io_config);

#ifdef USE_OPENSSL
		/* set the TLS options */
		xio_setoption(tls_io, "tls_version", (const void*)10);
		xio_setoption(tls_io, "tls_validation_callback", verify_remote_certificate);
		xio_setoption(tls_io, "tls_validation_callback_data", &tls_io_config);
#endif

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
		AMQP_VALUE source = messaging_create_source(EH_NAME);
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
			result = -1;
		}
		else
		{
			message_count = 0;

			while (message_count < EH_MAX_MESSAGE_COUNT)
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

			result = 0;
		}

		messagereceiver_destroy(message_receiver);
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

	printf("Press a key and hit <enter> to exit\r\n");
	getc(stdin);

#ifdef _CRTDBG_MAP_ALLOC
	_CrtDumpMemoryLeaks();
#endif

	return result;
}
