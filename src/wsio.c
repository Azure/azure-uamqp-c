// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stddef.h>
#include <stdio.h>
#include "wsio.h"
#include "amqpalloc.h"
#include "logger.h"
#include "list.h"
#include "libwebsockets.h"

typedef struct PENDING_SOCKET_IO_TAG
{
	unsigned char* bytes;
	size_t size;
	ON_SEND_COMPLETE on_send_complete;
	void* callback_context;
	LIST_HANDLE pending_io_list;
} PENDING_SOCKET_IO;

typedef struct WSIO_INSTANCE_TAG
{
	ON_BYTES_RECEIVED on_bytes_received;
	ON_IO_STATE_CHANGED on_io_state_changed;
	LOGGER_LOG logger_log;
	void* callback_context;
	IO_STATE io_state;
	LIST_HANDLE pending_io_list;
	struct lws_context* ws_context;
	struct lws* wsi;
	int port;
	char* host;
	char* relative_path;
	char* trusted_ca;
	struct lws_protocols* protocols;
	bool use_ssl;
} WSIO_INSTANCE;

static int add_pending_io(WSIO_INSTANCE* ws_io_instance, const unsigned char* buffer, size_t size, ON_SEND_COMPLETE on_send_complete, void* callback_context)
{
	int result;
	PENDING_SOCKET_IO* pending_socket_io = (PENDING_SOCKET_IO*)amqpalloc_malloc(sizeof(PENDING_SOCKET_IO));
	if (pending_socket_io == NULL)
	{
		result = __LINE__;
	}
	else
	{
		pending_socket_io->bytes = (unsigned char*)amqpalloc_malloc(size);
		if (pending_socket_io->bytes == NULL)
		{
			amqpalloc_free(pending_socket_io);
			result = __LINE__;
		}
		else
		{
			pending_socket_io->size = size;
			pending_socket_io->on_send_complete = on_send_complete;
			pending_socket_io->callback_context = callback_context;
			pending_socket_io->pending_io_list = ws_io_instance->pending_io_list;
			(void)memcpy(pending_socket_io->bytes, buffer, size);

			if (list_add(ws_io_instance->pending_io_list, pending_socket_io) == NULL)
			{
				amqpalloc_free(pending_socket_io->bytes);
				amqpalloc_free(pending_socket_io);
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

static const IO_INTERFACE_DESCRIPTION ws_io_interface_description =
{
	wsio_create,
	wsio_destroy,
	wsio_open,
	wsio_close,
	wsio_send,
	wsio_dowork
};

static void set_io_state(WSIO_INSTANCE* wsio_instance, IO_STATE io_state)
{
	IO_STATE previous_state = wsio_instance->io_state;
	wsio_instance->io_state = io_state;
	if (wsio_instance->on_io_state_changed != NULL)
	{
		wsio_instance->on_io_state_changed(wsio_instance->callback_context, io_state, previous_state);
	}
}

static int ws_sb_cbs_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	WSIO_INSTANCE* wsio_instance = (WSIO_INSTANCE*)user;
	switch (reason)
	{
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		set_io_state(wsio_instance, IO_STATE_OPEN);
		break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		set_io_state(wsio_instance, IO_STATE_ERROR);
		break;

	case LWS_CALLBACK_CLOSED:
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
	{
		LIST_ITEM_HANDLE first_pending_io = list_get_head_item(wsio_instance->pending_io_list);
		if (first_pending_io != NULL)
		{
			PENDING_SOCKET_IO* pending_socket_io = (PENDING_SOCKET_IO*)list_item_get_value(first_pending_io);
			if (pending_socket_io == NULL)
			{
				set_io_state(wsio_instance, IO_STATE_ERROR);
			}
			else
			{
				unsigned char* ws_buffer = (unsigned char*)amqpalloc_malloc(LWS_SEND_BUFFER_PRE_PADDING + pending_socket_io->size + LWS_SEND_BUFFER_POST_PADDING);
				if (ws_buffer == NULL)
				{
					set_io_state(wsio_instance, IO_STATE_ERROR);
				}
				else
				{
					(void)memcpy(ws_buffer + LWS_SEND_BUFFER_PRE_PADDING, pending_socket_io->bytes, pending_socket_io->size);
					int n = lws_write(wsio_instance->wsi, &ws_buffer[LWS_SEND_BUFFER_PRE_PADDING], pending_socket_io->size, LWS_WRITE_BINARY);
					if (n < 0)
					{
						/* error */
						set_io_state(wsio_instance, IO_STATE_ERROR);
						pending_socket_io->on_send_complete(pending_socket_io->callback_context, IO_SEND_ERROR);
					}
					else
					{
						if ((size_t)n < pending_socket_io->size)
						{
							/* make pending */
							(void)memmove(pending_socket_io->bytes, pending_socket_io->bytes + n, (pending_socket_io->size - (size_t)n));
						}
						else
						{
							if (pending_socket_io->on_send_complete != NULL)
							{
								pending_socket_io->on_send_complete(pending_socket_io->callback_context, IO_SEND_OK);
							}

							amqpalloc_free(pending_socket_io->bytes);
							amqpalloc_free(pending_socket_io);
							if (list_remove(wsio_instance->pending_io_list, first_pending_io) != 0)
							{
								set_io_state(wsio_instance, IO_STATE_ERROR);
							}
						}
					}

					if (list_get_head_item(wsio_instance->pending_io_list) != NULL)
					{
						(void)lws_callback_on_writable(wsi);
					}

					amqpalloc_free(ws_buffer);
				}
			}
		}

		break;
	}

	case LWS_CALLBACK_CLIENT_RECEIVE:
	{
		wsio_instance->on_bytes_received(wsio_instance->callback_context, in, len);
		break;
	}

	case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
		break;

	case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
	{
		X509_STORE* cert_store = SSL_CTX_get_cert_store(user);
		BIO* cert_memory_bio;
		X509* certificate;
		
		cert_memory_bio = BIO_new(BIO_s_mem());
		if (cert_memory_bio == NULL)
		{
			/* error */
		}
		else
		{
			if (BIO_puts(cert_memory_bio, wsio_instance->trusted_ca) < (int)strlen(wsio_instance->trusted_ca))
			{
				/* error */
			}
			else
			{
				do
				{
					certificate = PEM_read_bio_X509(cert_memory_bio, NULL, 0, NULL);
					if (certificate != NULL)
					{
						if (!X509_STORE_add_cert(cert_store, certificate))
						{
							break;
						}
					}
				} while (certificate != NULL);

				if (certificate != NULL)
				{
					/* error */
				}
			}

			BIO_free(cert_memory_bio);
		}

		break;
	}

	default:
		break;
	}

	return 0;
}

CONCRETE_IO_HANDLE wsio_create(void* io_create_parameters, LOGGER_LOG logger_log)
{
	WSIO_CONFIG* ws_io_config = io_create_parameters;
	WSIO_INSTANCE* result;

	if ((ws_io_config == NULL) ||
		(ws_io_config->host == NULL) ||
		(ws_io_config->protocol_name == NULL) ||
		(ws_io_config->relative_path == NULL))
	{
		result = NULL;
	}
	else
	{
		result = amqpalloc_malloc(sizeof(WSIO_INSTANCE));
		if (result != NULL)
		{
			result->on_bytes_received = NULL;
			result->on_io_state_changed = NULL;
			result->logger_log = logger_log;
			result->callback_context = NULL;
			result->wsi = NULL;
			result->ws_context = NULL;

			result->pending_io_list = list_create();
			if (result->pending_io_list == NULL)
			{
				amqpalloc_free(result);
				result = NULL;
			}
			else
			{
				result->host = (char*)amqpalloc_malloc(strlen(ws_io_config->host) + 1);
				if (result->host == NULL)
				{
					list_destroy(result->pending_io_list);
					amqpalloc_free(result);
					result = NULL;
				}
				else
				{
					result->relative_path = (char*)amqpalloc_malloc(strlen(ws_io_config->relative_path) + 1);
					if (result->relative_path == NULL)
					{
						amqpalloc_free(result->host);
						list_destroy(result->pending_io_list);
						amqpalloc_free(result);
						result = NULL;
					}
					else
					{
						result->protocols = (struct lws_protocols*)amqpalloc_malloc(sizeof(struct lws_protocols) * 2);
						if (result->protocols == NULL)
						{
							amqpalloc_free(result->relative_path);
							amqpalloc_free(result->host);
							list_destroy(result->pending_io_list);
							amqpalloc_free(result);
							result = NULL;
						}
						else
						{
							result->trusted_ca = NULL;

							result->protocols[0].callback = ws_sb_cbs_callback;
							result->protocols[0].id = 0;
							result->protocols[0].name = ws_io_config->protocol_name;
                            //result->protocols[0].owning_server = NULL;
							result->protocols[0].per_session_data_size = 0;
                            //result->protocols[0].protocol_index = 0;
							result->protocols[0].rx_buffer_size = 1;
							result->protocols[0].user = result;

							result->protocols[1].callback = NULL;
							result->protocols[1].id = 0;
							result->protocols[1].name = NULL;
							//result->protocols[1].owning_server = NULL;
							result->protocols[1].per_session_data_size = 0;
                            //result->protocols[1].protocol_index = 0;
							result->protocols[1].rx_buffer_size = 0;
							result->protocols[1].user = NULL;

							(void)strcpy(result->host, ws_io_config->host);
							(void)strcpy(result->relative_path, ws_io_config->relative_path);
							result->port = ws_io_config->port;
							result->use_ssl = ws_io_config->use_ssl;

							set_io_state(result, IO_STATE_NOT_OPEN);

							if (ws_io_config->trusted_ca != NULL)
							{
								result->trusted_ca = (char*)amqpalloc_malloc(strlen(ws_io_config->trusted_ca) + 1);
								if (result->trusted_ca == NULL)
								{
									amqpalloc_free(result->protocols);
									amqpalloc_free(result->relative_path);
									amqpalloc_free(result->host);
									list_destroy(result->pending_io_list);
									amqpalloc_free(result);
									result = NULL;
								}
								else
								{
									(void)strcpy(result->trusted_ca, ws_io_config->trusted_ca);
								}
							}
						}
					}
				}
			}
		}
	}

	return result;
}

void wsio_destroy(CONCRETE_IO_HANDLE ws_io)
{
	if (ws_io != NULL)
	{
		WSIO_INSTANCE* wsio_instance = (WSIO_INSTANCE*)ws_io;

		/* clear all pending IOs */
		LIST_ITEM_HANDLE first_pending_io;
		while ((first_pending_io = list_get_head_item(wsio_instance->pending_io_list)) != NULL)
		{
			PENDING_SOCKET_IO* pending_socket_io = (PENDING_SOCKET_IO*)list_item_get_value(first_pending_io);
			if (pending_socket_io != NULL)
			{
				amqpalloc_free(pending_socket_io->bytes);
				amqpalloc_free(pending_socket_io);
			}

			list_remove(wsio_instance->pending_io_list, first_pending_io);
		}

		amqpalloc_free(wsio_instance->protocols);
		amqpalloc_free(wsio_instance->host);
		amqpalloc_free(wsio_instance->relative_path);
		amqpalloc_free(wsio_instance->trusted_ca);

		list_destroy(wsio_instance->pending_io_list);

		amqpalloc_free(ws_io);
	}
}

int wsio_open(CONCRETE_IO_HANDLE ws_io, ON_BYTES_RECEIVED on_bytes_received, ON_IO_STATE_CHANGED on_io_state_changed, void* callback_context)
{
	int result = 0;

	if (ws_io == NULL)
	{
		result = __LINE__;
	}
	else
	{
		WSIO_INSTANCE* wsio_instance = (WSIO_INSTANCE*)ws_io;

		wsio_instance->on_bytes_received = on_bytes_received;
		wsio_instance->on_io_state_changed = on_io_state_changed;
		wsio_instance->callback_context = callback_context;

		int ietf_version = -1; /* latest */
		struct lws_context_creation_info info;

		memset(&info, 0, sizeof info);

		info.port = CONTEXT_PORT_NO_LISTEN;
		info.protocols = wsio_instance->protocols;
		info.extensions = lws_get_internal_extensions();
		info.gid = -1;
		info.uid = -1;
		info.user = wsio_instance;

		wsio_instance->ws_context =lws_create_context(&info);
		if (wsio_instance->ws_context == NULL)
		{
			printf("Creating libwebsocket context failed\n");
			result = __LINE__;
		}
		else
		{
			wsio_instance->wsi = lws_client_connect(wsio_instance->ws_context, wsio_instance->host, wsio_instance->port, wsio_instance->use_ssl, wsio_instance->relative_path, wsio_instance->host, wsio_instance->host, wsio_instance->protocols[0].name, ietf_version);
			if (wsio_instance->wsi == NULL)
			{
				result = __LINE__;
			}
			else
			{
				set_io_state(wsio_instance, IO_STATE_OPENING);
				result = 0;
			}
		}
	}
	
	return result;
}

int wsio_close(CONCRETE_IO_HANDLE ws_io)
{
	int result = 0;

	if (ws_io == NULL)
	{
		result = __LINE__;
	}
	else
	{
		WSIO_INSTANCE* wsio_instance = (WSIO_INSTANCE*)ws_io;

		if (wsio_instance->io_state != IO_STATE_NOT_OPEN)
		{
			lws_context_destroy(wsio_instance->ws_context);

			set_io_state(wsio_instance, IO_STATE_NOT_OPEN);
		}

		result = 0;
	}

	return result;
}

int wsio_send(CONCRETE_IO_HANDLE ws_io, const void* buffer, size_t size, ON_SEND_COMPLETE on_send_complete, void* callback_context)
{
	int result;

	if ((ws_io == NULL) ||
		(buffer == NULL) ||
		(size == 0))
	{
		/* Invalid arguments */
		result = __LINE__;
	}
	else
	{
		WSIO_INSTANCE* wsio_instance = (WSIO_INSTANCE*)ws_io;

		if (wsio_instance->io_state != IO_STATE_OPEN)
		{
			result = __LINE__;
		}
		else
		{
			if (wsio_instance->logger_log != NULL)
			{
				size_t i;
				for (i = 0; i < size; i++)
				{
					LOG(wsio_instance->logger_log, 0, " %02x", ((const unsigned char*)buffer)[i]);
				}
			}

			if (add_pending_io(wsio_instance, buffer, size, on_send_complete, callback_context) != 0)
			{
				result = __LINE__;
			}
			else
			{
				(void)lws_callback_on_writable(wsio_instance->wsi);
				result = 0;
			}
		}
	}

	return result;
}

void wsio_dowork(CONCRETE_IO_HANDLE ws_io)
{
	if (ws_io != NULL)
	{
		WSIO_INSTANCE* wsio_instance = (WSIO_INSTANCE*)ws_io;

		if ((wsio_instance->io_state == IO_STATE_OPEN) ||
			(wsio_instance->io_state == IO_STATE_OPENING))
		{
			(void)lws_service(wsio_instance->ws_context, 0);
		}
	}
}

const IO_INTERFACE_DESCRIPTION* wsio_get_interface_description(void)
{
	return &ws_io_interface_description;
}
