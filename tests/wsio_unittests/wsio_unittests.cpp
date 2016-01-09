// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "testrunnerswitcher.h"
#include "micromock.h"
#include "micromockcharstararenullterminatedstrings.h"
#include "xio.h"
#include "wsio.h"
#include "amqpalloc.h"
#include "list.h"
#include "libwebsockets.h"
#include "openssl/ssl.h"

static const void** list_items = NULL;
static size_t list_item_count = 0;

static const LIST_HANDLE TEST_LIST_HANDLE = (LIST_HANDLE)0x4242;
static const LIST_ITEM_HANDLE TEST_LIST_ITEM_HANDLE = (LIST_ITEM_HANDLE)0x11;
static struct libwebsocket_context* TEST_LIBWEBSOCKET_CONTEXT = (struct libwebsocket_context*)0x4243;
static void* TEST_USER_CONTEXT = (void*)0x4244;
static struct libwebsocket* TEST_LIBWEBSOCKET = (struct libwebsocket*)0x4245;
static struct libwebsocket_extension* TEST_INTERNAL_EXTENSIONS = (struct libwebsocket_extension*)0x4246;
static BIO* TEST_BIO = (BIO*)0x4247;
static BIO_METHOD* TEST_BIO_METHOD = (BIO_METHOD*)0x4248;
static X509_STORE* TEST_CERT_STORE = (X509_STORE*)0x4249;
static X509* TEST_X509_CERT = (X509*)0x424A;
static WSIO_CONFIG default_wsio_config = 
{
    "test_host",
    443,
    "test_ws_protocol",
    "a/b/c",
    false,
    "my_trusted_ca_payload"
};
static callback_function* saved_ws_callback;
static void* saved_ws_callback_context;

template<>
bool operator==<const lws_context_creation_info*>(const CMockValue<const lws_context_creation_info*>& lhs, const CMockValue<const lws_context_creation_info*>& rhs)
{
    bool result;

    if (lhs.GetValue() == rhs.GetValue())
    {
        result = true;
    }
    else
    {
        size_t currentProtocol = 0;

        result = lhs.GetValue()->gid == rhs.GetValue()->gid;
        result = result && (lhs.GetValue()->extensions == rhs.GetValue()->extensions);
        result = result && (lhs.GetValue()->token_limits == rhs.GetValue()->token_limits);
        result = result && (lhs.GetValue()->uid == rhs.GetValue()->uid);
        result = result && (lhs.GetValue()->ka_time == rhs.GetValue()->ka_time);
        if (rhs.GetValue()->ka_time != 0)
        {
            result = result && (lhs.GetValue()->ka_interval == rhs.GetValue()->ka_interval);
            result = result && (lhs.GetValue()->ka_probes == rhs.GetValue()->ka_probes);
        }

        result = result && (lhs.GetValue()->options == rhs.GetValue()->options);
        result = result && (lhs.GetValue()->iface == rhs.GetValue()->iface);
        result = result && (lhs.GetValue()->port == rhs.GetValue()->port);
        result = result && (lhs.GetValue()->provided_client_ssl_ctx == rhs.GetValue()->provided_client_ssl_ctx);
        if (rhs.GetValue()->http_proxy_address == NULL)
        {
            result = result && (lhs.GetValue()->http_proxy_address == rhs.GetValue()->http_proxy_address);
        }
        else
        {
            result = result && (strcmp(lhs.GetValue()->http_proxy_address, rhs.GetValue()->http_proxy_address) == 0);
            result = result && (lhs.GetValue()->http_proxy_port, rhs.GetValue()->http_proxy_port);
        }

        if (rhs.GetValue()->ssl_ca_filepath == NULL)
        {
            result = result && (lhs.GetValue()->ssl_ca_filepath == rhs.GetValue()->ssl_ca_filepath);
        }
        else
        {
            result = result && (strcmp(lhs.GetValue()->ssl_ca_filepath, rhs.GetValue()->ssl_ca_filepath) == 0);
        }

        if (rhs.GetValue()->ssl_cert_filepath == NULL)
        {
            result = result && (lhs.GetValue()->ssl_cert_filepath == rhs.GetValue()->ssl_cert_filepath);
        }
        else
        {
            result = result && (strcmp(lhs.GetValue()->ssl_cert_filepath, rhs.GetValue()->ssl_cert_filepath) == 0);
        }

        if (rhs.GetValue()->ssl_cipher_list == NULL)
        {
            result = result && (lhs.GetValue()->ssl_cipher_list == rhs.GetValue()->ssl_cipher_list);
        }
        else
        {
            result = result && (strcmp(lhs.GetValue()->ssl_cipher_list, rhs.GetValue()->ssl_cipher_list) == 0);
        }

        if (rhs.GetValue()->ssl_private_key_filepath == NULL)
        {
            result = result && (lhs.GetValue()->ssl_private_key_filepath == rhs.GetValue()->ssl_private_key_filepath);
        }
        else
        {
            result = result && (strcmp(lhs.GetValue()->ssl_private_key_filepath, rhs.GetValue()->ssl_private_key_filepath) == 0);
        }

        if (rhs.GetValue()->ssl_private_key_password == NULL)
        {
            result = result && (lhs.GetValue()->ssl_private_key_password == rhs.GetValue()->ssl_private_key_password);
        }
        else
        {
            result = result && (strcmp(lhs.GetValue()->ssl_private_key_password, rhs.GetValue()->ssl_private_key_password) == 0);
        }

        while (lhs.GetValue()->protocols[currentProtocol].name != NULL)
        {
            result = result && (lhs.GetValue()->protocols[currentProtocol].id == rhs.GetValue()->protocols[currentProtocol].id);
            result = result && (strcmp(lhs.GetValue()->protocols[currentProtocol].name, rhs.GetValue()->protocols[currentProtocol].name) == 0);
            result = result && (lhs.GetValue()->protocols[currentProtocol].owning_server == rhs.GetValue()->protocols[currentProtocol].owning_server);
            result = result && (lhs.GetValue()->protocols[currentProtocol].per_session_data_size == rhs.GetValue()->protocols[currentProtocol].per_session_data_size);
            result = result && (lhs.GetValue()->protocols[currentProtocol].protocol_index == rhs.GetValue()->protocols[currentProtocol].protocol_index);
            result = result && (lhs.GetValue()->protocols[currentProtocol].rx_buffer_size == rhs.GetValue()->protocols[currentProtocol].rx_buffer_size);
            result = result && (lhs.GetValue()->protocols[currentProtocol].user == rhs.GetValue()->protocols[currentProtocol].user);

            currentProtocol++;
        }

        if (rhs.GetValue()->protocols[currentProtocol].name != NULL)
        {
            result = false;
        }
    }

    return result;
}

template<>
bool operator==<lws_context_creation_info*>(const CMockValue<lws_context_creation_info*>& lhs, const CMockValue<lws_context_creation_info*>& rhs)
{
    const lws_context_creation_info* lhs_value = lhs.GetValue();
    const lws_context_creation_info* rhs_value = rhs.GetValue();

    return CMockValue<const lws_context_creation_info*>(lhs_value) == CMockValue<const lws_context_creation_info*>(rhs_value);
}

