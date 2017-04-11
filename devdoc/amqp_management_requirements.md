# amqp_management requirements

## Overview

`amqp_management` is module that implements the AMQP management draft specification portion that refers to operations (request/response pattern).

##Exposed API

```c
    typedef enum AMQP_MANAGEMENT_EXECUTE_OPERATION_RESULT_TAG
    {
        AMQP_MANAGEMENT_EXECUTE_OPERATION_OK,
        AMQP_MANAGEMENT_EXECUTE_OPERATION_ERROR,
        AMQP_MANAGEMENT_EXECUTE_OPERATION_FAILED_BAD_STATUS,
        AMQP_MANAGEMENT_EXECUTE_OPERATION_INSTANCE_CLOSED
    } AMQP_MANAGEMENT_EXECUTE_OPERATION_RESULT;

    typedef enum AMQP_MANAGEMENT_OPEN_RESULT_TAG
    {
        AMQP_MANAGEMENT_OPEN_OK,
        AMQP_MANAGEMENT_OPEN_ERROR,
        AMQP_MANAGEMENT_OPEN_CANCELLED
    } AMQP_MANAGEMENT_OPEN_RESULT;

    typedef struct AMQP_MANAGEMENT_INSTANCE_TAG* AMQP_MANAGEMENT_HANDLE;
    typedef void(*ON_AMQP_MANAGEMENT_OPEN_COMPLETE)(void* context, AMQP_MANAGEMENT_OPEN_RESULT open_result);
    typedef void(*ON_AMQP_MANAGEMENT_ERROR)(void* context);
    typedef void(*ON_AMQP_MANAGEMENT_EXECUTE_OPERATION_COMPLETE)(void* context, AMQP_MANAGEMENT_EXECUTE_OPERATION_RESULT execute_operation_result, unsigned int status_code, const char* status_description);

    MOCKABLE_FUNCTION(, AMQP_MANAGEMENT_HANDLE, amqp_management_create, SESSION_HANDLE, session, const char*, management_node);
    MOCKABLE_FUNCTION(, void, amqp_management_destroy, AMQP_MANAGEMENT_HANDLE, amqp_management);
    MOCKABLE_FUNCTION(, int, amqp_management_open_async, AMQP_MANAGEMENT_HANDLE, amqp_management, ON_AMQP_MANAGEMENT_OPEN_COMPLETE, on_amqp_management_open_complete, void*, on_amqp_management_open_complete_context, ON_AMQP_MANAGEMENT_ERROR, on_amqp_management_error, void*, on_amqp_management_error_context);
    MOCKABLE_FUNCTION(, int, amqp_management_close, AMQP_MANAGEMENT_HANDLE, amqp_management);
    MOCKABLE_FUNCTION(, int, amqp_management_execute_operation_async, AMQP_MANAGEMENT_HANDLE, amqp_management, const char*, operation, const char*, type, const char*, locales, MESSAGE_HANDLE, message, ON_AMQP_MANAGEMENT_EXECUTE_OPERATION_COMPLETE, on_execute_operation_complete, void*, context);
    MOCKABLE_FUNCTION(, void, amqp_management_set_trace, AMQP_MANAGEMENT_HANDLE, amqp_management, bool, trace_on);
```

### amqp_management_create

```c
AMQP_MANAGEMENT_HANDLE amqp_management_create(SESSION_HANDLE session, const char* management_node);
```

