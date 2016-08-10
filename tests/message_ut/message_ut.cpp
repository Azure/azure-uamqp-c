// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "testrunnerswitcher.h"
#include "micromock.h"
#include "micromockcharstararenullterminatedstrings.h"
#include "azure_uamqp_c/message.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_c_shared_utility/socketio.h"
#include "azure_uamqp_c/frame_codec.h"
#include "azure_uamqp_c/amqp_frame_codec.h"
#include "azure_uamqp_c/amqp_definitions.h"
#include "amqp_definitions_mocks.h"

static const HEADER_HANDLE custom_message_header = (HEADER_HANDLE)0x4242;
static const AMQP_VALUE custom_delivery_annotations = (AMQP_VALUE)0x4243;
static const AMQP_VALUE custom_message_annotations = (AMQP_VALUE)0x4244;
static const AMQP_VALUE cloned_delivery_annotations = (AMQP_VALUE)0x4245;
static const AMQP_VALUE cloned_message_annotations = (AMQP_VALUE)0x4246;
static const AMQP_VALUE custom_properties = (AMQP_VALUE)0x4247;
static const AMQP_VALUE cloned_properties = (AMQP_VALUE)0x4248;
static const AMQP_VALUE custom_application_properties = (AMQP_VALUE)0x4249;
static const AMQP_VALUE cloned_application_properties = (AMQP_VALUE)0x4250;
static const AMQP_VALUE custom_footer = (AMQP_VALUE)0x4251;
static const AMQP_VALUE cloned_footer = (AMQP_VALUE)0x4252;
static const AMQP_VALUE test_cloned_amqp_value = (AMQP_VALUE)0x4300;

TYPED_MOCK_CLASS(message_mocks, CGlobalMock)
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

	/* amqpvalue mocks */
	MOCK_STATIC_METHOD_1(, AMQP_VALUE, amqpvalue_clone, AMQP_VALUE, value)
	MOCK_METHOD_END(AMQP_VALUE, test_cloned_amqp_value);
	MOCK_STATIC_METHOD_1(, void, amqpvalue_destroy, AMQP_VALUE, value)
	MOCK_VOID_METHOD_END();
	MOCK_STATIC_METHOD_2(, int, amqpvalue_get_list_item_count, AMQP_VALUE, value, uint32_t*, size)
	MOCK_METHOD_END(int, 0);
};

extern "C"
{
	DECLARE_GLOBAL_MOCK_METHOD_1(message_mocks, , void*, amqpalloc_malloc, size_t, size);
	DECLARE_GLOBAL_MOCK_METHOD_2(message_mocks, , void*, amqpalloc_realloc, void*, ptr, size_t, size);
	DECLARE_GLOBAL_MOCK_METHOD_1(message_mocks, , void, amqpalloc_free, void*, ptr);

	DECLARE_GLOBAL_MOCK_METHOD_1(message_mocks, , AMQP_VALUE, amqpvalue_clone, AMQP_VALUE, value);
	DECLARE_GLOBAL_MOCK_METHOD_1(message_mocks, , void, amqpvalue_destroy, AMQP_VALUE, value);
	DECLARE_GLOBAL_MOCK_METHOD_2(message_mocks, , int, amqpvalue_get_list_item_count, AMQP_VALUE, value, uint32_t*, size);
}

MICROMOCK_MUTEX_HANDLE test_serialize_mutex;

BEGIN_TEST_SUITE(message_ut)

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

/* message_create */

/* Tests_SRS_MESSAGE_01_001: [message_create shall create a new AMQP message instance and on success it shall return a non-NULL handle for the newly created message instance.] */
TEST_FUNCTION(message_create_succeeds)
{
	// arrange
	message_mocks mocks;
	amqp_definitions_mocks definition_mocks;

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));

	// act
	MESSAGE_HANDLE message = message_create();

	// assert
	ASSERT_IS_NOT_NULL(message);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	message_destroy(message);
}

/* Tests_SRS_MESSAGE_01_001: [message_create shall create a new AMQP message instance and on success it shall return a non-NULL handle for the newly created message instance.] */
TEST_FUNCTION(message_create_2_times_yields_2_different_message_instances)
{
	// arrange
	message_mocks mocks;
	amqp_definitions_mocks definition_mocks;

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));

	// act
	MESSAGE_HANDLE message1 = message_create();
	MESSAGE_HANDLE message2 = message_create();

	// assert
	ASSERT_IS_NOT_NULL(message1);
	ASSERT_IS_NOT_NULL(message2);
	ASSERT_ARE_NOT_EQUAL(void_ptr, message1, message2);
	mocks.AssertActualAndExpectedCalls();

	// cleanup
	message_destroy(message1);
	message_destroy(message2);
}

/* Tests_SRS_MESSAGE_01_002: [If allocating memory for the message fails, message_create shall fail and return NULL.] */
TEST_FUNCTION(when_allocating_memory_for_the_message_fails_then_message_create_fails)
{
	// arrange
	message_mocks mocks;
	amqp_definitions_mocks definition_mocks;

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG))
		.SetReturn((void*)NULL);

	// act
	MESSAGE_HANDLE message = message_create();

	// assert
	ASSERT_IS_NULL(message);
}

