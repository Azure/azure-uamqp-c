// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <cstdint>
#include "testrunnerswitcher.h"
#include "micromock.h"
#include "micromockcharstararenullterminatedstrings.h"
#include "azure_uamqp_c/sasl_anonymous.h"

TYPED_MOCK_CLASS(amqp_frame_codec_mocks, CGlobalMock)
{
public:
	/* amqpalloc mocks */
	MOCK_STATIC_METHOD_1(, void*, amqpalloc_malloc, size_t, size)
	MOCK_METHOD_END(void*, malloc(size));
	MOCK_STATIC_METHOD_1(, void, amqpalloc_free, void*, ptr)
		free(ptr);
	MOCK_VOID_METHOD_END();
};

extern "C"
{
	DECLARE_GLOBAL_MOCK_METHOD_1(amqp_frame_codec_mocks, , void*, amqpalloc_malloc, size_t, size);
	DECLARE_GLOBAL_MOCK_METHOD_1(amqp_frame_codec_mocks, , void, amqpalloc_free, void*, ptr);
}

MICROMOCK_MUTEX_HANDLE test_serialize_mutex;

BEGIN_TEST_SUITE(sasl_anonymous_ut)

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
	if (!MicroMockReleaseMutex(test_serialize_mutex))
	{
		ASSERT_FAIL("Could not release test serialization mutex.");
	}
}

/* saslanonymous_create */

/* Tests_SRS_SASL_ANONYMOUS_01_001: [saslanonymous_create shall return on success a non-NULL handle to a new SASL anonymous mechanism.] */
TEST_FUNCTION(saslanonymous_create_with_valid_args_succeeds)
{
	// arrange
	amqp_frame_codec_mocks mocks;

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));

	// act
	CONCRETE_SASL_MECHANISM_HANDLE result = saslanonymous_create((void*)0x4242);

	// assert
	ASSERT_IS_NOT_NULL(result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslanonymous_destroy(result);
}

/* Tests_SRS_SASL_ANONYMOUS_01_002: [If allocating the memory needed for the saslanonymous instance fails then saslanonymous_create shall return NULL.] */
TEST_FUNCTION(when_allocating_memory_fails_then_saslanonymous_create_fails)
{
	// arrange
	amqp_frame_codec_mocks mocks;

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG))
		.SetReturn((void*)NULL);

	// act
	CONCRETE_SASL_MECHANISM_HANDLE result = saslanonymous_create((void*)0x4242);

	// assert
	ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASL_ANONYMOUS_01_003: [Since this is the ANONYMOUS SASL mechanism, config shall be ignored.] */
TEST_FUNCTION(saslanonymous_create_with_NULL_config_succeeds)
{
	// arrange
	amqp_frame_codec_mocks mocks;

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));

	// act
	CONCRETE_SASL_MECHANISM_HANDLE result = saslanonymous_create(NULL);

	// assert
	ASSERT_IS_NOT_NULL(result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslanonymous_destroy(result);
}

/* saslanonymous_destroy */

/* Tests_SRS_SASL_ANONYMOUS_01_004: [saslanonymous_destroy shall free all resources associated with the SASL mechanism.] */
TEST_FUNCTION(saslanonymous_destroy_frees_the_allocated_resources)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	CONCRETE_SASL_MECHANISM_HANDLE result = saslanonymous_create(NULL);
	mocks.ResetAllCalls();

	EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));

	// act
	saslanonymous_destroy(result);

	// assert
	// no explicit assert, uMock checks the calls
}

/* Tests_SRS_SASL_ANONYMOUS_01_005: [If the argument concrete_sasl_mechanism is NULL, saslanonymous_destroy shall do nothing.] */
TEST_FUNCTION(saslanonymous_destroy_with_NULL_argument_does_nothing)
{
	// arrange
	amqp_frame_codec_mocks mocks;

	// act
	saslanonymous_destroy(NULL);

	// assert
	// no explicit assert, uMock checks the calls
}

/* saslanonymous_get_init_bytes */

