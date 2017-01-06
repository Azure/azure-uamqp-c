#saslclientio requirements
 
##Overview

saslclientio is module that implements a concrete IO, that implements the section 5.3 of the AMQP ISO.

##Exposed API

```C
typedef struct SASLCLIENTIO_CONFIG_TAG
{
	XIO_HANDLE underlying_io;
	SASL_MECHANISM_HANDLE sasl_mechanism;
} SASLCLIENTIO_CONFIG;

extern CONCRETE_IO_HANDLE saslclientio_create(void* io_create_parameters);
extern void saslclientio_destroy(CONCRETE_IO_HANDLE sasl_client_io);
extern int saslclientio_open(CONCRETE_IO_HANDLE sasl_client_io, ON_BYTES_RECEIVED on_bytes_received, ON_IO_STATE_CHANGED on_io_state_changed, void* callback_context);
extern int saslclientio_close(CONCRETE_IO_HANDLE sasl_client_io);
extern int saslclientio_send(CONCRETE_IO_HANDLE sasl_client_io, const void* buffer, size_t size, ON_SEND_COMPLETE on_send_complete, void* callback_context);
extern void saslclientio_dowork(CONCRETE_IO_HANDLE sasl_client_io);
extern int saslclientio_setoption(CONCRETE_IO_HANDLE socket_io, const char* optionName, const void* value);

extern const IO_INTERFACE_DESCRIPTION* saslclientio_get_interface_description(void);
```

###saslclientio_create

```C
extern CONCRETE_IO_HANDLE saslclientio_create(void* io_create_parameters);
```

**SRS_SASLCLIENTIO_01_004: [**saslclientio_create shall return on success a non-NULL handle to a new SASL client IO instance.**]** 
**SRS_SASLCLIENTIO_01_005: [**If xio_create_parameters is NULL, saslclientio_create shall fail and return NULL.**]** 
**SRS_SASLCLIENTIO_01_006: [**If memory cannot be allocated for the new instance, saslclientio_create shall fail and return NULL.**]** 
**SRS_SASLCLIENTIO_01_089: [**saslclientio_create shall create a frame_codec to be used for encoding/decoding frames bycalling frame_codec_create and passing the underlying_io as argument.**]** 
**SRS_SASLCLIENTIO_01_090: [**If frame_codec_create fails, then saslclientio_create shall fail and return NULL.**]** 
**SRS_SASLCLIENTIO_01_084: [**saslclientio_create shall create a sasl_frame_codec to be used for SASL frame encoding/decoding by calling sasl_frame_codec_create and passing the just created frame_codec as argument.**]** 
**SRS_SASLCLIENTIO_01_085: [**If sasl_frame_codec_create fails, then saslclientio_create shall fail and return NULL.**]** 
**SRS_SASLCLIENTIO_01_092: [**If any of the sasl_mechanism or underlying_io members of the configuration structure are NULL, saslclientio_create shall fail and return NULL.**]** 

###saslclientio_destroy

```C
extern void saslclientio_destroy(CONCRETE_IO_HANDLE sasl_client_io);
```

**SRS_SASLCLIENTIO_01_007: [**saslclientio_destroy shall free all resources associated with the SASL client IO handle.**]** 
**SRS_SASLCLIENTIO_01_086: [**saslclientio_destroy shall destroy the sasl_frame_codec created in saslclientio_create by calling sasl_frame_codec_destroy.**]** 
**SRS_SASLCLIENTIO_01_091: [**saslclientio_destroy shall destroy the frame_codec created in saslclientio_create by calling frame_codec_destroy.**]** 
**SRS_SASLCLIENTIO_01_008: [**If the argument sasl_client_io is NULL, saslclientio_destroy shall do nothing.**]** 

###saslclientio_open

```C
extern int saslclientio_open(CONCRETE_IO_HANDLE sasl_client_io, ON_BYTES_RECEIVED on_bytes_received, ON_IO_STATE_CHANGED on_io_state_changed, void* callback_context);
```

