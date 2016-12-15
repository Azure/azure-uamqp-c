// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <cstdint>
#include <cstdbool>
#include "testrunnerswitcher.h"
#include "micromock.h"
#include "micromockcharstararenullterminatedstrings.h"
#include "azure_uamqp_c/saslclientio.h"
#include "azure_uamqp_c/frame_codec.h"
#include "azure_uamqp_c/sasl_frame_codec.h"
#include "azure_uamqp_c/sasl_mechanism.h"
#include "amqp_definitions_mocks.h"

/* Requirements that cannot really be tested due to design choices: */
/* Tests_SRS_SASLCLIENTIO_01_100: [do nothing] */
/* Tests_SRS_SASLCLIENTIO_01_108: [do nothing] */
/* Tests_SRS_SASLCLIENTIO_01_115: [do nothing] */
/* Tests_SRS_SASLCLIENTIO_01_104: [do nothing] */

static XIO_HANDLE test_underlying_io = (XIO_HANDLE)0x4242;
static SASL_MECHANISM_HANDLE test_sasl_mechanism = (SASL_MECHANISM_HANDLE)0x4243;
static FRAME_CODEC_HANDLE test_frame_codec = (FRAME_CODEC_HANDLE)0x4244;
static SASL_FRAME_CODEC_HANDLE test_sasl_frame_codec = (SASL_FRAME_CODEC_HANDLE)0x4245;
static AMQP_VALUE test_descriptor_value = (AMQP_VALUE)0x4246;
static AMQP_VALUE test_sasl_server_mechanism = (AMQP_VALUE)0x4247;
static const char* test_mechanism = "test_mechanism";
static void* test_context = (void*)0x4242;

static ON_BYTES_RECEIVED saved_on_bytes_received;
static ON_IO_OPEN_COMPLETE saved_on_io_open_complete;
static ON_IO_ERROR saved_on_io_error;
static void* saved_io_callback_context;

static ON_SASL_FRAME_RECEIVED saved_on_sasl_frame_received;
static ON_SASL_FRAME_CODEC_ERROR saved_on_sasl_frame_codec_error;
static void* saved_sasl_frame_codec_callback_context;

static ON_FRAME_RECEIVED saved_frame_received_callback;
static void* saved_frame_received_callback_context;

static ON_BYTES_ENCODED saved_on_bytes_encoded;
static void* saved_on_bytes_encoded_callback_context;

static ON_FRAME_CODEC_ERROR saved_on_frame_codec_error;
static void* saved_on_frame_codec_error_callback_context;


/* Tests_SRS_SASLCLIENTIO_01_002: [The protocol header consists of the upper case ASCII letters "AMQP" followed by a protocol id of three, followed by three unsigned bytes representing the major, minor, and revision of the specification version (currently 1 (SASL-MAJOR), 0 (SASLMINOR), 0 (SASL-REVISION)).] */
/* Tests_SRS_SASLCLIENTIO_01_124: [SASL-MAJOR 1 major protocol version.] */
/* Tests_SRS_SASLCLIENTIO_01_125: [SASL-MINOR 0 minor protocol version.] */
/* Tests_SRS_SASLCLIENTIO_01_126: [SASL-REVISION 0 protocol revision.] */
const unsigned char sasl_header[] = { 'A', 'M', 'Q', 'P', 3, 1, 0, 0 };
const unsigned char test_sasl_mechanisms_frame[] = { 'x', '1' }; /* these are some dummy bytes */
const unsigned char test_sasl_outcome[] = { 'x', '2' }; /* these are some dummy bytes */
const unsigned char test_sasl_challenge[] = { 'x', '3' }; /* these are some dummy bytes */

static AMQP_VALUE test_sasl_value = (AMQP_VALUE)0x5242;

static char expected_stringified_io[8192];
static char actual_stringified_io[8192];
static unsigned char* frame_codec_received_bytes;
static size_t frame_codec_received_byte_count;
static unsigned char* io_send_bytes;
static size_t io_send_byte_count;

template<>
bool operator==<const SASL_MECHANISM_BYTES*>(const CMockValue<const SASL_MECHANISM_BYTES*>& lhs, const CMockValue<const SASL_MECHANISM_BYTES*>& rhs)
{
	return (lhs.GetValue() == rhs.GetValue()) ||
		(((lhs.GetValue()->length == rhs.GetValue()->length) &&
		((rhs.GetValue()->length == 0) || (memcmp(lhs.GetValue()->bytes, rhs.GetValue()->bytes, lhs.GetValue()->length) == 0))));
}

void stringify_bytes(const unsigned char* bytes, size_t byte_count, char* output_string)
{
	size_t i;
	size_t pos = 0;

	output_string[pos++] = '[';
	for (i = 0; i < byte_count; i++)
	{
		(void)sprintf(&output_string[pos], "0x%02X", bytes[i]);
		if (i < byte_count - 1)
		{
			strcat(output_string, ",");
		}
		pos = strlen(output_string);
	}
	output_string[pos++] = ']';
	output_string[pos++] = '\0';
}

static const unsigned char test_challenge_bytes[] = { 0x42 };
static const unsigned char test_response_bytes[] = { 0x43, 0x44 };
static const amqp_binary some_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
static const SASL_MECHANISM_BYTES sasl_mechanism_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
static const SASL_MECHANISM_BYTES sasl_mechanism_response_bytes = { test_response_bytes, sizeof(test_response_bytes) };
static const amqp_binary response_binary_value = { test_response_bytes, sizeof(test_response_bytes) };

TYPED_MOCK_CLASS(saslclientio_mocks, CGlobalMock)
{
public:
	/* amqpalloc mocks */
	MOCK_STATIC_METHOD_1(, void*, amqpalloc_malloc, size_t, size)
	MOCK_METHOD_END(void*, malloc(size));
	MOCK_STATIC_METHOD_1(, void, amqpalloc_free, void*, ptr)
		free(ptr);
	MOCK_VOID_METHOD_END();

	/* frame_codec mocks */
	MOCK_STATIC_METHOD_2(, FRAME_CODEC_HANDLE, frame_codec_create, ON_FRAME_CODEC_ERROR, on_frame_codec_error, void*, callback_context)
		saved_on_frame_codec_error = on_frame_codec_error;
		saved_on_frame_codec_error_callback_context = callback_context;
	MOCK_METHOD_END(FRAME_CODEC_HANDLE, test_frame_codec);
	MOCK_STATIC_METHOD_1(, void, frame_codec_destroy, FRAME_CODEC_HANDLE, frame_codec);
	MOCK_VOID_METHOD_END();
	MOCK_STATIC_METHOD_3(, int, frame_codec_receive_bytes, FRAME_CODEC_HANDLE, frame_codec, const unsigned char*, buffer, size_t, size)
		unsigned char* new_bytes = (unsigned char*)realloc(frame_codec_received_bytes, frame_codec_received_byte_count + size);
		if (new_bytes != NULL)
		{
			frame_codec_received_bytes = new_bytes;
			(void)memcpy(frame_codec_received_bytes + frame_codec_received_byte_count, buffer, size);
			frame_codec_received_byte_count += size;
		}
	MOCK_METHOD_END(int, 0);
	MOCK_STATIC_METHOD_4(, int, frame_codec_subscribe, FRAME_CODEC_HANDLE, frame_codec, uint8_t, type, ON_FRAME_RECEIVED, frame_received_callback, void*, callback_context);
		saved_frame_received_callback = frame_received_callback;
		saved_frame_received_callback_context = callback_context;
	MOCK_METHOD_END(int, 0);
	MOCK_STATIC_METHOD_2(, int, frame_codec_unsubscribe, FRAME_CODEC_HANDLE, frame_codec, uint8_t, type);
	MOCK_METHOD_END(int, 0);
	MOCK_STATIC_METHOD_8(, int, frame_codec_encode_frame, FRAME_CODEC_HANDLE, frame_codec, uint8_t, type, const PAYLOAD*, payloads, size_t, payload_count, const unsigned char*, type_specific_bytes, uint32_t, type_specific_size, ON_BYTES_ENCODED, on_bytes_encoded, void*, callback_context);
	MOCK_METHOD_END(int, 0);

	/* sasl_frame_codec mocks */
	MOCK_STATIC_METHOD_4(, SASL_FRAME_CODEC_HANDLE, sasl_frame_codec_create, FRAME_CODEC_HANDLE, frame_codec, ON_SASL_FRAME_RECEIVED, on_sasl_frame_received, ON_SASL_FRAME_CODEC_ERROR, on_sasl_frame_codec_error, void*, callback_context)
		saved_on_sasl_frame_received = on_sasl_frame_received;
		saved_sasl_frame_codec_callback_context = callback_context;
		saved_on_sasl_frame_codec_error = on_sasl_frame_codec_error;
	MOCK_METHOD_END(SASL_FRAME_CODEC_HANDLE, test_sasl_frame_codec);
	MOCK_STATIC_METHOD_1(, void, sasl_frame_codec_destroy, SASL_FRAME_CODEC_HANDLE, sasl_frame_codec)
	MOCK_VOID_METHOD_END();
	MOCK_STATIC_METHOD_4(, int, sasl_frame_codec_encode_frame, SASL_FRAME_CODEC_HANDLE, sasl_frame_codec, const AMQP_VALUE, sasl_frame_value, ON_BYTES_ENCODED, on_bytes_encoded, void*, callback_context)
		saved_on_bytes_encoded = on_bytes_encoded;
		saved_on_bytes_encoded_callback_context = callback_context;
	MOCK_METHOD_END(int, 0);

	/* xio mocks */
	MOCK_STATIC_METHOD_7(, int, xio_open, XIO_HANDLE, xio, ON_IO_OPEN_COMPLETE, on_io_open_complete, void*, on_io_open_complete_context, ON_BYTES_RECEIVED, on_bytes_received, void*, on_bytes_received_context, ON_IO_ERROR, on_io_error, void*, on_io_error_context)
		saved_on_bytes_received = on_bytes_received;
		saved_on_io_open_complete = on_io_open_complete;
		saved_on_io_error = on_io_error;
		saved_io_callback_context = on_io_open_complete_context;
	MOCK_METHOD_END(int, 0);
	MOCK_STATIC_METHOD_3(, int, xio_close, XIO_HANDLE, xio, ON_IO_CLOSE_COMPLETE, on_io_close_complete, void*, callback_context)
	MOCK_METHOD_END(int, 0);
	MOCK_STATIC_METHOD_5(, int, xio_send, XIO_HANDLE, xio, const void*, buffer, size_t, size, ON_SEND_COMPLETE, on_send_complete, void*, callback_context)
		unsigned char* new_bytes = (unsigned char*)realloc(io_send_bytes, io_send_byte_count + size);
		if (new_bytes != NULL)
		{
			io_send_bytes = new_bytes;
			(void)memcpy(io_send_bytes + io_send_byte_count, buffer, size);
			io_send_byte_count += size;
		}
	MOCK_METHOD_END(int, 0);
    MOCK_STATIC_METHOD_1(, void, xio_dowork, XIO_HANDLE, xio)
    MOCK_VOID_METHOD_END();
    MOCK_STATIC_METHOD_3(, int, xio_setoption, XIO_HANDLE, xio, const char*, optionName, const void*, value)
    MOCK_METHOD_END(int, 0);

	/* sasl_mechanism mocks */
	MOCK_STATIC_METHOD_2(, int, saslmechanism_get_init_bytes, SASL_MECHANISM_HANDLE, sasl_mechanism, SASL_MECHANISM_BYTES*, init_bytes)
	MOCK_METHOD_END(int, 0);
	MOCK_STATIC_METHOD_1(, const char*, saslmechanism_get_mechanism_name, SASL_MECHANISM_HANDLE, sasl_mechanism)
	MOCK_METHOD_END(const char*, test_mechanism);
	MOCK_STATIC_METHOD_3(, int, saslmechanism_challenge, SASL_MECHANISM_HANDLE, sasl_mechanism, const SASL_MECHANISM_BYTES*, challenge_bytes, SASL_MECHANISM_BYTES*, response_bytes)
	MOCK_METHOD_END(int, 0);

	/* amqpvalue_to_string mocks */
	MOCK_STATIC_METHOD_1(, char*, amqpvalue_to_string, AMQP_VALUE, amqp_value)
	MOCK_METHOD_END(char*, NULL);

	/* amqpvalue mocks */
	MOCK_STATIC_METHOD_1(, void, amqpvalue_destroy, AMQP_VALUE, value)
	MOCK_VOID_METHOD_END();
	MOCK_STATIC_METHOD_1(, AMQP_VALUE, amqpvalue_get_inplace_descriptor, AMQP_VALUE, value)
	MOCK_METHOD_END(AMQP_VALUE, test_descriptor_value);
	MOCK_STATIC_METHOD_2(, int, amqpvalue_get_array_item_count, AMQP_VALUE, value, uint32_t*, size)
	MOCK_METHOD_END(int, 0);
	MOCK_STATIC_METHOD_2(, AMQP_VALUE, amqpvalue_get_array_item, AMQP_VALUE, value, uint32_t, index)
	MOCK_METHOD_END(AMQP_VALUE, test_sasl_server_mechanism);
	MOCK_STATIC_METHOD_2(, int, amqpvalue_get_symbol, AMQP_VALUE, value, const char**, symbol_value)
	MOCK_METHOD_END(int, 0);

	/* consumer mocks */
	MOCK_STATIC_METHOD_3(, void, test_on_bytes_received, void*, context, const unsigned char*, buffer, size_t, size);
	MOCK_VOID_METHOD_END();
	MOCK_STATIC_METHOD_2(, void, test_on_io_open_complete, void*, context, IO_OPEN_RESULT, io_open_result)
	MOCK_VOID_METHOD_END();
	MOCK_STATIC_METHOD_1(, void, test_on_io_error, void*, context)
	MOCK_VOID_METHOD_END();
	MOCK_STATIC_METHOD_2(, void, test_on_send_complete, void*, context, IO_SEND_RESULT, send_result)
	MOCK_VOID_METHOD_END();

    /*mocks for OptionHandler*/
    MOCK_STATIC_METHOD_3(,OPTIONHANDLER_HANDLE, OptionHandler_Create, pfCloneOption, cloneOption, pfDestroyOption, destroyOption, pfSetOption, setOption)
    MOCK_METHOD_END(OPTIONHANDLER_HANDLE, (OPTIONHANDLER_HANDLE)NULL);
};

