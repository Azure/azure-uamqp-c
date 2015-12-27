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

    STRICT_EXPECTED_CALL(mocks, test_on_io_open_complete((void*)0x4242, IO_OPEN_ERROR));

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

END_TEST_SUITE(wsio_unittests)
