// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cstddef>
#include <cstring>
#else
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#endif

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"
#include "umock_c/umock_c.h"
#include "umock_c/umock_c_negative_tests.h"
#include "umock_c/umocktypes_charptr.h"
#include "umock_c/umocktypes_bool.h"
#include "umock_c/umocktypes_stdint.h"

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
#include "azure_uamqp_c/link.h"
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/amqpvalue.h"
#include "azure_uamqp_c/amqpvalue_to_string.h"
#include "azure_uamqp_c/amqp_definitions.h"
#include "azure_uamqp_c/async_operation.h"

#undef ENABLE_MOCKS

#include "azure_uamqp_c/message_sender.h"

static TEST_MUTEX_HANDLE g_testByTest;

static LINK_HANDLE test_link = (LINK_HANDLE)0x4242;
static MESSAGE_HANDLE test_message = (MESSAGE_HANDLE)0x4243;
static AMQP_VALUE test_body_amqp_value = (AMQP_VALUE)0x4244;
static AMQP_VALUE test_amqp_value = (AMQP_VALUE)0x4245;

/* captured by the link_attach mock so tests can drive link state changes */
static ON_LINK_STATE_CHANGED saved_on_link_state_changed;
static void* saved_on_link_state_changed_context;

/* knobs used to steer the mocked link/message behaviour */
static LINK_TRANSFER_RESULT g_link_transfer_result;
static ASYNC_OPERATION_HANDLE g_link_transfer_async_result;
static MESSAGE_HANDLE g_message_clone_result;

/* Fake link: when enabled, link_transfer_async records each delivery as the real link does
   (on_delivery_settled + callback_context), so tests can settle it later like link.c would. */
#define MAX_FAKE_DELIVERIES 8
#define MAX_LIVE_SENDS 8

typedef struct FAKE_DELIVERY_TAG
{
    ON_DELIVERY_SETTLED on_delivery_settled;
    void* callback_context;
} FAKE_DELIVERY;

static bool g_fake_link_enabled;
static FAKE_DELIVERY* g_fake_deliveries[MAX_FAKE_DELIVERIES];
static size_t g_fake_delivery_count;
/* message sender pending sends that are allocated and not yet destroyed */
static ASYNC_OPERATION_HANDLE g_live_sends[MAX_LIVE_SENDS];
static size_t g_live_send_count;
/* settlements the fake link attempted on a pending send the sender had already destroyed */
static size_t g_stale_settle_count;
/* async_operation_cancel calls on a handle that is not an outstanding fake delivery */
static size_t g_stale_cancel_count;
/* action run inside link_detach, e.g. to emulate a synchronous session/connection failure */
static void (*g_link_detach_action)(void);
static int g_link_detach_result;

static size_t g_send_complete_count;
static MESSAGE_SEND_RESULT g_last_send_result;

MOCK_FUNCTION_WITH_CODE(, void, test_on_message_send_complete, void*, context, MESSAGE_SEND_RESULT, send_result, AMQP_VALUE, delivery_state);
MOCK_FUNCTION_END();