extern "C"
{
	DECLARE_GLOBAL_MOCK_METHOD_1(saslclientio_mocks, , void*, amqpalloc_malloc, size_t, size);
	DECLARE_GLOBAL_MOCK_METHOD_1(saslclientio_mocks, , void, amqpalloc_free, void*, ptr);

	DECLARE_GLOBAL_MOCK_METHOD_2(saslclientio_mocks, , FRAME_CODEC_HANDLE, frame_codec_create, ON_FRAME_CODEC_ERROR, on_frame_codec_error, void*, callback_context);
	DECLARE_GLOBAL_MOCK_METHOD_1(saslclientio_mocks, , void, frame_codec_destroy, FRAME_CODEC_HANDLE, frame_codec);
	DECLARE_GLOBAL_MOCK_METHOD_3(saslclientio_mocks, , int, frame_codec_receive_bytes, FRAME_CODEC_HANDLE, frame_codec, const unsigned char*, buffer, size_t, size)
	DECLARE_GLOBAL_MOCK_METHOD_4(saslclientio_mocks, , int, frame_codec_subscribe, FRAME_CODEC_HANDLE, frame_codec, uint8_t, type, ON_FRAME_RECEIVED, frame_received_callback, void*, callback_context);
	DECLARE_GLOBAL_MOCK_METHOD_2(saslclientio_mocks, , int, frame_codec_unsubscribe, FRAME_CODEC_HANDLE, frame_codec, uint8_t, type);
	DECLARE_GLOBAL_MOCK_METHOD_8(saslclientio_mocks, , int, frame_codec_encode_frame, FRAME_CODEC_HANDLE, frame_codec, uint8_t, type, const PAYLOAD*, payloads, size_t, payload_count, const unsigned char*, type_specific_bytes, uint32_t, type_specific_size, ON_BYTES_ENCODED, on_bytes_encoded, void*, callback_context);

	DECLARE_GLOBAL_MOCK_METHOD_4(saslclientio_mocks, , SASL_FRAME_CODEC_HANDLE, sasl_frame_codec_create, FRAME_CODEC_HANDLE, frame_codec, ON_SASL_FRAME_RECEIVED, on_sasl_frame_received, ON_SASL_FRAME_CODEC_ERROR, on_sasl_frame_codec_error, void*, callback_context);
	DECLARE_GLOBAL_MOCK_METHOD_1(saslclientio_mocks, , void, sasl_frame_codec_destroy, SASL_FRAME_CODEC_HANDLE, sasl_frame_codec);
	DECLARE_GLOBAL_MOCK_METHOD_4(saslclientio_mocks, , int, sasl_frame_codec_encode_frame, SASL_FRAME_CODEC_HANDLE, sasl_frame_codec, const AMQP_VALUE, sasl_frame_value, ON_BYTES_ENCODED, on_bytes_encoded, void*, callback_context)

	DECLARE_GLOBAL_MOCK_METHOD_7(saslclientio_mocks, , int, xio_open, XIO_HANDLE, xio, ON_IO_OPEN_COMPLETE, on_io_open_complete, void*, on_io_open_complete_context, ON_BYTES_RECEIVED, on_bytes_received, void*, on_bytes_received_context, ON_IO_ERROR, on_io_error, void*, on_io_error_context);
	DECLARE_GLOBAL_MOCK_METHOD_3(saslclientio_mocks, , int, xio_close, XIO_HANDLE, xio, ON_IO_CLOSE_COMPLETE, on_io_close_complete, void*, callback_context);
	DECLARE_GLOBAL_MOCK_METHOD_5(saslclientio_mocks, , int, xio_send, XIO_HANDLE, xio, const void*, buffer, size_t, size, ON_SEND_COMPLETE, on_send_complete, void*, callback_context);
    DECLARE_GLOBAL_MOCK_METHOD_1(saslclientio_mocks, , void, xio_dowork, XIO_HANDLE, xio);
    DECLARE_GLOBAL_MOCK_METHOD_3(saslclientio_mocks, , int, xio_setoption, XIO_HANDLE, xio, const char*, optionName, const void*, value);

	DECLARE_GLOBAL_MOCK_METHOD_2(saslclientio_mocks, , int, saslmechanism_get_init_bytes, SASL_MECHANISM_HANDLE, sasl_mechanism, SASL_MECHANISM_BYTES*, init_bytes);
	DECLARE_GLOBAL_MOCK_METHOD_1(saslclientio_mocks, , const char*, saslmechanism_get_mechanism_name, SASL_MECHANISM_HANDLE, sasl_mechanism);
	DECLARE_GLOBAL_MOCK_METHOD_3(saslclientio_mocks, , int, saslmechanism_challenge, SASL_MECHANISM_HANDLE, sasl_mechanism, const SASL_MECHANISM_BYTES*, challenge_bytes, SASL_MECHANISM_BYTES*, response_bytes);

	DECLARE_GLOBAL_MOCK_METHOD_1(saslclientio_mocks, , char*, amqpvalue_to_string, AMQP_VALUE, amqp_value)

	DECLARE_GLOBAL_MOCK_METHOD_1(saslclientio_mocks, , void, amqpvalue_destroy, AMQP_VALUE, value);
	DECLARE_GLOBAL_MOCK_METHOD_1(saslclientio_mocks, , AMQP_VALUE, amqpvalue_get_inplace_descriptor, AMQP_VALUE, value);
	DECLARE_GLOBAL_MOCK_METHOD_2(saslclientio_mocks, , int, amqpvalue_get_array_item_count, AMQP_VALUE, value, uint32_t*, size);
	DECLARE_GLOBAL_MOCK_METHOD_2(saslclientio_mocks, , AMQP_VALUE, amqpvalue_get_array_item, AMQP_VALUE, value, uint32_t, index);
	DECLARE_GLOBAL_MOCK_METHOD_2(saslclientio_mocks, , int, amqpvalue_get_symbol, AMQP_VALUE, value, const char**, symbol_value);

	DECLARE_GLOBAL_MOCK_METHOD_3(saslclientio_mocks, , void, test_on_bytes_received, void*, context, const unsigned char*, buffer, size_t, size);
	DECLARE_GLOBAL_MOCK_METHOD_2(saslclientio_mocks, , void, test_on_io_open_complete, void*, context, IO_OPEN_RESULT, io_open_result);
	DECLARE_GLOBAL_MOCK_METHOD_1(saslclientio_mocks, , void, test_on_io_error, void*, context);
	DECLARE_GLOBAL_MOCK_METHOD_2(saslclientio_mocks, , void, test_on_send_complete, void*, context, IO_SEND_RESULT, send_result);
    DECLARE_GLOBAL_MOCK_METHOD_3(saslclientio_mocks, , OPTIONHANDLER_HANDLE, OptionHandler_Create, pfCloneOption, cloneOption, pfDestroyOption, destroyOption, pfSetOption, setOption)
}

