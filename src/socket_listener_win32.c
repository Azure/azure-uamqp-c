// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "azure_uamqp_c/socket_listener.h"
#include "azure_uamqp_c/amqpalloc.h"
#include "winsock2.h"
#include "ws2tcpip.h"
#include "windows.h"
#include "azure_c_shared_utility/socketio.h"

typedef struct SOCKET_LISTENER_INSTANCE_TAG
{
	int port;
	SOCKET socket;
	ON_SOCKET_ACCEPTED on_socket_accepted;
	void* callback_context;
} SOCKET_LISTENER_INSTANCE;

SOCKET_LISTENER_HANDLE socketlistener_create(int port)
{
	SOCKET_LISTENER_INSTANCE* result = (SOCKET_LISTENER_INSTANCE*)amqpalloc_malloc(sizeof(SOCKET_LISTENER_INSTANCE));
	if (result != NULL)
	{
		result->port = port;
		result->on_socket_accepted = NULL;
		result->callback_context = NULL;
	}

	return (SOCKET_LISTENER_HANDLE)result;
}

void socketlistener_destroy(SOCKET_LISTENER_HANDLE socket_listener)
{
	if (socket_listener != NULL)
	{
		socketlistener_stop(socket_listener);
		amqpalloc_free(socket_listener);
	}
}

int socketlistener_start(SOCKET_LISTENER_HANDLE socket_listener, ON_SOCKET_ACCEPTED on_socket_accepted, void* callback_context)
{
	int result;

	if (socket_listener == NULL)
	{
		result = __LINE__;
	}
	else
	{
		SOCKET_LISTENER_INSTANCE* socket_listener_instance = (SOCKET_LISTENER_INSTANCE*)socket_listener;

		socket_listener_instance->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (socket_listener_instance->socket == INVALID_SOCKET)
		{
			result = __LINE__;
		}
		else
		{
			socket_listener_instance->on_socket_accepted = on_socket_accepted;
			socket_listener_instance->callback_context = callback_context;

			ADDRINFO* addrInfo = NULL;
			char portString[16];
			ADDRINFO addrHint = { 0 };

			addrHint.ai_family = AF_INET;
			addrHint.ai_socktype = SOCK_STREAM;
			addrHint.ai_protocol = 0;
			sprintf(portString, "%u", socket_listener_instance->port);
			if (getaddrinfo(NULL, portString, &addrHint, &addrInfo) != 0)
			{
				(void)closesocket(socket_listener_instance->socket);
				socket_listener_instance->socket = INVALID_SOCKET;
				result = __LINE__;
			}
			else
			{
				u_long iMode = 1;

				if (bind(socket_listener_instance->socket, addrInfo->ai_addr, (int)addrInfo->ai_addrlen) == SOCKET_ERROR)
				{
					(void)closesocket(socket_listener_instance->socket);
					socket_listener_instance->socket = INVALID_SOCKET;
					result = __LINE__;
				}
				else if (ioctlsocket(socket_listener_instance->socket, FIONBIO, &iMode) != 0)
				{
					(void)closesocket(socket_listener_instance->socket);
					socket_listener_instance->socket = INVALID_SOCKET;
					result = __LINE__;
				}
				else
				{
					if (listen(socket_listener_instance->socket, SOMAXCONN) == SOCKET_ERROR)
					{
						(void)closesocket(socket_listener_instance->socket);
						socket_listener_instance->socket = INVALID_SOCKET;
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

	return result;
}

int socketlistener_stop(SOCKET_LISTENER_HANDLE socket_listener)
{
	int result;

	if (socket_listener == NULL)
	{
		result = __LINE__;
	}
	else
	{
		SOCKET_LISTENER_INSTANCE* socket_listener_instance = (SOCKET_LISTENER_INSTANCE*)socket_listener;

		socket_listener_instance->on_socket_accepted = NULL;
		socket_listener_instance->callback_context = NULL;

		(void)closesocket(socket_listener_instance->socket);
		socket_listener_instance->socket = INVALID_SOCKET;

		result = 0;
	}

	return result;
}

void socketlistener_dowork(SOCKET_LISTENER_HANDLE socket_listener)
{
	if (socket_listener != NULL)
	{
		SOCKET_LISTENER_INSTANCE* socket_listener_instance = (SOCKET_LISTENER_INSTANCE*)socket_listener;
		SOCKET accepted_socket = accept(socket_listener_instance->socket, NULL, NULL);
		if (accepted_socket != INVALID_SOCKET)
		{
			if (socket_listener_instance->on_socket_accepted != NULL)
			{
				SOCKETIO_CONFIG socketio_config;
                socketio_config.hostname = NULL;
                socketio_config.port = socket_listener_instance->port;
                socketio_config.accepted_socket = &accepted_socket;
                XIO_HANDLE io = xio_create(socketio_get_interface_description(), &socketio_config);
				if (io == NULL)
				{
					(void)closesocket(accepted_socket);
				}
				else
				{
					socket_listener_instance->on_socket_accepted(socket_listener_instance->callback_context, io);
				}
			}
			else
			{
				(void)closesocket(accepted_socket);
			}
		}
	}
}