/* message_clone */

/* Tests_SRS_MESSAGE_01_003: [message_clone shall clone a message entirely and on success return a non-NULL handle to the cloned message.] */
/* Tests_SRS_MESSAGE_01_005: [If a header exists on the source message it shall be cloned by using header_clone.] */
/* Tests_SRS_MESSAGE_01_006: [If delivery annotations exist on the source message they shall be cloned by using annotations_clone.] */
/* Tests_SRS_MESSAGE_01_007: [If message annotations exist on the source message they shall be cloned by using annotations_clone.] */
/* Tests_SRS_MESSAGE_01_008: [If message properties exist on the source message they shall be cloned by using properties_clone.] */
/* Tests_SRS_MESSAGE_01_009: [If application properties exist on the source message they shall be cloned by using amqpvalue_clone.] */
/* Tests_SRS_MESSAGE_01_010: [If a footer exists on the source message it shall be cloned by using annotations_clone.] */
/* Tests_SRS_MESSAGE_01_011: [If an AMQP data has been set as message body on the source message it shall be cloned by allocating memory for the binary payload.] */
TEST_FUNCTION(message_clone_with_a_valid_argument_succeeds)
{
	// arrange
/*	message_mocks mocks;
	amqp_definitions_mocks definition_mocks;
	MESSAGE_HANDLE source_message = message_create();
	unsigned char data_section[2] = { 0x42, 0x43 };

	(void)message_set_header(source_message, custom_message_header);
	STRICT_EXPECTED_CALL(mocks, annotations_clone(custom_delivery_annotations))
		.SetReturn(cloned_delivery_annotations);
	(void)message_set_delivery_annotations(source_message, custom_delivery_annotations);
	STRICT_EXPECTED_CALL(mocks, annotations_clone(custom_message_annotations))
		.SetReturn(cloned_message_annotations);
	(void)message_set_message_annotations(source_message, custom_message_annotations);
	STRICT_EXPECTED_CALL(mocks, amqpvalue_clone(custom_properties))
		.SetReturn(cloned_properties);
	(void)message_set_properties(source_message, custom_properties);
	STRICT_EXPECTED_CALL(mocks, amqpvalue_clone(custom_application_properties))
		.SetReturn(cloned_application_properties);
	(void)message_set_application_properties(source_message, custom_application_properties);
	STRICT_EXPECTED_CALL(mocks, annotations_clone(custom_footer))
		.SetReturn(cloned_footer);
	(void)message_set_footer(source_message, custom_footer);
	BINARY_DATA binary_data = { data_section, sizeof(data_section) };
	(void)message_add_body_amqp_data(source_message, binary_data);
	mocks.ResetAllCalls();
	definition_mocks.ResetAllCalls();

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
	STRICT_EXPECTED_CALL(definition_mocks, header_clone(test_header_handle));
	STRICT_EXPECTED_CALL(mocks, annotations_clone(cloned_delivery_annotations));
	STRICT_EXPECTED_CALL(mocks, annotations_clone(cloned_message_annotations));
	STRICT_EXPECTED_CALL(definition_mocks, properties_clone(test_properties_handle));
	STRICT_EXPECTED_CALL(mocks, amqpvalue_clone(cloned_application_properties));
	STRICT_EXPECTED_CALL(mocks, annotations_clone(cloned_footer));
	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG));
	STRICT_EXPECTED_CALL(mocks, amqpalloc_malloc(sizeof(data_section)));

	// act
	MESSAGE_HANDLE message = message_clone(source_message);

	// assert
	ASSERT_IS_NOT_NULL(message);
	mocks.AssertActualAndExpectedCalls();
	definition_mocks.AssertActualAndExpectedCalls();

	// cleanup
	message_destroy(source_message);
	message_destroy(message);*/
}

/* Tests_SRS_MESSAGE_01_062: [If source_message is NULL, message_clone shall fail and return NULL.] */
TEST_FUNCTION(message_clone_with_NULL_message_source_fails)
{
	// arrange
	message_mocks mocks;
	amqp_definitions_mocks definition_mocks;

	// act
	MESSAGE_HANDLE message = message_clone(NULL);

	// assert
	ASSERT_IS_NULL(message);
}

/* Tests_SRS_MESSAGE_01_004: [If allocating memory for the new cloned message fails, message_clone shall fail and return NULL.] */
TEST_FUNCTION(when_allocating_memory_fails_then_message_clone_fails)
{
	// arrange
	message_mocks mocks;
	amqp_definitions_mocks definition_mocks;
	MESSAGE_HANDLE source_message = message_create();
	(void)message_set_header(source_message, custom_message_header);
	mocks.ResetAllCalls();
	definition_mocks.ResetAllCalls();

	EXPECTED_CALL(mocks, amqpalloc_malloc(IGNORED_NUM_ARG))
		.SetReturn((void*)NULL);

	// act
	MESSAGE_HANDLE message = message_clone(source_message);

	// assert
	ASSERT_IS_NULL(message);
	mocks.AssertActualAndExpectedCalls();
	definition_mocks.AssertActualAndExpectedCalls();

	// cleanup
	message_destroy(source_message);
}

END_TEST_SUITE(message_ut)
