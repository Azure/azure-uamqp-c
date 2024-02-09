// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#else
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#endif

#include "azure_macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"
#include "umock_c/umock_c.h"
#include "umock_c/umock_c_negative_tests.h"
#include "umock_c/umocktypes_bool.h"

static void* my_gballoc_malloc(size_t size)
{
    return malloc(size);
}

static void* my_gballoc_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

static void* my_gballoc_realloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

static void my_gballoc_free(void* ptr)
{
    free(ptr);
}

#define ENABLE_MOCKS

#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/amqpvalue.h"
#include "azure_uamqp_c/amqp_definitions.h"
#include "azure_uamqp_c/amqp_frame_codec.h"
#include "azure_uamqp_c/async_operation.h"

#undef ENABLE_MOCKS

#include "azure_uamqp_c/link.h"

static SESSION_HANDLE TEST_SESSION_HANDLE = (SESSION_HANDLE)0x4000;
const char* TEST_LINK_NAME_1 = "test_link_name_1";
static TICK_COUNTER_HANDLE TEST_TICK_COUNTER_HANDLE = (TICK_COUNTER_HANDLE)0x4001;
static SINGLYLINKEDLIST_HANDLE TEST_SINGLYLINKEDLIST_HANDLE = (SINGLYLINKEDLIST_HANDLE)0x4002;
static LINK_ENDPOINT_HANDLE TEST_LINK_ENDPOINT = (LINK_ENDPOINT_HANDLE)0x4003;
const AMQP_VALUE TEST_LINK_SOURCE = (AMQP_VALUE)0x4004;
const AMQP_VALUE TEST_LINK_TARGET = (AMQP_VALUE)0x4005;

static TEST_MUTEX_HANDLE g_testByTest;

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

static int umocktypes_copy_bool_ptr(bool** destination, const bool** source)
{
    int result;

    *destination = (bool*)my_gballoc_malloc(sizeof(bool));
    if (*destination == NULL)
    {
        result = MU_FAILURE;
    }
    else
    {
        *(*destination) = *(*source);

        result = 0;
    }

    return result;
}

static void umocktypes_free_bool_ptr(bool** value)
{
    if (*value != NULL)
    {
        my_gballoc_free(*value);
    }
}

static char* umocktypes_stringify_bool_ptr(const bool** value)
{
    char* result;

    result = (char*)my_gballoc_malloc(8);
    if (result != NULL)
    {
        if (*value == NULL)
        {
            (void)strcpy(result, "{NULL}");
        }
        else if (*(*value) == true)
        {
            (void)strcpy(result, "{true}");
        }
        else
        {
            (void)strcpy(result, "{false}");
        }
    }

    return result;
}

