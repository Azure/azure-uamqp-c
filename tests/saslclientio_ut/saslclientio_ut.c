// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#else
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#endif
#include "testrunnerswitcher.h"
#include "umock_c.h"
#include "umocktypes_stdint.h"

static void* my_gballoc_malloc(size_t size)
{
    return malloc(size);
}

static void my_gballoc_free(void* ptr)
{
    free(ptr);
}

#define ENABLE_MOCKS

#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_uamqp_c/amqpvalue.h"
#include "azure_uamqp_c/amqpvalue_to_string.h"
#include "azure_uamqp_c/frame_codec.h"
#include "azure_uamqp_c/sasl_frame_codec.h"
#include "azure_uamqp_c/sasl_mechanism.h"
#include "azure_uamqp_c/amqp_definitions.h"

#undef ENABLE_MOCKS

#include "azure_uamqp_c/saslclientio.h"

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
static const unsigned char sasl_header[] = { 'A', 'M', 'Q', 'P', 3, 1, 0, 0 };
static const unsigned char test_sasl_mechanisms_frame[] = { 'x', '1' }; /* these are some dummy bytes */
static const unsigned char test_sasl_outcome[] = { 'x', '2' }; /* these are some dummy bytes */
static const unsigned char test_sasl_challenge[] = { 'x', '3' }; /* these are some dummy bytes */

static AMQP_VALUE test_sasl_value = (AMQP_VALUE)0x5242;

static char expected_stringified_io[8192];
static char actual_stringified_io[8192];
static unsigned char* frame_codec_received_bytes;
static size_t frame_codec_received_byte_count;
static unsigned char* io_send_bytes;
static size_t io_send_byte_count;

static int umocktypes_copy_SASL_MECHANISM_BYTES_ptr(SASL_MECHANISM_BYTES** destination, const SASL_MECHANISM_BYTES** source)
{
    int result;

    if (*source == NULL)
    {
        *destination = NULL;
        result = 0;
    }
    else
    {
        *destination = (SASL_MECHANISM_BYTES*)my_gballoc_malloc(sizeof(SASL_MECHANISM_BYTES));
        if (*destination == NULL)
        {
            result = __LINE__;
        }
        else
        {
            (*destination)->length = (*source)->length;
            if ((*destination)->length == 0)
            {
                (*destination)->bytes = NULL;
                result = 0;
            }
            else
            {
                (*destination)->bytes = (const unsigned char*)my_gballoc_malloc((*destination)->length);
                if ((*destination)->bytes == NULL)
                {
                    my_gballoc_free(*destination);
                    result = __LINE__;
                }
                else
                {
                    (void)memcpy((void*)(*destination)->bytes, (*source)->bytes, (*destination)->length);
                    result = 0;
                }
            }
        }
    }

    return result;
}

static void umocktypes_free_SASL_MECHANISM_BYTES_ptr(SASL_MECHANISM_BYTES** value)
{
    if (*value != NULL)
    {
        my_gballoc_free((void*)(*value)->bytes);
        my_gballoc_free(*value);
    }
}

static char* umocktypes_stringify_SASL_MECHANISM_BYTES_ptr(const SASL_MECHANISM_BYTES** value)
{
    char* result;
    if (*value == NULL)
    {
        result = (char*)my_gballoc_malloc(5);
        if (result != NULL)
        {
            (void)memcpy(result, "NULL", 5);
        }
    }
    else
    {
        result = (char*)my_gballoc_malloc(3 + (5 * (*value)->length));
        if (result != NULL)
        {
            size_t pos = 0;
            size_t i;

            result[pos++] = '[';
            for (i = 0; i < (*value)->length; i++)
            {
                (void)sprintf(&result[pos], "0x%02X ", ((unsigned char*)(*value)->bytes)[i]);
                pos += 5;
            }
            result[pos++] = ']';
            result[pos++] = '\0';
        }
    }

    return result;
}

static int umocktypes_are_equal_SASL_MECHANISM_BYTES_ptr(SASL_MECHANISM_BYTES** left, SASL_MECHANISM_BYTES** right)
{
    int result;

    if (*left == *right)
    {
        result = 1;
    }
    else
    {
        if ((*left == NULL) ||
            (*right == NULL))
        {
            result = 0;
        }
        else
        {
            if ((*left)->length != (*right)->length)
            {
                result = 0;
            }
            else
            {
                if ((*left)->length == 0)
                {
                    result = 1;
                }
                else
                {
                    result = (memcmp((*left)->bytes, (*right)->bytes, (*left)->length) == 0) ? 1 : 0;
                }
            }
        }
    }

    return result;
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

static FRAME_CODEC_HANDLE my_frame_codec_create(ON_FRAME_CODEC_ERROR on_frame_codec_error, void* callback_context)
{
    saved_on_frame_codec_error = on_frame_codec_error;
    saved_on_frame_codec_error_callback_context = callback_context;
    return test_frame_codec;
}

static int my_frame_codec_receive_bytes(FRAME_CODEC_HANDLE frame_codec, const unsigned char* buffer, size_t size)
{
    unsigned char* new_bytes = (unsigned char*)realloc(frame_codec_received_bytes, frame_codec_received_byte_count + size);
    (void)frame_codec;
    if (new_bytes != NULL)
    {
        frame_codec_received_bytes = new_bytes;
        (void)memcpy(frame_codec_received_bytes + frame_codec_received_byte_count, buffer, size);
        frame_codec_received_byte_count += size;
    }
    return 0;
}

static int my_frame_codec_subscribe(FRAME_CODEC_HANDLE frame_codec, uint8_t type, ON_FRAME_RECEIVED frame_received_callback, void* callback_context)
{
    (void)type;
    (void)frame_codec;
    saved_frame_received_callback = frame_received_callback;
    saved_frame_received_callback_context = callback_context;
    return 0;
}

static SASL_FRAME_CODEC_HANDLE my_sasl_frame_codec_create(FRAME_CODEC_HANDLE frame_codec, ON_SASL_FRAME_RECEIVED on_sasl_frame_received, ON_SASL_FRAME_CODEC_ERROR on_sasl_frame_codec_error, void* callback_context)
{
    (void)frame_codec;
    saved_on_sasl_frame_received = on_sasl_frame_received;
    saved_sasl_frame_codec_callback_context = callback_context;
    saved_on_sasl_frame_codec_error = on_sasl_frame_codec_error;
    return test_sasl_frame_codec;
}

static int my_sasl_frame_codec_encode_frame(SASL_FRAME_CODEC_HANDLE sasl_frame_codec, const AMQP_VALUE sasl_frame_value, ON_BYTES_ENCODED on_bytes_encoded, void* callback_context)
{
    (void)sasl_frame_codec;
    (void)sasl_frame_value;
    saved_on_bytes_encoded = on_bytes_encoded;
    saved_on_bytes_encoded_callback_context = callback_context;
    return 0;
}

static int my_xio_open(XIO_HANDLE xio, ON_IO_OPEN_COMPLETE on_io_open_complete, void* on_io_open_complete_context, ON_BYTES_RECEIVED on_bytes_received, void* on_bytes_received_context, ON_IO_ERROR on_io_error, void* on_io_error_context)
{
    (void)on_io_error_context;
    (void)on_bytes_received_context;
    (void)xio;
    saved_on_bytes_received = on_bytes_received;
    saved_on_io_open_complete = on_io_open_complete;
    saved_on_io_error = on_io_error;
    saved_io_callback_context = on_io_open_complete_context;
    return 0;
}

static int my_xio_send(XIO_HANDLE xio, const void* buffer, size_t size, ON_SEND_COMPLETE on_send_complete, void* callback_context)
{
    unsigned char* new_bytes = (unsigned char*)realloc(io_send_bytes, io_send_byte_count + size);
    (void)callback_context;
    (void)on_send_complete;
    (void)xio;
    if (new_bytes != NULL)
    {
        io_send_bytes = new_bytes;
        (void)memcpy(io_send_bytes + io_send_byte_count, buffer, size);
        io_send_byte_count += size;
    }
    return 0;
}

MOCK_FUNCTION_WITH_CODE(, void, test_on_bytes_received, void*, context, const unsigned char*, buffer, size_t, size)
MOCK_FUNCTION_END()
MOCK_FUNCTION_WITH_CODE(, void, test_on_io_open_complete, void*, context, IO_OPEN_RESULT, io_open_result)
MOCK_FUNCTION_END()
MOCK_FUNCTION_WITH_CODE(, void, test_on_io_error, void*, context)
MOCK_FUNCTION_END()
MOCK_FUNCTION_WITH_CODE(, void, test_on_send_complete, void*, context, IO_SEND_RESULT, send_result)
MOCK_FUNCTION_END()

static TEST_MUTEX_HANDLE g_testByTest;
static TEST_MUTEX_HANDLE g_dllByDll;

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

BEGIN_TEST_SUITE(saslclientio_ut)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;

    TEST_INITIALIZE_MEMORY_DEBUG(g_dllByDll);
    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    umock_c_init(on_umock_c_error);

    result = umocktypes_stdint_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, my_gballoc_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, my_gballoc_free);
    REGISTER_GLOBAL_MOCK_HOOK(frame_codec_create, my_frame_codec_create);
    REGISTER_GLOBAL_MOCK_HOOK(frame_codec_receive_bytes, my_frame_codec_receive_bytes);
    REGISTER_GLOBAL_MOCK_HOOK(frame_codec_subscribe, my_frame_codec_subscribe);
    REGISTER_GLOBAL_MOCK_RETURN(frame_codec_unsubscribe, 0);
    REGISTER_GLOBAL_MOCK_RETURN(frame_codec_encode_frame, 0);
    REGISTER_GLOBAL_MOCK_HOOK(sasl_frame_codec_create, my_sasl_frame_codec_create);
    REGISTER_GLOBAL_MOCK_HOOK(sasl_frame_codec_encode_frame, my_sasl_frame_codec_encode_frame);
    REGISTER_GLOBAL_MOCK_HOOK(xio_open, my_xio_open);
    REGISTER_GLOBAL_MOCK_HOOK(xio_send, my_xio_send);
    REGISTER_GLOBAL_MOCK_RETURN(xio_close, 0);
    REGISTER_GLOBAL_MOCK_RETURN(xio_setoption, 0);
    REGISTER_GLOBAL_MOCK_RETURN(saslmechanism_get_init_bytes, 0);
    REGISTER_GLOBAL_MOCK_RETURN(saslmechanism_get_mechanism_name, test_mechanism);
    REGISTER_GLOBAL_MOCK_RETURN(saslmechanism_challenge, 0);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_to_string, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_inplace_descriptor, test_descriptor_value);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_array_item_count, 0);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_array_item, test_sasl_server_mechanism);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_symbol, 0);
    REGISTER_GLOBAL_MOCK_RETURN(OptionHandler_Create, NULL);

    REGISTER_TYPE(SASL_MECHANISM_BYTES*, SASL_MECHANISM_BYTES_ptr);

    REGISTER_UMOCK_ALIAS_TYPE(const SASL_MECHANISM_BYTES*, SASL_MECHANISM_BYTES*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_FRAME_CODEC_ERROR, void*);
    REGISTER_UMOCK_ALIAS_TYPE(FRAME_CODEC_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_SASL_FRAME_RECEIVED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_SASL_FRAME_CODEC_ERROR, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SASL_FRAME_CODEC_HANDLE, void*);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_deinit();

    TEST_MUTEX_DESTROY(g_testByTest);
    TEST_DEINITIALIZE_MEMORY_DEBUG(g_dllByDll);
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    if (TEST_MUTEX_ACQUIRE(g_testByTest))
    {
        ASSERT_FAIL("our mutex is ABANDONED. Failure in test framework");
    }

    umock_c_reset_all_calls();
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

    TEST_MUTEX_RELEASE(g_testByTest);
}