**SRS_SASLCLIENTIO_01_009: [**saslclientio_open shall call xio_open on the underlying_io passed to saslclientio_create.**]** 
**SRS_SASLCLIENTIO_01_010: [**On success, saslclientio_open shall return 0.**]** 
**SRS_SASLCLIENTIO_01_011: [**If any of the sasl_client_io or on_bytes_received arguments is NULL, saslclientio_open shall fail and return a non-zero value.**]** 
**SRS_SASLCLIENTIO_01_012: [**If the open of the underlying_io fails, saslclientio_open shall fail and return non-zero value.**]** 
**SRS_SASLCLIENTIO_01_013: [**saslclientio_open shall pass to xio_open a callback for receiving bytes and a state changed callback for the underlying_io state changes.**]** 

###saslclientio_close

```C
extern int saslclientio_close(CONCRETE_IO_HANDLE sasl_client_io);
```

**SRS_SASLCLIENTIO_01_015: [**saslclientio_close shall close the underlying io handle passed in saslclientio_create by calling xio_close.**]** 
**SRS_SASLCLIENTIO_01_016: [**On success, saslclientio_close shall return 0.**]** 
**SRS_SASLCLIENTIO_01_098: [**saslclientio_close shall only perform the close if the state is OPEN, OPENING or ERROR.**]** 
**SRS_SASLCLIENTIO_01_097: [**If saslclientio_close is called when the IO is in the IO_STATE_NOT_OPEN state, no call to the underlying IO shall be made and saslclientio_close shall return 0.**]** 
**SRS_SASLCLIENTIO_01_017: [**If sasl_client_io is NULL, saslclientio_close shall fail and return a non-zero value.**]** 
**SRS_SASLCLIENTIO_01_018: [**If xio_close fails, then saslclientio_close shall return a non-zero value.**]** 

###saslclientio_send

```C
extern int saslclientio_send(CONCRETE_IO_HANDLE sasl_client_io, const void* buffer, size_t size, ON_SEND_COMPLETE on_send_complete, void* callback_context);
```

**SRS_SASLCLIENTIO_01_019: [**If saslclientio_send is called while the SASL client IO state is not IO_STATE_OPEN, saslclientio_send shall fail and return a non-zero value.**]** 
**SRS_SASLCLIENTIO_01_020: [**If the SASL client IO state is IO_STATE_OPEN, saslclientio_send shall call xio_send on the underlying_io passed to saslclientio_create, while passing as arguments the buffer, size, on_send_complete and callback_context.**]** 
**SRS_SASLCLIENTIO_01_021: [**On success, saslclientio_send shall return 0.**]** 
**SRS_SASLCLIENTIO_01_022: [**If the saslio or buffer argument is NULL, saslclientio_send shall fail and return a non-zero value.**]** 
**SRS_SASLCLIENTIO_01_023: [**If size is 0, saslclientio_send shall fail and return a non-zero value.**]** 
**SRS_SASLCLIENTIO_01_024: [**If the call to xio_send fails, then saslclientio_send shall fail and return a non-zero value.**]** 

###saslclientio_dowork

```C
extern void saslclientio_dowork(CONCRETE_IO_HANDLE sasl_client_io);
```

**SRS_SASLCLIENTIO_01_025: [**saslclientio_dowork shall call the xio_dowork on the underlying_io passed in saslclientio_create.**]** 
**SRS_SASLCLIENTIO_01_099: [**If the state of the IO is NOT_OPEN or ERROR, saslclientio_dowork shall make no calls to the underlying IO.**]** 
**SRS_SASLCLIENTIO_01_026: [**If the sasl_client_io argument is NULL, saslclientio_dowork shall do nothing.**]** 

###saslclientio_setoption

```C
extern int saslclientio_setoption(CONCRETE_IO_HANDLE socket_io, const char* optionName, const void* value);
```

