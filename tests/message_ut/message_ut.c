// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cstddef>
#else
#include <stdlib.h>
#include <stddef.h>
#endif
#include "testrunnerswitcher.h"
#include "umock_c.h"

void* my_gballoc_malloc(size_t size)
{
    return malloc(size);
}

void* my_gballoc_realloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

void my_gballoc_free(void* ptr)
{
    free(ptr);
}

#define ENABLE_MOCKS

#include "azure_c_shared_utility/gballoc.h"
#include "azure_uamqp_c/amqpvalue.h"
#include "azure_uamqp_c/amqp_definitions.h"

#undef ENABLE_MOCKS

#include "azure_uamqp_c/message.h"

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

static TEST_MUTEX_HANDLE g_testByTest;
static TEST_MUTEX_HANDLE g_dllByDll;

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

BEGIN_TEST_SUITE(message_ut)

TEST_SUITE_INITIALIZE(suite_init)
{
    TEST_INITIALIZE_MEMORY_DEBUG(g_dllByDll);
    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    umock_c_init(on_umock_c_error);

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, my_gballoc_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_realloc, my_gballoc_realloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, my_gballoc_free);
    REGISTER_UMOCK_ALIAS_TYPE(HEADER_HANDLE, void*);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_deinit();

    TEST_MUTEX_DESTROY(g_testByTest);
    TEST_DEINITIALIZE_MEMORY_DEBUG(g_dllByDll);
}

TEST_FUNCTION_INITIALIZE(test_init)
{
    if (TEST_MUTEX_ACQUIRE(g_testByTest))
    {
        ASSERT_FAIL("our mutex is ABANDONED. Failure in test framework");
    }

    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(test_cleanup)
{
    TEST_MUTEX_RELEASE(g_testByTest);
}

/* message_create */

/* Tests_SRS_MESSAGE_01_001: [message_create shall create a new AMQP message instance and on success it shall return a non-NULL handle for the newly created message instance.] */
TEST_FUNCTION(message_create_succeeds)
{
	// arrange
	EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));

	// act
	MESSAGE_HANDLE message = message_create();

	// assert
	ASSERT_IS_NOT_NULL(message);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

	// cleanup
	message_destroy(message);
}

/* Tests_SRS_MESSAGE_01_001: [message_create shall create a new AMQP message instance and on success it shall return a non-NULL handle for the newly created message instance.] */
TEST_FUNCTION(message_create_2_times_yields_2_different_message_instances)
{
	// arrange
	EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
	EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));

	// act
	MESSAGE_HANDLE message1 = message_create();
	MESSAGE_HANDLE message2 = message_create();

	// assert
	ASSERT_IS_NOT_NULL(message1);
	ASSERT_IS_NOT_NULL(message2);
	ASSERT_ARE_NOT_EQUAL(void_ptr, message1, message2);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

	// cleanup
	message_destroy(message1);
	message_destroy(message2);
}

/* Tests_SRS_MESSAGE_01_002: [If allocating memory for the message fails, message_create shall fail and return NULL.] */
TEST_FUNCTION(when_allocating_memory_for_the_message_fails_then_message_create_fails)
{
	// arrange
	EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG))
		.SetReturn(NULL);

	// act
	MESSAGE_HANDLE message = message_create();

	// assert
	ASSERT_IS_NULL(message);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
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
/*	MESSAGE_HANDLE source_message = message_create();
	unsigned char data_section[2] = { 0x42, 0x43 };

	(void)message_set_header(source_message, custom_message_header);
	STRICT_EXPECTED_CALL(annotations_clone(custom_delivery_annotations))
		.SetReturn(cloned_delivery_annotations);
	(void)message_set_delivery_annotations(source_message, custom_delivery_annotations);
	STRICT_EXPECTED_CALL(annotations_clone(custom_message_annotations))
		.SetReturn(cloned_message_annotations);
	(void)message_set_message_annotations(source_message, custom_message_annotations);
	STRICT_EXPECTED_CALL(amqpvalue_clone(custom_properties))
		.SetReturn(cloned_properties);
	(void)message_set_properties(source_message, custom_properties);
	STRICT_EXPECTED_CALL(amqpvalue_clone(custom_application_properties))
		.SetReturn(cloned_application_properties);
	(void)message_set_application_properties(source_message, custom_application_properties);
	STRICT_EXPECTED_CALL(annotations_clone(custom_footer))
		.SetReturn(cloned_footer);
	(void)message_set_footer(source_message, custom_footer);
	BINARY_DATA binary_data = { data_section, sizeof(data_section) };
	(void)message_add_body_amqp_data(source_message, binary_data);
	mocks.ResetAllCalls();
	definition_mocks.ResetAllCalls();

	EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
	STRICT_EXPECTED_CALL(header_clone(test_header_handle));
	STRICT_EXPECTED_CALL(annotations_clone(cloned_delivery_annotations));
	STRICT_EXPECTED_CALL(annotations_clone(cloned_message_annotations));
	STRICT_EXPECTED_CALL(properties_clone(test_properties_handle));
	STRICT_EXPECTED_CALL(amqpvalue_clone(cloned_application_properties));
	STRICT_EXPECTED_CALL(annotations_clone(cloned_footer));
	EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
	STRICT_EXPECTED_CALL(gballoc_malloc(sizeof(data_section)));

	// act
	MESSAGE_HANDLE message = message_clone(source_message);

	// assert
	ASSERT_IS_NOT_NULL(message);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

	// cleanup
	message_destroy(source_message);
	message_destroy(message);*/
}

/* Tests_SRS_MESSAGE_01_062: [If source_message is NULL, message_clone shall fail and return NULL.] */
TEST_FUNCTION(message_clone_with_NULL_message_source_fails)
{
	// arrange

	// act
	MESSAGE_HANDLE message = message_clone(NULL);

	// assert
	ASSERT_IS_NULL(message);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_MESSAGE_01_004: [If allocating memory for the new cloned message fails, message_clone shall fail and return NULL.] */
TEST_FUNCTION(when_allocating_memory_fails_then_message_clone_fails)
{
	// arrange
	MESSAGE_HANDLE source_message = message_create();
	(void)message_set_header(source_message, custom_message_header);
    umock_c_reset_all_calls();

	EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG))
		.SetReturn(NULL);

	// act
	MESSAGE_HANDLE message = message_clone(source_message);

	// assert
	ASSERT_IS_NULL(message);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

	// cleanup
	message_destroy(source_message);
}

END_TEST_SUITE(message_ut)
