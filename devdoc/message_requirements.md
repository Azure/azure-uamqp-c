#message requirements
 
##Overview

message is module that stores AMQP messages per the Messaging layer in the AMQP ISO.

##Exposed API

```C
	extern MESSAGE_HANDLE message_create(void);
	extern MESSAGE_HANDLE message_clone(MESSAGE_HANDLE source_message);
	extern void message_destroy(MESSAGE_HANDLE message);
	extern int message_set_header(MESSAGE_HANDLE handle, HEADER_HANDLE message_header);
	extern int message_get_header(MESSAGE_HANDLE handle, HEADER_HANDLE* message_header);
	extern int message_set_delivery_annotations(MESSAGE_HANDLE handle, annotations delivery_annotations);
	extern int message_get_delivery_annotations(MESSAGE_HANDLE handle, annotations* delivery_annotations);
	extern int message_set_message_annotations(MESSAGE_HANDLE handle, annotations delivery_annotations);
	extern int message_get_message_annotations(MESSAGE_HANDLE handle, annotations* delivery_annotations);
	extern int message_set_properties(MESSAGE_HANDLE handle, PROPERTIES_HANDLE properties);
	extern int message_get_properties(MESSAGE_HANDLE handle, PROPERTIES_HANDLE* properties);
	extern int message_set_application_properties(MESSAGE_HANDLE handle, AMQP_VALUE application_properties);
	extern int message_get_application_properties(MESSAGE_HANDLE handle, AMQP_VALUE* application_properties);
	extern int message_set_footer(MESSAGE_HANDLE handle, annotations footer);
	extern int message_get_footer(MESSAGE_HANDLE handle, annotations* footer);
	extern int message_set_body_amqp_data(MESSAGE_HANDLE handle, BINARY_DATA binary_data);
	extern int message_get_body_amqp_data(MESSAGE_HANDLE handle, BINARY_DATA* binary_data);
```

###message_create

```C
extern MESSAGE_HANDLE message_create(void);
```

**SRS_MESSAGE_01_001: [**message_create shall create a new AMQP message instance and on success it shall return a non-NULL handle for the newly created message instance.**]** 
**SRS_MESSAGE_01_002: [**If allocating memory for the message fails, message_create shall fail and return NULL.**]** 

###message_clone

```C
extern MESSAGE_HANDLE message_clone(MESSAGE_HANDLE source_message);
```

**SRS_MESSAGE_01_003: [**message_clone shall clone a message entirely and on success return a non-NULL handle to the cloned message.**]** 
**SRS_MESSAGE_01_062: [**If source_message is NULL, message_clone shall fail and return NULL.**]** 
**SRS_MESSAGE_01_004: [**If allocating memory for the new cloned message fails, message_clone shall fail and return NULL.**]** 
**SRS_MESSAGE_01_005: [**If a header exists on the source message it shall be cloned by using header_clone.**]** 
**SRS_MESSAGE_01_006: [**If delivery annotations exist on the source message they shall be cloned by using annotations_clone.**]** 
**SRS_MESSAGE_01_007: [**If message annotations exist on the source message they shall be cloned by using annotations_clone.**]** 
**SRS_MESSAGE_01_008: [**If message properties exist on the source message they shall be cloned by using properties_clone.**]** 
**SRS_MESSAGE_01_009: [**If application properties exist on the source message they shall be cloned by using amqpvalue_clone.**]** 
**SRS_MESSAGE_01_010: [**If a footer exists on the source message it shall be cloned by using annotations_clone.**]** 
**SRS_MESSAGE_01_011: [**If an AMQP data has been set as message body on the source message it shall be cloned by allocating memory for the binary payload.**]** 
**SRS_MESSAGE_01_012: [**If any cloning operation for the members of the source message fails, then message_clone shall fail and return NULL.**]** 

###message_destroy

```C
extern void message_destroy(MESSAGE_HANDLE message);
```

**SRS_MESSAGE_01_013: [**message_destroy shall free all resources allocated by the message instance identified by the message argument.**]** 
**SRS_MESSAGE_01_014: [**If message is NULL, message_destroy shall do nothing.**]** 
**SRS_MESSAGE_01_015: [**The message header shall be freed by calling header_destroy.**]** 
**SRS_MESSAGE_01_016: [**The delivery annotations shall be freed by calling annotations_destroy.**]** 
**SRS_MESSAGE_01_017: [**The message annotations shall be freed by calling annotations_destroy.**]** 
**SRS_MESSAGE_01_018: [**The message properties shall be freed by calling properties_destroy.**]** 
**SRS_MESSAGE_01_019: [**The application properties shall be freed by calling amqpvalue_destroy.**]** 
**SRS_MESSAGE_01_020: [**The message footer shall be freed by calling annotations_destroy.**]** 
**SRS_MESSAGE_01_021: [**If the message body is made of an AMQP data, the memory buffer holding it shall be freed by calling amqpalloc_free.**]** 

###message_set_header

```C
extern int message_set_header(MESSAGE_HANDLE message, HEADER_HANDLE message_header);
```

**SRS_MESSAGE_01_022: [**message_set_header shall copy the contents of message_header as the header for the message instance identified by message.**]** 
**SRS_MESSAGE_01_023: [**On success it shall return 0.**]** 
**SRS_MESSAGE_01_024: [**If message or message_header is NULL, message_set_header shall fail and return a non-zero value.**]** 
**SRS_MESSAGE_01_025: [**Cloning the header shall be done by calling header_clone.**]** 
**SRS_MESSAGE_01_026: [**If header_clone fails, message_set_header shall fail and return a non-zero value.**]** 