TYPED_MOCK_CLASS(wsio_mocks, CGlobalMock)
{
public:
	/* amqpalloc mocks */
	MOCK_STATIC_METHOD_1(, void*, amqpalloc_malloc, size_t, size)
	MOCK_METHOD_END(void*, malloc(size));
	MOCK_STATIC_METHOD_2(, void*, amqpalloc_realloc, void*, ptr, size_t, size)
	MOCK_METHOD_END(void*, realloc(ptr, size));
	MOCK_STATIC_METHOD_1(, void, amqpalloc_free, void*, ptr)
		free(ptr);
	MOCK_VOID_METHOD_END();

    // list mocks
    MOCK_STATIC_METHOD_0(, LIST_HANDLE, list_create)
    MOCK_METHOD_END(LIST_HANDLE, TEST_LIST_HANDLE);
    MOCK_STATIC_METHOD_1(, void, list_destroy, LIST_HANDLE, list)
    MOCK_VOID_METHOD_END();
    MOCK_STATIC_METHOD_2(, int, list_remove, LIST_HANDLE, list, LIST_ITEM_HANDLE, item)
        size_t index = (size_t)item - 1;
        (void)memmove(&list_items[index], &list_items[index + 1], sizeof(const void*) * (list_item_count - index - 1));
        list_item_count--;
    MOCK_METHOD_END(int, 0);

    MOCK_STATIC_METHOD_1(, LIST_ITEM_HANDLE, list_get_head_item, LIST_HANDLE, list)
        LIST_ITEM_HANDLE list_item_handle = NULL;
        if (list_item_count > 0)
        {
            list_item_handle = (LIST_ITEM_HANDLE)1;
        }
        else
        {
            list_item_handle = NULL;
        }
    MOCK_METHOD_END(LIST_ITEM_HANDLE, list_item_handle);

    MOCK_STATIC_METHOD_2(, LIST_ITEM_HANDLE, list_add, LIST_HANDLE, list, const void*, item)
        const void** items = (const void**)realloc(list_items, (list_item_count + 1) * sizeof(item));
        if (items != NULL)
        {
            list_items = items;
            list_items[list_item_count++] = item;
        }
    MOCK_METHOD_END(LIST_ITEM_HANDLE, (LIST_ITEM_HANDLE)list_item_count);
    MOCK_STATIC_METHOD_1(, const void*, list_item_get_value, LIST_ITEM_HANDLE, item_handle)
    MOCK_METHOD_END(const void*, (const void*)list_items[(size_t)item_handle - 1]);
    MOCK_STATIC_METHOD_3(, LIST_ITEM_HANDLE, list_find, LIST_HANDLE, handle, LIST_MATCH_FUNCTION, match_function, const void*, match_context)
        size_t i;
        const void* found_item = NULL;
        for (i = 0; i < list_item_count; i++)
        {
            if (match_function((LIST_ITEM_HANDLE)list_items[i], match_context))
            {
                found_item = list_items[i];
                break;
            }
        }
    MOCK_METHOD_END(LIST_ITEM_HANDLE, (LIST_ITEM_HANDLE)found_item);
    MOCK_STATIC_METHOD_3(, int, list_remove_matching_item, LIST_HANDLE, handle, LIST_MATCH_FUNCTION, match_function, const void*, match_context)
        size_t i;
        int res = __LINE__;
        for (i = 0; i < list_item_count; i++)
        {
            if (match_function((LIST_ITEM_HANDLE)list_items[i], match_context))
            {
                (void)memcpy(&list_items[i], &list_items[i + 1], (list_item_count - i - 1) * sizeof(const void*));
                list_item_count--;
                res = 0;
                break;
            }
        }
    MOCK_METHOD_END(int, res);

    // libwebsockets mocks
    MOCK_STATIC_METHOD_1(, struct libwebsocket_context*, libwebsocket_create_context, struct lws_context_creation_info*, info)
        saved_ws_callback = info->protocols[0].callback;
        saved_ws_callback_context = info->user;
    MOCK_METHOD_END(struct libwebsocket_context*, TEST_LIBWEBSOCKET_CONTEXT)
    MOCK_STATIC_METHOD_1(, void, libwebsocket_context_destroy, struct libwebsocket_context*, context)
    MOCK_VOID_METHOD_END()
    MOCK_STATIC_METHOD_2(, int, libwebsocket_service, struct libwebsocket_context*, context, int, timeout_ms)
    MOCK_METHOD_END(int, 0)
    MOCK_STATIC_METHOD_1(, void*, libwebsocket_context_user, struct libwebsocket_context*, context)
    MOCK_METHOD_END(void*, TEST_USER_CONTEXT)
    MOCK_STATIC_METHOD_4(, int, libwebsocket_write, struct libwebsocket*, wsi, unsigned char*, buf, size_t, len, enum libwebsocket_write_protocol, protocol)
    MOCK_METHOD_END(int, 0)
    MOCK_STATIC_METHOD_2(, int, libwebsocket_callback_on_writable, struct libwebsocket_context*, context, struct libwebsocket*, wsi)
    MOCK_METHOD_END(int, 0)
    MOCK_STATIC_METHOD_9(, struct libwebsocket*, libwebsocket_client_connect, struct libwebsocket_context*, clients, const char*, address, int, port, int, ssl_connection, const char*, path, const char*, host, const char*, origin, const char*, protocol, int, ietf_version_or_minus_one)
    MOCK_METHOD_END(struct libwebsocket*, TEST_LIBWEBSOCKET)
    MOCK_STATIC_METHOD_0(, struct libwebsocket_extension*, libwebsocket_get_internal_extensions)
    MOCK_METHOD_END(struct libwebsocket_extension*, TEST_INTERNAL_EXTENSIONS)

    // openssl mocks
    MOCK_STATIC_METHOD_1(, BIO*, BIO_new, BIO_METHOD*, type)
    MOCK_METHOD_END(BIO*, TEST_BIO)
    MOCK_STATIC_METHOD_1(, int, BIO_free, BIO*, a)
    MOCK_METHOD_END(int, 0)
    MOCK_STATIC_METHOD_2(, int, BIO_puts, BIO*, bp, const char*, buf)
    MOCK_METHOD_END(int, 0)
    MOCK_STATIC_METHOD_0(, BIO_METHOD*, BIO_s_mem);
    MOCK_METHOD_END(BIO_METHOD*, TEST_BIO_METHOD)
    MOCK_STATIC_METHOD_1(, X509_STORE*, SSL_CTX_get_cert_store, const SSL_CTX*, ctx);
    MOCK_METHOD_END(X509_STORE*, TEST_CERT_STORE)
    MOCK_STATIC_METHOD_2(, int, X509_STORE_add_cert, X509_STORE*, ctx, X509*, x);
    MOCK_METHOD_END(int, 0)
    MOCK_STATIC_METHOD_4(, X509*, PEM_read_bio_X509, BIO*, bp, X509**, x, pem_password_cb*, cb, void*, u);
    MOCK_METHOD_END(X509*, TEST_X509_CERT)

    // consumer mocks
    MOCK_STATIC_METHOD_2(, void, test_on_io_open_complete, void*, context, IO_OPEN_RESULT, io_open_result);
    MOCK_VOID_METHOD_END()
    MOCK_STATIC_METHOD_3(, void, test_on_bytes_received, void*, context, const unsigned char*, buffer, size_t, size);
    MOCK_VOID_METHOD_END()
    MOCK_STATIC_METHOD_1(, void, test_on_io_error, void*, context);
    MOCK_VOID_METHOD_END()
    MOCK_STATIC_METHOD_1(, void, test_on_io_close_complete, void*, context);
    MOCK_VOID_METHOD_END()
};