/* saslclientio_create */

/* Tests_SRS_SASLCLIENTIO_01_004: [saslclientio_create shall return on success a non-NULL handle to a new SASL client IO instance.] */
/* Tests_SRS_SASLCLIENTIO_01_089: [saslclientio_create shall create a frame_codec to be used for encoding/decoding frames bycalling frame_codec_create and passing the underlying_io as argument.] */
/* Tests_SRS_SASLCLIENTIO_01_084: [saslclientio_create shall create a sasl_frame_codec to be used for SASL frame encoding/decoding by calling sasl_frame_codec_create and passing the just created frame_codec as argument.] */
TEST_FUNCTION(saslclientio_create_with_valid_args_succeeds)
{
    // arrange
    CONCRETE_IO_HANDLE result;
    SASLCLIENTIO_CONFIG saslclientio_config;
    saslclientio_config.underlying_io = test_underlying_io;
    saslclientio_config.sasl_mechanism = test_sasl_mechanism;

    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(frame_codec_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(sasl_frame_codec_create(test_frame_codec, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .ValidateArgument(1);

    // act
    result = saslclientio_create(&saslclientio_config);

    // assert
    ASSERT_IS_NOT_NULL(result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(result);
}

/* Tests_SRS_SASLCLIENTIO_01_005: [If xio_create_parameters is NULL, saslclientio_create shall fail and return NULL.] */
TEST_FUNCTION(saslclientio_create_with_NULL_config_fails)
{
    // arrange

    // act
    CONCRETE_IO_HANDLE result = saslclientio_create(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASLCLIENTIO_01_006: [If memory cannot be allocated for the new instance, saslclientio_create shall fail and return NULL.] */
TEST_FUNCTION(when_allocating_memory_for_the_new_instance_fails_then_saslclientio_create_fails)
{
    // arrange
    CONCRETE_IO_HANDLE result;
    SASLCLIENTIO_CONFIG saslclientio_config;
    saslclientio_config.underlying_io = test_underlying_io;
    saslclientio_config.sasl_mechanism = test_sasl_mechanism;

    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG))
        .SetReturn((void*)NULL);

    // act
    result = saslclientio_create(&saslclientio_config);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASLCLIENTIO_01_090: [If frame_codec_create fails, then saslclientio_create shall fail and return NULL.] */
TEST_FUNCTION(when_creating_the_frame_codec_fails_then_saslclientio_create_fails)
{
    // arrange
    CONCRETE_IO_HANDLE result;
    SASLCLIENTIO_CONFIG saslclientio_config;
    saslclientio_config.underlying_io = test_underlying_io;
    saslclientio_config.sasl_mechanism = test_sasl_mechanism;

    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(frame_codec_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(NULL);
    EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    // act
    result = saslclientio_create(&saslclientio_config);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASLCLIENTIO_01_085: [If sasl_frame_codec_create fails, then saslclientio_create shall fail and return NULL.] */
TEST_FUNCTION(when_creating_the_sasl_frame_codec_fails_then_saslclientio_create_fails)
{
    // arrange
    CONCRETE_IO_HANDLE result;
    SASLCLIENTIO_CONFIG saslclientio_config;
    saslclientio_config.underlying_io = test_underlying_io;
    saslclientio_config.sasl_mechanism = test_sasl_mechanism;

    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(frame_codec_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(sasl_frame_codec_create(test_frame_codec, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .ValidateArgument(1)
        .SetReturn(NULL);
    STRICT_EXPECTED_CALL(frame_codec_destroy(test_frame_codec));
    EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    // act
    result = saslclientio_create(&saslclientio_config);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASLCLIENTIO_01_092: [If any of the sasl_mechanism or underlying_io members of the configuration structure are NULL, saslclientio_create shall fail and return NULL.] */
TEST_FUNCTION(saslclientio_create_with_a_NULL_underlying_io_fails)
{
    // arrange
    CONCRETE_IO_HANDLE result;
    SASLCLIENTIO_CONFIG saslclientio_config;
    saslclientio_config.underlying_io = NULL;
    saslclientio_config.sasl_mechanism = test_sasl_mechanism;

    // act
    result = saslclientio_create(&saslclientio_config);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASLCLIENTIO_01_092: [If any of the sasl_mechanism or underlying_io members of the configuration structure are NULL, saslclientio_create shall fail and return NULL.] */
TEST_FUNCTION(saslclientio_create_with_a_NULL_sasl_mechanism_fails)
{
    // arrange
    CONCRETE_IO_HANDLE result;
    SASLCLIENTIO_CONFIG saslclientio_config;
    saslclientio_config.underlying_io = test_underlying_io;
    saslclientio_config.sasl_mechanism = NULL;

    // act
    result = saslclientio_create(&saslclientio_config);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);
}

/* saslclientio_destroy */

/* Tests_SRS_SASLCLIENTIO_01_007: [saslclientio_destroy shall free all resources associated with the SASL client IO handle.]  */
/* Tests_SRS_SASLCLIENTIO_01_086: [saslclientio_destroy shall destroy the sasl_frame_codec created in saslclientio_create by calling sasl_frame_codec_destroy.] */
/* Tests_SRS_SASLCLIENTIO_01_091: [saslclientio_destroy shall destroy the frame_codec created in saslclientio_create by calling frame_codec_destroy.] */
TEST_FUNCTION(saslclientio_destroy_frees_the_resources_allocated_in_create)
{
    // arrange
    CONCRETE_IO_HANDLE sasl_io;
    SASLCLIENTIO_CONFIG saslclientio_config;
    saslclientio_config.underlying_io = test_underlying_io;
    saslclientio_config.sasl_mechanism = test_sasl_mechanism;
    sasl_io = saslclientio_create(&saslclientio_config);
    umock_c_reset_all_calls();

    EXPECTED_CALL(sasl_frame_codec_destroy(test_sasl_frame_codec));
    EXPECTED_CALL(frame_codec_destroy(test_frame_codec));
    EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    // act
    saslclientio_destroy(sasl_io);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_SASLCLIENTIO_01_008: [If the argument sasl_io is NULL, saslclientio_destroy shall do nothing.] */
TEST_FUNCTION(saslclientio_destroy_with_NULL_argument_does_nothing)
{
    // arrange

    // act
    saslclientio_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* saslclientio_open */

#if 0
/* Tests_SRS_SASLCLIENTIO_01_009: [saslclientio_open shall call xio_open on the underlying_io passed to saslclientio_create.] */
/* Tests_SRS_SASLCLIENTIO_01_010: [On success, saslclientio_open shall return 0.] */
/* Tests_SRS_SASLCLIENTIO_01_013: [saslclientio_open shall pass to xio_open a callback for receiving bytes and a state changed callback for the underlying_io state changes.] */
TEST_FUNCTION(saslclientio_open_with_valid_args_succeeds)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    int result;
    umock_c_reset_all_calls();

    EXPECTED_CALL(xio_open(test_underlying_io, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .ValidateArgument(1);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_OPENING, IO_STATE_NOT_OPEN));

    // act
    result = saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_011: [If any of the sasl_io or on_bytes_received arguments is NULL, saslclientio_open shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_open_with_NULL_sasl_io_handle_fails)
{
    // arrange

    // act
    int result = saslclientio_open(NULL, test_on_bytes_received, test_on_io_state_changed, test_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_SASLCLIENTIO_01_011: [If any of the sasl_io or on_bytes_received arguments is NULL, saslclientio_open shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_open_with_NULL_on_bytes_received_fails)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    umock_c_reset_all_calls();

    // act
    int result = saslclientio_open(sasl_io, NULL, test_on_io_state_changed, test_context);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_012: [If the open of the underlying_io fails, saslclientio_open shall fail and return non-zero value.] */
TEST_FUNCTION(when_opening_the_underlying_io_fails_saslclientio_open_fails)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    umock_c_reset_all_calls();

    EXPECTED_CALL(xio_open(test_underlying_io, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .ValidateArgument(1)
        .SetReturn(1);

    // act
    int result = saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

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
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(xio_close(test_underlying_io));
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_NOT_OPEN, IO_STATE_OPENING));

    // act
    int result = saslclientio_close(sasl_io);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

static void setup_successful_sasl_handshake(CONCRETE_IO_HANDLE sasl_io)
{
    sasl_code sasl_outcome_code = sasl_code_ok;
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_outcome, sizeof(test_sasl_outcome));
    umock_c_reset_all_calls();
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_outcome_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_outcome(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_outcome_handle, sizeof(test_sasl_outcome_handle));
    STRICT_EXPECTED_CALL(sasl_outcome_get_code(test_sasl_outcome_handle, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &sasl_outcome_code, sizeof(sasl_outcome_code));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
}

/* Tests_SRS_SASLCLIENTIO_01_015: [saslclientio_close shall close the underlying io handle passed in saslclientio_create by calling xio_close.] */
/* Tests_SRS_SASLCLIENTIO_01_098: [saslclientio_close shall only perform the close if the state is OPEN, OPENING or ERROR.] */
/* Tests_SRS_SASLCLIENTIO_01_016: [On success, saslclientio_close shall return 0.] */
TEST_FUNCTION(saslclientio_close_when_the_io_state_is_OPEN_closes_the_underlying_io)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(xio_close(test_underlying_io));
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_NOT_OPEN, IO_STATE_OPEN));

    // act
    int result = saslclientio_close(sasl_io);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_015: [saslclientio_close shall close the underlying io handle passed in saslclientio_create by calling xio_close.] */
/* Tests_SRS_SASLCLIENTIO_01_098: [saslclientio_close shall only perform the close if the state is OPEN, OPENING or ERROR.] */
/* Tests_SRS_SASLCLIENTIO_01_016: [On success, saslclientio_close shall return 0.] */
TEST_FUNCTION(saslclientio_close_when_the_io_state_is_ERROR_closes_the_underlying_io)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_NOT_OPEN);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(xio_close(test_underlying_io));
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_NOT_OPEN, IO_STATE_ERROR));

    // act
    int result = saslclientio_close(sasl_io);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_097: [If saslclientio_close is called when the IO is in the IO_STATE_NOT_OPEN state, no call to the underlying IO shall be made and saslclientio_close shall return 0.] */
TEST_FUNCTION(saslclientio_close_when_the_io_state_is_NOT_OPEN_succeeds_without_calling_the_underlying_io)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    umock_c_reset_all_calls();

    // act
    int result = saslclientio_close(sasl_io);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_097: [If saslclientio_close is called when the IO is in the IO_STATE_NOT_OPEN state, no call to the underlying IO shall be made and saslclientio_close shall return 0.] */
TEST_FUNCTION(saslclientio_close_when_the_io_state_is_NOT_OPEN_due_to_a_previous_close_succeeds_without_calling_the_underlying_io)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_NOT_OPEN);
    (void)saslclientio_close(sasl_io);
    umock_c_reset_all_calls();

    // act
    int result = saslclientio_close(sasl_io);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_017: [If sasl_io is NULL, saslclientio_close shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_close_with_NULL_sasl_io_fails)
{
    // arrange

    // act
    int result = saslclientio_close(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_SASLCLIENTIO_01_018: [If xio_close fails, then saslclientio_close shall return a non-zero value.] */
TEST_FUNCTION(when_xio_close_fails_saslclientio_close_fails)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(xio_close(test_underlying_io))
        .SetReturn(1);

    // act
    int result = saslclientio_close(sasl_io);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* saslclientio_send */

/* Tests_SRS_SASLCLIENTIO_01_019: [If saslclientio_send is called while the SASL client IO state is not IO_STATE_OPEN, saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_send_when_io_state_is_NOT_OPEN_fails)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    umock_c_reset_all_calls();
    unsigned char test_buffer[] = { 0x42 };

    // act
    int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_019: [If saslclientio_send is called while the SASL client IO state is not IO_STATE_OPEN, saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_send_when_io_state_is_OPENING_fails)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();
    unsigned char test_buffer[] = { 0x42 };

    // act
    int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_019: [If saslclientio_send is called while the SASL client IO state is not IO_STATE_OPEN, saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_send_when_io_state_is_ERROR_fails)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_NOT_OPEN);
    umock_c_reset_all_calls();
    unsigned char test_buffer[] = { 0x42 };

    // act
    int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_020: [If the SASL client IO state is IO_STATE_OPEN, saslclientio_send shall call xio_send on the underlying_io passed to saslclientio_create, while passing as arguments the buffer, size, on_send_complete and callback_context.] */
/* Tests_SRS_SASLCLIENTIO_01_021: [On success, saslclientio_send shall return 0.] */
TEST_FUNCTION(saslclientio_send_when_io_state_is_OPEN_calls_the_underlying_io_send)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();
    unsigned char test_buffer[] = { 0x42 };

    STRICT_EXPECTED_CALL(xio_send(test_underlying_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context))
        .ValidateArgumentBuffer(2, test_buffer, sizeof(test_buffer));

    // act
    int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_020: [If the SASL client IO state is IO_STATE_OPEN, saslclientio_send shall call xio_send on the underlying_io passed to saslclientio_create, while passing as arguments the buffer, size, on_send_complete and callback_context.] */
/* Tests_SRS_SASLCLIENTIO_01_021: [On success, saslclientio_send shall return 0.] */
TEST_FUNCTION(saslclientio_send_with_NULL_on_send_complete_passes_NULL_to_the_underlying_io)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();
    unsigned char test_buffer[] = { 0x42 };

    STRICT_EXPECTED_CALL(xio_send(test_underlying_io, test_buffer, sizeof(test_buffer), NULL, test_context))
        .ValidateArgumentBuffer(2, test_buffer, sizeof(test_buffer));

    // act
    int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), NULL, test_context);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_020: [If the SASL client IO state is IO_STATE_OPEN, saslclientio_send shall call xio_send on the underlying_io passed to saslclientio_create, while passing as arguments the buffer, size, on_send_complete and callback_context.] */
/* Tests_SRS_SASLCLIENTIO_01_021: [On success, saslclientio_send shall return 0.] */
TEST_FUNCTION(saslclientio_send_with_NULL_on_send_complete_context_passes_NULL_to_the_underlying_io)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();
    unsigned char test_buffer[] = { 0x42 };

    STRICT_EXPECTED_CALL(xio_send(test_underlying_io, test_buffer, sizeof(test_buffer), test_on_send_complete, NULL))
        .ValidateArgumentBuffer(2, test_buffer, sizeof(test_buffer));

    // act
    int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), test_on_send_complete, NULL);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_022: [If the saslio or buffer argument is NULL, saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_send_with_NULL_sasl_io_fails)
{
    // arrange
    unsigned char test_buffer[] = { 0x42 };

    // act
    int result = saslclientio_send(NULL, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_SASLCLIENTIO_01_022: [If the saslio or buffer argument is NULL, saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_send_with_NULL_buffer_fails)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();
    unsigned char test_buffer[] = { 0x42 };

    // act
    int result = saslclientio_send(sasl_io, NULL, sizeof(test_buffer), test_on_send_complete, test_context);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_023: [If size is 0, saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(saslclientio_send_with_0_size_fails)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();
    unsigned char test_buffer[] = { 0x42 };

    // act
    int result = saslclientio_send(sasl_io, test_buffer, 0, test_on_send_complete, test_context);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_024: [If the call to xio_send fails, then saslclientio_send shall fail and return a non-zero value.] */
TEST_FUNCTION(when_the_underlying_xio_send_fails_then_saslclientio_send_fails)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();
    unsigned char test_buffer[] = { 0x42 };

    STRICT_EXPECTED_CALL(xio_send(test_underlying_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context))
        .ValidateArgumentBuffer(2, test_buffer, sizeof(test_buffer))
        .SetReturn(1);

    // act
    int result = saslclientio_send(sasl_io, test_buffer, sizeof(test_buffer), test_on_send_complete, test_context);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* saslclientio_dowork */

/* Tests_SRS_SASLCLIENTIO_01_025: [saslclientio_dowork shall call the xio_dowork on the underlying_io passed in saslclientio_create.] */
TEST_FUNCTION(when_the_io_state_is_OPEN_xio_dowork_calls_the_underlying_IO)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(xio_dowork(test_underlying_io));

    // act
    saslclientio_dowork(sasl_io);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_025: [saslclientio_dowork shall call the xio_dowork on the underlying_io passed in saslclientio_create.] */
TEST_FUNCTION(when_the_io_state_is_OPENING_xio_dowork_calls_the_underlying_IO)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(xio_dowork(test_underlying_io));

    // act
    saslclientio_dowork(sasl_io);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_025: [saslclientio_dowork shall call the xio_dowork on the underlying_io passed in saslclientio_create.] */
TEST_FUNCTION(when_the_io_state_is_NOT_OPEN_xio_dowork_does_nothing)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    umock_c_reset_all_calls();

    // act
    saslclientio_dowork(sasl_io);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_025: [saslclientio_dowork shall call the xio_dowork on the underlying_io passed in saslclientio_create.] */
TEST_FUNCTION(when_the_io_state_is_ERROR_xio_dowork_does_nothing)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_NOT_OPEN);
    umock_c_reset_all_calls();

    // act
    saslclientio_dowork(sasl_io);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_026: [If the sasl_io argument is NULL, saslclientio_dowork shall do nothing.] */
TEST_FUNCTION(saslclientio_dowork_with_NULL_sasl_io_handle_does_nothing)
{
    // arrange

    // act
    saslclientio_dowork(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* saslclientio_get_interface_description */

/* Tests_SRS_SASLCLIENTIO_01_087: [saslclientio_get_interface_description shall return a pointer to an IO_INTERFACE_DESCRIPTION structure that contains pointers to the functions: saslclientio_create, saslclientio_destroy, saslclientio_open, saslclientio_close, saslclientio_send and saslclientio_dowork.] */
TEST_FUNCTION(saslclientio_get_interface_description_returns_the_saslclientio_interface_functions)
{
    // arrange

    // act
    const IO_INTERFACE_DESCRIPTION* result = saslclientio_get_interface_description();

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
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
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char test_bytes[] = { 0x42, 0x43 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_bytes_received(test_context, test_bytes, sizeof(test_bytes)));

    // act
    saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_027: [When the on_bytes_received callback passed to the underlying IO is called and the SASL client IO state is IO_STATE_OPEN, the bytes shall be indicated to the user of SASL client IO by calling the on_bytes_received that was passed in saslclientio_open.] */
/* Tests_SRS_SASLCLIENTIO_01_029: [The context argument shall be set to the callback_context passed in saslclientio_open.] */
TEST_FUNCTION(when_io_state_is_open_and_bytes_are_received_and_context_passed_to_open_was_NULL_NULL_is_passed_as_context_to_the_on_bytes_received_call)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char test_bytes[] = { 0x42, 0x43 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, NULL);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_bytes_received(NULL, test_bytes, sizeof(test_bytes)));

    // act
    saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_028: [If buffer is NULL or size is zero, nothing should be indicated as received and the saslio state shall be switched to ERROR the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_io_state_is_open_and_bytes_are_received_with_bytes_NULL_nothing_is_indicated_up)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char test_bytes[] = { 0x42, 0x43 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

    // act
    saved_on_bytes_received(saved_io_callback_context, NULL, sizeof(test_bytes));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_028: [If buffer is NULL or size is zero, nothing should be indicated as received and the saslio state shall be switched to ERROR the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_io_state_is_open_and_bytes_are_received_with_size_zero_nothing_is_indicated_up)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char test_bytes[] = { 0x42, 0x43 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

    // act
    saved_on_bytes_received(saved_io_callback_context, test_bytes, 0);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_031: [If bytes are received when the SASL client IO state is IO_STATE_ERROR, SASL client IO shall do nothing.]  */
TEST_FUNCTION(when_io_state_is_ERROR_and_bytes_are_received_nothing_is_done)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    unsigned char test_bytes[] = { 0x42, 0x43 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_NOT_OPEN);
    umock_c_reset_all_calls();

    // act
    saved_on_bytes_received(saved_io_callback_context, test_bytes, 1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_030: [If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.] */
/* Tests_SRS_SASLCLIENTIO_01_003: [Other than using a protocol id of three, the exchange of SASL layer headers follows the same rules specified in the version negotiation section of the transport specification (See Part 2: section 2.2).] */
TEST_FUNCTION(when_io_state_is_opening_and_1_byte_is_received_it_is_used_for_the_header)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    // act
    saved_on_bytes_received(saved_io_callback_context, sasl_header, 1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_030: [If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.] */
/* Tests_SRS_SASLCLIENTIO_01_003: [Other than using a protocol id of three, the exchange of SASL layer headers follows the same rules specified in the version negotiation section of the transport specification (See Part 2: section 2.2).] */
/* Tests_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
TEST_FUNCTION(when_io_state_is_opening_and_1_bad_byte_is_received_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    unsigned char test_bytes[] = { 0x42 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_030: [If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.] */
/* Tests_SRS_SASLCLIENTIO_01_003: [Other than using a protocol id of three, the exchange of SASL layer headers follows the same rules specified in the version negotiation section of the transport specification (See Part 2: section 2.2).] */
/* Tests_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
TEST_FUNCTION(when_io_state_is_opening_and_the_last_header_byte_is_bad_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    unsigned char test_bytes[] = { 'A', 'M', 'Q', 'P', 3, 1, 0, 'x' };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

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
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    EXPECTED_CALL(xio_send(test_underlying_io, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .ValidateArgument(1).ExpectedAtLeastTimes(1);

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
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
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    EXPECTED_CALL(xio_send(test_underlying_io, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .ValidateArgument(1)
        .SetReturn(1);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_030: [If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.] */
/* Tests_SRS_SASLCLIENTIO_01_003: [Other than using a protocol id of three, the exchange of SASL layer headers follows the same rules specified in the version negotiation section of the transport specification (See Part 2: section 2.2).] */
/* Tests_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
TEST_FUNCTION(when_a_bad_header_is_received_after_a_good_one_has_been_sent_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    unsigned char test_bytes[] = { 'A', 'M', 'Q', 'P', 3, 1, 0, 'x' };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
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
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    umock_c_reset_all_calls();

    // act
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_030: [If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.] */
/* Tests_SRS_SASLCLIENTIO_01_067: [The SASL frame exchange shall be started as soon as the SASL header handshake is done.] */
/* Tests_SRS_SASLCLIENTIO_01_068: [During the SASL frame exchange that constitutes the handshake the received bytes from the underlying IO shall be fed to the frame_codec instance created in saslclientio_create by calling frame_codec_receive_bytes.] */
TEST_FUNCTION(when_one_byte_is_received_after_header_handshake_it_is_sent_to_the_frame_codec)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    unsigned char test_bytes[] = { 0x42 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    umock_c_reset_all_calls();

    EXPECTED_CALL(frame_codec_receive_bytes(test_frame_codec, IGNORED_PTR_ARG, IGNORED_NUM_ARG))
        .ValidateArgument(1).ExpectedAtLeastTimes(1);

    // act
    saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
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
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    unsigned char test_bytes[] = { 0x42 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    umock_c_reset_all_calls();

    EXPECTED_CALL(frame_codec_receive_bytes(test_frame_codec, IGNORED_PTR_ARG, IGNORED_NUM_ARG))
        .ValidateArgument(1)
        .SetReturn(1);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_bytes_received(saved_io_callback_context, test_bytes, sizeof(test_bytes));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
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
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_NOT_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_102: [raise ERROR] */
TEST_FUNCTION(ERROR_received_in_the_state_OPEN_sets_the_state_to_ERROR_and_triggers_callback)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_103: [do nothing] */
TEST_FUNCTION(ERROR_received_in_the_state_ERROR_does_nothing)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);
    umock_c_reset_all_calls();

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_109: [do nothing] */
TEST_FUNCTION(OPENING_received_in_the_state_OPENING_does_nothing)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPENING, IO_STATE_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_110: [raise ERROR] */
TEST_FUNCTION(OPENING_received_in_the_state_OPEN_raises_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPENING, IO_STATE_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_111: [do nothing] */
TEST_FUNCTION(OPENING_received_in_the_state_ERROR_does_nothing)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);
    umock_c_reset_all_calls();

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPENING, IO_STATE_ERROR);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_114: [raise ERROR] */
TEST_FUNCTION(NOT_OPEN_received_in_the_state_OPENING_raises_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_NOT_OPEN, IO_STATE_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_113: [raise ERROR] */
TEST_FUNCTION(NOT_OPEN_received_in_the_state_OPEN_raises_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_NOT_OPEN, IO_STATE_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_112: [do nothing] */
TEST_FUNCTION(NOT_OPEN_received_in_the_state_ERROR_does_nothing)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);
    umock_c_reset_all_calls();

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_NOT_OPEN, IO_STATE_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_104: [do nothing] */
TEST_FUNCTION(OPEN_received_in_the_state_NOT_OPEN_does_nothing)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);
    umock_c_reset_all_calls();

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_NOT_OPEN, IO_STATE_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_106: [do nothing] */
TEST_FUNCTION(OPEN_received_in_the_state_OPEN_does_nothing)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_OPENING);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_107: [do nothing] */
TEST_FUNCTION(OPEN_received_in_the_state_ERROR_does_nothing)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_ERROR, IO_STATE_OPEN);
    umock_c_reset_all_calls();

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_OPENING);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_077: [If sending the SASL header fails, the SASL client IO state shall be set to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_sending_the_header_with_xio_send_fails_then_the_io_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    umock_c_reset_all_calls();

    EXPECTED_CALL(xio_send(test_underlying_io, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .ValidateArgument(1).ExpectedAtLeastTimes(1).SetReturn(1);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_116: [Any underlying IO state changes to state OPEN after the header exchange has been started shall trigger no action.] */
TEST_FUNCTION(when_underlying_io_sets_the_state_to_OPEN_after_the_header_exchange_was_started_nothing_shall_be_done)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, test_logger_log);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    umock_c_reset_all_calls();

    // act
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_OPEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

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
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    uint32_t mechanisms_count = 1;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item(sasl_server_mechanisms, 0));
    STRICT_EXPECTED_CALL(amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    STRICT_EXPECTED_CALL(saslmechanism_get_mechanism_name(test_sasl_mechanism));
    SASL_MECHANISM_BYTES init_bytes = { NULL, 0 };
    STRICT_EXPECTED_CALL(sasl_init_create(test_mechanism));
    EXPECTED_CALL(saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes));
    STRICT_EXPECTED_CALL(amqpvalue_create_sasl_init(test_sasl_init_handle));
    STRICT_EXPECTED_CALL(sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_init_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(sasl_init_destroy(test_sasl_init_handle));
    STRICT_EXPECTED_CALL(amqpvalue_destroy(test_sasl_init_amqp_value));
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_047: [A block of opaque data passed to the security mechanism.] */
/* Tests_SRS_SASLCLIENTIO_01_048: [The contents of this data are defined by the SASL security mechanism.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_a_sasl_init_frame_is_send_with_the_mechanism_name_and_the_init_bytes)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char test_init_bytes[] = { 0x42, 0x43 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    uint32_t mechanisms_count = 1;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item(sasl_server_mechanisms, 0));
    STRICT_EXPECTED_CALL(amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    STRICT_EXPECTED_CALL(saslmechanism_get_mechanism_name(test_sasl_mechanism));
    SASL_MECHANISM_BYTES init_bytes = { test_init_bytes, sizeof(test_init_bytes) };
    STRICT_EXPECTED_CALL(sasl_init_create(test_mechanism));
    EXPECTED_CALL(saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes));
    STRICT_EXPECTED_CALL(amqpvalue_create_sasl_init(test_sasl_init_handle));
    amqp_binary expected_creds = { test_init_bytes, sizeof(test_init_bytes) };
    STRICT_EXPECTED_CALL(sasl_init_set_initial_response(test_sasl_init_handle, expected_creds));
    STRICT_EXPECTED_CALL(sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_init_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(sasl_init_destroy(test_sasl_init_handle));
    STRICT_EXPECTED_CALL(amqpvalue_destroy(test_sasl_init_amqp_value));
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_118: [If on_sasl_frame_received_callback is called in the OPENING state but the header exchange has not yet been completed, then the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_when_header_handshake_is_not_done_the_IO_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_117: [If on_sasl_frame_received_callback is called when the state of the IO is OPEN then the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_in_the_OPEN_state_the_IO_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_getting_the_descriptor_fails_the_IO_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value))
        .SetReturn((AMQP_VALUE)NULL);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_getting_the_mechanism_name_fails_the_IO_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    uint32_t mechanisms_count = 1;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
    STRICT_EXPECTED_CALL(saslmechanism_get_mechanism_name(test_sasl_mechanism))
        .SetReturn((const char*)NULL);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_creating_the_sasl_init_value_fails_the_IO_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    uint32_t mechanisms_count = 1;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item(sasl_server_mechanisms, 0));
    STRICT_EXPECTED_CALL(amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    STRICT_EXPECTED_CALL(saslmechanism_get_mechanism_name(test_sasl_mechanism));
    SASL_MECHANISM_BYTES init_bytes = { NULL, 0 };
    STRICT_EXPECTED_CALL(sasl_init_create(test_mechanism))
        .SetReturn((SASL_INIT_HANDLE)NULL);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_getting_the_initial_bytes_fails_the_IO_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    uint32_t mechanisms_count = 1;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item(sasl_server_mechanisms, 0));
    STRICT_EXPECTED_CALL(amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    STRICT_EXPECTED_CALL(saslmechanism_get_mechanism_name(test_sasl_mechanism));
    SASL_MECHANISM_BYTES init_bytes = { NULL, 0 };
    STRICT_EXPECTED_CALL(sasl_init_create(test_mechanism));
    EXPECTED_CALL(saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(sasl_init_destroy(test_sasl_init_handle));
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_getting_the_AMQP_VALUE_fails_the_IO_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    uint32_t mechanisms_count = 1;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item(sasl_server_mechanisms, 0));
    STRICT_EXPECTED_CALL(amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    STRICT_EXPECTED_CALL(saslmechanism_get_mechanism_name(test_sasl_mechanism));
    SASL_MECHANISM_BYTES init_bytes = { NULL, 0 };
    STRICT_EXPECTED_CALL(sasl_init_create(test_mechanism));
    EXPECTED_CALL(saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes));
    STRICT_EXPECTED_CALL(amqpvalue_create_sasl_init(test_sasl_init_handle))
        .SetReturn((AMQP_VALUE)NULL);
    STRICT_EXPECTED_CALL(sasl_init_destroy(test_sasl_init_handle));
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_071: [If sasl_frame_codec_encode_frame fails, then the state of SASL client IO shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_encoding_the_sasl_frame_fails_the_IO_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    uint32_t mechanisms_count = 1;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item(sasl_server_mechanisms, 0));
    STRICT_EXPECTED_CALL(amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    STRICT_EXPECTED_CALL(saslmechanism_get_mechanism_name(test_sasl_mechanism));
    SASL_MECHANISM_BYTES init_bytes = { NULL, 0 };
    STRICT_EXPECTED_CALL(sasl_init_create(test_mechanism));
    EXPECTED_CALL(saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes));
    STRICT_EXPECTED_CALL(amqpvalue_create_sasl_init(test_sasl_init_handle));
    STRICT_EXPECTED_CALL(sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_init_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(amqpvalue_destroy(test_sasl_init_amqp_value));
    STRICT_EXPECTED_CALL(sasl_init_destroy(test_sasl_init_handle));
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_071: [If sasl_frame_codec_encode_frame fails, then the state of SASL client IO shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_a_SASL_mechanism_is_received_and_setting_the_init_bytes_fails_the_IO_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char test_init_bytes[] = { 0x42 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    uint32_t mechanisms_count = 1;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item(sasl_server_mechanisms, 0));
    STRICT_EXPECTED_CALL(amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    STRICT_EXPECTED_CALL(saslmechanism_get_mechanism_name(test_sasl_mechanism));
    SASL_MECHANISM_BYTES init_bytes = { test_init_bytes, sizeof(test_init_bytes) };
    STRICT_EXPECTED_CALL(sasl_init_create(test_mechanism));
    STRICT_EXPECTED_CALL(saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes));
    amqp_binary expected_creds = { test_init_bytes, sizeof(test_init_bytes) };
    STRICT_EXPECTED_CALL(sasl_init_set_initial_response(test_sasl_init_handle, expected_creds));
    STRICT_EXPECTED_CALL(amqpvalue_create_sasl_init(test_sasl_init_handle));
    STRICT_EXPECTED_CALL(sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_init_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(amqpvalue_destroy(test_sasl_init_amqp_value));
    STRICT_EXPECTED_CALL(sasl_init_destroy(test_sasl_init_handle));
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

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
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_outcome, sizeof(test_sasl_outcome));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_outcome_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    sasl_code sasl_outcome_code = sasl_code_ok;
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_outcome(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_outcome_handle, sizeof(test_sasl_outcome_handle));
    STRICT_EXPECTED_CALL(sasl_outcome_get_code(test_sasl_outcome_handle, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &sasl_outcome_code, sizeof(sasl_outcome_code));
    STRICT_EXPECTED_CALL(sasl_outcome_destroy(test_sasl_outcome_handle));

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_OPEN, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

void when_an_outcome_with_error_code_is_received_the_SASL_IO_state_is_set_to_ERROR(sasl_code test_sasl_code)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_outcome, sizeof(test_sasl_outcome));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_outcome_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    sasl_code sasl_outcome_code = test_sasl_code;
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_outcome(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_outcome_handle, sizeof(test_sasl_outcome_handle));
    STRICT_EXPECTED_CALL(sasl_outcome_get_code(test_sasl_outcome_handle, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &sasl_outcome_code, sizeof(sasl_outcome_code));
    STRICT_EXPECTED_CALL(sasl_outcome_destroy(test_sasl_outcome_handle));

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

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
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_outcome, sizeof(test_sasl_outcome));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_outcome_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_032: [The peer acting as the SASL server MUST announce supported authentication mechanisms using the sasl-mechanisms frame.] */
TEST_FUNCTION(when_a_SASL_challenge_is_received_before_mechanisms_the_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

static void setup_succesfull_challenge_response(void)
{

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle));
    STRICT_EXPECTED_CALL(sasl_challenge_get_challenge(test_sasl_challenge_handle, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &some_challenge_bytes, sizeof(some_challenge_bytes));
    STRICT_EXPECTED_CALL(saslmechanism_challenge(test_sasl_mechanism, &sasl_mechanism_challenge_bytes, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(3, &sasl_mechanism_response_bytes, sizeof(sasl_mechanism_response_bytes));
    STRICT_EXPECTED_CALL(sasl_response_create(response_binary_value));
    STRICT_EXPECTED_CALL(amqpvalue_create_sasl_response(test_sasl_response_handle));
    STRICT_EXPECTED_CALL(sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_response_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(sasl_response_destroy(test_sasl_response_handle));
    STRICT_EXPECTED_CALL(amqpvalue_destroy(test_sasl_response_amqp_value));
    STRICT_EXPECTED_CALL(sasl_challenge_destroy(test_sasl_challenge_handle));
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
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
    umock_c_reset_all_calls();

    setup_succesfull_challenge_response();

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_getting_the_sasl_challenge_fails_then_the_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char test_challenge_bytes[] = { 0x42 };
    unsigned char test_response_bytes[] = { 0x43, 0x44 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_getting_the_challenge_bytes_fails_then_the_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char test_challenge_bytes[] = { 0x42 };
    unsigned char test_response_bytes[] = { 0x43, 0x44 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle));
    amqp_binary some_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
    STRICT_EXPECTED_CALL(sasl_challenge_get_challenge(test_sasl_challenge_handle, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &some_challenge_bytes, sizeof(some_challenge_bytes))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
    STRICT_EXPECTED_CALL(sasl_challenge_destroy(test_sasl_challenge_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_the_sasl_mechanism_challenge_response_function_fails_then_the_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char test_challenge_bytes[] = { 0x42 };
    unsigned char test_response_bytes[] = { 0x43, 0x44 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle));
    amqp_binary some_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
    STRICT_EXPECTED_CALL(sasl_challenge_get_challenge(test_sasl_challenge_handle, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &some_challenge_bytes, sizeof(some_challenge_bytes));
    SASL_MECHANISM_BYTES sasl_mechanism_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
    SASL_MECHANISM_BYTES sasl_mechanism_response_bytes = { test_response_bytes, sizeof(test_response_bytes) };
    STRICT_EXPECTED_CALL(saslmechanism_challenge(test_sasl_mechanism, &sasl_mechanism_challenge_bytes, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(3, &sasl_mechanism_response_bytes, sizeof(sasl_mechanism_response_bytes))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
    STRICT_EXPECTED_CALL(sasl_challenge_destroy(test_sasl_challenge_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_creating_the_sasl_response_fails_the_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char test_challenge_bytes[] = { 0x42 };
    unsigned char test_response_bytes[] = { 0x43, 0x44 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle));
    amqp_binary some_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
    STRICT_EXPECTED_CALL(sasl_challenge_get_challenge(test_sasl_challenge_handle, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &some_challenge_bytes, sizeof(some_challenge_bytes));
    SASL_MECHANISM_BYTES sasl_mechanism_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
    SASL_MECHANISM_BYTES sasl_mechanism_response_bytes = { test_response_bytes, sizeof(test_response_bytes) };
    STRICT_EXPECTED_CALL(saslmechanism_challenge(test_sasl_mechanism, &sasl_mechanism_challenge_bytes, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(3, &sasl_mechanism_response_bytes, sizeof(sasl_mechanism_response_bytes));
    amqp_binary response_binary_value = { test_response_bytes, sizeof(test_response_bytes) };
    STRICT_EXPECTED_CALL(sasl_response_create(response_binary_value))
        .SetReturn((SASL_RESPONSE_HANDLE)NULL);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
    STRICT_EXPECTED_CALL(sasl_challenge_destroy(test_sasl_challenge_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_creating_the_AMQP_VALUE_for_sasl_response_fails_the_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char test_challenge_bytes[] = { 0x42 };
    unsigned char test_response_bytes[] = { 0x43, 0x44 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle));
    amqp_binary some_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
    STRICT_EXPECTED_CALL(sasl_challenge_get_challenge(test_sasl_challenge_handle, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &some_challenge_bytes, sizeof(some_challenge_bytes));
    SASL_MECHANISM_BYTES sasl_mechanism_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
    SASL_MECHANISM_BYTES sasl_mechanism_response_bytes = { test_response_bytes, sizeof(test_response_bytes) };
    STRICT_EXPECTED_CALL(saslmechanism_challenge(test_sasl_mechanism, &sasl_mechanism_challenge_bytes, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(3, &sasl_mechanism_response_bytes, sizeof(sasl_mechanism_response_bytes));
    amqp_binary response_binary_value = { test_response_bytes, sizeof(test_response_bytes) };
    STRICT_EXPECTED_CALL(sasl_response_create(response_binary_value));
    STRICT_EXPECTED_CALL(amqpvalue_create_sasl_response(test_sasl_response_handle))
        .SetReturn((AMQP_VALUE)NULL);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
    STRICT_EXPECTED_CALL(sasl_response_destroy(test_sasl_response_handle));
    STRICT_EXPECTED_CALL(sasl_challenge_destroy(test_sasl_challenge_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_encoding_the_sasl_frame_for_sasl_response_fails_the_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char test_challenge_bytes[] = { 0x42 };
    unsigned char test_response_bytes[] = { 0x43, 0x44 };
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_challenge(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_challenge_handle, sizeof(test_sasl_challenge_handle));
    amqp_binary some_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
    STRICT_EXPECTED_CALL(sasl_challenge_get_challenge(test_sasl_challenge_handle, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &some_challenge_bytes, sizeof(some_challenge_bytes));
    SASL_MECHANISM_BYTES sasl_mechanism_challenge_bytes = { test_challenge_bytes, sizeof(test_challenge_bytes) };
    SASL_MECHANISM_BYTES sasl_mechanism_response_bytes = { test_response_bytes, sizeof(test_response_bytes) };
    STRICT_EXPECTED_CALL(saslmechanism_challenge(test_sasl_mechanism, &sasl_mechanism_challenge_bytes, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(3, &sasl_mechanism_response_bytes, sizeof(sasl_mechanism_response_bytes));
    amqp_binary response_binary_value = { test_response_bytes, sizeof(test_response_bytes) };
    STRICT_EXPECTED_CALL(sasl_response_create(response_binary_value));
    STRICT_EXPECTED_CALL(amqpvalue_create_sasl_response(test_sasl_response_handle));
    STRICT_EXPECTED_CALL(sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_response_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));
    STRICT_EXPECTED_CALL(sasl_response_destroy(test_sasl_response_handle));
    STRICT_EXPECTED_CALL(amqpvalue_destroy(test_sasl_response_amqp_value));
    STRICT_EXPECTED_CALL(sasl_challenge_destroy(test_sasl_challenge_handle));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_039: [the SASL challenge/response step can occur zero or more times depending on the details of the SASL mechanism chosen.] */
TEST_FUNCTION(SASL_challenge_response_twice_succeed)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
    umock_c_reset_all_calls();

    setup_succesfull_challenge_response();
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    setup_succesfull_challenge_response();

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_039: [the SASL challenge/response step can occur zero or more times depending on the details of the SASL mechanism chosen.] */
TEST_FUNCTION(SASL_challenge_response_256_times_succeeds)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    size_t i;

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
    umock_c_reset_all_calls();

    // act
    for (i = 0; i < 256; i++)
    {
        setup_succesfull_challenge_response();
        saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    }

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_039: [the SASL challenge/response step can occur zero or more times depending on the details of the SASL mechanism chosen.] */
/* Tests_SRS_SASLCLIENTIO_01_072: [When the SASL handshake is complete, if the handshake is successful, the SASL client IO state shall be switched to IO_STATE_OPEN and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(SASL_challenge_response_256_times_followed_by_outcome_succeeds)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    size_t i;

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    saved_on_bytes_received(saved_io_callback_context, test_sasl_challenge, sizeof(test_sasl_challenge));
    umock_c_reset_all_calls();

    for (i = 0; i < 256; i++)
    {
        setup_succesfull_challenge_response();
        saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);
    }

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_challenge_type_by_descriptor(test_descriptor_value))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_sasl_outcome_type_by_descriptor(test_descriptor_value))
        .SetReturn(true);
    sasl_code sasl_outcome_code = sasl_code_ok;
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_outcome(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_outcome_handle, sizeof(test_sasl_outcome_handle));
    STRICT_EXPECTED_CALL(sasl_outcome_get_code(test_sasl_outcome_handle, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &sasl_outcome_code, sizeof(sasl_outcome_code));
    STRICT_EXPECTED_CALL(sasl_outcome_destroy(test_sasl_outcome_handle));

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_OPEN, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_the_mechanisms_sasl_value_cannot_be_decoded_the_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_042: [It is invalid for this list to be null or empty.] */
TEST_FUNCTION(when_a_NULL_list_is_received_in_the_SASL_mechanisms_then_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_042: [It is invalid for this list to be null or empty.] */
TEST_FUNCTION(when_an_empty_array_is_received_in_the_SASL_mechanisms_then_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    uint32_t mechanisms_count = 0;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_getting_the_mechanisms_array_item_count_fails_then_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    uint32_t mechanisms_count = 0;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
TEST_FUNCTION(when_the_mechanisms_array_does_not_contain_a_usable_SASL_mechanism_then_the_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(saslmechanism_get_mechanism_name(test_sasl_mechanism));
    uint32_t mechanisms_count = 1;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item(sasl_server_mechanisms, 0));
    const char* test_sasl_server_mechanism_name = "blahblah";
    STRICT_EXPECTED_CALL(amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_server_mechanism_name, sizeof(test_sasl_server_mechanism_name));
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
TEST_FUNCTION(when_the_mechanisms_array_has_2_mechanisms_and_none_matches_the_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(saslmechanism_get_mechanism_name(test_sasl_mechanism));
    uint32_t mechanisms_count = 2;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item(sasl_server_mechanisms, 0));
    const char* test_sasl_server_mechanism_name_1 = "blahblah";
    STRICT_EXPECTED_CALL(amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_server_mechanism_name_1, sizeof(test_sasl_server_mechanism_name_1));
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item(sasl_server_mechanisms, 1));
    const char* test_sasl_server_mechanism_name_2 = "another_blah";
    STRICT_EXPECTED_CALL(amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_server_mechanism_name_2, sizeof(test_sasl_server_mechanism_name_2));
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

static void setup_send_init(void)
{
    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(test_sasl_value));
    STRICT_EXPECTED_CALL(is_sasl_mechanisms_type_by_descriptor(test_descriptor_value)).SetReturn(true);
    STRICT_EXPECTED_CALL(amqpvalue_get_sasl_mechanisms(test_sasl_value, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_sasl_mechanisms_handle, sizeof(test_sasl_mechanisms_handle));
    AMQP_VALUE sasl_server_mechanisms;
    STRICT_EXPECTED_CALL(sasl_mechanisms_get_sasl_server_mechanisms(test_sasl_mechanisms_handle, IGNORED_PTR_ARG));
    uint32_t mechanisms_count = 1;
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count))
        .CopyOutArgumentBuffer(2, &mechanisms_count, sizeof(mechanisms_count));
    STRICT_EXPECTED_CALL(amqpvalue_get_array_item(sasl_server_mechanisms, 0));
    STRICT_EXPECTED_CALL(amqpvalue_get_symbol(test_sasl_server_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &test_mechanism, sizeof(test_mechanism));
    STRICT_EXPECTED_CALL(saslmechanism_get_mechanism_name(test_sasl_mechanism));
    SASL_MECHANISM_BYTES init_bytes = { NULL, 0 };
    STRICT_EXPECTED_CALL(sasl_init_create(test_mechanism));
    EXPECTED_CALL(saslmechanism_get_init_bytes(test_sasl_mechanism, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &init_bytes, sizeof(init_bytes));
    STRICT_EXPECTED_CALL(amqpvalue_create_sasl_init(test_sasl_init_handle));
    STRICT_EXPECTED_CALL(sasl_frame_codec_encode_frame(test_sasl_frame_codec, test_sasl_init_amqp_value, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(sasl_init_destroy(test_sasl_init_handle));
    STRICT_EXPECTED_CALL(amqpvalue_destroy(test_sasl_init_amqp_value));
    STRICT_EXPECTED_CALL(sasl_mechanisms_destroy(test_sasl_mechanisms_handle));
}

/* Tests_SRS_SASLCLIENTIO_01_120: [When SASL client IO is notified by sasl_frame_codec of bytes that have been encoded via the on_bytes_encoded callback and SASL client IO is in the state OPENING, SASL client IO shall send these bytes by using xio_send.] */
TEST_FUNCTION(when_encoded_bytes_are_received_they_are_given_to_xio_send)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char encoded_bytes[] = { 0x42, 0x43 };

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    setup_send_init();
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    STRICT_EXPECTED_CALL(xio_send(test_underlying_io, encoded_bytes, sizeof(encoded_bytes), NULL, NULL))
        .ValidateArgumentBuffer(2, &encoded_bytes, sizeof(encoded_bytes));

    // act
    saved_on_bytes_encoded(saved_on_bytes_encoded_callback_context, encoded_bytes, sizeof(encoded_bytes), true);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_120: [When SASL client IO is notified by sasl_frame_codec of bytes that have been encoded via the on_bytes_encoded callback and SASL client IO is in the state OPENING, SASL client IO shall send these bytes by using xio_send.] */
TEST_FUNCTION(when_encoded_bytes_are_received_with_encoded_complete_flag_set_to_false_they_are_given_to_xio_send)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char encoded_bytes[] = { 0x42, 0x43 };

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    setup_send_init();
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    STRICT_EXPECTED_CALL(xio_send(test_underlying_io, encoded_bytes, sizeof(encoded_bytes), NULL, NULL))
        .ValidateArgumentBuffer(2, &encoded_bytes, sizeof(encoded_bytes));

    // act
    saved_on_bytes_encoded(saved_on_bytes_encoded_callback_context, encoded_bytes, sizeof(encoded_bytes), false);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_121: [If xio_send fails, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_xio_send_fails_when_sending_encoded_bytes_then_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    unsigned char encoded_bytes[] = { 0x42, 0x43 };

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    setup_send_init();
    saved_on_sasl_frame_received(saved_sasl_frame_codec_callback_context, test_sasl_value);

    STRICT_EXPECTED_CALL(xio_send(test_underlying_io, encoded_bytes, sizeof(encoded_bytes), NULL, NULL))
        .ValidateArgumentBuffer(2, &encoded_bytes, sizeof(encoded_bytes))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_bytes_encoded(saved_on_bytes_encoded_callback_context, encoded_bytes, sizeof(encoded_bytes), false);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_122: [When on_frame_codec_error is called while in the OPENING or OPEN state the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_the_frame_codec_triggers_an_error_in_the_OPENING_state_the_saslclientio_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_frame_codec_error(saved_on_frame_codec_error_callback_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_122: [When on_frame_codec_error is called while in the OPENING or OPEN state the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_the_frame_codec_triggers_an_error_in_the_OPEN_state_the_saslclientio_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

    // act
    saved_on_frame_codec_error(saved_on_frame_codec_error_callback_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_123: [When on_frame_codec_error is called in the ERROR state nothing shall be done.] */
TEST_FUNCTION(when_the_frame_codec_triggers_an_error_in_the_ERROR_state_nothing_is_done)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    saved_on_frame_codec_error(saved_sasl_frame_codec_callback_context);
    umock_c_reset_all_calls();

    // act
    saved_on_frame_codec_error(saved_on_frame_codec_error_callback_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_124: [When on_sasl_frame_codec_error is called while in the OPENING or OPEN state the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_the_sasl_frame_codec_triggers_an_error_in_the_OPENING_state_the_saslclientio_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);

    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    saved_io_state_changed(saved_io_callback_context, IO_STATE_OPEN, IO_STATE_NOT_OPEN);
    saved_on_bytes_received(saved_io_callback_context, sasl_header, sizeof(sasl_header));
    saved_on_bytes_received(saved_io_callback_context, test_sasl_mechanisms_frame, sizeof(test_sasl_mechanisms_frame));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPENING));

    // act
    saved_on_sasl_frame_codec_error(saved_sasl_frame_codec_callback_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_124: [When on_sasl_frame_codec_error is called while in the OPENING or OPEN state the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
TEST_FUNCTION(when_the_sasl_frame_codec_triggers_an_error_in_the_OPEN_state_the_saslclientio_state_is_set_to_ERROR)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_on_io_state_changed(test_context, IO_STATE_ERROR, IO_STATE_OPEN));

    // act
    saved_on_sasl_frame_codec_error(saved_sasl_frame_codec_callback_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}

/* Tests_SRS_SASLCLIENTIO_01_125: [When on_sasl_frame_codec_error is called in the ERROR state nothing shall be done.] */
TEST_FUNCTION(when_the_sasl_frame_codec_triggers_an_error_in_the_ERROR_state_nothing_is_done)
{
    // arrange
    SASLCLIENTIO_CONFIG saslclientio_config = { test_underlying_io, test_sasl_mechanism };
    CONCRETE_IO_HANDLE sasl_io = saslclientio_create(&saslclientio_config, NULL);
    (void)saslclientio_open(sasl_io, test_on_io_open_complete, test_on_bytes_received, test_on_io_error, test_context);
    setup_successful_sasl_handshake(sasl_io);
    saved_on_sasl_frame_codec_error(saved_sasl_frame_codec_callback_context);
    umock_c_reset_all_calls();

    // act
    saved_on_sasl_frame_codec_error(saved_sasl_frame_codec_callback_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    saslclientio_destroy(sasl_io);
}
#endif

END_TEST_SUITE(saslclientio_ut)