TEST_DEFINE_ENUM_TYPE(MESSAGE_SEND_RESULT, MESSAGE_SEND_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(MESSAGE_SEND_RESULT, MESSAGE_SEND_RESULT_VALUES);

static int my_link_attach(LINK_HANDLE link, ON_TRANSFER_RECEIVED on_transfer_received, ON_LINK_STATE_CHANGED on_link_state_changed, ON_LINK_FLOW_ON on_link_flow_on, void* callback_context)
{
    (void)link;
    (void)on_transfer_received;
    (void)on_link_flow_on;
    saved_on_link_state_changed = on_link_state_changed;
    saved_on_link_state_changed_context = callback_context;
    return 0;
}

static ASYNC_OPERATION_HANDLE my_link_transfer_async(LINK_HANDLE handle, message_format message_format, PAYLOAD* payloads, size_t payload_count, ON_DELIVERY_SETTLED on_delivery_settled, void* callback_context, LINK_TRANSFER_RESULT* link_transfer_result, tickcounter_ms_t timeout)
{
    (void)handle;
    (void)message_format;
    (void)payloads;
    (void)payload_count;
    (void)timeout;
    if (g_fake_link_enabled)
    {
        FAKE_DELIVERY* delivery = (FAKE_DELIVERY*)my_gballoc_malloc(sizeof(FAKE_DELIVERY));
        ASSERT_IS_NOT_NULL(delivery);
        ASSERT_IS_TRUE(g_fake_delivery_count < MAX_FAKE_DELIVERIES);
        delivery->on_delivery_settled = on_delivery_settled;
        delivery->callback_context = callback_context;
        g_fake_deliveries[g_fake_delivery_count++] = delivery;
        return (ASYNC_OPERATION_HANDLE)delivery;
    }
    (void)on_delivery_settled;
    (void)callback_context;
    *link_transfer_result = g_link_transfer_result;
    return g_link_transfer_async_result;
}

static int my_link_detach(LINK_HANDLE link, bool close, const char* error_condition, const char* error_description, AMQP_VALUE info)
{
    (void)link;
    (void)close;
    (void)error_condition;
    (void)error_description;
    (void)info;
    if (g_link_detach_action != NULL)
    {
        g_link_detach_action();
    }
    return g_link_detach_result;
}

static void test_send_complete(void* context, MESSAGE_SEND_RESULT send_result, AMQP_VALUE delivery_state)
{
    (void)context;
    (void)delivery_state;
    g_send_complete_count++;
    g_last_send_result = send_result;
}

static MESSAGE_HANDLE my_message_clone(MESSAGE_HANDLE source_message)
{
    (void)source_message;
    return g_message_clone_result;
}

static int my_message_get_body_type(MESSAGE_HANDLE message, MESSAGE_BODY_TYPE* body_type)
{
    (void)message;
    *body_type = MESSAGE_BODY_TYPE_VALUE;
    return 0;
}

static int my_message_get_message_format(MESSAGE_HANDLE message, uint32_t* message_format)
{
    (void)message;
    *message_format = 0;
    return 0;
}

static int my_message_get_header(MESSAGE_HANDLE message, HEADER_HANDLE* message_header)
{
    (void)message;
    *message_header = NULL;
    return 0;
}

static int my_message_get_message_annotations(MESSAGE_HANDLE message, message_annotations* annotations)
{
    (void)message;
    *annotations = NULL;
    return 0;
}

static int my_message_get_properties(MESSAGE_HANDLE message, PROPERTIES_HANDLE* properties)
{
    (void)message;
    *properties = NULL;
    return 0;
}

static int my_message_get_application_properties(MESSAGE_HANDLE message, AMQP_VALUE* application_properties)
{
    (void)message;
    *application_properties = NULL;
    return 0;
}

static int my_message_get_body_amqp_value_in_place(MESSAGE_HANDLE message, AMQP_VALUE* body_amqp_value)
{
    (void)message;
    *body_amqp_value = test_body_amqp_value;
    return 0;
}

static int my_amqpvalue_get_encoded_size(AMQP_VALUE value, size_t* encoded_size)
{
    (void)value;
    *encoded_size = 4;
    return 0;
}

static int my_amqpvalue_encode(AMQP_VALUE value, AMQPVALUE_ENCODER_OUTPUT encoder_output, void* context)
{
    (void)value;
    (void)encoder_output;
    (void)context;
    return 0;
}

/* The real async_operation allocates a single block whose base is the handle and whose
   context is an interior pointer; the mock reproduces that layout exactly so that any
   mismatch between what is destroyed and what is still referenced is detectable. */
static ASYNC_OPERATION_HANDLE my_async_operation_create(ASYNC_OPERATION_CANCEL_HANDLER_FUNC async_operation_cancel_handler, size_t context_size)
{
    ASYNC_OPERATION_CANCEL_HANDLER_FUNC* result = (ASYNC_OPERATION_CANCEL_HANDLER_FUNC*)my_gballoc_malloc(context_size);
    if (result != NULL)
    {
        (void)memset(result, 0, context_size);
        *result = async_operation_cancel_handler;
        ASSERT_IS_TRUE(g_live_send_count < MAX_LIVE_SENDS);
        g_live_sends[g_live_send_count++] = (ASYNC_OPERATION_HANDLE)result;
    }
    return (ASYNC_OPERATION_HANDLE)result;
}

static void my_async_operation_destroy(ASYNC_OPERATION_HANDLE async_operation)
{
    size_t i;
    for (i = 0; i < g_live_send_count; i++)
    {
        if (g_live_sends[i] == async_operation)
        {
            g_live_sends[i] = g_live_sends[--g_live_send_count];
            break;
        }
    }
    my_gballoc_free(async_operation);
}

static bool is_live_send(void* handle)
{
    size_t i;
    for (i = 0; i < g_live_send_count; i++)
    {
        if (g_live_sends[i] == (ASYNC_OPERATION_HANDLE)handle)
        {
            return true;
        }
    }
    return false;
}

/* Removes delivery [index] from the fake link, settling it first, as link.c does. A settle whose
   context is a destroyed pending send is counted and skipped instead of dereferenced. */
static void fake_link_settle_and_remove(size_t index, LINK_DELIVERY_SETTLE_REASON reason)
{
    FAKE_DELIVERY* delivery = g_fake_deliveries[index];
    g_fake_deliveries[index] = g_fake_deliveries[--g_fake_delivery_count];

    if (is_live_send(delivery->callback_context))
    {
        delivery->on_delivery_settled(delivery->callback_context, 0, reason, NULL);
    }
    else
    {
        g_stale_settle_count++;
    }
    my_gballoc_free(delivery);
}

/* Emulates link.c remove_all_pending_deliveries(link, true): peer detach, session DISCARDING/ERROR. */
static void fake_link_remove_all_pending_deliveries(void)
{
    while (g_fake_delivery_count > 0)
    {
        fake_link_settle_and_remove(0, LINK_DELIVERY_SETTLE_REASON_NOT_DELIVERED);
    }
}

/* Emulates link_detach failing to send and the connection/session failing synchronously:
   link.c on_session_state_changed drains pending deliveries and then reports the new state. */
static void fake_link_detach_fails_synchronously(void)
{
    fake_link_remove_all_pending_deliveries();
    saved_on_link_state_changed(saved_on_link_state_changed_context, LINK_STATE_DETACHED, LINK_STATE_ATTACHED);
}

/* Emulates link_transfer_cancel_handler: settle as CANCELLED, then drop the delivery. */
static int my_async_operation_cancel(ASYNC_OPERATION_HANDLE async_operation)
{
    size_t i;
    for (i = 0; i < g_fake_delivery_count; i++)
    {
        if (g_fake_deliveries[i] == (FAKE_DELIVERY*)async_operation)
        {
            fake_link_settle_and_remove(i, LINK_DELIVERY_SETTLE_REASON_CANCELLED);
            return 0;
        }
    }
    g_stale_cancel_count++;
    return MU_FAILURE;
}


MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

/* drives a freshly created message sender into MESSAGE_SENDER_STATE_OPEN */
static MESSAGE_SENDER_HANDLE create_and_open_message_sender(void)
{
    MESSAGE_SENDER_HANDLE message_sender = messagesender_create(test_link, NULL, NULL);
    (void)messagesender_open(message_sender);
    saved_on_link_state_changed(saved_on_link_state_changed_context, LINK_STATE_ATTACHED, LINK_STATE_HALF_ATTACHED_ATTACH_SENT);
    return message_sender;
}

/* Opens a sender over the fake link with one send in flight on the link. */
static MESSAGE_SENDER_HANDLE create_sender_with_one_in_flight_send(void)
{
    MESSAGE_SENDER_HANDLE message_sender = create_and_open_message_sender();
    ASYNC_OPERATION_HANDLE send;

    g_fake_link_enabled = true;
    send = messagesender_send_async(message_sender, test_message, test_send_complete, (void*)0x4711, 0);
    ASSERT_IS_NOT_NULL(send);
    ASSERT_ARE_EQUAL(size_t, 1, g_fake_delivery_count);
    ASSERT_ARE_EQUAL(size_t, 0, g_send_complete_count);
    return message_sender;
}

BEGIN_TEST_SUITE(messagesender_ut)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;

    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    umock_c_init(on_umock_c_error);

    result = umocktypes_charptr_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);
    result = umocktypes_bool_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);
    result = umocktypes_stdint_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, my_gballoc_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_calloc, my_gballoc_calloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_realloc, my_gballoc_realloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, my_gballoc_free);

    REGISTER_GLOBAL_MOCK_HOOK(link_attach, my_link_attach);
    REGISTER_GLOBAL_MOCK_HOOK(link_detach, my_link_detach);
    REGISTER_GLOBAL_MOCK_HOOK(link_transfer_async, my_link_transfer_async);

    REGISTER_GLOBAL_MOCK_HOOK(message_clone, my_message_clone);
    REGISTER_GLOBAL_MOCK_HOOK(message_get_body_type, my_message_get_body_type);
    REGISTER_GLOBAL_MOCK_HOOK(message_get_message_format, my_message_get_message_format);
    REGISTER_GLOBAL_MOCK_HOOK(message_get_header, my_message_get_header);
    REGISTER_GLOBAL_MOCK_HOOK(message_get_message_annotations, my_message_get_message_annotations);
    REGISTER_GLOBAL_MOCK_HOOK(message_get_properties, my_message_get_properties);
    REGISTER_GLOBAL_MOCK_HOOK(message_get_application_properties, my_message_get_application_properties);
    REGISTER_GLOBAL_MOCK_HOOK(message_get_body_amqp_value_in_place, my_message_get_body_amqp_value_in_place);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_amqp_value, test_amqp_value);
    REGISTER_GLOBAL_MOCK_HOOK(amqpvalue_get_encoded_size, my_amqpvalue_get_encoded_size);
    REGISTER_GLOBAL_MOCK_HOOK(amqpvalue_encode, my_amqpvalue_encode);

    REGISTER_GLOBAL_MOCK_HOOK(async_operation_create, my_async_operation_create);
    REGISTER_GLOBAL_MOCK_HOOK(async_operation_destroy, my_async_operation_destroy);
    REGISTER_GLOBAL_MOCK_HOOK(async_operation_cancel, my_async_operation_cancel);

    REGISTER_TYPE(MESSAGE_SEND_RESULT, MESSAGE_SEND_RESULT);

    REGISTER_UMOCK_ALIAS_TYPE(LINK_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MESSAGE_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(AMQP_VALUE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(HEADER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(PROPERTIES_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ASYNC_OPERATION_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ASYNC_OPERATION_CANCEL_HANDLER_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_TRANSFER_RECEIVED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_LINK_STATE_CHANGED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_LINK_FLOW_ON, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_DELIVERY_SETTLED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(AMQPVALUE_ENCODER_OUTPUT, void*);
    REGISTER_UMOCK_ALIAS_TYPE(message_format, uint32_t);
    REGISTER_UMOCK_ALIAS_TYPE(tickcounter_ms_t, uint64_t);
    REGISTER_UMOCK_ALIAS_TYPE(message_annotations, void*);
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

    saved_on_link_state_changed = NULL;
    saved_on_link_state_changed_context = NULL;
    g_link_transfer_result = LINK_TRANSFER_ERROR;
    g_link_transfer_async_result = NULL;
    g_message_clone_result = test_message;

    g_fake_link_enabled = false;
    g_fake_delivery_count = 0;
    g_live_send_count = 0;
    g_stale_settle_count = 0;
    g_stale_cancel_count = 0;
    g_link_detach_action = NULL;
    g_link_detach_result = 0;
    g_send_complete_count = 0;
    g_last_send_result = MESSAGE_SEND_OK;
}

TEST_FUNCTION_CLEANUP(test_cleanup)
{
    /* release anything a test left on the fake link without settling it */
    while (g_fake_delivery_count > 0)
    {
        my_gballoc_free(g_fake_deliveries[--g_fake_delivery_count]);
    }

    TEST_MUTEX_RELEASE(g_testByTest);
}

/* messagesender_create */

TEST_FUNCTION(messagesender_create_returns_a_valid_handle)
{
    // arrange
    MESSAGE_SENDER_HANDLE message_sender;
    STRICT_EXPECTED_CALL(gballoc_calloc(IGNORED_ARG, IGNORED_ARG));

    // act
    message_sender = messagesender_create(test_link, NULL, NULL);

    // assert
    ASSERT_IS_NOT_NULL(message_sender);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    messagesender_destroy(message_sender);
}

/* messagesender_destroy */

TEST_FUNCTION(messagesender_destroy_with_NULL_handle_does_nothing)
{
    // arrange

    // act
    messagesender_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* messagesender_send_async */

TEST_FUNCTION(messagesender_send_async_with_NULL_message_sender_fails)
{
    // arrange
    ASYNC_OPERATION_HANDLE result;

    // act
    result = messagesender_send_async(NULL, test_message, test_on_message_send_complete, (void*)0x4711, 0);

    // assert
    ASSERT_IS_NULL(result);
}

TEST_FUNCTION(messagesender_send_async_with_NULL_message_fails)
{
    // arrange
    MESSAGE_SENDER_HANDLE message_sender = create_and_open_message_sender();
    ASYNC_OPERATION_HANDLE result;
    umock_c_reset_all_calls();

    // act
    result = messagesender_send_async(message_sender, NULL, test_on_message_send_complete, (void*)0x4711, 0);

    // assert
    ASSERT_IS_NULL(result);

    // cleanup
    messagesender_destroy(message_sender);
}

/* Tests that the pending send is removed from the pending sends array, and not merely
   destroyed, when the link is busy and cloning the message for the retry fails.
   Leaving the destroyed operation in the array made every later walk of it read and
   free freed memory (use after free followed by a double free). */
TEST_FUNCTION(when_the_link_is_busy_and_cloning_the_message_fails_the_pending_send_is_removed_from_the_list)
{
    // arrange
    MESSAGE_SENDER_HANDLE message_sender = create_and_open_message_sender();
    ASYNC_OPERATION_HANDLE result;

    g_link_transfer_result = LINK_TRANSFER_BUSY;
    g_link_transfer_async_result = NULL;
    g_message_clone_result = NULL;

    // act
    result = messagesender_send_async(message_sender, test_message, test_on_message_send_complete, (void*)0x4711, 0);

    // assert
    ASSERT_IS_NULL(result);

    /* Closing walks the pending sends array. The async operation was already destroyed by
       the failed send, so the only thing left to release is the message sender itself; any
       further async_operation_destroy or completion callback here means the stale entry was
       left behind. */
    umock_c_reset_all_calls();
    STRICT_EXPECTED_CALL(link_detach(test_link, true, NULL, NULL, NULL));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_ARG));

    // act
    messagesender_destroy(message_sender);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Same guarantee on the sibling failure path, which has always removed the entry. */
TEST_FUNCTION(when_sending_the_message_fails_the_pending_send_is_removed_from_the_list)
{
    // arrange
    MESSAGE_SENDER_HANDLE message_sender = create_and_open_message_sender();
    ASYNC_OPERATION_HANDLE result;

    g_link_transfer_result = LINK_TRANSFER_ERROR;
    g_link_transfer_async_result = NULL;

    // act
    result = messagesender_send_async(message_sender, test_message, test_on_message_send_complete, (void*)0x4711, 0);

    // assert
    ASSERT_IS_NULL(result);

    umock_c_reset_all_calls();
    STRICT_EXPECTED_CALL(link_detach(test_link, true, NULL, NULL, NULL));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_ARG));

    // act
    messagesender_destroy(message_sender);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* messagesender_close / messagesender_destroy with sends in flight on the link.
   The link keeps every in-flight delivery with the pending send as its callback context and settles
   it later (peer detach, disposition, timeout, session DISCARDING/ERROR). Once the sender has
   completed and destroyed a pending send, the link must not be able to settle it again. */

/* link_detach fails to send and the session fails synchronously, so the link drains its deliveries
   from inside link_detach. Each send must be completed once and never settled after destruction. */
TEST_FUNCTION(close_when_link_detach_fails_synchronously_completes_each_in_flight_send_once)
{
    // arrange
    MESSAGE_SENDER_HANDLE message_sender = create_sender_with_one_in_flight_send();
    g_link_detach_action = fake_link_detach_fails_synchronously;
    g_link_detach_result = MU_FAILURE;

    // act
    (void)messagesender_close(message_sender);

    // assert
    ASSERT_ARE_EQUAL(size_t, 0, g_stale_settle_count);
    ASSERT_ARE_EQUAL(size_t, 1, g_send_complete_count);
    ASSERT_ARE_EQUAL(MESSAGE_SEND_RESULT, MESSAGE_SEND_ERROR, g_last_send_result);
    ASSERT_ARE_EQUAL(size_t, 0, g_live_send_count);

    // cleanup
    g_link_detach_action = NULL;
    messagesender_destroy(message_sender);
}

/* link_detach succeeds; the link settles its deliveries later, when the peer answers the detach. */
TEST_FUNCTION(close_when_link_detach_succeeds_the_link_cannot_settle_a_destroyed_send_later)
{
    // arrange
    MESSAGE_SENDER_HANDLE message_sender = create_sender_with_one_in_flight_send();

    // act
    (void)messagesender_close(message_sender);
    fake_link_remove_all_pending_deliveries();

    // assert
    ASSERT_ARE_EQUAL(size_t, 0, g_stale_settle_count);
    ASSERT_ARE_EQUAL(size_t, 0, g_stale_cancel_count);
    ASSERT_ARE_EQUAL(size_t, 1, g_send_complete_count);
    ASSERT_ARE_EQUAL(MESSAGE_SEND_RESULT, MESSAGE_SEND_ERROR, g_last_send_result);
    ASSERT_ARE_EQUAL(size_t, 0, g_live_send_count);

    // cleanup
    messagesender_destroy(message_sender);
}

/* Same as above, but the sender is destroyed before the link settles its deliveries. */
TEST_FUNCTION(destroy_when_link_detach_succeeds_the_link_cannot_settle_a_destroyed_send_later)
{
    // arrange
    MESSAGE_SENDER_HANDLE message_sender = create_sender_with_one_in_flight_send();

    // act
    messagesender_destroy(message_sender);
    fake_link_remove_all_pending_deliveries();

    // assert
    ASSERT_ARE_EQUAL(size_t, 0, g_stale_settle_count);
    ASSERT_ARE_EQUAL(size_t, 0, g_stale_cancel_count);
    ASSERT_ARE_EQUAL(size_t, 1, g_send_complete_count);
    ASSERT_ARE_EQUAL(MESSAGE_SEND_RESULT, MESSAGE_SEND_ERROR, g_last_send_result);
    ASSERT_ARE_EQUAL(size_t, 0, g_live_send_count);
}

/* link_detach from a half attached state reports DETACHED without draining its deliveries. */
TEST_FUNCTION(link_detached_without_draining_deliveries_the_link_cannot_settle_a_destroyed_send_later)
{
    // arrange
    MESSAGE_SENDER_HANDLE message_sender = create_sender_with_one_in_flight_send();

    // act
    saved_on_link_state_changed(saved_on_link_state_changed_context, LINK_STATE_DETACHED, LINK_STATE_HALF_ATTACHED_ATTACH_SENT);
    fake_link_remove_all_pending_deliveries();

    // assert
    ASSERT_ARE_EQUAL(size_t, 0, g_stale_settle_count);
    ASSERT_ARE_EQUAL(size_t, 0, g_stale_cancel_count);
    ASSERT_ARE_EQUAL(size_t, 1, g_send_complete_count);
    ASSERT_ARE_EQUAL(MESSAGE_SEND_RESULT, MESSAGE_SEND_ERROR, g_last_send_result);

    // cleanup
    messagesender_destroy(message_sender);
}

/* The link drains its deliveries before reporting LINK_STATE_ERROR; each send completes once. */
TEST_FUNCTION(link_error_after_the_link_drained_its_deliveries_completes_each_send_once)
{
    // arrange
    MESSAGE_SENDER_HANDLE message_sender = create_sender_with_one_in_flight_send();

    // act
    fake_link_remove_all_pending_deliveries();
    saved_on_link_state_changed(saved_on_link_state_changed_context, LINK_STATE_ERROR, LINK_STATE_ATTACHED);

    // assert
    ASSERT_ARE_EQUAL(size_t, 0, g_stale_settle_count);
    ASSERT_ARE_EQUAL(size_t, 1, g_send_complete_count);
    ASSERT_ARE_EQUAL(MESSAGE_SEND_RESULT, MESSAGE_SEND_ERROR, g_last_send_result);
    ASSERT_ARE_EQUAL(size_t, 0, g_live_send_count);

    // cleanup
    messagesender_destroy(message_sender);
}

/* A disposition without a delivery state settles and destroys the link delivery but leaves the
   send pending; closing must not cancel the already destroyed link delivery. */
TEST_FUNCTION(close_after_the_link_settled_a_delivery_does_not_cancel_it_again)
{
    // arrange
    MESSAGE_SENDER_HANDLE message_sender = create_sender_with_one_in_flight_send();
    fake_link_settle_and_remove(0, LINK_DELIVERY_SETTLE_REASON_DISPOSITION_RECEIVED);

    // act
    (void)messagesender_close(message_sender);

    // assert
    ASSERT_ARE_EQUAL(size_t, 0, g_stale_cancel_count);
    ASSERT_ARE_EQUAL(size_t, 0, g_stale_settle_count);
    ASSERT_ARE_EQUAL(size_t, 1, g_send_complete_count);
    ASSERT_ARE_EQUAL(MESSAGE_SEND_RESULT, MESSAGE_SEND_ERROR, g_last_send_result);

    // cleanup
    messagesender_destroy(message_sender);
}

END_TEST_SUITE(messagesender_ut)