extern "C"
{
	DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , void*, amqpalloc_malloc, size_t, size);
	DECLARE_GLOBAL_MOCK_METHOD_2(wsio_mocks, , void*, amqpalloc_realloc, void*, ptr, size_t, size);
	DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , void, amqpalloc_free, void*, ptr);

    DECLARE_GLOBAL_MOCK_METHOD_0(wsio_mocks, , LIST_HANDLE, list_create);
    DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , void, list_destroy, LIST_HANDLE, list);
    DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , LIST_ITEM_HANDLE, list_get_head_item, LIST_HANDLE, list);
    DECLARE_GLOBAL_MOCK_METHOD_2(wsio_mocks, , int, list_remove, LIST_HANDLE, list, LIST_ITEM_HANDLE, item);
    DECLARE_GLOBAL_MOCK_METHOD_2(wsio_mocks, , LIST_ITEM_HANDLE, list_add, LIST_HANDLE, list, const void*, item);
    DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , const void*, list_item_get_value, LIST_ITEM_HANDLE, item_handle);
    DECLARE_GLOBAL_MOCK_METHOD_3(wsio_mocks, , LIST_ITEM_HANDLE, list_find, LIST_HANDLE, handle, LIST_MATCH_FUNCTION, match_function, const void*, match_context);
    DECLARE_GLOBAL_MOCK_METHOD_3(wsio_mocks, , int, list_remove_matching_item, LIST_HANDLE, handle, LIST_MATCH_FUNCTION, match_function, const void*, match_context);

    DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , struct libwebsocket_context*, libwebsocket_create_context, struct lws_context_creation_info*, info);
    DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , void, libwebsocket_context_destroy, struct libwebsocket_context*, context);
    DECLARE_GLOBAL_MOCK_METHOD_2(wsio_mocks, , int, libwebsocket_service, struct libwebsocket_context*, context, int, timeout_ms);
    DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , void*, libwebsocket_context_user, struct libwebsocket_context*, context);
    DECLARE_GLOBAL_MOCK_METHOD_4(wsio_mocks, , int, libwebsocket_write, struct libwebsocket*, wsi, unsigned char*, buf, size_t, len, enum libwebsocket_write_protocol, protocol);
    DECLARE_GLOBAL_MOCK_METHOD_2(wsio_mocks, , int, libwebsocket_callback_on_writable, struct libwebsocket_context*, context, struct libwebsocket*, wsi);
    DECLARE_GLOBAL_MOCK_METHOD_9(wsio_mocks, , struct libwebsocket*, libwebsocket_client_connect, struct libwebsocket_context*, clients, const char*, address, int, port, int, ssl_connection, const char*, path, const char*, host, const char*, origin, const char*, protocol, int, ietf_version_or_minus_one);
    DECLARE_GLOBAL_MOCK_METHOD_0(wsio_mocks, , struct libwebsocket_extension*, libwebsocket_get_internal_extensions);

    DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , BIO*, BIO_new, BIO_METHOD*, type);
    DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , int, BIO_free, BIO*, a);
    DECLARE_GLOBAL_MOCK_METHOD_2(wsio_mocks, , int, BIO_puts, BIO*, bp, const char*, buf);
    DECLARE_GLOBAL_MOCK_METHOD_0(wsio_mocks, , BIO_METHOD*, BIO_s_mem);
    DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , X509_STORE*, SSL_CTX_get_cert_store, const SSL_CTX*, ctx);
    DECLARE_GLOBAL_MOCK_METHOD_2(wsio_mocks, , int, X509_STORE_add_cert, X509_STORE*, ctx, X509*, x);
    DECLARE_GLOBAL_MOCK_METHOD_4(wsio_mocks, , X509*, PEM_read_bio_X509, BIO*, bp, X509**, x, pem_password_cb*, cb, void*, u);

    DECLARE_GLOBAL_MOCK_METHOD_2(wsio_mocks, , void, test_on_io_open_complete, void*, context, IO_OPEN_RESULT, io_open_result);
    DECLARE_GLOBAL_MOCK_METHOD_3(wsio_mocks, , void, test_on_bytes_received, void*, context, const unsigned char*, buffer, size_t, size);
    DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , void, test_on_io_error, void*, context);
    DECLARE_GLOBAL_MOCK_METHOD_1(wsio_mocks, , void, test_on_io_close_complete, void*, context);

    extern void test_logger_log(unsigned int options, char* format, ...)
	{
        (void)options;
		(void)format;
	}
}

MICROMOCK_MUTEX_HANDLE test_serialize_mutex;

BEGIN_TEST_SUITE(wsio_unittests)

TEST_SUITE_INITIALIZE(suite_init)
{
	test_serialize_mutex = MicroMockCreateMutex();
	ASSERT_IS_NOT_NULL(test_serialize_mutex);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
	MicroMockDestroyMutex(test_serialize_mutex);
}

TEST_FUNCTION_INITIALIZE(method_init)
{
	if (!MicroMockAcquireMutex(test_serialize_mutex))
	{
		ASSERT_FAIL("Could not acquire test serialization mutex.");
	}
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
    free(list_items);
    list_items = NULL;
    list_item_count = 0;
	if (!MicroMockReleaseMutex(test_serialize_mutex))
	{
		ASSERT_FAIL("Could not release test serialization mutex.");
	}
}

/* wsio_create */

/* Tests_SRS_WSIO_01_001: [wsio_create shall create an instance of a wsio and return a non-NULL handle to it.] */
/* Tests_SRS_WSIO_01_098: [wsio_create shall create a pending IO list that is to be used when sending buffers over the libwebsockets IO by calling list_create.] */
/* Tests_SRS_WSIO_01_003: [io_create_parameters shall be used as a WSIO_CONFIG*.] */
/* Tests_SRS_WSIO_01_006: [The members host, protocol_name, relative_path and trusted_ca shall be copied for later use (they are needed when the IO is opened).] */
TEST_FUNCTION(wsio_create_with_valid_args_succeeds)
{
	// arrange
	wsio_mocks mocks;

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(mocks, list_create());
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));

	// act
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);

	// assert
	ASSERT_IS_NOT_NULL(wsio);
    mocks.AssertActualAndExpectedCalls();

	// cleanup
	wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_002: [If the argument io_create_parameters is NULL then wsio_create shall return NULL.] */
