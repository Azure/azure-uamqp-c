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
#include "openssl/ssl.h"

typedef enum IO_STATE_TAG
{
	IO_STATE_NOT_OPEN,
	IO_STATE_OPENING,
	IO_STATE_OPEN,
	IO_STATE_CLOSING,
	IO_STATE_ERROR
} IO_STATE;

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
	ON_IO_OPEN_COMPLETE on_io_open_complete;
	ON_IO_ERROR on_io_error;
	LOGGER_LOG logger_log;
	void* open_callback_context;
	IO_STATE io_state;
	LIST_HANDLE pending_io_list;
	struct libwebsocket_context* ws_context;
	struct libwebsocket* wsi;
	int port;
	char* host;
	char* relative_path;
    char* protocol_name;
	char* trusted_ca;
	struct libwebsocket_protocols* protocols;
	bool use_ssl;
} WSIO_INSTANCE;

static void indicate_error(WSIO_INSTANCE* ws_io_instance)
{
	if (ws_io_instance->on_io_error != NULL)
	{
		ws_io_instance->on_io_error(ws_io_instance->open_callback_context);
	}
}

static void indicate_open_complete(WSIO_INSTANCE* ws_io_instance, IO_OPEN_RESULT open_result)
{
	if (ws_io_instance->on_io_open_complete != NULL)
	{
		ws_io_instance->on_io_open_complete(ws_io_instance->open_callback_context, open_result);
	}
}

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