**SRS_SASLCLIENTIO_03_001: [**saslclientio_setoption shall forward options to underlying io.**]** 

###saslclientio_get_interface_description

```C
extern const IO_INTERFACE_DESCRIPTION* saslclientio_get_interface_description(void);
```
**SRS_SASLCLIENTIO_01_087: [**saslclientio_get_interface_description shall return a pointer to an IO_INTERFACE_DESCRIPTION structure that contains pointers to the functions: saslclientio_create, saslclientio_destroy, saslclientio_open, saslclientio_close, saslclientio_send, saslclientio_setoptions and saslclientio_dowork.**]** 

###on_bytes_received

**SRS_SASLCLIENTIO_01_027: [**When the on_bytes_received callback passed to the underlying IO is called and the SASL client IO state is IO_STATE_OPEN, the bytes shall be indicated to the user of SASL client IO by calling the on_bytes_received that was passed in saslclientio_open.**]** 
**SRS_SASLCLIENTIO_01_028: [**If buffer is NULL or size is zero, nothing should be indicated as received and the saslio state shall be switched to ERROR the on_state_changed callback shall be triggered.**]** 
**SRS_SASLCLIENTIO_01_029: [**The context argument shall be set to the callback_context passed in saslclientio_open.**]** 
**SRS_SASLCLIENTIO_01_030: [**If bytes are received when the SASL client IO state is IO_STATE_OPENING, the bytes shall be consumed by the SASL client IO to satisfy the SASL handshake.**]** 
**SRS_SASLCLIENTIO_01_031: [**If bytes are received when the SASL client IO state is IO_STATE_ERROR, SASL client IO shall do nothing.**]** 

###on_state_changed

The following actions matrix shall be implemented when a new state change is received from the underlying IO:
|          | new underlying IO state																																				|
|----------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
|SASL state|NOT_OPEN							  |OPENING								   |OPEN											|ERROR									|
|----------|--------------------------------------|----------------------------------------|------------------------------------------------|---------------------------------------|
|NOT_OPEN  |**SRS_SASLCLIENTIO_01_115: [**do nothing**]** |	**SRS_SASLCLIENTIO_01_108: [**do nothing**]**  |**SRS_SASLCLIENTIO_01_104: [**do nothing**]**			|**SRS_SASLCLIENTIO_01_100: [**do nothing**]**	| 
|OPENING   |**SRS_SASLCLIENTIO_01_114: [**raise ERROR**]**|	**SRS_SASLCLIENTIO_01_109: [**do nothing**]**  |**SRS_SASLCLIENTIO_01_105: [**start header exchange**]**|**SRS_SASLCLIENTIO_01_101: [**raise ERROR**]**	| 
|OPEN	   |**SRS_SASLCLIENTIO_01_113: [**raise ERROR**]**| **SRS_SASLCLIENTIO_01_110: [**raise ERROR**]** |**SRS_SASLCLIENTIO_01_106: [**do nothing**]** 			|**SRS_SASLCLIENTIO_01_102: [**raise ERROR**]**	|
|ERROR     |**SRS_SASLCLIENTIO_01_112: [**do nothing**]** | **SRS_SASLCLIENTIO_01_111: [**do nothing**]**  |**SRS_SASLCLIENTIO_01_107: [**do nothing**]** 			|**SRS_SASLCLIENTIO_01_103: [**do nothing**]**	|
|----------|--------------------------------------|----------------------------------------|------------------------------------------------|---------------------------------------|

Starting the header exchange is done as follows:

**SRS_SASLCLIENTIO_01_078: [**SASL client IO shall start the header exchange by sending the SASL header.**]** 
**SRS_SASLCLIENTIO_01_095: [**Sending the header shall be done by using xio_send.**]** 
**SRS_SASLCLIENTIO_01_077: [**If sending the SASL header fails, the SASL client IO state shall be set to IO_STATE_ERROR and the on_state_changed callback shall be triggered.**]** 
**SRS_SASLCLIENTIO_01_116: [**Any underlying IO state changes to state OPEN after the header exchange has been started shall trigger no action.**]** 