TEST_FUNCTION(wsio_create_with_NULL_io_create_parameters_fails)
{
    // arrange
    wsio_mocks mocks;

    // act
    CONCRETE_IO_HANDLE wsio = wsio_create(NULL, test_logger_log);

    // assert
    ASSERT_IS_NULL(wsio);
}

/* Tests_SRS_WSIO_01_004: [If any of the WSIO_CONFIG fields host, protocol_name or relative_path is NULL then wsio_create shall return NULL.] */
TEST_FUNCTION(wsio_create_with_NULL_hostname_fails)
{
    // arrange
    wsio_mocks mocks;
    static WSIO_CONFIG test_wsio_config =
    {
        NULL,
        443,
        "test_ws_protocol",
        "a/b/c",
        false,
        "my_trusted_ca_payload"
    };

    // act
    CONCRETE_IO_HANDLE wsio = wsio_create(&test_wsio_config, test_logger_log);

    // assert
    ASSERT_IS_NULL(wsio);
}

/* Tests_SRS_WSIO_01_004: [If any of the WSIO_CONFIG fields host, protocol_name or relative_path is NULL then wsio_create shall return NULL.] */
TEST_FUNCTION(wsio_create_with_NULL_protocol_name_fails)
{
    // arrange
    wsio_mocks mocks;
    static WSIO_CONFIG test_wsio_config =
    {
        "testhost",
        443,
        NULL,
        "a/b/c",
        false,
        "my_trusted_ca_payload"
    };

    // act
    CONCRETE_IO_HANDLE wsio = wsio_create(&test_wsio_config, test_logger_log);

    // assert
    ASSERT_IS_NULL(wsio);
}

/* Tests_SRS_WSIO_01_004: [If any of the WSIO_CONFIG fields host, protocol_name or relative_path is NULL then wsio_create shall return NULL.] */
TEST_FUNCTION(wsio_create_with_NULL_relative_path_fails)
{
    // arrange
    wsio_mocks mocks;
    static WSIO_CONFIG test_wsio_config =
    {
        "testhost",
        443,
        "test_ws_protocol",
        NULL,
        false,
        "my_trusted_ca_payload"
    };

    // act
    CONCRETE_IO_HANDLE wsio = wsio_create(&test_wsio_config, test_logger_log);

    // assert
    ASSERT_IS_NULL(wsio);
}

/* Tests_SRS_WSIO_01_100: [The trusted_ca member shall be optional (it can be NULL).] */
/* Tests_SRS_WSIO_01_006: [The members host, protocol_name, relative_path and trusted_ca shall be copied for later use (they are needed when the IO is opened).] */
TEST_FUNCTION(wsio_create_with_NULL_trusted_ca_and_the_Rest_of_the_args_valid_succeeds)
{
    // arrange
    wsio_mocks mocks;
    static WSIO_CONFIG test_wsio_config =
    {
        "testhost",
        443,
        "test_ws_protocol",
        "a/b/c",
        false,
        NULL
    };

    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(mocks, list_create());
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));

    // act
    CONCRETE_IO_HANDLE wsio = wsio_create(&test_wsio_config, test_logger_log);

    // assert
    ASSERT_IS_NOT_NULL(wsio);
    mocks.AssertActualAndExpectedCalls();

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_005: [If allocating memory for the new wsio instance fails then wsio_create shall return NULL.] */
TEST_FUNCTION(when_allocating_memory_for_the_instance_fails_wsio_create_fails)
{
    // arrange
    wsio_mocks mocks;

    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG))
        .SetReturn((void*)NULL);

    // act
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);

    // assert
    ASSERT_IS_NULL(wsio);
}

/* Tests_SRS_WSIO_01_099: [If list_create fails then wsio_create shall fail and return NULL.] */
TEST_FUNCTION(when_creating_the_pending_io_list_fails_wsio_create_fails)
{
    // arrange
    wsio_mocks mocks;

    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(mocks, list_create())
        .SetReturn((LIST_HANDLE)NULL);
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));

    // act
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);

    // assert
    ASSERT_IS_NULL(wsio);
}

/* Tests_SRS_WSIO_01_005: [If allocating memory for the new wsio instance fails then wsio_create shall return NULL.] */
TEST_FUNCTION(when_allocating_memory_for_the_host_name_fails_wsio_create_fails)
{
    // arrange
    wsio_mocks mocks;

    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(mocks, list_create());
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG))
        .SetReturn((LIST_HANDLE)NULL);
    STRICT_EXPECTED_CALL(mocks, list_destroy(TEST_LIST_HANDLE));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));

    // act
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);

    // assert
    ASSERT_IS_NULL(wsio);
}

/* Tests_SRS_WSIO_01_005: [If allocating memory for the new wsio instance fails then wsio_create shall return NULL.] */
TEST_FUNCTION(when_allocating_memory_for_the_protocol_name_fails_wsio_create_fails)
{
    // arrange
    wsio_mocks mocks;

    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(mocks, list_create());
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG))
        .SetReturn((LIST_HANDLE)NULL);
    STRICT_EXPECTED_CALL(mocks, list_destroy(TEST_LIST_HANDLE));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));

    // act
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);

    // assert
    ASSERT_IS_NULL(wsio);
}

/* Tests_SRS_WSIO_01_005: [If allocating memory for the new wsio instance fails then wsio_create shall return NULL.] */
TEST_FUNCTION(when_allocating_memory_for_the_relative_path_fails_wsio_create_fails)
{
    // arrange
    wsio_mocks mocks;

    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(mocks, list_create());
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG))
        .SetReturn((LIST_HANDLE)NULL);
    STRICT_EXPECTED_CALL(mocks, list_destroy(TEST_LIST_HANDLE));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));

    // act
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);

    // assert
    ASSERT_IS_NULL(wsio);
}

/* Tests_SRS_WSIO_01_005: [If allocating memory for the new wsio instance fails then wsio_create shall return NULL.] */
TEST_FUNCTION(when_allocating_memory_for_the_protocols_fails_wsio_create_fails)
{
    // arrange
    wsio_mocks mocks;

    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(mocks, list_create());
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG))
        .SetReturn((LIST_HANDLE)NULL);
    STRICT_EXPECTED_CALL(mocks, list_destroy(TEST_LIST_HANDLE));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));

    // act
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);

    // assert
    ASSERT_IS_NULL(wsio);
}

/* Tests_SRS_WSIO_01_005: [If allocating memory for the new wsio instance fails then wsio_create shall return NULL.] */
TEST_FUNCTION(when_allocating_memory_for_the_trusted_ca_fails_wsio_create_fails)
{
    // arrange
    wsio_mocks mocks;

    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(mocks, list_create());
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG))
        .SetReturn((LIST_HANDLE)NULL);
    STRICT_EXPECTED_CALL(mocks, list_destroy(TEST_LIST_HANDLE));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));

    // act
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);

    // assert
    ASSERT_IS_NULL(wsio);
}

/* wsio_destroy */

