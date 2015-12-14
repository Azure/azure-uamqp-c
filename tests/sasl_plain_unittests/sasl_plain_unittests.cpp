// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <cstdint>
#include "testrunnerswitcher.h"
#include "micromock.h"
#include "micromockcharstararenullterminatedstrings.h"
#include "amqpalloc.h"
#include "sasl_plain.h"
#include "logger.h"

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

BEGIN_TEST_SUITE(sasl_plain_unittests)

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

/* saslplain_create */

/* Tests_SRS_SASL_PLAIN_01_001: [saslplain_create shall return on success a non-NULL handle to a new SASL plain mechanism.] */
TEST_FUNCTION(saslplain_create_with_valid_args_succeeds)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	SASL_PLAIN_CONFIG sasl_plain_config = { "test_authcid", "test_pwd", "test_authzid" };

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));

	// act
	CONCRETE_SASL_MECHANISM_HANDLE result = saslplain_create(&sasl_plain_config);

	// assert
	ASSERT_IS_NOT_NULL(result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslplain_destroy(result);
}

/* Tests_SRS_SASL_PLAIN_01_002: [If allocating the memory needed for the saslplain instance fails then saslplain_create shall return NULL.] */
TEST_FUNCTION(when_allocating_memory_fails_then_saslplain_create_fails)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	SASL_PLAIN_CONFIG sasl_plain_config = { "test_authcid", "test_pwd", "test_authzid" };

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG))
		.SetReturn((void*)NULL);

	// act
	CONCRETE_SASL_MECHANISM_HANDLE result = saslplain_create(&sasl_plain_config);

	// assert
	ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASL_PLAIN_01_002: [If allocating the memory needed for the saslplain instance fails then saslplain_create shall return NULL.] */
TEST_FUNCTION(when_allocating_memory_for_the_config_fails_then_saslplain_create_fails)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	SASL_PLAIN_CONFIG sasl_plain_config = { "test_authcid", "test_pwd", "test_authzid" };

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG))
		.SetReturn((void*)NULL);
	EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));

	// act
	CONCRETE_SASL_MECHANISM_HANDLE result = saslplain_create(&sasl_plain_config);

	// assert
	ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASL_PLAIN_01_003: [If the config argument is NULL, then saslplain_create shall fail and return NULL.] */
TEST_FUNCTION(saslplain_create_with_NULL_config_fails)
{
	// arrange
	amqp_frame_codec_mocks mocks;

	// act
	CONCRETE_SASL_MECHANISM_HANDLE result = saslplain_create(NULL);

	// assert
	ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASL_PLAIN_01_004: [If either the authcid or passwd member of the config structure is NULL, then saslplain_create shall fail and return NULL.] */
TEST_FUNCTION(saslplain_create_with_NULL_authcid_fails)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	SASL_PLAIN_CONFIG sasl_plain_config = { NULL, "test_pwd", "test_authzid" };

	// act
	CONCRETE_SASL_MECHANISM_HANDLE result = saslplain_create(&sasl_plain_config);

	// assert
	ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASL_PLAIN_01_004: [If either the authcid or passwd member of the config structure is NULL, then saslplain_create shall fail and return NULL.] */
TEST_FUNCTION(saslplain_create_with_NULL_passwd_fails)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	SASL_PLAIN_CONFIG sasl_plain_config = { "test_authcid", NULL, "test_authzid" };

	// act
	CONCRETE_SASL_MECHANISM_HANDLE result = saslplain_create(&sasl_plain_config);

	// assert
	ASSERT_IS_NULL(result);
}

/* Tests_SRS_SASL_PLAIN_01_004: [If either the authcid or passwd member of the config structure is NULL, then saslplain_create shall fail and return NULL.] */
TEST_FUNCTION(saslplain_create_with_NULL_authzis_succeeds)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	SASL_PLAIN_CONFIG sasl_plain_config = { "test_authcid", "test_pwd", NULL };

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));

	// act
	CONCRETE_SASL_MECHANISM_HANDLE result = saslplain_create(&sasl_plain_config);

	// assert
	ASSERT_IS_NOT_NULL(result);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	saslplain_destroy(result);
}

/* saslplain_destroy */

/* Tests_SRS_SASL_PLAIN_01_005: [saslplain_destroy shall free all resources associated with the SASL mechanism.] */
TEST_FUNCTION(saslplain_destroy_frees_the_allocated_memory)
{
	// arrange
	amqp_frame_codec_mocks mocks;
	SASL_PLAIN_CONFIG sasl_plain_config = { "test_authcid", "test_pwd", NULL };
	CONCRETE_SASL_MECHANISM_HANDLE sasl_plain = saslplain_create(&sasl_plain_config);
	mocks.ResetAllCalls();

	EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));
	EXPECTED_CALL(mocks, amqpalloc_free(IGNORED_PTR_ARG));

	// act
	saslplain_destroy(sasl_plain);

	// assert
	// uMock checks the calls
}

/* Tests_SRS_SASL_PLAIN_01_006: [If the argument concrete_sasl_mechanism is NULL, saslplain_destroy shall do nothing.] */
TEST_FUNCTION(saslplain_destroy_with_NULL_handle_does_nothing)
{
	// arrange
	amqp_frame_codec_mocks mocks;

	// act
	saslplain_destroy(NULL);

	// assert
	// uMock checks the calls
}

END_TEST_SUITE(sasl_plain_unittests)