###on_sasl_frame_received_callback

**SRS_SASLCLIENTIO_01_117: [**If on_sasl_frame_received_callback is called when the state of the IO is OPEN then the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.**]** 
**SRS_SASLCLIENTIO_01_118: [**If on_sasl_frame_received_callback is called in the OPENING state but the header exchange has not yet been completed, then the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.**]** 
When a frame is indicated as received by sasl_frame_codec it shall be processed as described in the ISO. 
**SRS_SASLCLIENTIO_01_070: [**When a frame needs to be sent as part of the SASL handshake frame exchange, the send shall be done by calling sasl_frame_codec_encode_frame.**]** 
**SRS_SASLCLIENTIO_01_071: [**If sasl_frame_codec_encode_frame fails, then the state of SASL client IO shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.**]** 
**SRS_SASLCLIENTIO_01_072: [**When the SASL handshake is complete, if the handshake is successful, the SASL client IO state shall be switched to IO_STATE_OPEN and the on_state_changed callback shall be triggered.**]** 
**SRS_SASLCLIENTIO_01_119: [**If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.**]** 
**SRS_SASLCLIENTIO_01_073: [**If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.**]**

###on_bytes_encoded

**SRS_SASLCLIENTIO_01_120: [**When SASL client IO is notified by sasl_frame_codec of bytes that have been encoded via the on_bytes_encoded callback and SASL client IO is in the state OPENING, SASL client IO shall send these bytes by using xio_send.**]** 
**SRS_SASLCLIENTIO_01_121: [**If xio_send fails, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.**]** 

###on_frame_codec_error

**SRS_SASLCLIENTIO_01_122: [**When on_frame_codec_error is called while in the OPENING or OPEN state the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.**]** 
**SRS_SASLCLIENTIO_01_123: [**When on_frame_codec_error is called in the ERROR state nothing shall be done.**]** 

###on_sasl_frame_codec_error

**SRS_SASLCLIENTIO_01_124: [**When on_sasl_frame_codec_error is called while in the OPENING or OPEN state the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.**]** 
**SRS_SASLCLIENTIO_01_125: [**When on_sasl_frame_codec_error is called in the ERROR state nothing shall be done.**]** 

##SASL negotiation

**SRS_SASLCLIENTIO_01_067: [**The SASL frame exchange shall be started as soon as the SASL header handshake is done.**]** 
**SRS_SASLCLIENTIO_01_068: [**During the SASL frame exchange that constitutes the handshake the received bytes from the underlying IO shall be fed to the frame_codec instance created in saslclientio_create by calling frame_codec_receive_bytes.**]** 
**SRS_SASLCLIENTIO_01_088: [**If frame_codec_receive_bytes fails, the state of SASL client IO shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.**]** 

##ISO section

**SRS_SASLCLIENTIO_01_001: [**To establish a SASL layer, each peer MUST start by sending a protocol header.**]** 
**SRS_SASLCLIENTIO_01_002: [**The protocol header consists of the upper case ASCII letters "AMQP" followed by a protocol id of three, followed by three unsigned bytes representing the major, minor, and revision of the specification version (currently 1 (SASL-MAJOR), 0 (SASLMINOR), 0 (SASL-REVISION)).**]** In total this is an 8-octet sequence:

...

Figure 5.3: Protocol Header for SASL Security Layer

**SRS_SASLCLIENTIO_01_003: [**Other than using a protocol id of three, the exchange of SASL layer headers follows the same rules specified in the version negotiation section of the transport specification (See Part 2: section 2.2).**]** 
The following diagram illustrates the interaction involved in creating a SASL security layer:

...

Figure 5.4: Establishing a SASL Security Layer

SASL Negotiation