/* Tests_SRS_WSIO_01_007: [wsio_destroy shall free all resources associated with the wsio instance.] */
/* Tests_SRS_WSIO_01_101: [wsio_destroy shall obtain all the IO items by repetitively querying for the head of the pending IO list and freeing that head item.] */
TEST_FUNCTION(when_wsio_destroy_is_called_all_resources_are_freed)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, list_get_head_item(TEST_LIST_HANDLE));
    STRICT_EXPECTED_CALL(mocks, list_destroy(TEST_LIST_HANDLE));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // host_name
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // relative_path
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // protocol_name
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // protocols
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // trusted_ca
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // instance

    // act
    wsio_destroy(wsio);

    // assert
    // no explicit assert, uMock checks the calls
}

/* Tests_SRS_WSIO_01_008: [If ws_io is NULL, wsio_destroy shall do nothing.] */
TEST_FUNCTION(wsio_destroy_with_NULL_handle_does_nothing)
{
    // arrange
    wsio_mocks mocks;

    // act
    wsio_destroy(NULL);

    // assert
    // no explicit assert, uMock checks the calls
}

/* Tests_SRS_WSIO_01_009: [wsio_destroy shall execute a close action if the IO has already been open or an open action is already pending.] */
TEST_FUNCTION(wsio_destroy_closes_the_underlying_libwebsocket_before_destroying_all_resources)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, test_on_io_open_complete((void*)0x4242, IO_OPEN_CANCELLED));

    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_destroy(TEST_LIBWEBSOCKET_CONTEXT));
    STRICT_EXPECTED_CALL(mocks, list_get_head_item(TEST_LIST_HANDLE));
    STRICT_EXPECTED_CALL(mocks, list_destroy(TEST_LIST_HANDLE));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // host_name
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // relative_path
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // protocol_name
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // protocols
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // trusted_ca
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // instance

    // act
    wsio_destroy(wsio);

    // assert
    // no explicit assert, uMock checks the calls
}