/* Tests_SRS_SASL_ANONYMOUS_01_006: [saslanonymous_get_init_bytes shall validate the concrete_sasl_mechanism argument and set the length of the init_bytes argument to be zero.] */
/* Tests_SRS_SASL_ANONYMOUS_01_012: [The bytes field of init_buffer shall be set to NULL.] */
/* Tests_SRS_SASL_ANONYMOUS_01_011: [On success saslanonymous_get_init_bytes shall return zero.] */
TEST_FUNCTION(saslannymous_get_init_bytes_sets_the_bytes_to_NULL_and_length_to_zero)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	CONCRETE_SASL_MECHANISM_HANDLE saslanonymous = saslanonymous_create(NULL);
	SASL_MECHANISM_BYTES init_bytes;
	mocks.ResetAllCalls();

	// act
	int result = saslanonymous_get_init_bytes(saslanonymous, &init_bytes);

	// assert
	ASSERT_IS_NULL(init_bytes.bytes);
	ASSERT_ARE_EQUAL(size_t, 0, init_bytes.length);
	ASSERT_ARE_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslanonymous_destroy(saslanonymous);
}

/* Tests_SRS_SASL_ANONYMOUS_01_007: [If the any argument is NULL, saslanonymous_get_init_bytes shall return a non-zero value.] */
TEST_FUNCTION(saslannymous_get_init_bytes_with_NULL_concrete_sasl_mechanism_fails)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	SASL_MECHANISM_BYTES init_bytes;
	mocks.ResetAllCalls();

	// act
	int result = saslanonymous_get_init_bytes(NULL, &init_bytes);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_SASL_ANONYMOUS_01_007: [If the any argument is NULL, saslanonymous_get_init_bytes shall return a non-zero value.] */
TEST_FUNCTION(saslannymous_get_init_bytes_with_NULL_init_bytes_fails)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	CONCRETE_SASL_MECHANISM_HANDLE saslanonymous = saslanonymous_create(NULL);
	mocks.ResetAllCalls();

	// act
	int result = saslanonymous_get_init_bytes(saslanonymous, NULL);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslanonymous_destroy(saslanonymous);
}

/* saslanonymous_get_mechanism_name */

/* Tests_SRS_SASL_ANONYMOUS_01_008: [saslanonymous_get_mechanism_name shall validate the argument concrete_sasl_mechanism and on success it shall return a pointer to the string "ANONYMOUS".] */
TEST_FUNCTION(saslanonymous_get_mechanism_name_with_non_NULL_concrete_sasl_mechanism_succeeds)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	CONCRETE_SASL_MECHANISM_HANDLE saslanonymous = saslanonymous_create(NULL);
	mocks.ResetAllCalls();

	// act
	const char* result = saslanonymous_get_mechanism_name(saslanonymous);

	// assert
	ASSERT_ARE_EQUAL(char_ptr, "ANONYMOUS", result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslanonymous_destroy(saslanonymous);
}

/* Tests_SRS_SASL_ANONYMOUS_01_009: [If the argument concrete_sasl_mechanism is NULL, saslanonymous_get_mechanism_name shall return NULL.] */
TEST_FUNCTION(saslanonymous_get_mechanism_name_with_NULL_concrete_sasl_mechanism_fails)
{
	// arrange
	amqp_frame_codec_mocks mocks;

	// act
	const char* result = saslanonymous_get_mechanism_name(NULL);

	// assert
	ASSERT_IS_NULL(result);
}

/* saslanonymous_challenge */

/* Tests_SRS_SASL_ANONYMOUS_01_013: [saslanonymous_challenge shall set the response_bytes buffer to NULL and 0 size as the ANONYMOUS SASL mechanism does not implement challenge/response.] */
/* Tests_SRS_SASL_ANONYMOUS_01_014: [On success, saslanonymous_challenge shall return 0.] */
TEST_FUNCTION(saslanonymous_challenge_returns_a_NULL_response_bytes_buffer)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	CONCRETE_SASL_MECHANISM_HANDLE saslanonymous = saslanonymous_create(NULL);
	SASL_MECHANISM_BYTES challenge_bytes;
	SASL_MECHANISM_BYTES response_bytes;
	mocks.ResetAllCalls();

	// act
	int result = saslanonymous_challenge(saslanonymous, &challenge_bytes, &response_bytes);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	ASSERT_IS_NULL(response_bytes.bytes);
	ASSERT_ARE_EQUAL(size_t, 0, response_bytes.length);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslanonymous_destroy(saslanonymous);
}