**SRS_SASLCLIENTIO_01_032: [**The peer acting as the SASL server MUST announce supported authentication mechanisms using the sasl-mechanisms frame.**]** 
**SRS_SASLCLIENTIO_01_033: [**The partner MUST then choose one of the supported mechanisms and initiate a sasl exchange.**]** 

SASL Client SASL Server

**SRS_SASLCLIENTIO_01_034: [**<-- SASL-MECHANISMS**]** 
**SRS_SASLCLIENTIO_01_035: [**SASL-INIT -->**]** 
...
**SRS_SASLCLIENTIO_01_036: [**<-- SASL-CHALLENGE ***]** 
**SRS_SASLCLIENTIO_01_037: [**SASL-RESPONSE -->**]** 
...
**SRS_SASLCLIENTIO_01_038: [**<-- SASL-OUTCOME**]** 

* Note that **SRS_SASLCLIENTIO_01_039: [**the SASL challenge/response step can occur zero or more times depending on the details of the SASL mechanism chosen.**]** 

Figure 5.6: SASL Exchange

**SRS_SASLCLIENTIO_01_040: [**The peer playing the role of the SASL client and the peer playing the role of the SASL server MUST correspond to the TCP client and server respectively.**]** 

5.3.3 Security Frame Bodies

5.3.3.1 SASL Mechanisms

Advertise available sasl mechanisms.

\<type name="sasl-mechanisms" class="composite" source="list" provides="sasl-frame">
\<descriptor name="amqp:sasl-mechanisms:list" code="0x00000000:0x00000040"/>
\<field name="sasl-server-mechanisms" type="symbol" multiple="true" mandatory="true"/>
\</type>

Advertises the available SASL mechanisms that can be used for authentication.

Field Details

sasl-server-mechanisms supported sasl mechanisms

**SRS_SASLCLIENTIO_01_041: [**A list of the sasl security mechanisms supported by the sending peer.**]** **SRS_SASLCLIENTIO_01_042: [**It is invalid for this list to be null or empty.**]** If the sending peer does not require its partner to authenticate with it, then it SHOULD send a list of one element with its value as the SASL mechanism ANONYMOUS. **SRS_SASLCLIENTIO_01_043: [**The server mechanisms are ordered in decreasing level of preference.**]** 

5.3.3.2 SASL Init

Initiate sasl exchange.

\<type name="sasl-init" class="composite" source="list" provides="sasl-frame">

\<descriptor name="amqp:sasl-init:list" code="0x00000000:0x00000041"/>

\<field name="mechanism" type="symbol" mandatory="true"/>

\<field name="initial-response" type="binary"/>

\<field name="hostname" type="string"/>

\</type>

**SRS_SASLCLIENTIO_01_054: [**Selects the sasl mechanism and provides the initial response if needed.**]** 

Field Details

mechanism selected security mechanism

**SRS_SASLCLIENTIO_01_045: [**The name of the SASL mechanism used for the SASL exchange.**]** If the selected mechanism is not supported by the receiving peer, it MUST close the connection with the authentication-failure close-code. **SRS_SASLCLIENTIO_01_046: [**Each peer MUST authenticate using the highest-level security profile it can handle from the list provided by the partner.**]** 
initial-response security response data
**SRS_SASLCLIENTIO_01_047: [**A block of opaque data passed to the security mechanism.**]** **SRS_SASLCLIENTIO_01_048: [**The contents of this data are defined by the SASL security mechanism.**]** 
hostname the name of the target host
**SRS_SASLCLIENTIO_01_049: [**The DNS name of the host (either fully qualified or relative) to which the sending peer is connecting.**]** 
**SRS_SASLCLIENTIO_01_050: [**It is not mandatory to provide the hostname.**]** If no hostname is provided the receiving peer SHOULD select a default based on its own configuration.
This field can be used by AMQP proxies to determine the correct back-end service to connect the client to, and to determine the domain to validate the client's credentials against.
**SRS_SASLCLIENTIO_01_051: [**This field might already have been specified by the server name indication extension as described in RFC-4366 [RFC4366**]**, if a TLS layer is used, in which case this field SHOULD either be null or contain the same value.] It is undefined what a different value to those already specified means.