/* Tests_SRS_WSIO_01_101: [wsio_destroy shall obtain all the IO items by repetitively querying for the head of the pending IO list and freeing that head item.] */
/* Tests_SRS_WSIO_01_102: [Each freed item shall be removed from the list by using list_remove.] */
TEST_FUNCTION(wsio_destroy_frees_a_pending_io)
{
    // arrange
    wsio_mocks mocks;
    unsigned char test_buffer[] = { 0x42 };
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_user(TEST_LIBWEBSOCKET_CONTEXT))
        .SetReturn(saved_ws_callback_context);
    (void)saved_ws_callback(TEST_LIBWEBSOCKET_CONTEXT, TEST_LIBWEBSOCKET, LWS_CALLBACK_CLIENT_ESTABLISHED, saved_ws_callback_context, NULL, 0);
    (void)wsio_send(wsio, test_buffer, sizeof(test_buffer), NULL, NULL);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, list_get_head_item(TEST_LIST_HANDLE));
    EXPECTED_CALL(mocks, list_item_get_value(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(mocks, list_remove(TEST_LIST_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(2);

    STRICT_EXPECTED_CALL(mocks, list_get_head_item(TEST_LIST_HANDLE));

    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_destroy(TEST_LIBWEBSOCKET_CONTEXT));
    STRICT_EXPECTED_CALL(mocks, list_destroy(TEST_LIST_HANDLE));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // host_name
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // relative_path
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // protocol_name
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // protocols
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // trusted_ca
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // instance

    // act
    wsio_destroy(wsio);

    // assert
    // no explicit assert, uMock checks the calls
}

/* Tests_SRS_WSIO_01_101: [wsio_destroy shall obtain all the IO items by repetitively querying for the head of the pending IO list and freeing that head item.] */
/* Tests_SRS_WSIO_01_102: [Each freed item shall be removed from the list by using list_remove.] */
TEST_FUNCTION(wsio_destroy_frees_2_pending_ios)
{
    // arrange
    wsio_mocks mocks;
    unsigned char test_buffer[] = { 0x42 };
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_user(TEST_LIBWEBSOCKET_CONTEXT))
        .SetReturn(saved_ws_callback_context);
    (void)saved_ws_callback(TEST_LIBWEBSOCKET_CONTEXT, TEST_LIBWEBSOCKET, LWS_CALLBACK_CLIENT_ESTABLISHED, saved_ws_callback_context, NULL, 0);
    (void)wsio_send(wsio, test_buffer, sizeof(test_buffer), NULL, NULL);
    (void)wsio_send(wsio, test_buffer, sizeof(test_buffer), NULL, NULL);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, list_get_head_item(TEST_LIST_HANDLE));
    EXPECTED_CALL(mocks, list_item_get_value(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(mocks, list_remove(TEST_LIST_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    STRICT_EXPECTED_CALL(mocks, list_get_head_item(TEST_LIST_HANDLE));
    EXPECTED_CALL(mocks, list_item_get_value(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(mocks, list_remove(TEST_LIST_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(2);

    STRICT_EXPECTED_CALL(mocks, list_get_head_item(TEST_LIST_HANDLE));

    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_destroy(TEST_LIBWEBSOCKET_CONTEXT));
    STRICT_EXPECTED_CALL(mocks, list_destroy(TEST_LIST_HANDLE));
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // host_name
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // relative_path
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // protocol_name
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // protocols
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // trusted_ca
    EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG)); // instance

    // act
    wsio_destroy(wsio);

    // assert
    // no explicit assert, uMock checks the calls
}

/* wsio_open */

/* Tests_SRS_WSIO_01_010: [wsio_open shall create a context for the libwebsockets connection by calling libwebsocket_create_context.] */
/* Tests_SRS_WSIO_01_011: [The port member of the info argument shall be set to CONTEXT_PORT_NO_LISTEN.] */
/* Tests_SRS_WSIO_01_091: [The extensions field shall be set to the internal extensions obtained by calling libwebsocket_get_internal_extensions.] */
/* Tests_SRS_WSIO_01_092: [gid and uid shall be set to -1.] */
/* Tests_SRS_WSIO_01_093: [The members iface, token_limits, ssl_cert_filepath, ssl_private_key_filepath, ssl_private_key_password, ssl_ca_filepath, ssl_cipher_list and provided_client_ssl_ctx shall be set to NULL.] */
/* Tests_SRS_WSIO_01_094: [No proxy support shall be implemented, thus setting http_proxy_address to NULL.] */
/* Tests_SRS_WSIO_01_095: [The member options shall be set to 0.] */
/* Tests_SRS_WSIO_01_096: [The member user shall be set to a user context that will be later passed by the libwebsockets callbacks.] */
/* Tests_SRS_WSIO_01_097: [Keep alive shall not be supported, thus ka_time shall be set to 0.] */
/* Tests_SRS_WSIO_01_012: [The protocols member shall be populated with 2 protocol entries, one containing the actual protocol to be used and one empty (fields shall be NULL or 0).] */
/* Tests_SRS_WSIO_01_013: [callback shall be set to a callback used by the wsio module to listen to libwebsockets events.] */
/* Tests_SRS_WSIO_01_014: [id shall be set to 0] */
/* Tests_SRS_WSIO_01_015: [name shall be set to protocol_name as passed to wsio_create] */
/* Tests_SRS_WSIO_01_016: [per_session_data_size shall be set to 0] */
/* Tests_SRS_WSIO_01_017: [rx_buffer_size shall be set to 0, as there is no need for atomic frames] */
/* Tests_SRS_WSIO_01_019: [user shall be set to NULL] */
/* Tests_SRS_WSIO_01_020: [owning_server shall be set to NULL] */
/* Tests_SRS_WSIO_01_021: [protocol_index shall be set to 0] */
/* Tests_SRS_WSIO_01_023: [wsio_open shall trigger the libwebsocket connect by calling libwebsocket_client_connect and passing to it the following arguments] */
/* Tests_SRS_WSIO_01_024: [clients shall be the context created earlier in wsio_open] */
/* Tests_SRS_WSIO_01_025: [address shall be the hostname passed to wsio_create] */
/* Tests_SRS_WSIO_01_026: [port shall be the port passed to wsio_create] */
/* Tests_SRS_WSIO_01_103: [otherwise it shall be 0.] */
/* Tests_SRS_WSIO_01_028: [path shall be the relative_path passed in wsio_create] */
/* Tests_SRS_WSIO_01_029: [host shall be the host passed to wsio_create] */
/* Tests_SRS_WSIO_01_030: [origin shall be the host passed to wsio_create] */
/* Tests_SRS_WSIO_01_031: [protocol shall be the protocol_name passed to wsio_create] */
/* Tests_SRS_WSIO_01_032: [ietf_version_or_minus_one shall be -1] */
/* Tests_SRS_WSIO_01_104: [On success, wsio_open shall return 0.] */
TEST_FUNCTION(wsio_open_with_proper_arguments_succeeds)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    mocks.ResetAllCalls();

    lws_context_creation_info lws_context_info;
    libwebsocket_protocols protocols[] = 
    {
        { default_wsio_config.protocol_name, NULL, 0, 0, 0, NULL, NULL, 0 },
        { NULL, NULL, 0, 0, 0, NULL, NULL, 0 }
    };

    lws_context_info.port = CONTEXT_PORT_NO_LISTEN;
    lws_context_info.extensions = TEST_INTERNAL_EXTENSIONS;
    lws_context_info.gid = -1;
    lws_context_info.uid = -1;
    lws_context_info.iface = NULL;
    lws_context_info.token_limits = NULL;
    lws_context_info.ssl_cert_filepath = NULL;
    lws_context_info.ssl_private_key_filepath = NULL;
    lws_context_info.ssl_private_key_password = NULL;
    lws_context_info.ssl_ca_filepath = NULL;
    lws_context_info.ssl_cipher_list = NULL;
    lws_context_info.provided_client_ssl_ctx = NULL;
    lws_context_info.http_proxy_address = NULL;
    lws_context_info.options = 0;
    lws_context_info.ka_time = 0;

    lws_context_info.protocols = protocols;

    STRICT_EXPECTED_CALL(mocks, libwebsocket_get_internal_extensions());
    STRICT_EXPECTED_CALL(mocks, libwebsocket_create_context(&lws_context_info));
    STRICT_EXPECTED_CALL(mocks, libwebsocket_client_connect(TEST_LIBWEBSOCKET_CONTEXT, default_wsio_config.host, default_wsio_config.port, 0, default_wsio_config.relative_path, default_wsio_config.host, default_wsio_config.host, default_wsio_config.protocol_name, -1));

    // act
    int result = wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    mocks.AssertActualAndExpectedCalls();
    
    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_015: [name shall be set to protocol_name as passed to wsio_create] */
/* Tests_SRS_WSIO_01_025: [address shall be the hostname passed to wsio_create] */
/* Tests_SRS_WSIO_01_026: [port shall be the port passed to wsio_create] */
/* Tests_SRS_WSIO_01_027: [if use_ssl passed in wsio_create is true, the use_ssl argument shall be 1] */
/* Tests_SRS_WSIO_01_028: [path shall be the relative_path passed in wsio_create] */
/* Tests_SRS_WSIO_01_029: [host shall be the host passed to wsio_create] */
/* Tests_SRS_WSIO_01_030: [origin shall be the host passed to wsio_create] */
/* Tests_SRS_WSIO_01_031: [protocol shall be the protocol_name passed to wsio_create] */
/* Tests_SRS_WSIO_01_091: [The extensions field shall be set to the internal extensions obtained by calling libwebsocket_get_internal_extensions.] */
/* Tests_SRS_WSIO_01_104: [On success, wsio_open shall return 0.] */
TEST_FUNCTION(wsio_open_with_different_config_succeeds)
{
    // arrange
    wsio_mocks mocks;
    static WSIO_CONFIG wsio_config =
    {
        "hagauaga",
        1234,
        "another_proto",
        "d1/e2/f3",
        true,
        "booohoo"
    };
    CONCRETE_IO_HANDLE wsio = wsio_create(&wsio_config, test_logger_log);
    mocks.ResetAllCalls();

    lws_context_creation_info lws_context_info;
    libwebsocket_protocols protocols[] =
    {
        { wsio_config.protocol_name, NULL, 0, 0, 0, NULL, NULL, 0 },
        { NULL, NULL, 0, 0, 0, NULL, NULL, 0 }
    };

    lws_context_info.port = CONTEXT_PORT_NO_LISTEN;
    lws_context_info.extensions = (libwebsocket_extension*)NULL;
    lws_context_info.gid = -1;
    lws_context_info.uid = -1;
    lws_context_info.iface = NULL;
    lws_context_info.token_limits = NULL;
    lws_context_info.ssl_cert_filepath = NULL;
    lws_context_info.ssl_private_key_filepath = NULL;
    lws_context_info.ssl_private_key_password = NULL;
    lws_context_info.ssl_ca_filepath = NULL;
    lws_context_info.ssl_cipher_list = NULL;
    lws_context_info.provided_client_ssl_ctx = NULL;
    lws_context_info.http_proxy_address = NULL;
    lws_context_info.options = 0;
    lws_context_info.ka_time = 0;

    lws_context_info.protocols = protocols;

    STRICT_EXPECTED_CALL(mocks, libwebsocket_get_internal_extensions())
        .SetReturn((libwebsocket_extension*)NULL);
    STRICT_EXPECTED_CALL(mocks, libwebsocket_create_context(&lws_context_info));
    STRICT_EXPECTED_CALL(mocks, libwebsocket_client_connect(TEST_LIBWEBSOCKET_CONTEXT, wsio_config.host, wsio_config.port, 1, wsio_config.relative_path, wsio_config.host, wsio_config.host, wsio_config.protocol_name, -1));

    // act
    int result = wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    mocks.AssertActualAndExpectedCalls();

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_022: [If creating the context fails then wsio_open shall fail and return a non-zero value.] */
TEST_FUNCTION(when_creating_the_libwebsockets_context_fails_then_wsio_open_fails)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    mocks.ResetAllCalls();

    lws_context_creation_info lws_context_info;
    libwebsocket_protocols protocols[] =
    {
        { default_wsio_config.protocol_name, NULL, 0, 0, 0, NULL, NULL, 0 },
        { NULL, NULL, 0, 0, 0, NULL, NULL, 0 }
    };

    lws_context_info.port = CONTEXT_PORT_NO_LISTEN;
    lws_context_info.extensions = TEST_INTERNAL_EXTENSIONS;
    lws_context_info.gid = -1;
    lws_context_info.uid = -1;
    lws_context_info.iface = NULL;
    lws_context_info.token_limits = NULL;
    lws_context_info.ssl_cert_filepath = NULL;
    lws_context_info.ssl_private_key_filepath = NULL;
    lws_context_info.ssl_private_key_password = NULL;
    lws_context_info.ssl_ca_filepath = NULL;
    lws_context_info.ssl_cipher_list = NULL;
    lws_context_info.provided_client_ssl_ctx = NULL;
    lws_context_info.http_proxy_address = NULL;
    lws_context_info.options = 0;
    lws_context_info.ka_time = 0;

    lws_context_info.protocols = protocols;

    STRICT_EXPECTED_CALL(mocks, libwebsocket_get_internal_extensions());
    STRICT_EXPECTED_CALL(mocks, libwebsocket_create_context(&lws_context_info))
        .SetReturn((libwebsocket_context*)NULL);

    // act
    int result = wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_WSIO_01_033: [If libwebsocket_client_connect fails then wsio_open shall fail and return a non-zero value.] */
TEST_FUNCTION(when_libwebsocket_client_connect_fails_then_wsio_open_fails)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    mocks.ResetAllCalls();

    lws_context_creation_info lws_context_info;
    libwebsocket_protocols protocols[] =
    {
        { default_wsio_config.protocol_name, NULL, 0, 0, 0, NULL, NULL, 0 },
        { NULL, NULL, 0, 0, 0, NULL, NULL, 0 }
    };

    lws_context_info.port = CONTEXT_PORT_NO_LISTEN;
    lws_context_info.extensions = TEST_INTERNAL_EXTENSIONS;
    lws_context_info.gid = -1;
    lws_context_info.uid = -1;
    lws_context_info.iface = NULL;
    lws_context_info.token_limits = NULL;
    lws_context_info.ssl_cert_filepath = NULL;
    lws_context_info.ssl_private_key_filepath = NULL;
    lws_context_info.ssl_private_key_password = NULL;
    lws_context_info.ssl_ca_filepath = NULL;
    lws_context_info.ssl_cipher_list = NULL;
    lws_context_info.provided_client_ssl_ctx = NULL;
    lws_context_info.http_proxy_address = NULL;
    lws_context_info.options = 0;
    lws_context_info.ka_time = 0;

    lws_context_info.protocols = protocols;

    STRICT_EXPECTED_CALL(mocks, libwebsocket_get_internal_extensions());
    STRICT_EXPECTED_CALL(mocks, libwebsocket_create_context(&lws_context_info));
    STRICT_EXPECTED_CALL(mocks, libwebsocket_client_connect(TEST_LIBWEBSOCKET_CONTEXT, default_wsio_config.host, default_wsio_config.port, 0, default_wsio_config.relative_path, default_wsio_config.host, default_wsio_config.host, default_wsio_config.protocol_name, -1))
        .SetReturn((libwebsocket*)NULL);
    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_destroy(TEST_LIBWEBSOCKET_CONTEXT));

    // act
    int result = wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_WSIO_01_034: [If another open is in progress or has completed successfully (the IO is open), wsio_open shall fail and return a non-zero value without performing any connection related activities.] */
TEST_FUNCTION(a_second_wsio_open_while_opening_fails)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    mocks.ResetAllCalls();

    // act
    int result = wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    mocks.AssertActualAndExpectedCalls();

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_036: [The callback on_io_open_complete shall be called with io_open_result being set to IO_OPEN_OK when the open action is succesfull.] */
/* Tests_SRS_WSIO_01_039: [The callback_context argument shall be passed to on_io_open_complete as is.] */
TEST_FUNCTION(when_ws_callback_indicates_a_connect_complete_then_the_on_open_complete_callback_is_triggered)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_user(TEST_LIBWEBSOCKET_CONTEXT))
        .SetReturn(saved_ws_callback_context);
    STRICT_EXPECTED_CALL(mocks, test_on_io_open_complete((void*)0x4242, IO_OPEN_OK));

    // act
    (void)saved_ws_callback(TEST_LIBWEBSOCKET_CONTEXT, TEST_LIBWEBSOCKET, LWS_CALLBACK_CLIENT_ESTABLISHED, NULL, NULL, 0);

    // assert
    mocks.AssertActualAndExpectedCalls();

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_037: [If any error occurs while the open action is in progress, the callback on_io_open_complete shall be called with io_open_result being set to IO_OPEN_ERROR.] */
/* Tests_SRS_WSIO_01_039: [The callback_context argument shall be passed to on_io_open_complete as is.] */
TEST_FUNCTION(when_ws_callback_indicates_a_connection_error_then_the_on_open_complete_callback_is_triggered_with_error)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_user(TEST_LIBWEBSOCKET_CONTEXT))
        .SetReturn(saved_ws_callback_context);
    STRICT_EXPECTED_CALL(mocks, test_on_io_open_complete((void*)0x4242, IO_OPEN_ERROR));
    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_destroy(TEST_LIBWEBSOCKET_CONTEXT));

    // act
    (void)saved_ws_callback(TEST_LIBWEBSOCKET_CONTEXT, TEST_LIBWEBSOCKET, LWS_CALLBACK_CLIENT_CONNECTION_ERROR, NULL, NULL, 0);

    // assert
    mocks.AssertActualAndExpectedCalls();

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_038: [If wsio_close is called while the open action is in progress, the callback on_io_open_complete shall be called with io_open_result being set to IO_OPEN_CANCELLED and then the wsio_close shall proceed to close the IO.] */
/* Tests_SRS_WSIO_01_039: [The callback_context argument shall be passed to on_io_open_complete as is.] */
TEST_FUNCTION(when_wsio_close_is_called_while_an_open_action_is_in_progress_the_on_io_open_complete_callback_is_triggered_with_cancelled)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, test_on_io_open_complete((void*)0x4242, IO_OPEN_CANCELLED));
    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_destroy(TEST_LIBWEBSOCKET_CONTEXT));

    // act
    wsio_close(wsio, NULL, NULL);

    // assert
    mocks.AssertActualAndExpectedCalls();

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_039: [The callback_context argument shall be passed to on_io_open_complete as is.] */
TEST_FUNCTION(when_ws_callback_indicates_a_connection_error_then_the_on_open_complete_callback_is_triggered_with_error_and_NULL_callback_context)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, NULL);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_user(TEST_LIBWEBSOCKET_CONTEXT))
        .SetReturn(saved_ws_callback_context);
    STRICT_EXPECTED_CALL(mocks, test_on_io_open_complete(NULL, IO_OPEN_ERROR));
    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_destroy(TEST_LIBWEBSOCKET_CONTEXT));

    // act
    (void)saved_ws_callback(TEST_LIBWEBSOCKET_CONTEXT, TEST_LIBWEBSOCKET, LWS_CALLBACK_CLIENT_CONNECTION_ERROR, NULL, NULL, 0);

    // assert
    mocks.AssertActualAndExpectedCalls();

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_039: [The callback_context argument shall be passed to on_io_open_complete as is.] */
TEST_FUNCTION(when_ws_callback_indicates_a_connect_complete_then_the_on_open_complete_callback_is_triggered_with_NULL_callback_context)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, NULL);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_user(TEST_LIBWEBSOCKET_CONTEXT))
        .SetReturn(saved_ws_callback_context);
    STRICT_EXPECTED_CALL(mocks, test_on_io_open_complete(NULL, IO_OPEN_OK));

    // act
    (void)saved_ws_callback(TEST_LIBWEBSOCKET_CONTEXT, TEST_LIBWEBSOCKET, LWS_CALLBACK_CLIENT_ESTABLISHED, NULL, NULL, 0);

    // assert
    mocks.AssertActualAndExpectedCalls();

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_040: [The argument on_io_open_complete shall be optional, if NULL is passed by the caller then no open complete callback shall be triggered.] */
TEST_FUNCTION(when_ws_callback_indicates_a_connect_complete_and_no_on_open_complete_callback_was_supplied_no_callback_is_triggered)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, NULL, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_user(TEST_LIBWEBSOCKET_CONTEXT))
        .SetReturn(saved_ws_callback_context);

    // act
    (void)saved_ws_callback(TEST_LIBWEBSOCKET_CONTEXT, TEST_LIBWEBSOCKET, LWS_CALLBACK_CLIENT_ESTABLISHED, NULL, NULL, 0);

    // assert
    mocks.AssertActualAndExpectedCalls();

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_040: [The argument on_io_open_complete shall be optional, if NULL is passed by the caller then no open complete callback shall be triggered.] */
TEST_FUNCTION(when_ws_callback_indicates_a_connect_error_and_no_on_open_complete_callback_was_supplied_no_callback_is_triggered)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, NULL, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_user(TEST_LIBWEBSOCKET_CONTEXT))
        .SetReturn(saved_ws_callback_context);
    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_destroy(TEST_LIBWEBSOCKET_CONTEXT));

    // act
    (void)saved_ws_callback(TEST_LIBWEBSOCKET_CONTEXT, TEST_LIBWEBSOCKET, LWS_CALLBACK_CLIENT_CONNECTION_ERROR, NULL, NULL, 0);

    // assert
    mocks.AssertActualAndExpectedCalls();

    // cleanup
    wsio_destroy(wsio);
}