/* Tests_SRS_SASL_ANONYMOUS_01_014: [On success, saslanonymous_challenge shall return 0.] */
TEST_FUNCTION(saslanonymous_with_NULL_challenge_bytes_returns_a_NULL_response_bytes_buffer)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	CONCRETE_SASL_MECHANISM_HANDLE saslanonymous = saslanonymous_create(NULL);
	SASL_MECHANISM_BYTES response_bytes;
	mocks.ResetAllCalls();

	// act
	int result = saslanonymous_challenge(saslanonymous, NULL, &response_bytes);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	ASSERT_IS_NULL(response_bytes.bytes);
	ASSERT_ARE_EQUAL(size_t, 0, response_bytes.length);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslanonymous_destroy(saslanonymous);
}

/* Tests_SRS_SASL_ANONYMOUS_01_015: [If the concrete_sasl_mechanism or response_bytes argument is NULL then saslanonymous_challenge shall fail and return a non-zero value.] */
TEST_FUNCTION(saslanonymous_challenge_with_NULL_handle_fails)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	SASL_MECHANISM_BYTES challenge_bytes;
	SASL_MECHANISM_BYTES response_bytes;

	// act
	int result = saslanonymous_challenge(NULL, &challenge_bytes, &response_bytes);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_SASL_ANONYMOUS_01_015: [If the concrete_sasl_mechanism or response_bytes argument is NULL then saslanonymous_challenge shall fail and return a non-zero value.] */
TEST_FUNCTION(saslanonymous_challenge_with_NULL_response_bytes_fails)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	CONCRETE_SASL_MECHANISM_HANDLE saslanonymous = saslanonymous_create(NULL);
	SASL_MECHANISM_BYTES challenge_bytes;
	mocks.ResetAllCalls();

	// act
	int result = saslanonymous_challenge(saslanonymous, &challenge_bytes, NULL);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslanonymous_destroy(saslanonymous);
}

/* saslanonymous_get_interface */

/* Tests_SRS_SASL_ANONYMOUS_01_010: [saslanonymous_get_interface shall return a pointer to a SASL_MECHANISM_INTERFACE_DESCRIPTION  structure that contains pointers to the functions: saslanonymous_create, saslanonymous_destroy, saslanonymous_get_init_bytes, saslanonymous_get_mechanism_name, saslanonymous_challenge.] */
TEST_FUNCTION(saslanonymous_get_interface_returns_the_sasl_anonymous_mechanism_interface)
{
	// arrange
	amqp_frame_codec_mocks mocks;

	// act
	const SASL_MECHANISM_INTERFACE_DESCRIPTION* result = saslanonymous_get_interface();

	// assert
	ASSERT_ARE_EQUAL(void_ptr, (void_ptr)saslanonymous_create, (void_ptr)result->concrete_sasl_mechanism_create);
	ASSERT_ARE_EQUAL(void_ptr, (void_ptr)saslanonymous_destroy, (void_ptr)result->concrete_sasl_mechanism_destroy);
	ASSERT_ARE_EQUAL(void_ptr, (void_ptr)saslanonymous_get_init_bytes, (void_ptr)result->concrete_sasl_mechanism_get_init_bytes);
	ASSERT_ARE_EQUAL(void_ptr, (void_ptr)saslanonymous_get_mechanism_name, (void_ptr)result->concrete_sasl_mechanism_get_mechanism_name);
	ASSERT_ARE_EQUAL(void_ptr, (void_ptr)saslanonymous_challenge, (void_ptr)result->concrete_sasl_mechanism_challenge);
}

END_TEST_SUITE(sasl_anonymous_ut)