MICROMOCK_MUTEX_HANDLE test_serialize_mutex;

BEGIN_TEST_SUITE(saslclientio_ut)

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
	if (frame_codec_received_bytes != NULL)
	{
		free(frame_codec_received_bytes);
		frame_codec_received_bytes = NULL;
	}
	frame_codec_received_byte_count = 0;

	if (io_send_bytes != NULL)
	{
		free(io_send_bytes);
		io_send_bytes = NULL;
	}
	io_send_byte_count = 0;

	if (!MicroMockReleaseMutex(test_serialize_mutex))
	{
		ASSERT_FAIL("Could not release test serialization mutex.");
	}
}

/* saslclientio_create */

/* Tests_SRS_SASLCLIENTIO_01_004: [saslclientio_create shall return on success a non-NULL handle to a new SASL client IO instance.] */
/* Tests_SRS_SASLCLIENTIO_01_089: [saslclientio_create shall create a frame_codec to be used for encoding/decoding frames bycalling frame_codec_create and passing the underlying_io as argument.] */
/* Tests_SRS_SASLCLIENTIO_01_084: [saslclientio_create shall create a sasl_frame_codec to be used for SASL frame encoding/decoding by calling sasl_frame_codec_create and passing the just created frame_codec as argument.] */
TEST_FUNCTION(saslclientio_create_with_valid_args_succeeds)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
	EXPECTED_CALL(mocks, frame_codec_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
	EXPECTED_CALL(mocks, sasl_frame_codec_create(test_frame_codec, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.ValidateArgument(1);

	// act
	CONCRETE_IO_HANDLE result = saslclientio_create(&saslclientio_config);

	// assert
	ASSERT_IS_NOT_NULL(result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(result);
}

/* Tests_SRS_SASLCLIENTIO_01_005: [If xio_create_parameters is NULL, saslclientio_create shall fail and return NULL.] */
TEST_FUNCTION(saslclientio_create_with_NULL_config_fails)
{
	// arrange
	saslclientio_mocks mocks;

	// act
	CONCRETE_IO_HANDLE result = saslclientio_create(NULL);

	// assert
	ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASLCLIENTIO_01_006: [If memory cannot be allocated for the new instance, saslclientio_create shall fail and return NULL.] */
TEST_FUNCTION(when_allocating_memory_for_the_new_instance_fails_then_saslclientio_create_fails)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG))
		.SetReturn((void*)NULL);

	// act
	CONCRETE_IO_HANDLE result = saslclientio_create(&saslclientio_config);

	// assert
	ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASLCLIENTIO_01_090: [If frame_codec_create fails, then saslclientio_create shall fail and return NULL.] */
TEST_FUNCTION(when_creating_the_frame_codec_fails_then_saslclientio_create_fails)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
	EXPECTED_CALL(mocks, frame_codec_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.SetReturn((FRAME_CODEC_HANDLE)NULL);
	EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));

	// act
	CONCRETE_IO_HANDLE result = saslclientio_create(&saslclientio_config);

	// assert
	ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASLCLIENTIO_01_085: [If sasl_frame_codec_create fails, then saslclientio_create shall fail and return NULL.] */
TEST_FUNCTION(when_creating_the_sasl_frame_codec_fails_then_saslclientio_create_fails)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
	EXPECTED_CALL(mocks, frame_codec_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
	EXPECTED_CALL(mocks, sasl_frame_codec_create(test_frame_codec, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.ValidateArgument(1)
		.SetReturn((SASL_FRAME_CODEC_HANDLE)NULL);
	STRICT_EXPECTED_CALL(mocks, frame_codec_destroy(test_frame_codec));
	EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));

	// act
	CONCRETE_IO_HANDLE result = saslclientio_create(&saslclientio_config);

	// assert
	ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASLCLIENTIO_01_092: [If any of the sasl_mechanism or underlying_io members of the configuration structure are NULL, saslclientio_create shall fail and return NULL.] */
TEST_FUNCTION(saslclientio_create_with_a_NULL_underlying_io_fails)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { NULL, test_sasl_mechanism };

	// act
	CONCRETE_IO_HANDLE result = saslclientio_create(&saslclientio_config);

	// assert
	ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASLCLIENTIO_01_092: [If any of the sasl_mechanism or underlying_io members of the configuration structure are NULL, saslclientio_create shall fail and return NULL.] */
TEST_FUNCTION(saslclientio_create_with_a_NULL_sasl_mechanism_fails)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, NULL };

	// act
	CONCRETE_IO_HANDLE result = saslclientio_create(&saslclientio_config);

	// assert
	ASSERT_IS_NULL(result);
}

/* saslclientio_destroy */

/* Tests_SRS_SASLCLIENTIO_01_007: [saslclientio_destroy shall free all resources associated with the SASL client IO handle.]  */
/* Tests_SRS_SASLCLIENTIO_01_086: [saslclientio_destroy shall destroy the sasl_frame_codec created in saslclientio_create by calling sasl_frame_codec_destroy.] */
/* Tests_SRS_SASLCLIENTIO_01_091: [saslclientio_destroy shall destroy the frame_codec created in saslclientio_create by calling frame_codec_destroy.] */
TEST_FUNCTION(saslclientio_destroy_frees_the_resources_allocated_in_create)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config);
	mocks.ResetAllCalls();

	EXPECTED_CALL(mocks, frame_codec_destroy(test_frame_codec));
	EXPECTED_CALL(mocks, sasl_frame_codec_destroy(test_sasl_frame_codec));
	EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));

	// act
	saslclientio_destroy(sasl_io);

	// assert
	// no explicit assert, uMock checks the calls
}

/* Tests_SRS_SASLCLIENTIO_01_008: [If the argument sasl_io is NULL, saslclientio_destroy shall do nothing.] */
TEST_FUNCTION(saslclientio_destroy_with_NULL_argument_does_nothing)
{
	// arrange
	saslclientio_mocks mocks;

	// act
	saslclientio_destroy(NULL);

	// assert
	// no explicit assert, uMock checks the calls
}

/* saslclientio_open */

#if 0
/* Tests_SRS_SASLCLIENTIO_01_009: [saslclientio_open shall call xio_open on the underlying_io passed to saslclientio_create.] */
/* Tests_SRS_SASLCLIENTIO_01_010: [On success, saslclientio_open shall return 0.] */
/* Tests_SRS_SASLCLIENTIO_01_013: [saslclientio_open shall pass to xio_open a callback for receiving bytes and a state changed callback for the underlying_io state changes.] */
TEST_FUNCTION(saslclientio_open_with_valid_args_succeeds)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	mocks.ResetAllCalls();

	EXPECTED_CALL(mocks, xio_open(test_underlying_io, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.ValidateArgument(1);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_OPENING, IO_STATE_NOT_OPEN));

	// act
	int result = saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_011: [If any of the sasl_io or on_bytes_received arguments is NULL, saslclientio_open shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_open_with_NULL_sasl_io_handle_fails)
{
	// arrange
	saslclientio_mocks mocks;

	// act
	int result = saslclientio_open(NULL, test_on_bytes_received, test_on_io_state_changed, test_context);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_SASLCLIENTIO_01_011: [If any of the sasl_io or on_bytes_received arguments is NULL, saslclientio_open shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_open_with_NULL_on_bytes_received_fails)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	mocks.ResetAllCalls();

	// act
	int result = saslclientio_open(sasl_io, NULL, test_on_io_state_changed, test_context);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_012: [If the open of the underlying_io fails, saslclientio_open shall fail and return non-zero value.] */
TEST_FUNCTION(when_opening_the_underlying_io_fails_saslclientio_open_fails)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	mocks.ResetAllCalls();

	EXPECTED_CALL(mocks, xio_open(test_underlying_io, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.ValidateArgument(1)
		.SetReturn(1);

	// act
	int result = saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* saslclientio_close */

/* Tests_SRS_SASLCLIENTIO_01_015: [saslclientio_close shall close the underlying io handle passed in saslclientio_create by calling xio_close.] */
/* Tests_SRS_SASLCLIENTIO_01_098: [saslclientio_close shall only perform the close if the state is OPEN, OPENING or ERROR.] */
/* Tests_SRS_SASLCLIENTIO_01_016: [On success, saslclientio_close shall return 0.] */
TEST_FUNCTION(saslclientio_close_when_the_io_state_is_OPENING_closes_the_underlying_io)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, xio_close(test_underlying_io));
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_NOT_OPEN, IO_STATE_OPENING));

	// act
	int result = saslclientio_close(sasl_io);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