/* wsio_close */

/* Tests_SRS_WSIO_01_041: [wsio_close shall close the websockets IO if an open action is either pending or has completed successfully (if the IO is open).] */
/* Tests_SRS_WSIO_01_049: [The argument on_io_close_complete shall be optional, if NULL is passed by the caller then no close complete callback shall be triggered.] */
/* Tests_SRS_WSIO_01_043: [wsio_close shall close the connection by calling libwebsocket_context_destroy.] */
/* Tests_SRS_WSIO_01_044: [On success wsio_close shall return 0.] */
TEST_FUNCTION(wsio_close_destroys_the_ws_context)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, test_on_io_open_complete((void*)0x4242, IO_OPEN_CANCELLED));
    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_destroy(TEST_LIBWEBSOCKET_CONTEXT));

    // act
    int result = wsio_close(wsio, NULL, NULL);

    // assert
    mocks.AssertActualAndExpectedCalls();
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_047: [The callback on_io_close_complete shall be called after the close action has been completed in the context of wsio_close (wsio_close is effectively blocking).] */
/* Tests_SRS_WSIO_01_048: [The callback_context argument shall be passed to on_io_close_complete as is.] */
TEST_FUNCTION(wsio_close_destroys_the_ws_context_and_calls_the_io_close_complete_callback)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, test_on_io_open_complete((void*)0x4242, IO_OPEN_CANCELLED));
    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_destroy(TEST_LIBWEBSOCKET_CONTEXT));
    STRICT_EXPECTED_CALL(mocks, test_on_io_close_complete((void*)0x4243));

    // act
    int result = wsio_close(wsio, test_on_io_close_complete, (void*)0x4243);

    // assert
    mocks.AssertActualAndExpectedCalls();
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_047: [The callback on_io_close_complete shall be called after the close action has been completed in the context of wsio_close (wsio_close is effectively blocking).] */
/* Tests_SRS_WSIO_01_048: [The callback_context argument shall be passed to on_io_close_complete as is.] */
TEST_FUNCTION(wsio_close_after_ws_connected_calls_the_io_close_complete_callback)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_user(TEST_LIBWEBSOCKET_CONTEXT))
        .SetReturn(saved_ws_callback_context);
    (void)saved_ws_callback(TEST_LIBWEBSOCKET_CONTEXT, TEST_LIBWEBSOCKET, LWS_CALLBACK_CLIENT_ESTABLISHED, saved_ws_callback_context, NULL, 0);
    mocks.ResetAllCalls();

    STRICT_EXPECTED_CALL(mocks, libwebsocket_context_destroy(TEST_LIBWEBSOCKET_CONTEXT));
    STRICT_EXPECTED_CALL(mocks, test_on_io_close_complete((void*)0x4243));

    // act
    int result = wsio_close(wsio, test_on_io_close_complete, (void*)0x4243);

    // assert
    mocks.AssertActualAndExpectedCalls();
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_042: [if ws_io is NULL, wsio_close shall return a non-zero value.] */
TEST_FUNCTION(wsio_close_with_NULL_handle_fails)
{
    // arrange
    wsio_mocks mocks;

    // act
    int result = wsio_close(NULL, test_on_io_close_complete, (void*)0x4242);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_WSIO_01_045: [wsio_close when no open action has been issued shall fail and return a non-zero value.] */
TEST_FUNCTION(wsio_close_when_not_open_fails)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    mocks.ResetAllCalls();

    // act
    int result = wsio_close(wsio, test_on_io_close_complete, (void*)0x4242);

    // assert
    mocks.AssertActualAndExpectedCalls();
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    wsio_destroy(wsio);
}

/* Tests_SRS_WSIO_01_046: [wsio_close after a wsio_close shall fail and return a non-zero value.]  */
TEST_FUNCTION(wsio_close_when_already_closed_fails)
{
    // arrange
    wsio_mocks mocks;
    CONCRETE_IO_HANDLE wsio = wsio_create(&default_wsio_config, test_logger_log);
    (void)wsio_open(wsio, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, (void*)0x4242);
    (void)wsio_close(wsio, test_on_io_close_complete, (void*)0x4242);
    mocks.ResetAllCalls();

    // act
    int result = wsio_close(wsio, test_on_io_close_complete, (void*)0x4242);

    // assert
    mocks.AssertActualAndExpectedCalls();
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    wsio_destroy(wsio);
}

END_TEST_SUITE(wsio_unittests)