5.3.3.3 SASL Challenge

Security mechanism challenge.

\<type name="sasl-challenge" class="composite" source="list" provides="sasl-frame">

\<descriptor name="amqp:sasl-challenge:list" code="0x00000000:0x00000042"/>

\<field name="challenge" type="binary" mandatory="true"/>

\</type>

**SRS_SASLCLIENTIO_01_052: [**Send the SASL challenge data as defined by the SASL specification.**]** 

Field Details

challenge security challenge data

**SRS_SASLCLIENTIO_01_053: [**Challenge information, a block of opaque binary data passed to the security mechanism.**]** 

5.3.3.4 SASL Response

Security mechanism response.

\<type name="sasl-response" class="composite" source="list" provides="sasl-frame">

\<descriptor name="amqp:sasl-response:list" code="0x00000000:0x00000043"/>

\<field name="response" type="binary" mandatory="true"/>

\</type>

**SRS_SASLCLIENTIO_01_055: [**Send the SASL response data as defined by the SASL specification.**]** 

Field Details

response security response data

**SRS_SASLCLIENTIO_01_056: [**A block of opaque data passed to the security mechanism.**]** **SRS_SASLCLIENTIO_01_057: [**The contents of this data are defined by the SASL security mechanism.**]** 

5.3.3.5 SASL Outcome

Indicates the outcome of the sasl dialog.

\<type name="sasl-outcome" class="composite" source="list" provides="sasl-frame">

\<descriptor name="amqp:sasl-outcome:list" code="0x00000000:0x00000044"/>

\<field name="code" type="sasl-code" mandatory="true"/>

\<field name="additional-data" type="binary"/>

\</type>

**SRS_SASLCLIENTIO_01_058: [**This frame indicates the outcome of the SASL dialog.**]****SRS_SASLCLIENTIO_01_059: [**Upon successful completion of the SASL dialog the security layer has been established**]**, and the peers MUST exchange protocol headers to either start a nested security layer, or to establish the AMQP connection.

Field Details

code indicates the outcome of the sasl dialog

**SRS_SASLCLIENTIO_01_060: [**A reply-code indicating the outcome of the SASL dialog.**]** 

additional-data additional data as specified in RFC-4422

**SRS_SASLCLIENTIO_01_061: [**The additional-data field carries additional data on successful authentication outcome as specified by the SASL specification [RFC4422**]**.] If the authentication is unsuccessful, this field is not set.

5.3.3.6 SASL Code

Codes to indicate the outcome of the sasl dialog.

\<type name="sasl-code" class="restricted" source="ubyte">

\<choice name="ok" value="0"/>

\<choice name="auth" value="1"/>

\<choice name="sys" value="2"/>

\<choice name="sys-perm" value="3"/>

\<choice name="sys-temp" value="4"/>

\</type>

Valid Values

**SRS_SASLCLIENTIO_01_062: [**0 Connection authentication succeeded.**]** 
**SRS_SASLCLIENTIO_01_063: [**1 Connection authentication failed due to an unspecified problem with the supplied credentials.**]** 
**SRS_SASLCLIENTIO_01_064: [**2 Connection authentication failed due to a system error.**]** 
**SRS_SASLCLIENTIO_01_065: [**3 Connection authentication failed due to a system error that is unlikely to be corrected without intervention.**]** 
**SRS_SASLCLIENTIO_01_066: [**4 Connection authentication failed due to a transient system error.**]** 

5.3.4 Constant Definitions

**SRS_SASLCLIENTIO_01_124: [**SASL-MAJOR 1 major protocol version.**]** 
**SRS_SASLCLIENTIO_01_125: [**SASL-MINOR 0 minor protocol version.**]** 
**SRS_SASLCLIENTIO_01_126: [**SASL-REVISION 0 protocol revision.**]** 