###message_get_header

```C
extern int message_get_header(MESSAGE_HANDLE message, HEADER_HANDLE* message_header);
```

**SRS_MESSAGE_01_027: [**message_get_header shall copy the contents of header for the message instance identified by message into the argument message_header.**]** 
**SRS_MESSAGE_01_028: [**On success, message_get_header shall return 0.**]** **SRS_MESSAGE_01_029: [**If message or message_header is NULL, message_get_header shall fail and return a non-zero value.**]** 
**SRS_MESSAGE_01_030: [**Cloning the header shall be done by calling header_clone.**]** 
**SRS_MESSAGE_01_031: [**If header_clone fails, message_get_header shall fail and return a non-zero value.**]** 

###message_set_delivery_annotations

```C
extern int message_set_delivery_annotations(MESSAGE_HANDLE message, annotations delivery_annotations);
```

**SRS_MESSAGE_01_032: [**message_set_delivery_annotations shall copy the contents of delivery_annotations as the delivery annotations for the message instance identified by message.**]** 
**SRS_MESSAGE_01_033: [**On success it shall return 0.**]** 
**SRS_MESSAGE_01_034: [**If message or delivery_annotations is NULL, message_set_delivery_annotations shall fail and return a non-zero value.**]** 
**SRS_MESSAGE_01_035: [**Cloning the delivery annotations shall be done by calling annotations_clone.**]** 
**SRS_MESSAGE_01_036: [**If annotations_clone fails, message_set_delivery_annotations shall fail and return a non-zero value.**]** 

###message_get_delivery_annotations

```C
extern int message_get_delivery_annotations(MESSAGE_HANDLE message, annotations* delivery_annotations);
```

**SRS_MESSAGE_01_037: [**message_get_delivery_annotations shall copy the contents of delivery annotations for the message instance identified by message into the argument delivery_annotations.**]** 
**SRS_MESSAGE_01_038: [**On success, message_get_delivery_annotations shall return 0.**]** 
**SRS_MESSAGE_01_039: [**If message or delivery_annotations is NULL, message_get_delivery_annotations shall fail and return a non-zero value.**]** 
**SRS_MESSAGE_01_040: [**Cloning the delivery annotations shall be done by calling annotations_clone.**]** 
**SRS_MESSAGE_01_041: [**If annotations_clone fails, message_get_delivery_annotations shall fail and return a non-zero value.**]** 

###message_set_message_annotations

```C
extern int message_set_message_annotations(MESSAGE_HANDLE message, annotations delivery_annotations);
```

**SRS_MESSAGE_01_042: [**message_set_message_annotations shall copy the contents of message_annotations as the message annotations for the message instance identified by message.**]** 
**SRS_MESSAGE_01_043: [**On success it shall return 0.**]** 
**SRS_MESSAGE_01_044: [**If message or message_annotations is NULL, message_set_message_annotations shall fail and return a non-zero value.**]** 
**SRS_MESSAGE_01_045: [**Cloning the message annotations shall be done by calling annotations_clone.**]** 
**SRS_MESSAGE_01_046: [**If annotations_clone fails, message_set_message_annotations shall fail and return a non-zero value.**]** 

###message_get_message_annotations

```C
extern int message_get_message_annotations(MESSAGE_HANDLE message, annotations* delivery_annotations);
```

**SRS_MESSAGE_01_047: [**message_get_message_annotations shall copy the contents of message annotations for the message instance identified by message into the argument message_annotations.**]** 
**SRS_MESSAGE_01_048: [**On success, message_get_message_annotations shall return 0.**]** 
**SRS_MESSAGE_01_049: [**If message or message_annotations is NULL, message_get_message_annotations shall fail and return a non-zero value.**]** 
**SRS_MESSAGE_01_050: [**Cloning the message annotations shall be done by calling annotations_clone.**]** 
**SRS_MESSAGE_01_051: [**If annotations_clone fails, message_get_message_annotations shall fail and return a non-zero value.**]** 

###message_set_properties

```C
extern int message_set_properties(MESSAGE_HANDLE message, PROPERTIES_HANDLE properties);
```

**SRS_MESSAGE_01_052: [**message_set_properties shall copy the contents of properties as the message properties for the message instance identified by message.**]** 
**SRS_MESSAGE_01_053: [**On success it shall return 0.**]** 
**SRS_MESSAGE_01_054: [**If message or properties is NULL, message_set_properties shall fail and return a non-zero value.**]** 
**SRS_MESSAGE_01_055: [**Cloning the message properties shall be done by calling properties_clone.**]** 
**SRS_MESSAGE_01_056: [**If properties_clone fails, message_set_properties shall fail and return a non-zero value.**]** 

###message_get_properties

```C
extern int message_get_properties(MESSAGE_HANDLE message, PROPERTIES_HANDLE* properties);
```

**SRS_MESSAGE_01_057: [**message_get_properties shall copy the contents of message properties for the message instance identified by message into the argument properties.**]** 
**SRS_MESSAGE_01_058: [**On success, message_get_properties shall return 0.**]** 
**SRS_MESSAGE_01_059: [**If message or properties is NULL, message_get_properties shall fail and return a non-zero value.**]** 
**SRS_MESSAGE_01_060: [**Cloning the message properties shall be done by calling properties_clone.**]** 
**SRS_MESSAGE_01_061: [**If properties_clone fails, message_get_properties shall fail and return a non-zero value.**]** 

