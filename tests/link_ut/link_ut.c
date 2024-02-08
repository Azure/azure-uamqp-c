// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cstddef>
#else
#include <stdlib.h>
#include <stddef.h>
#endif

#include "azure_macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"
#include "umock_c/umock_c.h"
#include "umock_c/umock_c_negative_tests.h"

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

static TEST_MUTEX_HANDLE g_testByTest;

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

BEGIN_TEST_SUITE(link_ut)

TEST_SUITE_INITIALIZE(suite_init)
{
    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    umock_c_init(on_umock_c_error);

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

    REGISTER_GLOBAL_MOCK_RETURNS(tickcounter_create, TEST_TICK_COUNTER_HANDLE, NULL);
    REGISTER_GLOBAL_MOCK_RETURNS(singlylinkedlist_create, TEST_SINGLYLINKEDLIST_HANDLE, NULL);
    REGISTER_GLOBAL_MOCK_RETURNS(session_create_link_endpoint, TEST_LINK_ENDPOINT, NULL);
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
    AMQP_VALUE link_source;
    AMQP_VALUE link_target;

    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(gballoc_calloc(IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(amqpvalue_clone(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(amqpvalue_clone(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(tickcounter_create());
    STRICT_EXPECTED_CALL(singlylinkedlist_create());
    STRICT_EXPECTED_CALL(gballoc_calloc(IGNORED_NUM_ARG, IGNORED_NUM_ARG));
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

TEST_FUNCTION(link_frame_received_succeeds)
{
    // arrange

    // STRICT_EXPECTED_CALL(gballoc_calloc(IGNORED_NUM_ARG, IGNORED_NUM_ARG));

    // act
    // message = message_create();

    // assert
    // ASSERT_IS_NOT_NULL(message);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    // message_destroy(message);
}


END_TEST_SUITE(link_ut)