static int ws_sb_cbs_callback(struct libwebsocket_context *this, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len)
{
	WSIO_INSTANCE* wsio_instance = libwebsocket_context_user(this);
	switch (reason)
	{
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
        if (wsio_instance->io_state == IO_STATE_OPENING)
        {
            /* Codes_SRS_WSIO_01_036: [The callback on_io_open_complete shall be called with io_open_result being set to IO_OPEN_OK when the open action is succesfull.] */
            wsio_instance->io_state = IO_STATE_OPEN;
            indicate_open_complete(wsio_instance, IO_OPEN_OK);
        }

		break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        if (wsio_instance->io_state == IO_STATE_OPENING)
        {
            /* Codes_SRS_WSIO_01_037: [If any error occurs while the open action is in progress, the callback on_io_open_complete shall be called with io_open_result being set to IO_OPEN_ERROR.] */
            indicate_open_complete(wsio_instance, IO_OPEN_ERROR);
            libwebsocket_context_destroy(wsio_instance->ws_context);
            wsio_instance->io_state = IO_STATE_NOT_OPEN;
        }

		break;

	case LWS_CALLBACK_CLOSED:
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
	{
		WSIO_INSTANCE* wsio_instance = libwebsocket_context_user(this);
		LIST_ITEM_HANDLE first_pending_io = list_get_head_item(wsio_instance->pending_io_list);
		if (first_pending_io != NULL)
		{
			PENDING_SOCKET_IO* pending_socket_io = (PENDING_SOCKET_IO*)list_item_get_value(first_pending_io);
			if (pending_socket_io == NULL)
			{
				wsio_instance->io_state = IO_STATE_ERROR;
				indicate_error(wsio_instance);
			}
			else
			{
				unsigned char* ws_buffer = (unsigned char*)amqpalloc_malloc(LWS_SEND_BUFFER_PRE_PADDING + pending_socket_io->size + LWS_SEND_BUFFER_POST_PADDING);
				if (ws_buffer == NULL)
				{
					wsio_instance->io_state = IO_STATE_ERROR;
					indicate_error(wsio_instance);
				}
				else
				{
					(void)memcpy(ws_buffer + LWS_SEND_BUFFER_PRE_PADDING, pending_socket_io->bytes, pending_socket_io->size);
					int n = libwebsocket_write(wsio_instance->wsi, &ws_buffer[LWS_SEND_BUFFER_PRE_PADDING], pending_socket_io->size, LWS_WRITE_BINARY);
					if (n < 0)
					{
						/* error */
						wsio_instance->io_state = IO_STATE_ERROR;
						indicate_error(wsio_instance);
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
								wsio_instance->io_state = IO_STATE_ERROR;
								indicate_error(wsio_instance);
							}
						}
					}

					if (list_get_head_item(wsio_instance->pending_io_list) != NULL)
					{
						(void)libwebsocket_callback_on_writable(this, wsi);
					}

					amqpalloc_free(ws_buffer);
				}
			}
		}

		break;
	}

	case LWS_CALLBACK_CLIENT_RECEIVE:
	{
		WSIO_INSTANCE* wsio_instance = libwebsocket_context_user(this);
		wsio_instance->on_bytes_received(wsio_instance->open_callback_context, in, len);
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
    /* Codes_SRS_WSIO_01_003: [io_create_parameters shall be used as a WSIO_CONFIG*.] */
    WSIO_CONFIG* ws_io_config = io_create_parameters;
	WSIO_INSTANCE* result;

	if ((ws_io_config == NULL) ||
        /* Codes_SRS_WSIO_01_004: [If any of the WSIO_CONFIG fields host, protocol_name or relative_path is NULL then wsio_create shall return NULL.] */
        (ws_io_config->host == NULL) ||
		(ws_io_config->protocol_name == NULL) ||
		(ws_io_config->relative_path == NULL))
	{
		result = NULL;
	}
	else
	{
        /* Codes_SRS_WSIO_01_001: [wsio_create shall create an instance of a wsio and return a non-NULL handle to it.] */
		result = amqpalloc_malloc(sizeof(WSIO_INSTANCE));
		if (result != NULL)
		{
			result->on_bytes_received = NULL;
			result->on_io_open_complete = NULL;
			result->on_io_error = NULL;
			result->logger_log = logger_log;
			result->open_callback_context = NULL;
			result->wsi = NULL;
			result->ws_context = NULL;

            /* Codes_SRS_WSIO_01_098: [wsio_create shall create a pending IO list that is to be used when sending buffers over the libwebsockets IO by calling list_create.] */
            result->pending_io_list = list_create();
			if (result->pending_io_list == NULL)
			{
                /* Codes_SRS_WSIO_01_099: [If list_create fails then wsio_create shall fail and return NULL.] */
				amqpalloc_free(result);
				result = NULL;
			}
			else
			{
                /* Codes_SRS_WSIO_01_006: [The members host, protocol_name, relative_path and trusted_ca shall be copied for later use (they are needed when the IO is opened).] */
                result->host = (char*)amqpalloc_malloc(strlen(ws_io_config->host) + 1);
				if (result->host == NULL)
				{
                    /* Codes_SRS_WSIO_01_005: [If allocating memory for the new wsio instance fails then wsio_create shall return NULL.] */
                    list_destroy(result->pending_io_list);
					amqpalloc_free(result);
					result = NULL;
				}
				else
				{
					result->relative_path = (char*)amqpalloc_malloc(strlen(ws_io_config->relative_path) + 1);
					if (result->relative_path == NULL)
					{
                        /* Codes_SRS_WSIO_01_005: [If allocating memory for the new wsio instance fails then wsio_create shall return NULL.] */
                        amqpalloc_free(result->host);
						list_destroy(result->pending_io_list);
						amqpalloc_free(result);
						result = NULL;
					}
					else
					{
                        result->protocol_name = (char*)amqpalloc_malloc(strlen(ws_io_config->protocol_name) + 1);
                        if (result->protocol_name == NULL)
                        {
                            /* Codes_SRS_WSIO_01_005: [If allocating memory for the new wsio instance fails then wsio_create shall return NULL.] */
                            amqpalloc_free(result->relative_path);
                            amqpalloc_free(result->host);
                            list_destroy(result->pending_io_list);
                            amqpalloc_free(result);
                            result = NULL;
                        }
                        else
                        {
                            (void)strcpy(result->protocol_name, ws_io_config->protocol_name);

                            result->protocols = (struct libwebsocket_protocols*)amqpalloc_malloc(sizeof(struct libwebsocket_protocols) * 2);
                            if (result->protocols == NULL)
                            {
                                /* Codes_SRS_WSIO_01_005: [If allocating memory for the new wsio instance fails then wsio_create shall return NULL.] */
                                amqpalloc_free(result->relative_path);
                                amqpalloc_free(result->protocol_name);
                                amqpalloc_free(result->host);
                                list_destroy(result->pending_io_list);
                                amqpalloc_free(result);
                                result = NULL;
                            }
                            else
                            {
                                result->trusted_ca = NULL;

                                /* Codes_SRS_WSIO_01_012: [The protocols member shall be populated with 2 protocol entries, one containing the actual protocol to be used and one empty (fields shall be NULL or 0).] */
                                /* Codes_SRS_WSIO_01_013: [callback shall be set to a callback used by the wsio module to listen to libwebsockets events.] */
                                result->protocols[0].callback = ws_sb_cbs_callback;
                                /* Codes_SRS_WSIO_01_014: [id shall be set to 0] */
                                result->protocols[0].id = 0;
                                /* Codes_SRS_WSIO_01_015: [name shall be set to protocol_name as passed to wsio_create] */
                                result->protocols[0].name = result->protocol_name;
                                /* Codes_SRS_WSIO_01_020: [owning_server shall be set to NULL] */
                                result->protocols[0].owning_server = NULL;
                                /* Codes_SRS_WSIO_01_016: [per_session_data_size shall be set to 0] */
                                result->protocols[0].per_session_data_size = 0;
                                /* Codes_SRS_WSIO_01_021: [protocol_index shall be set to 0] */
                                result->protocols[0].protocol_index = 0;
                                /* Codes_SRS_WSIO_01_017: [rx_buffer_size shall be set to 0, as there is no need for atomic frames] */
                                result->protocols[0].rx_buffer_size = 0;
                                /* Codes_SRS_WSIO_01_019: [user shall be set to NULL] */
                                result->protocols[0].user = NULL;

                                result->protocols[1].callback = NULL;
                                result->protocols[1].id = 0;
                                result->protocols[1].name = NULL;
                                result->protocols[1].owning_server = NULL;
                                result->protocols[1].per_session_data_size = 0;
                                result->protocols[1].protocol_index = 0;
                                result->protocols[1].rx_buffer_size = 0;
                                result->protocols[1].user = NULL;

                                (void)strcpy(result->host, ws_io_config->host);
                                (void)strcpy(result->relative_path, ws_io_config->relative_path);
                                result->port = ws_io_config->port;
                                result->use_ssl = ws_io_config->use_ssl;
                                result->io_state = IO_STATE_NOT_OPEN;

                                /* Codes_SRS_WSIO_01_100: [The trusted_ca member shall be optional (it can be NULL).] */
                                if (ws_io_config->trusted_ca != NULL)
                                {
                                    result->trusted_ca = (char*)amqpalloc_malloc(strlen(ws_io_config->trusted_ca) + 1);
                                    if (result->trusted_ca == NULL)
                                    {
                                        /* Codes_SRS_WSIO_01_005: [If allocating memory for the new wsio instance fails then wsio_create shall return NULL.] */
                                        amqpalloc_free(result->protocols);
                                        amqpalloc_free(result->protocol_name);
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
	}

	return result;
}

void wsio_destroy(CONCRETE_IO_HANDLE ws_io)
{
    /* Codes_SRS_WSIO_01_008: [If ws_io is NULL, wsio_destroy shall do nothing.] */
    if (ws_io != NULL)
	{
		WSIO_INSTANCE* wsio_instance = (WSIO_INSTANCE*)ws_io;

        /* Codes_SRS_WSIO_01_007: [wsio_destroy shall free all resources associated with the wsio instance.] */

        /* Codes_SRS_WSIO_01_009: [wsio_destroy shall execute a close action if the IO has already been open or an open action is already pending.] */
        (void)wsio_close(wsio_instance, NULL, NULL);

        /* Codes_SRS_WSIO_01_101: [wsio_destroy shall obtain all the IO items by repetitively querying for the head of the pending IO list and freeing that head item.] */
		LIST_ITEM_HANDLE first_pending_io;
		while ((first_pending_io = list_get_head_item(wsio_instance->pending_io_list)) != NULL)
		{
			PENDING_SOCKET_IO* pending_socket_io = (PENDING_SOCKET_IO*)list_item_get_value(first_pending_io);
			if (pending_socket_io != NULL)
			{
				amqpalloc_free(pending_socket_io->bytes);
				amqpalloc_free(pending_socket_io);
			}

            /* Codes_SRS_WSIO_01_102: [Each freed item shall be removed from the list by using list_remove.] */
            (void)list_remove(wsio_instance->pending_io_list, first_pending_io);
		}

		amqpalloc_free(wsio_instance->protocols);
		amqpalloc_free(wsio_instance->host);
        amqpalloc_free(wsio_instance->protocol_name);
		amqpalloc_free(wsio_instance->relative_path);
		amqpalloc_free(wsio_instance->trusted_ca);

		list_destroy(wsio_instance->pending_io_list);

		amqpalloc_free(ws_io);
	}
}

int wsio_open(CONCRETE_IO_HANDLE ws_io, ON_IO_OPEN_COMPLETE on_io_open_complete, ON_BYTES_RECEIVED on_bytes_received, ON_IO_ERROR on_io_error, void* callback_context)
{
	int result = 0;

	if (ws_io == NULL)
	{
		result = __LINE__;
	}
	else
	{
		WSIO_INSTANCE* wsio_instance = (WSIO_INSTANCE*)ws_io;

        /* Codes_SRS_WSIO_01_034: [If another open is in progress or has completed successfully (the IO is open), wsio_open shall fail and return a non-zero value without performing any connection related activities.] */
        if (wsio_instance->io_state != IO_STATE_NOT_OPEN)
        {
            result = __LINE__;
        }
        else
        {
            wsio_instance->on_bytes_received = on_bytes_received;
            wsio_instance->on_io_open_complete = on_io_open_complete;
            wsio_instance->on_io_error = on_io_error;
            wsio_instance->open_callback_context = callback_context;

            int ietf_version = -1; /* latest */
            struct lws_context_creation_info info;

            memset(&info, 0, sizeof info);

            /* Codes_SRS_WSIO_01_011: [The port member of the info argument shall be set to CONTEXT_PORT_NO_LISTEN.] */
            info.port = CONTEXT_PORT_NO_LISTEN;
            /* Codes_SRS_WSIO_01_012: [The protocols member shall be populated with 2 protocol entries, one containing the actual protocol to be used and one empty (fields shall be NULL or 0).] */
            info.protocols = wsio_instance->protocols;
            /* Codes_SRS_WSIO_01_091: [The extensions field shall be set to the internal extensions obtained by calling libwebsocket_get_internal_extensions.] */
            info.extensions = libwebsocket_get_internal_extensions();
            /* Codes_SRS_WSIO_01_092: [gid and uid shall be set to -1.] */
            info.gid = -1;
            info.uid = -1;
            /* Codes_SRS_WSIO_01_096: [The member user shall be set to a user context that will be later passed by the libwebsockets callbacks.] */
            info.user = wsio_instance;
            /* Codes_SRS_WSIO_01_093: [The members iface, token_limits, ssl_cert_filepath, ssl_private_key_filepath, ssl_private_key_password, ssl_ca_filepath, ssl_cipher_list and provided_client_ssl_ctx shall be set to NULL.] */
            info.iface = NULL;
            info.token_limits = NULL;
            info.ssl_ca_filepath = NULL;
            info.ssl_cert_filepath = NULL;
            info.ssl_cipher_list = NULL;
            info.ssl_private_key_filepath = NULL;
            info.ssl_private_key_password = NULL;
            info.provided_client_ssl_ctx = NULL;
            /* Codes_SRS_WSIO_01_094: [No proxy support shall be implemented, thus setting http_proxy_address to NULL.] */
            info.http_proxy_address = NULL;
            /* Codes_SRS_WSIO_01_095: [The member options shall be set to 0.] */
            info.options = 0;
            /* Codes_SRS_WSIO_01_097: [Keep alive shall not be supported, thus ka_time shall be set to 0.] */
            info.ka_time = 0;

            /* Codes_SRS_WSIO_01_010: [wsio_open shall create a context for the libwebsockets connection by calling libwebsocket_create_context.] */
            wsio_instance->ws_context = libwebsocket_create_context(&info);
            if (wsio_instance->ws_context == NULL)
            {
                /* Codes_SRS_WSIO_01_022: [If creating the context fails then wsio_open shall fail and return a non-zero value.] */
                result = __LINE__;
            }
            else
            {
                wsio_instance->io_state = IO_STATE_OPENING;

                /* Codes_SRS_WSIO_01_023: [wsio_open shall trigger the libwebsocket connect by calling libwebsocket_client_connect and passing to it the following arguments] */
                /* Codes_SRS_WSIO_01_024: [clients shall be the context created earlier in wsio_open] */
                /* Codes_SRS_WSIO_01_025: [address shall be the hostname passed to wsio_create] */
                /* Codes_SRS_WSIO_01_026: [port shall be the port passed to wsio_create] */
                /* Codes_SRS_WSIO_01_103: [otherwise it shall be 0.] */
                /* Codes_SRS_WSIO_01_028: [path shall be the relative_path passed in wsio_create] */
                /* Codes_SRS_WSIO_01_029: [host shall be the host passed to wsio_create] */
                /* Codes_SRS_WSIO_01_030: [origin shall be the host passed to wsio_create] */
                /* Codes_SRS_WSIO_01_031: [protocol shall be the protocol_name passed to wsio_create] */
                /* Codes_SRS_WSIO_01_032: [ietf_version_or_minus_one shall be -1] */
                wsio_instance->wsi = libwebsocket_client_connect(wsio_instance->ws_context, wsio_instance->host, wsio_instance->port, wsio_instance->use_ssl, wsio_instance->relative_path, wsio_instance->host, wsio_instance->host, wsio_instance->protocols[0].name, ietf_version);
                if (wsio_instance->wsi == NULL)
                {
                    /* Codes_SRS_WSIO_01_033: [If libwebsocket_client_connect fails then wsio_open shall fail and return a non-zero value.] */
                    libwebsocket_context_destroy(wsio_instance->ws_context);
                    wsio_instance->io_state = IO_STATE_NOT_OPEN;
                    result = __LINE__;
                }
                else
                {
                    /* Codes_SRS_WSIO_01_104: [On success, wsio_open shall return 0.] */
                    result = 0;
                }
            }
        }
	}
	
	return result;
}

int wsio_close(CONCRETE_IO_HANDLE ws_io, ON_IO_CLOSE_COMPLETE on_io_close_complete, void* callback_context)
{
	int result = 0;

	if (ws_io == NULL)
	{
		result = __LINE__;
	}
	else
	{
		WSIO_INSTANCE* wsio_instance = (WSIO_INSTANCE*)ws_io;

        if (wsio_instance->io_state == IO_STATE_NOT_OPEN)
        {
            result = __LINE__;
        }
        else
        {
            /* Codes_SRS_WSIO_01_038: [If wsio_close is called while the open action is in progress, the callback on_io_open_complete shall be called with io_open_result being set to IO_OPEN_CANCELLED and then the wsio_close shall proceed to close the IO.] */
            if (wsio_instance->io_state == IO_STATE_OPENING)
            {
                indicate_open_complete(wsio_instance, IO_OPEN_CANCELLED);
            }

            libwebsocket_context_destroy(wsio_instance->ws_context);
            wsio_instance->io_state = IO_STATE_NOT_OPEN;
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
				(void)libwebsocket_callback_on_writable(wsio_instance->ws_context, wsio_instance->wsi);
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
			(void)libwebsocket_service(wsio_instance->ws_context, 0);
		}
	}
}

const IO_INTERFACE_DESCRIPTION* wsio_get_interface_description(void)
{
	return &ws_io_interface_description;
}