static void setup_successful_sasl_handshake(saslclientio_mocks* mocks, amqp_definitions_mocks* definitions_mocks, CONCRETE_IO_HANDLE sasl_io)
{
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL((*definitions_mocks), is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL((*mocks), amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_outcome, sizeof(test_sasl_outcome));
	definitions_mocks->ResetAllCalls();
	STRICT_EXPECTED_CALL((*definitions_mocks), is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL((*definitions_mocks), is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL((*definitions_mocks), is_sasl_outcome_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	sasl_code sasl_outcome_code = sasl_code_ok;
	STRICT_EXPECTED_CALL((*definitions_mocks), amqpvalue_get_sasl_outcome(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_outcome_handle, sizeof(test_sasl_outcome_handle));
	STRICT_EXPECTED_CALL((*definitions_mocks), sasl_outcome_get_code(test_sasl_outcome_handle, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &sasl_outcome_code, sizeof(sasl_outcome_code));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
}

/* Tests_SRS_SASLCLIENTIO_01_015: [saslclientio_close shall close the underlying io handle passed in saslclientio_create by calling xio_close.] */
/* Tests_SRS_SASLCLIENTIO_01_098: [saslclientio_close shall only perform the close if the state is OPEN, OPENING or ERROR.] */
/* Tests_SRS_SASLCLIENTIO_01_016: [On success, saslclientio_close shall return 0.] */
TEST_FUNCTION(saslclientio_close_when_the_io_state_is_OPEN_closes_the_underlying_io)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	definitions_mocks.ResetAllCalls();
	mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, xio_close(test_underlying_io));
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_NOT_OPEN, IO_STATE_OPEN));

	// act
	int result = saslclientio_close(sasl_io);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_015: [saslclientio_close shall close the underlying io handle passed in saslclientio_create by calling xio_close.] */
/* Tests_SRS_SASLCLIENTIO_01_098: [saslclientio_close shall only perform the close if the state is OPEN, OPENING or ERROR.] */
/* Tests_SRS_SASLCLIENTIO_01_016: [On success, saslclientio_close shall return 0.] */
TEST_FUNCTION(saslclientio_close_when_the_io_state_is_ERROR_closes_the_underlying_io)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_NOT_OPEN);
	mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, xio_close(test_underlying_io));
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_NOT_OPEN, IO_STATE_ERROR));

	// act
	int result = saslclientio_close(sasl_io);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_097: [If saslclientio_close is called when the IO is in the IO_STATE_NOT_OPEN state, no call to the underlying IO shall be made and saslclientio_close shall return 0.] */
TEST_FUNCTION(saslclientio_close_when_the_io_state_is_NOT_OPEN_succeeds_without_calling_the_underlying_io)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	mocks.ResetAllCalls();

	// act
	int result = saslclientio_close(sasl_io);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_097: [If saslclientio_close is called when the IO is in the IO_STATE_NOT_OPEN state, no call to the underlying IO shall be made and saslclientio_close shall return 0.] */
TEST_FUNCTION(saslclientio_close_when_the_io_state_is_NOT_OPEN_due_to_a_previous_close_succeeds_without_calling_the_underlying_io)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_NOT_OPEN);
	(void)saslclientio_close(sasl_io);
	mocks.ResetAllCalls();

	// act
	int result = saslclientio_close(sasl_io);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_017: [If sasl_io is NULL, saslclientio_close shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_close_with_NULL_sasl_io_fails)
{
	// arrange
	saslclientio_mocks mocks;

	// act
	int result = saslclientio_close(NULL);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_SASLCLIENTIO_01_018: [If xio_close fails, then saslclientio_close shall return a non-zero value.] */
TEST_FUNCTION(when_xio_close_fails_saslclientio_close_fails)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, xio_close(test_underlying_io))
		.SetReturn(1);

	// act
	int result = saslclientio_close(sasl_io);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* saslclientio_send */

/* Tests_SRS_SASLCLIENTIO_01_019: [If saslclientio_send is called while the SASL client IO state is not IO_STATE_OPEN, saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_send_when_io_state_is_NOT_OPEN_fails)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	mocks.ResetAllCalls();
	unsigned char test_buffer[] = { 0x42 };

	// act
	int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_019: [If saslclientio_send is called while the SASL client IO state is not IO_STATE_OPEN, saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_send_when_io_state_is_OPENING_fails)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();
	unsigned char test_buffer[] = { 0x42 };

	// act
	int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_019: [If saslclientio_send is called while the SASL client IO state is not IO_STATE_OPEN, saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_send_when_io_state_is_ERROR_fails)
{
	// arrange
	saslclientio_mocks mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_NOT_OPEN);
	mocks.ResetAllCalls();
	unsigned char test_buffer[] = { 0x42 };

	// act
	int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_020: [If the SASL client IO state is IO_STATE_OPEN, saslclientio_send shall call xio_send on the underlying_io passed to saslclientio_create, while passing as arguments the buffer, size, on_send_complete and callback_context.] */
/* Tests_SRS_SASLCLIENTIO_01_021: [On success, saslclientio_send shall return 0.] */
TEST_FUNCTION(saslclientio_send_when_io_state_is_OPEN_calls_the_underlying_io_send)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();
	unsigned char test_buffer[] = { 0x42 };

	STRICT_EXPECTED_CALL(mocks, xio_send(test_underlying_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context))
		.ValidateArgumentBuffer(2, test_buffer, sizeof(test_buffer));

	// act
	int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_020: [If the SASL client IO state is IO_STATE_OPEN, saslclientio_send shall call xio_send on the underlying_io passed to saslclientio_create, while passing as arguments the buffer, size, on_send_complete and callback_context.] */
/* Tests_SRS_SASLCLIENTIO_01_021: [On success, saslclientio_send shall return 0.] */
TEST_FUNCTION(saslclientio_send_with_NULL_on_send_complete_passes_NULL_to_the_underlying_io)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();
	unsigned char test_buffer[] = { 0x42 };

	STRICT_EXPECTED_CALL(mocks, xio_send(test_underlying_io, test_buffer, sizeof(test_buffer), NULL, test_context))
		.ValidateArgumentBuffer(2, test_buffer, sizeof(test_buffer));

	// act
	int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), NULL, test_context);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_020: [If the SASL client IO state is IO_STATE_OPEN, saslclientio_send shall call xio_send on the underlying_io passed to saslclientio_create, while passing as arguments the buffer, size, on_send_complete and callback_context.] */
/* Tests_SRS_SASLCLIENTIO_01_021: [On success, saslclientio_send shall return 0.] */
TEST_FUNCTION(saslclientio_send_with_NULL_on_send_complete_context_passes_NULL_to_the_underlying_io)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();
	unsigned char test_buffer[] = { 0x42 };

	STRICT_EXPECTED_CALL(mocks, xio_send(test_underlying_io, test_buffer, sizeof(test_buffer), test_on_send_complete, NULL))
		.ValidateArgumentBuffer(2, test_buffer, sizeof(test_buffer));

	// act
	int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), test_on_send_complete, NULL);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_022: [If the saslio or buffer argument is NULL, saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_send_with_NULL_sasl_io_fails)
{
	// arrange
	saslclientio_mocks mocks;
	unsigned char test_buffer[] = { 0x42 };

	// act
	int result = saslclientio_send(NULL, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_SASLCLIENTIO_01_022: [If the saslio or buffer argument is NULL, saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_send_with_NULL_buffer_fails)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();
	unsigned char test_buffer[] = { 0x42 };

	// act
	int result = saslclientio_send(sasl_io, NULL, sizeof(test_buffer), test_on_send_complete, test_context);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_023: [If size is 0, saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_send_with_0_size_fails)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();
	unsigned char test_buffer[] = { 0x42 };

	// act
	int result = saslclientio_send(sasl_io, test_buffer, 0, test_on_send_complete, test_context);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_024: [If the call to xio_send fails, then saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(when_the_underlying_xio_send_fails_then_saslclientio_send_fails)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();
	unsigned char test_buffer[] = { 0x42 };

	STRICT_EXPECTED_CALL(mocks, xio_send(test_underlying_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context))
		.ValidateArgumentBuffer(2, test_buffer, sizeof(test_buffer))
		.SetReturn(1);

	// act
	int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* saslclientio_dowork */

/* Tests_SRS_SASLCLIENTIO_01_025: [saslclientio_dowork shall call the xio_dowork on the underlying_io passed in saslclientio_create.] */
TEST_FUNCTION(when_the_io_state_is_OPEN_xio_dowork_calls_the_underlying_IO)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, xio_dowork(test_underlying_io));

	// act
	saslclientio_dowork(sasl_io);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_025: [saslclientio_dowork shall call the xio_dowork on the underlying_io passed in saslclientio_create.] */
TEST_FUNCTION(when_the_io_state_is_OPENING_xio_dowork_calls_the_underlying_IO)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, xio_dowork(test_underlying_io));

	// act
	saslclientio_dowork(sasl_io);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_025: [saslclientio_dowork shall call the xio_dowork on the underlying_io passed in saslclientio_create.] */
TEST_FUNCTION(when_the_io_state_is_NOT_OPEN_xio_dowork_does_nothing)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saslclientio_dowork(sasl_io);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_025: [saslclientio_dowork shall call the xio_dowork on the underlying_io passed in saslclientio_create.] */
TEST_FUNCTION(when_the_io_state_is_ERROR_xio_dowork_does_nothing)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_NOT_OPEN);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saslclientio_dowork(sasl_io);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_026: [If the sasl_io argument is NULL, saslclientio_dowork shall do nothing.] */
TEST_FUNCTION(saslclientio_dowork_with_NULL_sasl_io_handle_does_nothing)
{
	// arrange
	saslclientio_mocks mocks;

	// act
	saslclientio_dowork(NULL);

	// assert
	// no explicit assert, uMock checks the calls
}

/* saslclientio_get_interface_description */

/* Tests_SRS_SASLCLIENTIO_01_087: [saslclientio_get_interface_description shall return a pointer to an IO_INTERFACE_DESCRIPTION structure that contains pointers to the functions: saslclientio_create, saslclientio_destroy, saslclientio_open, saslclientio_close, saslclientio_send and saslclientio_dowork.] */
TEST_FUNCTION(saslclientio_get_interface_description_returns_the_saslclientio_interface_functions)
{
	// arrange
	saslclientio_mocks mocks;

	// act
	const IO_INTERFACE_DESCRIPTION* result = saslclientio_get_interface_description();

	// assert
	ASSERT_ARE_EQUAL(void_ptr, (void_ptr)saslclientio_create, (void_ptr)result->concrete_io_create);
	ASSERT_ARE_EQUAL(void_ptr, (void_ptr)saslclientio_destroy, (void_ptr)result->concrete_io_destroy);
	ASSERT_ARE_EQUAL(void_ptr, (void_ptr)saslclientio_open, (void_ptr)result->concrete_io_open);
	ASSERT_ARE_EQUAL(void_ptr, (void_ptr)saslclientio_close, (void_ptr)result->concrete_io_close);
	ASSERT_ARE_EQUAL(void_ptr, (void_ptr)saslclientio_send, (void_ptr)result->concrete_io_send);
	ASSERT_ARE_EQUAL(void_ptr, (void_ptr)saslclientio_dowork, (void_ptr)result->concrete_io_dowork);
}