static int umocktypes_are_equal_bool_ptr(bool** left, bool** right)
{
    int result;

    if (*left == *right)
    {
        result = 1;
    }
    else
    {
        if (*(*left) == *(*right))
        {
            result = 1;
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

static int umocktypes_copy_FLOW_HANDLE(FLOW_HANDLE* destination, const FLOW_HANDLE* source)
{
    int result = 0;

    *(destination) = *(source);

    return result;
}

static void umocktypes_free_FLOW_HANDLE(FLOW_HANDLE* value)
{
    (void)value;
}

static char* umocktypes_stringify_FLOW_HANDLE(const FLOW_HANDLE* value)
{
    char temp_buffer[32];
    char* result;
    size_t length = sprintf(temp_buffer, "%p", (void*)*value);
    if (length < 0)
    {
        result = NULL;
    }
    else
    {
        result = (char*)malloc(length + 1);
        if (result != NULL)
        {
            (void)memcpy(result, temp_buffer, length + 1);
        }
    }
    return result;
}

static int umocktypes_are_equal_FLOW_HANDLE(FLOW_HANDLE* left, FLOW_HANDLE* right)
{
    int result;

    if (*left == *right)
    {
        result = 1;
    }
    else
    {
        result = 0;
    }

    return result;
}

static TRANSFER_HANDLE test_on_transfer_received_transfer;
static uint32_t test_on_transfer_received_payload_size;
static unsigned char test_on_transfer_received_payload_bytes[2048];
static AMQP_VALUE test_on_transfer_received(void* context, TRANSFER_HANDLE transfer, uint32_t payload_size, const unsigned char* payload_bytes)
{
    (void)context;
    test_on_transfer_received_transfer = transfer;
    test_on_transfer_received_payload_size = payload_size;
    memcpy(test_on_transfer_received_payload_bytes, payload_bytes, payload_size);

    return (AMQP_VALUE)0x6000;
}

static LINK_STATE test_on_link_state_changed_new_link_state;
LINK_STATE test_on_link_state_changed_previous_link_state;
static void test_on_link_state_changed(void* context, LINK_STATE new_link_state, LINK_STATE previous_link_state)
{
    (void)context;
    test_on_link_state_changed_new_link_state = new_link_state;
    test_on_link_state_changed_previous_link_state = previous_link_state;
}

static void test_on_link_flow_on(void* context)
{
    (void)context;
}

static LINK_HANDLE create_link(role link_role)
{
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(gballoc_calloc(IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(amqpvalue_clone(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(amqpvalue_clone(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(tickcounter_create());
    STRICT_EXPECTED_CALL(singlylinkedlist_create());
    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(session_create_link_endpoint(TEST_SESSION_HANDLE, TEST_LINK_NAME_1));
    STRICT_EXPECTED_CALL(session_set_link_endpoint_callback(TEST_LINK_ENDPOINT, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    return link_create(TEST_SESSION_HANDLE, TEST_LINK_NAME_1, link_role, TEST_LINK_SOURCE, TEST_LINK_TARGET);
}

static int attach_link(LINK_HANDLE link, ON_ENDPOINT_FRAME_RECEIVED* on_frame_received)
{
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(session_begin(TEST_SESSION_HANDLE));
    STRICT_EXPECTED_CALL(session_start_link_endpoint(TEST_LINK_ENDPOINT, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, link))
        .CaptureArgumentValue_frame_received_callback(on_frame_received);

    return link_attach(link, test_on_transfer_received, test_on_link_state_changed, test_on_link_flow_on, NULL);
}

BEGIN_TEST_SUITE(link_ut)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;

    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    umock_c_init(on_umock_c_error);

    result = umocktypes_bool_register_types();
    ASSERT_ARE_EQUAL(int, 0, result, "Failed registering bool types");

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, my_gballoc_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_calloc, my_gballoc_calloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_realloc, my_gballoc_realloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, my_gballoc_free);
    REGISTER_UMOCK_ALIAS_TYPE(AMQP_VALUE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(TICK_COUNTER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SINGLYLINKEDLIST_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SESSION_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LINK_ENDPOINT_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_LINK_ENDPOINT_DESTROYED_CALLBACK, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_ENDPOINT_FRAME_RECEIVED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_TRANSFER_RECEIVED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_LINK_STATE_CHANGED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_LINK_FLOW_ON, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_SESSION_STATE_CHANGED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_SESSION_FLOW_ON, void*);

    REGISTER_GLOBAL_MOCK_RETURNS(tickcounter_create, TEST_TICK_COUNTER_HANDLE, NULL);
    REGISTER_GLOBAL_MOCK_RETURNS(singlylinkedlist_create, TEST_SINGLYLINKEDLIST_HANDLE, NULL);
    REGISTER_GLOBAL_MOCK_RETURNS(session_create_link_endpoint, TEST_LINK_ENDPOINT, NULL);
    REGISTER_GLOBAL_MOCK_RETURNS(session_start_link_endpoint, 0, 1);

    REGISTER_TYPE(FLOW_HANDLE, FLOW_HANDLE);
    REGISTER_TYPE(bool*, bool_ptr);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_deinit();

    TEST_MUTEX_DESTROY(g_testByTest);
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

TEST_FUNCTION(link_create_succeeds)
{
    // arrange
    AMQP_VALUE link_source = TEST_LINK_SOURCE;
    AMQP_VALUE link_target = TEST_LINK_TARGET;

    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(gballoc_calloc(IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(amqpvalue_clone(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(amqpvalue_clone(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(tickcounter_create());
    STRICT_EXPECTED_CALL(singlylinkedlist_create());
    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(session_create_link_endpoint(TEST_SESSION_HANDLE, TEST_LINK_NAME_1));
    STRICT_EXPECTED_CALL(session_set_link_endpoint_callback(TEST_LINK_ENDPOINT, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    LINK_HANDLE link = link_create(TEST_SESSION_HANDLE, TEST_LINK_NAME_1, role_receiver, link_source, link_target);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(link);

    // cleanup
    link_destroy(link);
}

TEST_FUNCTION(link_attach_succeeds)
{
    // arrange
    LINK_HANDLE link = create_link(role_receiver);
    ON_ENDPOINT_FRAME_RECEIVED on_frame_received = NULL;

    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(session_begin(TEST_SESSION_HANDLE));
    STRICT_EXPECTED_CALL(session_start_link_endpoint(TEST_LINK_ENDPOINT, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, link))
        .CaptureArgumentValue_frame_received_callback(&on_frame_received);

    // act
    int result = link_attach(link, test_on_transfer_received, test_on_link_state_changed, test_on_link_flow_on, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_IS_NOT_NULL(on_frame_received);

    // cleanup
    link_destroy(link);
}

TEST_FUNCTION(link_receiver_frame_received_succeeds)
{
    // arrange
    LINK_HANDLE link = create_link(role_receiver);
    ON_ENDPOINT_FRAME_RECEIVED on_frame_received = NULL;
    int attach_result = attach_link(link, &on_frame_received);
    ASSERT_ARE_EQUAL(int, 0, attach_result);

    AMQP_VALUE performative = (AMQP_VALUE)0x5000;
    AMQP_VALUE descriptor = (AMQP_VALUE)0x5001;
    FLOW_HANDLE flow = (FLOW_HANDLE)0x5002;
    uint32_t frame_payload_size = 30;
    const unsigned char payload_bytes[30] = { 0 };

    umock_c_reset_all_calls();
    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(performative))
        .SetReturn(descriptor);
    STRICT_EXPECTED_CALL(is_attach_type_by_descriptor(IGNORED_PTR_ARG))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_flow_type_by_descriptor(IGNORED_PTR_ARG))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(amqpvalue_get_flow(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &flow, sizeof(flow));
    STRICT_EXPECTED_CALL(flow_destroy(IGNORED_PTR_ARG));

    // act
    on_frame_received(link, performative, frame_payload_size, payload_bytes);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    link_destroy(link);
}

TEST_FUNCTION(link_sender_frame_received_succeeds)
{
    // arrange
    LINK_HANDLE link = create_link(role_sender);
    ON_ENDPOINT_FRAME_RECEIVED on_frame_received = NULL;
    int attach_result = attach_link(link, &on_frame_received);
    ASSERT_ARE_EQUAL(int, 0, attach_result);

    AMQP_VALUE performative = (AMQP_VALUE)0x5000;
    AMQP_VALUE descriptor = (AMQP_VALUE)0x5001;
    FLOW_HANDLE flow = (FLOW_HANDLE)0x5002;
    uint32_t frame_payload_size = 30;
    const unsigned char payload_bytes[30] = { 0 };
    uint32_t link_credit_value = 700;
    uint32_t delivery_count_value = 300;

    umock_c_reset_all_calls();
    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(performative))
        .SetReturn(descriptor);
    STRICT_EXPECTED_CALL(is_attach_type_by_descriptor(IGNORED_PTR_ARG))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_flow_type_by_descriptor(IGNORED_PTR_ARG))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(amqpvalue_get_flow(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &flow, sizeof(flow));
    STRICT_EXPECTED_CALL(flow_get_link_credit(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &link_credit_value, sizeof(link_credit_value));
    STRICT_EXPECTED_CALL(flow_get_delivery_count(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &delivery_count_value, sizeof(delivery_count_value));
    STRICT_EXPECTED_CALL(flow_destroy(IGNORED_PTR_ARG));

    // act
    on_frame_received(link, performative, frame_payload_size, payload_bytes);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    link_destroy(link);
}

TEST_FUNCTION(link_receiver_frame_received_get_flow_fails_no_double_free_fails)
{
    // arrange
    LINK_HANDLE link = create_link(role_receiver);
    ON_ENDPOINT_FRAME_RECEIVED on_frame_received = NULL;
    int attach_result = attach_link(link, &on_frame_received);
    ASSERT_ARE_EQUAL(int, 0, attach_result);

    AMQP_VALUE performative = (AMQP_VALUE)0x5000;
    AMQP_VALUE descriptor = (AMQP_VALUE)0x5001;
    FLOW_HANDLE flow = NULL;
    uint32_t frame_payload_size = 30;
    const unsigned char payload_bytes[30] = { 0 };

    umock_c_reset_all_calls();
    STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(performative))
        .SetReturn(descriptor);
    STRICT_EXPECTED_CALL(is_attach_type_by_descriptor(IGNORED_PTR_ARG))
        .SetReturn(false);
    STRICT_EXPECTED_CALL(is_flow_type_by_descriptor(IGNORED_PTR_ARG))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(amqpvalue_get_flow(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &flow, sizeof(flow))
        .SetReturn(1);

    // act
    on_frame_received(link, performative, frame_payload_size, payload_bytes);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    link_destroy(link);
}


END_TEST_SUITE(link_ut)
