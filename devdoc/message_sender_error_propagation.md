# Message Sender Error Propagation — Design Document

## Problem

When certain errors occur during AMQP message sending, the `on_message_send_complete`
callback is invoked with `delivery_state == NULL`. This prevents callers from extracting
detailed error information (condition, description, info fields), making it impossible to
diagnose the root cause of send failures.

**Reported reproduction scenarios:**
1. Removing the Send claim from the SAS Policy → `delivery_state` is NULL
2. Enabling an outbound firewall rule blocking communication → `delivery_state` is NULL

When `delivery_state` is not NULL, callers can extract a descriptor object with detailed
error information. The NULL case leaves callers "blind" to the underlying error.

## Root Cause

The AMQP detach frame contains an `error` field with condition, description, and info.
When the remote side detaches with an error (e.g., `amqp:unauthorized-access` for SAS
policy issues), `link.c` successfully extracts this error via `detach_get_error()`.
However, the error information is **never propagated** to pending delivery callbacks.

### Critical flow (detach with error):

```
1. Remote sends DETACH frame with error (e.g., SAS policy revoked)
2. link.c: on_link_endpoint_state_changed() processes detach
3. link.c: detach_get_error() extracts ERROR_HANDLE      ← error info IS available
4. link.c: remove_all_pending_deliveries(link, true)      ← called with no error context
     → on_delivery_settled(ctx, id, NOT_DELIVERED, NULL)  ← NULL passed, error info lost!
     → message_sender.c: on_message_send_complete(ctx, MESSAGE_SEND_ERROR, NULL)
5. link.c: on_link_detach_received(ctx, error)            ← message_sender doesn't subscribe
6. link.c: set_link_state(LINK_STATE_ERROR)
     → message_sender.c: indicate_all_messages_as_error() ← also passes NULL
7. link.c: error_destroy(error)                           ← error info destroyed
```

### All code paths where delivery_state is NULL:

| Location | Reason | Error info available? |
|----------|--------|-----------------------|
| `link.c:102` remove_all_pending_deliveries | NOT_DELIVERED during detach | **Yes** (in detach error) |
| `link.c:708` on_send_complete | IO send failure (pre-settled) | No |
| `link.c:1635` timeout handler | Delivery timeout | No |
| `link.c:1347` cancellation | Delivery cancelled | No |
| `message_sender.c:665` send_all_pending_messages | send_one_message failed | No |
| `message_sender.c:703` indicate_all_messages_as_error | Link state → ERROR/DETACHED | **Yes** (from detach) |

## Fix Approach

### Change 1: Propagate delivery_state through `remove_all_pending_deliveries`

**File:** `src/link.c`

Modify `remove_all_pending_deliveries` to accept an optional `AMQP_VALUE delivery_state`:

```c
// Before:
static void remove_all_pending_deliveries(LINK_INSTANCE* link, bool indicate_settled)

// After:
static void remove_all_pending_deliveries(LINK_INSTANCE* link, bool indicate_settled, AMQP_VALUE delivery_state)
```

The `delivery_state` is forwarded to each `on_delivery_settled` callback instead of
hardcoded NULL. Callers that have no error info pass NULL (no behavior change).

Update all callers:
- Detach handler: pass a constructed `rejected` delivery_state (see Change 2)
- Session DISCARDING: pass NULL (no error info at this level)
- Session ERROR: pass NULL (no error info at this level)

### Change 2: Construct rejected delivery_state from detach error and store on link

**File:** `src/link.c`, `inc/azure_uamqp_c/link.h`

In the detach handler, **before** calling `remove_all_pending_deliveries`, construct an
AMQP `rejected` delivery state from the error and store it on the `LINK_INSTANCE` for
later retrieval by `message_sender`:

```c
// Store on LINK_INSTANCE for later retrieval
if (link_instance->last_error_delivery_state != NULL)
{
    amqpvalue_destroy(link_instance->last_error_delivery_state);
    link_instance->last_error_delivery_state = NULL;
}

if (error != NULL)
{
    REJECTED_HANDLE rejected = rejected_create();
    if (rejected != NULL)
    {
        if (rejected_set_error(rejected, error) == 0)
        {
            link_instance->last_error_delivery_state = amqpvalue_create_rejected(rejected);
        }
        rejected_destroy(rejected);
    }
}

remove_all_pending_deliveries(link_instance, true, link_instance->last_error_delivery_state);
```

A new public getter `link_get_last_error_delivery_state()` is added to `link.h` so that
`message_sender` can retrieve the stored error without consuming the single detach event
subscription slot.

### Change 3: Forward error to unsent messages via message_sender

**File:** `src/message_sender.c`

Messages queued in message_sender but not yet transferred to the link layer are reported
via `indicate_all_messages_as_error()`, which currently passes NULL. To propagate error
info for these, query the link's stored last error via `link_get_last_error_delivery_state()`:

```c
static void indicate_all_messages_as_error(MESSAGE_SENDER_INSTANCE* message_sender)
{
    AMQP_VALUE error_delivery_state = link_get_last_error_delivery_state(message_sender->link);
    // ... pass error_delivery_state to each on_message_send_complete callback
}
```

This approach avoids consuming the link's single detach event subscription slot (which
is used by existing callers for redirect handling), keeping message_sender composable
with other detach listeners.

### Change 4: Fix message leak on NULL DISPOSITION_RECEIVED

**File:** `src/message_sender.c`

In `on_delivery_settled`, when `LINK_DELIVERY_SETTLE_REASON_DISPOSITION_RECEIVED` arrives
with `delivery_state == NULL`, the current code only calls `LogError` but **does not**:
- Call `on_message_send_complete` (user callback never fires)
- Call `remove_pending_message` (message leaks in the pending list)

Fix by adding the missing callback and cleanup:

```c
case LINK_DELIVERY_SETTLE_REASON_DISPOSITION_RECEIVED:
    if (delivery_state == NULL)
    {
        LogError("delivery state not provided");
        message_with_callback->on_message_send_complete(
            message_with_callback->context, MESSAGE_SEND_ERROR, NULL);
        remove_pending_message(message_sender, pending_send);
    }
    else
    {
        // ... existing handling ...
    }
    break;
```


## Value shape handed to callers (important)

`ON_MESSAGE_SEND_COMPLETE` has always received the **described part** of the delivery state,
not the composite value itself. `message_sender.c` has done this for the
`LINK_DELIVERY_SETTLE_REASON_DISPOSITION_RECEIVED` path since the callback was introduced:

```c
AMQP_VALUE described = amqpvalue_get_inplace_described_value(delivery_state);
message_with_callback->on_message_send_complete(ctx, MESSAGE_SEND_ERROR, described);
```

For a `rejected` outcome the described part is the list holding the `error`, so callers walk
it as a list. Passing the composite `*rejected(...)` value instead makes
`amqpvalue_get_list_item_count` fail (`Value is not of type LIST`), so no condition can be
extracted and an error is logged on every failed send.

`link.c` therefore stores and forwards the **composite** value, and `message_sender.c`
normalizes it with `get_described_delivery_state()` on every path before invoking the
callback. The error paths added here consequently deliver exactly the same shape callers
already parse today.

## Impact on Callers

**Backward compatible.** The `ON_MESSAGE_SEND_COMPLETE` callback signature is unchanged:

```c
typedef void(*ON_MESSAGE_SEND_COMPLETE)(
    void* context,
    MESSAGE_SEND_RESULT send_result,
    AMQP_VALUE delivery_state);  // now non-NULL in more error cases
```

Callers that already check `delivery_state != NULL` before extracting error details will
automatically benefit from the richer error info. Callers that ignore `delivery_state`
are completely unaffected. The value remains borrowed: it is owned by the link and must not
be destroyed by the callback.

### How callers extract the error:

```c
void on_message_send_complete(void* ctx, MESSAGE_SEND_RESULT result, AMQP_VALUE delivery_state)
{
    uint32_t item_count;

    // delivery_state is the described part of the outcome; for a rejected outcome it is
    // the list holding the error.
    if ((result == MESSAGE_SEND_ERROR) && (delivery_state != NULL) &&
        (amqpvalue_get_list_item_count(delivery_state, &item_count) == 0))
    {
        uint32_t i;
        for (i = 0; i < item_count; i++)
        {
            AMQP_VALUE item = amqpvalue_get_list_item(delivery_state, i);
            AMQP_VALUE fields = (item == NULL) ? NULL : amqpvalue_get_inplace_described_value(item);
            uint32_t field_count;

            if ((fields != NULL) && (amqpvalue_get_list_item_count(fields, &field_count) == 0))
            {
                AMQP_VALUE condition = amqpvalue_get_list_item(fields, 0);
                const char* condition_value;

                if ((condition != NULL) && (amqpvalue_get_symbol(condition, &condition_value) == 0))
                {
                    // e.g. "amqp:unauthorized-access", "amqp:connection:forced"
                }

                amqpvalue_destroy(condition);
            }

            amqpvalue_destroy(item);
        }
    }
}
```

## Staleness

`link->last_error_delivery_state` is cleared when the link reaches `LINK_STATE_ATTACHED`
again, so a link that recovers and is later closed normally does not report the reason of a
previous failure.

## Relationship to the message sender shutdown order

`messagesender_close` still calls `indicate_all_messages_as_error` *before* attempting the
detach. That ordering is a separate, pre-existing defect (the pending sends are destroyed
while the link still references them) and is not addressed here.