/* on_bytes_received */

/* Tests_SRS_SASLCLIENTIO_01_027: [When the on_bytes_received callback passed to the underlying IO is called and the SASL client IO state is IO_STATE_OPEN, the bytes shall be indicated to the user of SASL client IO by calling the on_bytes_received that was passed in saslclientio_open.] */
TEST_FUNCTION(when_io_state_is_open_and_bytes_are_received_they_are_indicated_up)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char test_bytes[] = { 0x42, 0x43 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_bytes_received(test_context, test_bytes, sizeof(test_bytes)));

	// act
	saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_027: [When the on_bytes_received callback passed to the underlying IO is called and the SASL client IO state is IO_STATE_OPEN, the bytes shall be indicated to the user of SASL client IO by calling the on_bytes_received that was passed in saslclientio_open.] */
/* Tests_SRS_SASLCLIENTIO_01_029: [The context argument shall be set to the callback_context passed in saslclientio_open.] */
TEST_FUNCTION(when_io_state_is_open_and_bytes_are_received_and_context_passed_to_open_was_NULL_NULL_is_passed_as_context_to_the_on_bytes_received_call)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char test_bytes[] = { 0x42, 0x43 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, NULL);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_bytes_received(NULL, test_bytes, sizeof(test_bytes)));

	// act
	saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_028: [If buffer is NULL or size is zero, nothing should be indicated as received and the saslio state shall be switched to ERROR the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_io_state_is_open_and_bytes_are_received_with_bytes_NULL_nothing_is_indicated_up)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char test_bytes[] = { 0x42, 0x43 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

	// act
	saved_on_bytes_received(saved_io_callback_context, NULL, sizeof(test_bytes));

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_028: [If buffer is NULL or size is zero, nothing should be indicated as received and the saslio state shall be switched to ERROR the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_io_state_is_open_and_bytes_are_received_with_size_zero_nothing_is_indicated_up)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char test_bytes[] = { 0x42, 0x43 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

	// act
	saved_on_bytes_received(saved_io_callback_context, test_bytes, 0);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_031: [If bytes are received when the SASL client IO state is IO_STATE_ERROR, SASL client IO shall do nothing.]  */
TEST_FUNCTION(when_io_state_is_ERROR_and_bytes_are_received_nothing_is_done)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	unsigned char test_bytes[] = { 0x42, 0x43 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_NOT_OPEN);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_on_bytes_received(saved_io_callback_context, test_bytes, 1);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_030: [If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.] */
/* Tests_SRS_SASLCLIENTIO_01_003: [Other than using a protocol id of three, the exchange of SASL layer headers follows the same rules specified in the version negotiation section of the transport specification (See Part 2: section 2.2).] */
TEST_FUNCTION(when_io_state_is_opening_and_1_byte_is_received_it_is_used_for_the_header)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_on_bytes_received(saved_io_callback_context, sasl_header, 1);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_030: [If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.] */
/* Tests_SRS_SASLCLIENTIO_01_003: [Other than using a protocol id of three, the exchange of SASL layer headers follows the same rules specified in the version negotiation section of the transport specification (See Part 2: section 2.2).] */
/* Tests_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
TEST_FUNCTION(when_io_state_is_opening_and_1_bad_byte_is_received_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	unsigned char test_bytes[] = { 0x42 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_030: [If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.] */
/* Tests_SRS_SASLCLIENTIO_01_003: [Other than using a protocol id of three, the exchange of SASL layer headers follows the same rules specified in the version negotiation section of the transport specification (See Part 2: section 2.2).] */
/* Tests_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
TEST_FUNCTION(when_io_state_is_opening_and_the_last_header_byte_is_bad_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	unsigned char test_bytes[] = { 'A', 'M', 'Q', 'P', 3, 1, 0, 'x' };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_030: [If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.] */
/* Tests_SRS_SASLCLIENTIO_01_003: [Other than using a protocol id of three, the exchange of SASL layer headers follows the same rules specified in the version negotiation section of the transport specification (See Part 2: section 2.2).] */
/* Tests_SRS_SASLCLIENTIO_01_001: [To establish a SASL layer, each peer MUST start by sending a protocol header.] */
/* Tests_SRS_SASLCLIENTIO_01_105: [start header exchange] */
/* Tests_SRS_SASLCLIENTIO_01_078: [SASL client IO shall start the header exchange by sending the SASL header.] */
/* Tests_SRS_SASLCLIENTIO_01_095: [Sending the header shall be done by using xio_send.] */
TEST_FUNCTION(when_underlying_IO_switches_the_state_to_OPEN_the_SASL_header_is_sent)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	EXPECTED_CALL(mocks, xio_send(test_underlying_io, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.ValidateArgument(1).ExpectedAtLeastTimes(1);

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();
	stringify_bytes(io_send_bytes, io_send_byte_count, actual_stringified_io);
	stringify_bytes(sasl_header, sizeof(sasl_header), expected_stringified_io);
	ASSERT_ARE_EQUAL(char_ptr, expected_stringified_io, actual_stringified_io);

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
TEST_FUNCTION(when_sending_the_header_fails_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	EXPECTED_CALL(mocks, xio_send(test_underlying_io, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.ValidateArgument(1)
		.SetReturn(1);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_030: [If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.] */
/* Tests_SRS_SASLCLIENTIO_01_003: [Other than using a protocol id of three, the exchange of SASL layer headers follows the same rules specified in the version negotiation section of the transport specification (See Part 2: section 2.2).] */
/* Tests_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
TEST_FUNCTION(when_a_bad_header_is_received_after_a_good_one_has_been_sent_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	unsigned char test_bytes[] = { 'A', 'M', 'Q', 'P', 3, 1, 0, 'x' };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

	// assert
	mocks.AssertActualAndExpectedCalls();
	stringify_bytes(io_send_bytes, io_send_byte_count, actual_stringified_io);
	stringify_bytes(sasl_header, sizeof(sasl_header), expected_stringified_io);
	ASSERT_ARE_EQUAL(char_ptr, expected_stringified_io, actual_stringified_io);

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_030: [If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.] */
/* Tests_SRS_SASLCLIENTIO_01_003: [Other than using a protocol id of three, the exchange of SASL layer headers follows the same rules specified in the version negotiation section of the transport specification (See Part 2: section 2.2).] */
TEST_FUNCTION(when_a_good_header_is_received_after_the_header_has_been_sent_yields_no_error)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_030: [If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.] */
/* Tests_SRS_SASLCLIENTIO_01_067: [The SASL frame exchange shall be started as soon as the SASL header handshake is done.] */
/* Tests_SRS_SASLCLIENTIO_01_068: [During the SASL frame exchange that constitutes the handshake the received bytes from the underlying IO shall be fed to the frame_codec instance created in saslclientio_create by calling frame_codec_receive_bytes.] */
TEST_FUNCTION(when_one_byte_is_received_after_header_handshake_it_is_sent_to_the_frame_codec)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	unsigned char test_bytes[] = { 0x42 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	EXPECTED_CALL(mocks, frame_codec_receive_bytes(test_frame_codec, IGNORED_PTR_ARG, IGNORED_NUM_ARG))
		.ValidateArgument(1).ExpectedAtLeastTimes(1);

	// act
	saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

	// assert
	mocks.AssertActualAndExpectedCalls();
	stringify_bytes(frame_codec_received_bytes, frame_codec_received_byte_count, actual_stringified_io);
	stringify_bytes(test_bytes, sizeof(test_bytes), expected_stringified_io);
	ASSERT_ARE_EQUAL(char_ptr, expected_stringified_io, actual_stringified_io);

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_088: [If frame_codec_receive_bytes fails, the state of SASL client IO shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_frame_codec_receive_bytes_fails_then_the_state_is_switched_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	unsigned char test_bytes[] = { 0x42 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	EXPECTED_CALL(mocks, frame_codec_receive_bytes(test_frame_codec, IGNORED_PTR_ARG, IGNORED_NUM_ARG))
		.ValidateArgument(1)
		.SetReturn(1);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

	// assert
	mocks.AssertActualAndExpectedCalls();
	stringify_bytes(frame_codec_received_bytes, frame_codec_received_byte_count, actual_stringified_io);
	stringify_bytes(test_bytes, sizeof(test_bytes), expected_stringified_io);
	ASSERT_ARE_EQUAL(char_ptr, expected_stringified_io, actual_stringified_io);

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_101: [raise ERROR] */
TEST_FUNCTION(ERROR_received_in_the_state_OPENING_sets_the_state_to_ERROR_and_triggers_callback)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_NOT_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_102: [raise ERROR] */
TEST_FUNCTION(ERROR_received_in_the_state_OPEN_sets_the_state_to_ERROR_and_triggers_callback)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_103: [do nothing] */
TEST_FUNCTION(ERROR_received_in_the_state_ERROR_does_nothing)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_109: [do nothing] */
TEST_FUNCTION(OPENING_received_in_the_state_OPENING_does_nothing)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPENING, IO_STATE_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_110: [raise ERROR] */
TEST_FUNCTION(OPENING_received_in_the_state_OPEN_raises_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPENING, IO_STATE_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_111: [do nothing] */
TEST_FUNCTION(OPENING_received_in_the_state_ERROR_does_nothing)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPENING, IO_STATE_ERROR);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_114: [raise ERROR] */
TEST_FUNCTION(NOT_OPEN_received_in_the_state_OPENING_raises_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_NOT_OPEN, IO_STATE_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_113: [raise ERROR] */
TEST_FUNCTION(NOT_OPEN_received_in_the_state_OPEN_raises_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_NOT_OPEN, IO_STATE_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_112: [do nothing] */
TEST_FUNCTION(NOT_OPEN_received_in_the_state_ERROR_does_nothing)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_NOT_OPEN, IO_STATE_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_104: [do nothing] */
TEST_FUNCTION(OPEN_received_in_the_state_NOT_OPEN_does_nothing)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_NOT_OPEN, IO_STATE_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_106: [do nothing] */
TEST_FUNCTION(OPEN_received_in_the_state_OPEN_does_nothing)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_OPENING);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_107: [do nothing] */
TEST_FUNCTION(OPEN_received_in_the_state_ERROR_does_nothing)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_OPENING);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_077: [If sending the SASL header fails, the SASL client IO state shall be set to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_sending_the_header_with_xio_send_fails_then_the_io_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	EXPECTED_CALL(mocks, xio_send(test_underlying_io, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.ValidateArgument(1).ExpectedAtLeastTimes(1).SetReturn(1);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_116: [Any underlying IO state changes to state OPEN after the header exchange has been started shall trigger no action.] */
TEST_FUNCTION(when_underlying_io_sets_the_state_to_OPEN_after_the_header_exchange_was_started_nothing_shall_be_done)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_OPEN);

	// assert
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_032: [The peer acting as the SASL server MUST announce supported authentication mechanisms using the sasl-mechanisms frame.] */
/* Tests_SRS_SASLCLIENTIO_01_040: [The peer playing the role of the SASL client and the peer playing the role of the SASL server MUST correspond to the TCP client and server respectively.] */
/* Tests_SRS_SASLCLIENTIO_01_070: [When a frame needs to be sent as part of the SASL handshake frame exchange, the send shall be done by calling sasl_frame_codec_encode_frame.] */
/* Tests_SRS_SASLCLIENTIO_01_034: [<-- SASL-MECHANISMS] */
/* Tests_SRS_SASLCLIENTIO_01_035: [SASL-INIT -->] */
/* Tests_SRS_SASLCLIENTIO_01_033: [The partner MUST then choose one of the supported mechanisms and initiate a sasl exchange.] */
/* Tests_SRS_SASLCLIENTIO_01_054: [Selects the sasl mechanism and provides the initial response if needed.] */
/* Tests_SRS_SASLCLIENTIO_01_045: [The name of the SASL mechanism used for the SASL exchange.] */
/* Tests_SRS_SASLCLIENTIO_01_070: [When a frame needs to be sent as part of the SASL handshake frame exchange, the send shall be done by calling sasl_frame_codec_encode_frame.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_after_the_header_exchange_a_sasl_init_frame_is_send_with_the_selected_mechanism)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	uint32_t mechanisms_count = 1;
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item(sasl_server_mechanisms, 0));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	STRICT_EXPECTED_CALL(mocks, saslmechanism_get_mechanism_name(test_sasl_mechanism));
	SASL_MECHANISM_BYTES init_bytes = { NULL, 0 };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_create(test_mechanism));
	EXPECTED_CALL(mocks, saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes));
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_create_sasl_init(test_sasl_init_handle));
	STRICT_EXPECTED_CALL(mocks, sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_init_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.IgnoreArgument(3).IgnoreArgument(4);
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_destroy(test_sasl_init_handle));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_destroy(test_sasl_init_amqp_value));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_047: [A block of opaque data passed to the security mechanism.] */
/* Tests_SRS_SASLCLIENTIO_01_048: [The contents of this data are defined by the SASL security mechanism.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_a_sasl_init_frame_is_send_with_the_mechanism_name_and_the_init_bytes)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char test_init_bytes[] = { 0x42, 0x43 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	uint32_t mechanisms_count = 1;
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item(sasl_server_mechanisms, 0));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	STRICT_EXPECTED_CALL(mocks, saslmechanism_get_mechanism_name(test_sasl_mechanism));
	SASL_MECHANISM_BYTES init_bytes = { test_init_bytes, sizeof(test_init_bytes) };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_create(test_mechanism));
	EXPECTED_CALL(mocks, saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes));
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_create_sasl_init(test_sasl_init_handle));
	amqp_binary expected_creds = { test_init_bytes, sizeof(test_init_bytes) };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_set_initial_response(test_sasl_init_handle, expected_creds));
	STRICT_EXPECTED_CALL(mocks, sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_init_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.IgnoreArgument(3).IgnoreArgument(4);
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_destroy(test_sasl_init_handle));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_destroy(test_sasl_init_amqp_value));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_118: [If on_sasl_frame_received_callback is called in the OPENING state but the header exchange has not yet been completed, then the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_when_header_handshake_is_not_done_the_IO_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_117: [If on_sasl_frame_received_callback is called when the state of the IO is OPEN then the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_in_the_OPEN_state_the_IO_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_getting_the_descriptor_fails_the_IO_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value))
		.SetReturn((AMQP_VALUE)NULL);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_getting_the_mechanism_name_fails_the_IO_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	uint32_t mechanisms_count = 1;
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
	STRICT_EXPECTED_CALL(mocks, saslmechanism_get_mechanism_name(test_sasl_mechanism))
		.SetReturn((const char*)NULL);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_creating_the_sasl_init_value_fails_the_IO_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	uint32_t mechanisms_count = 1;
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item(sasl_server_mechanisms, 0));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	STRICT_EXPECTED_CALL(mocks, saslmechanism_get_mechanism_name(test_sasl_mechanism));
	SASL_MECHANISM_BYTES init_bytes = { NULL, 0 };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_create(test_mechanism))
		.SetReturn((SASL_INIT_HANDLE)NULL);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_getting_the_initial_bytes_fails_the_IO_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	uint32_t mechanisms_count = 1;
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item(sasl_server_mechanisms, 0));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	STRICT_EXPECTED_CALL(mocks, saslmechanism_get_mechanism_name(test_sasl_mechanism));
	SASL_MECHANISM_BYTES init_bytes = { NULL, 0 };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_create(test_mechanism));
	EXPECTED_CALL(mocks, saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes))
		.SetReturn(1);
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_destroy(test_sasl_init_handle));
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_getting_the_AMQP_VALUE_fails_the_IO_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	uint32_t mechanisms_count = 1;
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item(sasl_server_mechanisms, 0));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	STRICT_EXPECTED_CALL(mocks, saslmechanism_get_mechanism_name(test_sasl_mechanism));
	SASL_MECHANISM_BYTES init_bytes = { NULL, 0 };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_create(test_mechanism));
	EXPECTED_CALL(mocks, saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes));
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_create_sasl_init(test_sasl_init_handle))
		.SetReturn((AMQP_VALUE)NULL);
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_destroy(test_sasl_init_handle));
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_071: [If sasl_frame_codec_encode_frame fails, then the state of SASL client IO shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_encoding_the_sasl_frame_fails_the_IO_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	uint32_t mechanisms_count = 1;
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item(sasl_server_mechanisms, 0));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	STRICT_EXPECTED_CALL(mocks, saslmechanism_get_mechanism_name(test_sasl_mechanism));
	SASL_MECHANISM_BYTES init_bytes = { NULL, 0 };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_create(test_mechanism));
	EXPECTED_CALL(mocks, saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes));
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_create_sasl_init(test_sasl_init_handle));
	STRICT_EXPECTED_CALL(mocks, sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_init_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.IgnoreArgument(3).IgnoreArgument(4)
		.SetReturn(1);
	STRICT_EXPECTED_CALL(mocks, amqpvalue_destroy(test_sasl_init_amqp_value));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_destroy(test_sasl_init_handle));
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_071: [If sasl_frame_codec_encode_frame fails, then the state of SASL client IO shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_setting_the_init_bytes_fails_the_IO_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char test_init_bytes[] = { 0x42 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	uint32_t mechanisms_count = 1;
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item(sasl_server_mechanisms, 0));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	STRICT_EXPECTED_CALL(mocks, saslmechanism_get_mechanism_name(test_sasl_mechanism));
	SASL_MECHANISM_BYTES init_bytes = { test_init_bytes, sizeof(test_init_bytes) };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_create(test_mechanism));
	STRICT_EXPECTED_CALL(mocks, saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes));
	amqp_binary expected_creds = { test_init_bytes, sizeof(test_init_bytes) };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_set_initial_response(test_sasl_init_handle, expected_creds));
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_create_sasl_init(test_sasl_init_handle));
	STRICT_EXPECTED_CALL(mocks, sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_init_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.IgnoreArgument(3).IgnoreArgument(4)
		.SetReturn(1);
	STRICT_EXPECTED_CALL(mocks, amqpvalue_destroy(test_sasl_init_amqp_value));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_init_destroy(test_sasl_init_handle));
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_058: [This frame indicates the outcome of the SASL dialog.] */
/* Tests_SRS_SASLCLIENTIO_01_059: [Upon successful completion of the SASL dialog the security layer has been established] */
/* Tests_SRS_SASLCLIENTIO_01_060: [A reply-code indicating the outcome of the SASL dialog.] */
/* Tests_SRS_SASLCLIENTIO_01_062: [0 Connection authentication succeeded.] */
/* Tests_SRS_SASLCLIENTIO_01_038: [<-- SASL-OUTCOME] */
/* Tests_SRS_SASLCLIENTIO_01_072: [When the SASL handshake is complete, if the handshake is successful, the SASL client IO state shall be switched to IO_STATE_OPEN and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_outcome_frame_is_received_with_ok_the_SASL_IO_state_is_switched_to_OPEN)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL(mocks, amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_outcome, sizeof(test_sasl_outcome));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_outcome_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	sasl_code sasl_outcome_code = sasl_code_ok;
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_outcome(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_outcome_handle, sizeof(test_sasl_outcome_handle));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_outcome_get_code(test_sasl_outcome_handle, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &sasl_outcome_code, sizeof(sasl_outcome_code));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_outcome_destroy(test_sasl_outcome_handle));

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_OPEN, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

void when_an_outcome_with_error_code_is_received_the_SASL_IO_state_is_set_to_ERROR(sasl_code test_sasl_code)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL(mocks, amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_outcome, sizeof(test_sasl_outcome));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_outcome_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	sasl_code sasl_outcome_code = test_sasl_code;
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_outcome(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_outcome_handle, sizeof(test_sasl_outcome_handle));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_outcome_get_code(test_sasl_outcome_handle, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &sasl_outcome_code, sizeof(sasl_outcome_code));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_outcome_destroy(test_sasl_outcome_handle));

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_063: [1 Connection authentication failed due to an unspecified problem with the supplied credentials.] */
TEST_FUNCTION(when_a_SASL_outcome_frame_is_received_with_auth_error_code_the_SASL_IO_state_is_switched_to_ERROR)
{
	when_an_outcome_with_error_code_is_received_the_SASL_IO_state_is_set_to_ERROR(sasl_code_auth);
}

/* Tests_SRS_SASLCLIENTIO_01_064: [2 Connection authentication failed due to a system error.] */
TEST_FUNCTION(when_a_SASL_outcome_frame_is_received_with_sys_error_code_the_SASL_IO_state_is_switched_to_ERROR)
{
	when_an_outcome_with_error_code_is_received_the_SASL_IO_state_is_set_to_ERROR(sasl_code_sys);
}

/* Tests_SRS_SASLCLIENTIO_01_065: [3 Connection authentication failed due to a system error that is unlikely to be corrected without intervention.] */
TEST_FUNCTION(when_a_SASL_outcome_frame_is_received_with_sys_perm_error_code_the_SASL_IO_state_is_switched_to_ERROR)
{
	when_an_outcome_with_error_code_is_received_the_SASL_IO_state_is_set_to_ERROR(sasl_code_sys_perm);
}

/* Tests_SRS_SASLCLIENTIO_01_066: [4 Connection authentication failed due to a transient system error.] */
TEST_FUNCTION(when_a_SASL_outcome_frame_is_received_with_sys_temp_error_code_the_SASL_IO_state_is_switched_to_ERROR)
{
	when_an_outcome_with_error_code_is_received_the_SASL_IO_state_is_set_to_ERROR(sasl_code_sys_temp);
}

/* Tests_SRS_SASLCLIENTIO_01_032: [The peer acting as the SASL server MUST announce supported authentication mechanisms using the sasl-mechanisms frame.] */
TEST_FUNCTION(when_a_SASL_outcome_frame_is_received_before_mechanisms_the_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_outcome, sizeof(test_sasl_outcome));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_outcome_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_032: [The peer acting as the SASL server MUST announce supported authentication mechanisms using the sasl-mechanisms frame.] */
TEST_FUNCTION(when_a_SASL_challenge_is_received_before_mechanisms_the_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

static void setup_succesfull_challenge_response(saslclientio_mocks* mocks, amqp_definitions_mocks* definitions_mocks)
{

	STRICT_EXPECTED_CALL((*mocks), amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL((*definitions_mocks), is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL((*definitions_mocks), is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	STRICT_EXPECTED_CALL((*definitions_mocks), amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle));
	STRICT_EXPECTED_CALL((*definitions_mocks), sasl_challenge_get_challenge(test_sasl_challenge_handle, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &some_challenge_bytes, sizeof(some_challenge_bytes));
	STRICT_EXPECTED_CALL((*mocks), saslmechanism_challenge(test_sasl_mechanism, &sasl_mechanism_challenge_bytes, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(3, &sasl_mechanism_response_bytes, sizeof(sasl_mechanism_response_bytes));
	STRICT_EXPECTED_CALL((*definitions_mocks), sasl_response_create(response_binary_value));
	STRICT_EXPECTED_CALL((*definitions_mocks), amqpvalue_create_sasl_response(test_sasl_response_handle));
	STRICT_EXPECTED_CALL((*mocks), sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_response_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.IgnoreArgument(3).IgnoreArgument(4);
	STRICT_EXPECTED_CALL((*definitions_mocks), sasl_response_destroy(test_sasl_response_handle));
	STRICT_EXPECTED_CALL((*mocks), amqpvalue_destroy(test_sasl_response_amqp_value));
	STRICT_EXPECTED_CALL((*definitions_mocks), sasl_challenge_destroy(test_sasl_challenge_handle));
}

/* Tests_SRS_SASLCLIENTIO_01_052: [Send the SASL challenge data as defined by the SASL specification.] */
/* Tests_SRS_SASLCLIENTIO_01_053: [Challenge information, a block of opaque binary data passed to the security mechanism.] */
/* Tests_SRS_SASLCLIENTIO_01_055: [Send the SASL response data as defined by the SASL specification.] */
/* Tests_SRS_SASLCLIENTIO_01_056: [A block of opaque data passed to the security mechanism.] */
/* Tests_SRS_SASLCLIENTIO_01_057: [The contents of this data are defined by the SASL security mechanism.] */
/* Tests_SRS_SASLCLIENTIO_01_036: [<-- SASL-CHALLENGE *] */
/* Tests_SRS_SASLCLIENTIO_01_037: [SASL-RESPONSE -->] */
/* Tests_SRS_SASLCLIENTIO_01_070: [When a frame needs to be sent as part of the SASL handshake frame exchange, the send shall be done by calling sasl_frame_codec_encode_frame.] */
TEST_FUNCTION(when_a_SASL_challenge_is_received_after_the_mechanisms_the_sasl_mechanism_challenge_processing_is_invoked)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL(mocks, amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	setup_succesfull_challenge_response(&mocks, &definitions_mocks);

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_getting_the_sasl_challenge_fails_then_the_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char test_challenge_bytes[] = { 0x42 };
	unsigned char test_response_bytes[] = { 0x43, 0x44 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL(mocks, amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle))
		.SetReturn(1);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_getting_the_challenge_bytes_fails_then_the_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char test_challenge_bytes[] = { 0x42 };
	unsigned char test_response_bytes[] = { 0x43, 0x44 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL(mocks, amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle));
	amqp_binary some_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_challenge_get_challenge(test_sasl_challenge_handle, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &some_challenge_bytes, sizeof(some_challenge_bytes))
		.SetReturn(1);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_challenge_destroy(test_sasl_challenge_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_the_sasl_mechanism_challenge_response_function_fails_then_the_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char test_challenge_bytes[] = { 0x42 };
	unsigned char test_response_bytes[] = { 0x43, 0x44 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL(mocks, amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle));
	amqp_binary some_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_challenge_get_challenge(test_sasl_challenge_handle, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &some_challenge_bytes, sizeof(some_challenge_bytes));
	SASL_MECHANISM_BYTES sasl_mechanism_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
	SASL_MECHANISM_BYTES sasl_mechanism_response_bytes = { test_response_bytes, sizeof(test_response_bytes) };
	STRICT_EXPECTED_CALL(mocks, saslmechanism_challenge(test_sasl_mechanism, &sasl_mechanism_challenge_bytes, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(3, &sasl_mechanism_response_bytes, sizeof(sasl_mechanism_response_bytes))
		.SetReturn(1);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_challenge_destroy(test_sasl_challenge_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_creating_the_sasl_response_fails_the_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char test_challenge_bytes[] = { 0x42 };
	unsigned char test_response_bytes[] = { 0x43, 0x44 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL(mocks, amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle));
	amqp_binary some_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_challenge_get_challenge(test_sasl_challenge_handle, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &some_challenge_bytes, sizeof(some_challenge_bytes));
	SASL_MECHANISM_BYTES sasl_mechanism_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
	SASL_MECHANISM_BYTES sasl_mechanism_response_bytes = { test_response_bytes, sizeof(test_response_bytes) };
	STRICT_EXPECTED_CALL(mocks, saslmechanism_challenge(test_sasl_mechanism, &sasl_mechanism_challenge_bytes, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(3, &sasl_mechanism_response_bytes, sizeof(sasl_mechanism_response_bytes));
	amqp_binary response_binary_value = { test_response_bytes, sizeof(test_response_bytes) };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_response_create(response_binary_value))
		.SetReturn((SASL_RESPONSE_HANDLE)NULL);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_challenge_destroy(test_sasl_challenge_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_creating_the_AMQP_VALUE_for_sasl_response_fails_the_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char test_challenge_bytes[] = { 0x42 };
	unsigned char test_response_bytes[] = { 0x43, 0x44 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL(mocks, amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle));
	amqp_binary some_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_challenge_get_challenge(test_sasl_challenge_handle, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &some_challenge_bytes, sizeof(some_challenge_bytes));
	SASL_MECHANISM_BYTES sasl_mechanism_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
	SASL_MECHANISM_BYTES sasl_mechanism_response_bytes = { test_response_bytes, sizeof(test_response_bytes) };
	STRICT_EXPECTED_CALL(mocks, saslmechanism_challenge(test_sasl_mechanism, &sasl_mechanism_challenge_bytes, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(3, &sasl_mechanism_response_bytes, sizeof(sasl_mechanism_response_bytes));
	amqp_binary response_binary_value = { test_response_bytes, sizeof(test_response_bytes) };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_response_create(response_binary_value));
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_create_sasl_response(test_sasl_response_handle))
		.SetReturn((AMQP_VALUE)NULL);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_response_destroy(test_sasl_response_handle));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_challenge_destroy(test_sasl_challenge_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_encoding_the_sasl_frame_for_sasl_response_fails_the_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char test_challenge_bytes[] = { 0x42 };
	unsigned char test_response_bytes[] = { 0x43, 0x44 };
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL(mocks, amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle));
	amqp_binary some_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_challenge_get_challenge(test_sasl_challenge_handle, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &some_challenge_bytes, sizeof(some_challenge_bytes));
	SASL_MECHANISM_BYTES sasl_mechanism_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
	SASL_MECHANISM_BYTES sasl_mechanism_response_bytes = { test_response_bytes, sizeof(test_response_bytes) };
	STRICT_EXPECTED_CALL(mocks, saslmechanism_challenge(test_sasl_mechanism, &sasl_mechanism_challenge_bytes, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(3, &sasl_mechanism_response_bytes, sizeof(sasl_mechanism_response_bytes));
	amqp_binary response_binary_value = { test_response_bytes, sizeof(test_response_bytes) };
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_response_create(response_binary_value));
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_create_sasl_response(test_sasl_response_handle));
	STRICT_EXPECTED_CALL(mocks, sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_response_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.IgnoreArgument(3).IgnoreArgument(4)
		.SetReturn(1);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_response_destroy(test_sasl_response_handle));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_destroy(test_sasl_response_amqp_value));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_challenge_destroy(test_sasl_challenge_handle));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_039: [the SASL challenge/response step can occur zero or more times depending on the details of the SASL mechanism chosen.] */
TEST_FUNCTION(SASL_challenge_response_twice_succeed)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL(mocks, amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	setup_succesfull_challenge_response(&mocks, &definitions_mocks);
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	setup_succesfull_challenge_response(&mocks, &definitions_mocks);

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_039: [the SASL challenge/response step can occur zero or more times depending on the details of the SASL mechanism chosen.] */
TEST_FUNCTION(SASL_challenge_response_256_times_succeeds)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	size_t i;

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL(mocks, amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	for (i = 0; i < 256; i++)
	{
		setup_succesfull_challenge_response(&mocks, &definitions_mocks);
		saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	}

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_039: [the SASL challenge/response step can occur zero or more times depending on the details of the SASL mechanism chosen.] */
/* Tests_SRS_SASLCLIENTIO_01_072: [When the SASL handshake is complete, if the handshake is successful, the SASL client IO state shall be switched to IO_STATE_OPEN and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(SASL_challenge_response_256_times_followed_by_outcome_succeeds)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	size_t i;

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	EXPECTED_CALL(mocks, amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	for (i = 0; i < 256; i++)
	{
		setup_succesfull_challenge_response(&mocks, &definitions_mocks);
		saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
	}

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_challenge_type_by_descriptor(test_descriptor_value))
		.SetReturn(false);
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_outcome_type_by_descriptor(test_descriptor_value))
		.SetReturn(true);
	sasl_code sasl_outcome_code = sasl_code_ok;
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_outcome(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_outcome_handle, sizeof(test_sasl_outcome_handle));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_outcome_get_code(test_sasl_outcome_handle, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &sasl_outcome_code, sizeof(sasl_outcome_code));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_outcome_destroy(test_sasl_outcome_handle));

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_OPEN, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_the_mechanisms_sasl_value_cannot_be_decoded_the_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2)
		.SetReturn(1);
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_042: [It is invalid for this list to be null or empty.] */
TEST_FUNCTION(when_a_NULL_list_is_received_in_the_SASL_mechanisms_then_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2)
		.SetReturn(1);
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_042: [It is invalid for this list to be null or empty.] */
TEST_FUNCTION(when_an_empty_array_is_received_in_the_SASL_mechanisms_then_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	uint32_t mechanisms_count = 0;
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_getting_the_mechanisms_array_item_count_fails_then_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	uint32_t mechanisms_count = 0;
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count))
		.SetReturn(1);
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
TEST_FUNCTION(when_the_mechanisms_array_does_not_contain_a_usable_SASL_mechanism_then_the_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	STRICT_EXPECTED_CALL(mocks, saslmechanism_get_mechanism_name(test_sasl_mechanism));
	uint32_t mechanisms_count = 1;
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item(sasl_server_mechanisms, 0));
	const char* test_sasl_server_mechanism_name = "blahblah";
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_server_mechanism_name, sizeof(test_sasl_server_mechanism_name));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
TEST_FUNCTION(when_the_mechanisms_array_has_2_mechanisms_and_none_matches_the_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL(definitions_mocks, is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
	STRICT_EXPECTED_CALL(definitions_mocks, amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	STRICT_EXPECTED_CALL(mocks, saslmechanism_get_mechanism_name(test_sasl_mechanism));
	uint32_t mechanisms_count = 2;
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item(sasl_server_mechanisms, 0));
	const char* test_sasl_server_mechanism_name_1 = "blahblah";
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_server_mechanism_name_1, sizeof(test_sasl_server_mechanism_name_1));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_array_item(sasl_server_mechanisms, 1));
	const char* test_sasl_server_mechanism_name_2 = "another_blah";
	STRICT_EXPECTED_CALL(mocks, amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_server_mechanism_name_2, sizeof(test_sasl_server_mechanism_name_2));
	STRICT_EXPECTED_CALL(definitions_mocks, sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

static void setup_send_init(saslclientio_mocks* mocks, amqp_definitions_mocks* definitions_mocks)
{
	STRICT_EXPECTED_CALL((*mocks), amqpvalue_get_inplace_descriptor(test_sasl_value));
	STRICT_EXPECTED_CALL((*definitions_mocks), is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
	STRICT_EXPECTED_CALL((*definitions_mocks), amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
	AMQP_VALUE sasl_server_mechanisms;
	STRICT_EXPECTED_CALL((*definitions_mocks), sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, &sasl_server_mechanisms))
		.IgnoreArgument(2);
	uint32_t mechanisms_count = 1;
	STRICT_EXPECTED_CALL((*mocks), amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
		.CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
	STRICT_EXPECTED_CALL((*mocks), amqpvalue_get_array_item(sasl_server_mechanisms, 0));
	STRICT_EXPECTED_CALL((*mocks), amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
	STRICT_EXPECTED_CALL((*mocks), saslmechanism_get_mechanism_name(test_sasl_mechanism));
	SASL_MECHANISM_BYTES init_bytes = { NULL, 0 };
	STRICT_EXPECTED_CALL((*definitions_mocks), sasl_init_create(test_mechanism));
	EXPECTED_CALL((*mocks), saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
		.CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes));
	STRICT_EXPECTED_CALL((*definitions_mocks), amqpvalue_create_sasl_init(test_sasl_init_handle));
	STRICT_EXPECTED_CALL((*mocks), sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_init_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.IgnoreArgument(3).IgnoreArgument(4);
	STRICT_EXPECTED_CALL((*definitions_mocks), sasl_init_destroy(test_sasl_init_handle));
	STRICT_EXPECTED_CALL((*mocks), amqpvalue_destroy(test_sasl_init_amqp_value));
	STRICT_EXPECTED_CALL((*definitions_mocks), sasl_mechanisms_destroy(test_sasl_mechanisms_handle));
}

/* Tests_SRS_SASLCLIENTIO_01_120: [When SASL client IO is notified by sasl_frame_codec of bytes that have been encoded via the on_bytes_encoded callback and SASL client IO is in the state OPENING, SASL client IO shall send these bytes by using xio_send.] */
TEST_FUNCTION(when_encoded_bytes_are_received_they_are_given_to_xio_send)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char encoded_bytes[] = { 0x42, 0x43 };

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	setup_send_init(&mocks, &definitions_mocks);
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	STRICT_EXPECTED_CALL(mocks, xio_send(test_underlying_io, encoded_bytes, sizeof(encoded_bytes), NULL, NULL))
		.ValidateArgumentBuffer(2, &encoded_bytes, sizeof(encoded_bytes));

	// act
	saved_on_bytes_encoded(saved_on_bytes_encoded_callback_context, encoded_bytes, sizeof(encoded_bytes), true);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_120: [When SASL client IO is notified by sasl_frame_codec of bytes that have been encoded via the on_bytes_encoded callback and SASL client IO is in the state OPENING, SASL client IO shall send these bytes by using xio_send.] */
TEST_FUNCTION(when_encoded_bytes_are_received_with_encoded_complete_flag_set_to_false_they_are_given_to_xio_send)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char encoded_bytes[] = { 0x42, 0x43 };

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	setup_send_init(&mocks, &definitions_mocks);
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	STRICT_EXPECTED_CALL(mocks, xio_send(test_underlying_io, encoded_bytes, sizeof(encoded_bytes), NULL, NULL))
		.ValidateArgumentBuffer(2, &encoded_bytes, sizeof(encoded_bytes));

	// act
	saved_on_bytes_encoded(saved_on_bytes_encoded_callback_context, encoded_bytes, sizeof(encoded_bytes), false);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_121: [If xio_send fails, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_xio_send_fails_when_sending_encoded_bytes_then_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	unsigned char encoded_bytes[] = { 0x42, 0x43 };

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	setup_send_init(&mocks, &definitions_mocks);
	saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

	STRICT_EXPECTED_CALL(mocks, xio_send(test_underlying_io, encoded_bytes, sizeof(encoded_bytes), NULL, NULL))
		.ValidateArgumentBuffer(2, &encoded_bytes, sizeof(encoded_bytes))
		.SetReturn(1);
	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_bytes_encoded(saved_on_bytes_encoded_callback_context, encoded_bytes, sizeof(encoded_bytes), false);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_122: [When on_frame_codec_error is called while in the OPENING or OPEN state the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_the_frame_codec_triggers_an_error_in_the_OPENING_state_the_saslclientio_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_frame_codec_error(saved_on_frame_codec_error_callback_context);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_122: [When on_frame_codec_error is called while in the OPENING or OPEN state the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_the_frame_codec_triggers_an_error_in_the_OPEN_state_the_saslclientio_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

	// act
	saved_on_frame_codec_error(saved_on_frame_codec_error_callback_context);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_123: [When on_frame_codec_error is called in the ERROR state nothing shall be done.] */
TEST_FUNCTION(when_the_frame_codec_triggers_an_error_in_the_ERROR_state_nothing_is_done)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	saved_on_frame_codec_error(saved_sasl_frame_codec_callback_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_on_frame_codec_error(saved_on_frame_codec_error_callback_context);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_124: [When on_sasl_frame_codec_error is called while in the OPENING or OPEN state the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_the_sasl_frame_codec_triggers_an_error_in_the_OPENING_state_the_saslclientio_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
	saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
	saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

	// act
	saved_on_sasl_frame_codec_error(saved_sasl_frame_codec_callback_context);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_124: [When on_sasl_frame_codec_error is called while in the OPENING or OPEN state the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_the_sasl_frame_codec_triggers_an_error_in_the_OPEN_state_the_saslclientio_state_is_set_to_ERROR)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	STRICT_EXPECTED_CALL(mocks, test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

	// act
	saved_on_sasl_frame_codec_error(saved_sasl_frame_codec_callback_context);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_125: [When on_sasl_frame_codec_error is called in the ERROR state nothing shall be done.] */
TEST_FUNCTION(when_the_sasl_frame_codec_triggers_an_error_in_the_ERROR_state_nothing_is_done)
{
	// arrange
	saslclientio_mocks mocks;
	amqp_definitions_mocks definitions_mocks;
	SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
	CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
	(void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
	setup_successful_sasl_handshake(&mocks, &definitions_mocks, sasl_io);
	saved_on_sasl_frame_codec_error(saved_sasl_frame_codec_callback_context);
	mocks.ResetAllCalls();
	definitions_mocks.ResetAllCalls();

	// act
	saved_on_sasl_frame_codec_error(saved_sasl_frame_codec_callback_context);

	// assert
	mocks.AssertActualAndExpectedCalls();
	definitions_mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslclientio_destroy(sasl_io);
}
#endif

END_TEST_SUITE(saslclientio_ut)