XX**SRS_AMQP_MANAGEMENT_01_001: [** `amqp_management_create` shall create a new CBS instance and on success return a non-NULL handle to it. **]**
XX**SRS_AMQP_MANAGEMENT_01_002: [** If `session` or `management_node` is NULL then `amqp_management_create` shall fail and return NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_030: [** If `management_node` is an empty string, then `amqp_management_create` shall fail and return NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_003: [** `amqp_management_create` shall create a singly linked list for pending operations by calling `singlylinkedlist_create`. **]**
XX**SRS_AMQP_MANAGEMENT_01_004: [** If `singlylinkedlist_create` fails, `amqp_management_create` shall fail and return NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_005: [** If allocating memory for the new handle fails, `amqp_management_create` shall fail and return NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_006: [** `amqp_management_create` shall create a sender link by calling `link_create`. **]**
XX**SRS_AMQP_MANAGEMENT_01_007: [** The `session` argument shall be set to `session`. **]**
XX**SRS_AMQP_MANAGEMENT_01_008: [** The `name` argument shall be constructed by concatenating the `management_node` value with `-sender`. **]**
XX**SRS_AMQP_MANAGEMENT_01_009: [** The `role` argument shall be `role_sender`. **]**
XX**SRS_AMQP_MANAGEMENT_01_010: [** The `source` argument shall be a value created by calling `messaging_create_source` with `management_node` as argument. **]**
XX**SRS_AMQP_MANAGEMENT_01_011: [** The `target` argument shall be a value created by calling `messaging_create_target` with `management_node` as argument. **]**
XX**SRS_AMQP_MANAGEMENT_01_012: [** If `messaging_create_source` fails then `amqp_management_create` shall fail and return NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_013: [** If `messaging_create_target` fails then `amqp_management_create` shall fail and return NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_014: [** If `link_create` fails when creating the sender link then `amqp_management_create` shall fail and return NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_015: [** `amqp_management_create` shall create a receiver link by calling `link_create`. **]**
XX**SRS_AMQP_MANAGEMENT_01_016: [** The `session` argument shall be set to `session`. **]**
XX**SRS_AMQP_MANAGEMENT_01_017: [** The `name` argument shall be constructed by concatenating the `management_node` value with `-receiver`. **]**
XX**SRS_AMQP_MANAGEMENT_01_018: [** The `role` argument shall be `role_receiver`. **]**
XX**SRS_AMQP_MANAGEMENT_01_019: [** The `source` argument shall be the value created by calling `messaging_create_source`. **]**
XX**SRS_AMQP_MANAGEMENT_01_020: [** The `target` argument shall be the value created by calling `messaging_create_target`. **]**
XX**SRS_AMQP_MANAGEMENT_01_021: [** If `link_create` fails when creating the receiver link then `amqp_management_create` shall fail and return NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_022: [** `amqp_management_create` shall create a message sender by calling `messagesender_create` and passing to it the sender link handle. **]**
XX**SRS_AMQP_MANAGEMENT_01_023: [** `amqp_management_create` shall create a message receiver by calling `messagereceiver_create` and passing to it the receiver link handle. **]**
XX**SRS_AMQP_MANAGEMENT_01_031: [** If `messagesender_create` fails then `amqp_management_create` shall fail and return NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_032: [** If `messagereceiver_create` fails then `amqp_management_create` shall fail and return NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_033: [** If any other error occurs `amqp_management_create` shall fail and return NULL. **]**

### amqp_management_destroy

```c
void amqp_management_destroy(AMQP_MANAGEMENT_HANDLE amqp_management);
```

XX**SRS_AMQP_MANAGEMENT_01_024: [** `amqp_management_destroy` shall free all the resources allocated by `amqp_management_create`. **]**
XX**SRS_AMQP_MANAGEMENT_01_025: [** If `amqp_management` is NULL, `amqp_management_destroy` shall do nothing. **]**
XX**SRS_AMQP_MANAGEMENT_01_026: [** `amqp_management_destroy` shall free the singly linked list by calling `singlylinkedlist_destroy`. **]**
XX**SRS_AMQP_MANAGEMENT_01_027: [** `amqp_management_destroy` shall free the sender and receiver links by calling `link_destroy`. **]**
XX**SRS_AMQP_MANAGEMENT_01_028: [** `amqp_management_destroy` shall free the message sender by calling `messagesender_destroy`. **]**
XX**SRS_AMQP_MANAGEMENT_01_029: [** `amqp_management_destroy` shall free the message receiver by calling `messagereceiver_destroy`. **]**
**SRS_AMQP_MANAGEMENT_01_034: [** If the AMQP management instance is OPEN or OPENING, `amqp_management_destroy` shall also perform all actions that would be done by `amqp_management_close`. **]**

### amqp_management_open_async

```c
int amqp_management_open_async(AMQP_MANAGEMENT_HANDLE amqp_management, ON_AMQP_MANAGEMENT_OPEN_COMPLETE on_amqp_management_open_complete, void* on_amqp_management_open_complete_context, ON_AMQP_MANAGEMENT_ERROR on_amqp_management_error, void* on_amqp_management_error_context);
```

XX**SRS_AMQP_MANAGEMENT_01_036: [** `amqp_management_open_async` shall start opening the AMQP management instance and save the callbacks so that they can be called when opening is complete. **]**
XX**SRS_AMQP_MANAGEMENT_01_037: [** On success it shall return 0. **]**
XX**SRS_AMQP_MANAGEMENT_01_038: [** If `amqp_management`, `on_amqp_management_open_complete` or `on_amqp_management_error` is NULL, `amqp_management_open_async` shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_044: [** `on_amqp_management_open_complete_context` and `on_amqp_management_error_context` shall be allowed to be NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_039: [** `amqp_management_open_async` shall open the message sender by calling `messagesender_open`. **]**
XX**SRS_AMQP_MANAGEMENT_01_040: [** `amqp_management_open_async` shall open the message receiver by calling `messagereceiver_open`. **]**
XX**SRS_AMQP_MANAGEMENT_01_041: [** If `messagesender_open` fails, `amqp_management_open_async` shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_042: [** If `messagereceiver_open` fails, `amqp_management_open_async` shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_043: [** If the AMQP management instance is already OPEN or OPENING, `amqp_management_open_async` shall fail and return a non-zero value. **]**

### amqp_management_close

```c
int amqp_management_close(AMQP_MANAGEMENT_HANDLE amqp_management);
```

XX**SRS_AMQP_MANAGEMENT_01_045: [** `amqp_management_close` shall close the AMQP management instance. **]**
XX**SRS_AMQP_MANAGEMENT_01_046: [** On success it shall return 0. **]**
XX**SRS_AMQP_MANAGEMENT_01_047: [** If `amqp_management` is NULL, `amqp_management_close` shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_048: [** `amqp_management_close` on an AMQP management instance that is OPENING shall trigger the `on_amqp_management_open_complete` callback with `AMQP_MANAGEMENT_OPEN_CANCELLED`, while also passing the context passed in `amqp_management_open_async`. **]**
XX**SRS_AMQP_MANAGEMENT_01_049: [** `amqp_management_close` on an AMQP management instance that is not OPEN, shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_050: [** `amqp_management_close` shall close the message sender by calling `messagesender_close`. **]**
XX**SRS_AMQP_MANAGEMENT_01_051: [** `amqp_management_close` shall close the message receiver by calling `messagereceiver_close`. **]**
XX**SRS_AMQP_MANAGEMENT_01_052: [** If `messagesender_close` fails, `amqp_management_close` shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_053: [** If `messagereceiver_close` fails, `amqp_management_close` shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_054: [** All pending operations shall be indicated complete with the code `AMQP_MANAGEMENT_EXECUTE_OPERATION_INSTANCE_CLOSED`. **]**

### amqp_management_execute_operation_async

```c
int amqp_management_execute_operation_async(AMQP_MANAGEMENT_HANDLE amqp_management, const char* operation, const char* type, const char* locales, MESSAGE_HANDLE message, ON_AMQP_MANAGEMENT_EXECUTE_OPERATION_COMPLETE on_execute_operation_complete, void* on_execute_operation_complete_context);
```

XX**SRS_AMQP_MANAGEMENT_01_055: [** `amqp_management_execute_operation_async` shall start an AMQP management operation. **]**
XX**SRS_AMQP_MANAGEMENT_01_056: [** On success it shall return 0. **]**
XX**SRS_AMQP_MANAGEMENT_01_057: [** If `amqp_management`, `operation`, `type` or `on_execute_operation_complete` is NULL, `amqp_management_execute_operation_async` shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_105: [** `on_execute_operation_complete_context` shall be allowed to be NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_102: [** If `message` is NULL, a new message shall be created by calling `message_create`. **]**
XX**SRS_AMQP_MANAGEMENT_01_103: [** Otherwise the existing message shall be cloned by using `message_clone` before being modified accordingly and used for the pending operation. **]**
XX**SRS_AMQP_MANAGEMENT_01_081: [** If `amqp_management_execute_operation_async` is called when not OPEN, it shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_104: [** If `amqp_management_execute_operation_async` is called when the AMQP management is in error, it shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_082: [** `amqp_management_execute_operation_async` shall obtain the application properties from the message by calling `message_get_application_properties`. **]**
XX**SRS_AMQP_MANAGEMENT_01_083: [** If no application properties were set on the message, a new application properties instance shall be created by calling `amqpvalue_create_map`; **]**
XX**SRS_AMQP_MANAGEMENT_01_084: [** For each of the arguments `operation`, `type` and `locales` an AMQP value of type string shall be created by calling `amqpvalue_create_string` in order to be used as key in the application properties map. **]**
XX**SRS_AMQP_MANAGEMENT_01_085: [** For each of the arguments `operation`, `type` and `locales` an AMQP value of type string containing the argument value shall be created by calling `amqpvalue_create_string` in order to be used as value in the application properties map. **]**
XX**SRS_AMQP_MANAGEMENT_01_093: [** If `locales` is NULL, no key/value pair shall be added for it in the application properties map. **]**
XX**SRS_AMQP_MANAGEMENT_01_086: [** The key/value pairs for `operation`, `type` and `locales` shall be added to the application properties map by calling `amqpvalue_set_map_value`. **]**
XX**SRS_AMQP_MANAGEMENT_01_087: [** The application properties obtained after adding the key/value pairs shall be set on the message by calling `message_set_application_properties`. **]**
XX**SRS_AMQP_MANAGEMENT_01_101: [** After setting the application properties, the application properties instance shall be freed by `amqpvalue_destroy`. **]**
XX**SRS_AMQP_MANAGEMENT_01_090: [** If any APIs used to create and set the application properties on the message fails, `amqp_management_execute_operation_async` shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_094: [** In order to set the message Id on the message, the properties shall be obtained by calling `message_get_properties`. **]**
XX**SRS_AMQP_MANAGEMENT_01_099: [** If the properties were not set on the message, a new properties instance shall be created by calling `properties_create`. **]**
XX**SRS_AMQP_MANAGEMENT_01_095: [** A message Id with the next ulong value to be used shall be created by calling `amqpvalue_create_message_id_ulong`. **]**
XX**SRS_AMQP_MANAGEMENT_01_096: [** The message Id value shall be set on the properties by calling `properties_set_message_id`. **]**
XX**SRS_AMQP_MANAGEMENT_01_097: [** The properties thus modified to contain the message Id shall be set on the message by calling `message_set_properties`. **]**
XX**SRS_AMQP_MANAGEMENT_01_100: [** After setting the properties, the properties instance shall be freed by `properties_destroy`. **]**
XX**SRS_AMQP_MANAGEMENT_01_098: [** If any API fails while setting the message Id, `amqp_management_execute_operation_async` shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_088: [** `amqp_management_execute_operation_async` shall send the message by calling `messagesender_send`. **]**
XX**SRS_AMQP_MANAGEMENT_01_089: [** If `messagesender_send` fails, `amqp_management_execute_operation_async` shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_091: [** Once the request message has been sent, an entry shall be stored in the pending operations list by calling `singlylinkedlist_add`. **]**
XX**SRS_AMQP_MANAGEMENT_01_092: [** If `singlylinkedlist_add` fails then `amqp_management_execute_operation_async` shall fail and return a non-zero value. **]**
XX**SRS_AMQP_MANAGEMENT_01_106: [** The message Id set on the message properties shall start at 0. **]**
XX**SRS_AMQP_MANAGEMENT_01_107: [** The message Id set on the message properties shall be incremented with each operation. **]**

### on_message_received

```c
AMQP_VALUE on_message_received(const void* context, MESSAGE_HANDLE message)
```

XX**SRS_AMQP_MANAGEMENT_01_108: [** When `on_message_received` is called with a NULL context, it shall do nothing and return NULL. **]**
XX**SRS_AMQP_MANAGEMENT_01_109: [** `on_message_received` shall obtain the application properties from the message by calling `message_get_application_properties`. **]**
XX**SRS_AMQP_MANAGEMENT_01_110: [** `on_message_received` shall obtain the message properties from the message by calling `message_get_properties`. **]**
XX**SRS_AMQP_MANAGEMENT_01_111: [** `on_message_received` shall obtain the correlation Id from the message properties by using `properties_get_correlation_id`. **]**
XX**SRS_AMQP_MANAGEMENT_01_112: [** `on_message_received` shall check if the correlation Id matches the stored message Id of any pending operation. **]**
XX**SRS_AMQP_MANAGEMENT_01_113: [** If obtaining the application properties or message properties fails, an error shall be indicated by calling `on_amqp_management_error` and passing the `on_amqp_management_error_context` to it. **]**
XX**SRS_AMQP_MANAGEMENT_01_114: [** If obtaining the correlation Id fails, an error shall be indicated by calling `on_amqp_management_error` and passing the `on_amqp_management_error_context` to it. **]**
XX**SRS_AMQP_MANAGEMENT_01_115: [** Iterating through the pending operations shall be done by using `singlylinkedlist_get_head_item` and `singlylinkedlist_get_next_item` until the enm of the pending operations singly linked list is reached. **]**
XX**SRS_AMQP_MANAGEMENT_01_116: [** Each pending operation item value shall be obtained by calling `singlylinkedlist_item_get_value`. **]**
XX**SRS_AMQP_MANAGEMENT_01_117: [** If iterating through the pending operations list fails, an error shall be indicated by calling `on_amqp_management_error` and passing the `on_amqp_management_error_context` to it. **]**
XX**SRS_AMQP_MANAGEMENT_01_118: [** If no pending operation is found matching the correlation Id, an error shall be indicated by calling `on_amqp_management_error` and passing the `on_amqp_management_error_context` to it. **]**
XX**SRS_AMQP_MANAGEMENT_01_119: [** `on_message_received` shall obtain the application properties map by calling `amqpvalue_get_inplace_described_value`. **]**
XX**SRS_AMQP_MANAGEMENT_01_120: [** An AMQP value used to lookup the status code shall be created by calling `amqpvalue_create_string` with `status-code` as argument. **]**
XX**SRS_AMQP_MANAGEMENT_01_121: [** The status code shall be looked up in the application properties by using `amqpvalue_get_map_value`. **]**
XX**SRS_AMQP_MANAGEMENT_01_133: [** The status code value shall be extracted from the value found in the map by using `amqpvalue_get_int`. **]**
XX**SRS_AMQP_MANAGEMENT_01_122: [** If status code is not found an error shall be indicated to the consumer by calling the `on_amqp_management_error` and passing the `on_amqp_management_error_context` to it. **]**
XX**SRS_AMQP_MANAGEMENT_01_123: [** An AMQP value used to lookup the status description shall be created by calling `amqpvalue_create_string` with `status-description` as argument. **]**
XX**SRS_AMQP_MANAGEMENT_01_124: [** The status description shall be looked up in the application properties by using `amqpvalue_get_map_value`. **]**
XX**SRS_AMQP_MANAGEMENT_01_125: [** If status description is not found, NULL shall be passed to the user callback as `status_description` argument. **]**
XX**SRS_AMQP_MANAGEMENT_01_134: [** The status description value shall be extracted from the value found in the map by using `amqpvalue_get_string`. **]**
XX**SRS_AMQP_MANAGEMENT_01_132: [** If any functions manipulating AMQP values, application properties, etc., fail, an error shall be indicated to the consumer by calling the `on_amqp_management_error` and passing the `on_amqp_management_error_context` to it. **]**
XX**SRS_AMQP_MANAGEMENT_01_126: [** If a corresponding correlation Id is found in the pending operations list, the callback associated with the pending operation shall be called. **]**
XX**SRS_AMQP_MANAGEMENT_01_127: [** If the operation succeeded the result callback argument shall be `AMQP_MANAGEMENT_EXECUTE_OPERATION_OK`. **]**
XX**SRS_AMQP_MANAGEMENT_01_128: [** If the status indicates that the operation failed, the result callback argument shall be `AMQP_MANAGEMENT_EXECUTE_OPERATION_FAILED_BAD_STATUS`. **]**
XX**SRS_AMQP_MANAGEMENT_01_129: [** After calling the callback, the pending operation shall be removed from the pending operations list by calling `singlylinkedlist_remove`. **]**
XX**SRS_AMQP_MANAGEMENT_01_130: [** The `on_message_received` shall call `messaging_delivery_accepted` and return the created delivery AMQP value. **]**
XX**SRS_AMQP_MANAGEMENT_01_131: [** All temporary values like AMQP values used as keys shall be freed before exiting the callback. **]**
XX**SRS_AMQP_MANAGEMENT_01_135: [** When an error occurs in creating AMQP values (for status code, etc.) `on_message_received` shall call `messaging_delivery_released` and return the created delivery AMQP value. **]**
XX**SRS_AMQP_MANAGEMENT_01_136: [** When `on_message_received` fails due to errors in parsing the response message `on_message_received` shall call `messaging_delivery_rejected` and return the created delivery AMQP value. **]**

### on_message_sender_state_changed

```c
void on_message_sender_state_changed(void* context, MESSAGE_SENDER_STATE new_state, MESSAGE_SENDER_STATE previous_state)
```

XX**SRS_AMQP_MANAGEMENT_01_137: [** When `on_message_sender_state_changed` is called with NULL `context`, it shall do nothing. **]**
XX**SRS_AMQP_MANAGEMENT_01_138: [** When `on_message_sender_state_changed` is called and the `new_state` is different than `previous_state`, the following actions shall be taken: **]**
XX**SRS_AMQP_MANAGEMENT_01_139: [** For the current state of AMQP management being `OPENING`: **]**
XX**SRS_AMQP_MANAGEMENT_01_140: [** - If `new_state` is `MESSAGE_SENDER_STATE_IDLE`, `MESSAGE_SENDER_STATE_CLOSING` or `MESSAGE_SENDER_STATE_ERROR`, the `on_amqp_management_open_complete` callback shall be called with `AMQP_MANAGEMENT_OPEN_ERROR`, while also passing the context passed in `amqp_management_open_async`. **]**
XX**SRS_AMQP_MANAGEMENT_01_141: [** - If `new_state` is `MESSAGE_SENDER_STATE_OPEN` and the message receiver already indicated its state as `MESSAGE_RECEIVER_STATE_OPEN`, the `on_amqp_management_open_complete` callback shall be called with `AMQP_MANAGEMENT_OPEN_OK`, while also passing the context passed in `amqp_management_open_async`. **]**
XX**SRS_AMQP_MANAGEMENT_01_142: [** - If `new_state` is `MESSAGE_SENDER_STATE_OPEN` and the message receiver did not yet indicate its state as `MESSAGE_RECEIVER_STATE_OPEN`, the `on_amqp_management_open_complete` callback shall not be called.**]**
XX**SRS_AMQP_MANAGEMENT_01_165: [** - If `new_state` is `MESSAGE_SENDER_STATE_OPEING` the transition shall be ignored. **]**
XX**SRS_AMQP_MANAGEMENT_01_144: [** For the current state of AMQP management being `OPEN`: **]**
XX**SRS_AMQP_MANAGEMENT_01_143: [** - If `new_state` is `MESSAGE_SENDER_STATE_IDLE`, `MESSAGE_SENDER_STATE_OPENING`, `MESSAGE_SENDER_STATE_CLOSING` or `MESSAGE_SENDER_STATE_ERROR` the `on_amqp_management_error` callback shall be invoked while passing the `on_amqp_management_error_context` as argument. **]**
XX**SRS_AMQP_MANAGEMENT_01_145: [** - If `new_state` is `MESSAGE_SENDER_STATE_OPEN`, `on_message_sender_state_changed` shall do nothing. **]**
XX**SRS_AMQP_MANAGEMENT_01_146: [** For the current state of AMQP management being `ERROR`: **]**
XX**SRS_AMQP_MANAGEMENT_01_147: [** - All state transitions shall be ignored. **]**
XX**SRS_AMQP_MANAGEMENT_01_148: [** When no state change is detected, `on_message_sender_state_changed` shall do nothing. **]**

### on_message_receiver_state_changed

```c
void on_message_receiver_state_changed(void* context, MESSAGE_RECEIVER_STATE new_state, MESSAGE_RECEIVER_STATE previous_state)
```

XX**SRS_AMQP_MANAGEMENT_01_149: [** When `on_message_receiver_state_changed` is called with NULL `context`, it shall do nothing. **]**
XX**SRS_AMQP_MANAGEMENT_01_150: [** When `on_message_receiver_state_changed` is called and the `new_state` is different than `previous_state`, the following actions shall be taken: **]**
XX**SRS_AMQP_MANAGEMENT_01_151: [** For the current state of AMQP management being `OPENING`: **]**
XX**SRS_AMQP_MANAGEMENT_01_152: [** - If `new_state` is `MESSAGE_RECEIVER_STATE_IDLE`, `MESSAGE_RECEIVER_STATE_CLOSING` or `MESSAGE_RECEIVER_STATE_ERROR`, the `on_amqp_management_open_complete` callback shall be called with `AMQP_MANAGEMENT_OPEN_ERROR`, while also passing the context passed in `amqp_management_open_async`. **]**
XX**SRS_AMQP_MANAGEMENT_01_153: [** - If `new_state` is `MESSAGE_RECEIVER_STATE_OPEN` and the message sender already indicated its state as `MESSAGE_SENDER_STATE_OPEN`, the `on_amqp_management_open_complete` callback shall be called with `AMQP_MANAGEMENT_OPEN_OK`, while also passing the context passed in `amqp_management_open_async`. **]**
XX**SRS_AMQP_MANAGEMENT_01_154: [** - If `new_state` is `MESSAGE_RECEIVER_STATE_OPEN` and the message sender did not yet indicate its state as `MESSAGE_SENDER_STATE_OPEN`, the `on_amqp_management_open_complete` callback shall not be called. **]**
XX**SRS_AMQP_MANAGEMENT_01_164: [** - If `new_state` is `MESSAGE_RECEIVER_STATE_OPEING` the transition shall be ignored. **]**
XX**SRS_AMQP_MANAGEMENT_01_155: [** For the current state of AMQP management being `OPEN`: **]**
XX**SRS_AMQP_MANAGEMENT_01_156: [** - If `new_state` is `MESSAGE_RECEIVER_STATE_IDLE`, `MESSAGE_RECEIVER_STATE_OPENING`, `MESSAGE_RECEIVER_STATE_CLOSING` or `MESSAGE_RECEIVER_STATE_ERROR` the `on_amqp_management_error` callback shall be invoked while passing the `on_amqp_management_error_context` as argument. **]**
XX**SRS_AMQP_MANAGEMENT_01_157: [** - If `new_state` is `MESSAGE_RECEIVER_STATE_OPEN`, `on_message_receiver_state_changed` shall do nothing. **]**
XX**SRS_AMQP_MANAGEMENT_01_158: [** For the current state of AMQP management being `ERROR`: **]**
XX**SRS_AMQP_MANAGEMENT_01_159: [** - All state transitions shall be ignored. **]**
XX**SRS_AMQP_MANAGEMENT_01_160: [** When no state change is detected, `on_message_receiver_state_changed` shall do nothing. **]**

### amqp_management_set_trace

```c
void amqp_management_set_trace(AMQP_MANAGEMENT_HANDLE amqp_management, bool trace_on);
```

XX**SRS_AMQP_MANAGEMENT_01_161: [** `amqp_management_set_trace` shall call `messagesender_set_trace` to enable/disable tracing on the message sender. **]**
XX**SRS_AMQP_MANAGEMENT_01_162: [** `amqp_management_set_trace` shall call `messagereceiver_set_trace` to enable/disable tracing on the message receiver. **]**
XX**SRS_AMQP_MANAGEMENT_01_163: [** If `amqp_management` is NULL, `amqp_management_set_trace` shal do nothing. **]**

### Relevant sections from the AMQP Management spec

Request Messages

XX**SRS_AMQP_MANAGEMENT_01_058: [** Request messages have the following application-properties: **]**

Key Value Type Mandatory? Description

XX**SRS_AMQP_MANAGEMENT_01_059: [** operation string Yes The management operation to be performed. **]** This is case-sensitive.

XX**SRS_AMQP_MANAGEMENT_01_061: [** type string Yes The Manageable Entity Type of the Manageable Entity to be managed. **]** This is case-sensitive.

XX**SRS_AMQP_MANAGEMENT_01_063: [** locales string No A list of locales that the sending peer permits for incoming informational text in response messages. **]**
**SRS_AMQP_MANAGEMENT_01_064: [** The value MUST be of the form (presented in the augmented BNF defined in section 2 of [RFC2616]) **]**:
**SRS_AMQP_MANAGEMENT_01_065: [** `#Language-­Tag` where Language-Tag is defined in [BCP47] **]**.
This list MUST be ordered in decreasing level of preference. The receiving partner will choose the first (most preferred) incoming locale from those which it supports. If none of the requested locales are supported, "en-US" MUST be chosen. Note that "en-US" need not be supplied in this list as it is always the fallback. The string is not case-sensitive.
Other application-properties MAY provide additional context. If an application-property is not recognized then it MUST be ignored.

3.2 Response Messages

XX**SRS_AMQP_MANAGEMENT_01_068: [** The correlation-id of the response message MUST be the correlation-id from the request message (if present) **]**, XX**SRS_AMQP_MANAGEMENT_01_069: [** else the message-id from the request message. **]**
XX**SRS_AMQP_MANAGEMENT_01_070: [** Response messages have the following application-properties: **]**

Key Value Type Mandatory? Description

XX**SRS_AMQP_MANAGEMENT_01_071: [** statusCode integer Yes HTTP response code [RFC2616] **]**

XX**SRS_AMQP_MANAGEMENT_01_072: [** statusDescription string No Description of the status. **]**

**SRS_AMQP_MANAGEMENT_01_073: [** The type and contents of the body are operation-specific. **]**

3.2.1 Successful Operations

XX**SRS_AMQP_MANAGEMENT_01_074: [** Successful operations MUST result in a statusCode in the 2xx range as defined in Section 10.2 of [RFC2616]. **]**
Further details including the form of the body are provided in the definition of each operation.

3.2.2 Unsuccessful Operations

XX**SRS_AMQP_MANAGEMENT_01_075: [** Unsuccessful operations MUST NOT result in a statusCode in the 2xx range as defined in Section 10.2 of [RFC2616]. **]**
The following error status code SHOULD be used for the following common failure scenarios:

statusCode Label Meaning

501 Not Implemented The operation is not supported.
404 Not Found The Manageable Entity on which to perform the operation could not be found

Further details of operation-specific codes are provided in the definition of each operation.
The status description of a response to an unsuccessful operation SHOULD provide further information on the nature of the failure.

The form of the body of a response to an unsuccessful operation is unspecified and MAY be implementation-dependent.
**SRS_AMQP_MANAGEMENT_01_080: [** Clients SHOULD ignore the body of response message if the statusCode is not in the 2xx range. **]**