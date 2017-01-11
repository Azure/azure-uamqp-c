// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "azure_uamqp_c/saslclientio.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_uamqp_c/amqpalloc.h"
#include "azure_uamqp_c/frame_codec.h"
#include "azure_uamqp_c/sasl_frame_codec.h"
#include "azure_uamqp_c/amqp_definitions.h"
#include "azure_uamqp_c/amqpvalue_to_string.h"

typedef enum IO_STATE_TAG
{
    IO_STATE_NOT_OPEN,
    IO_STATE_OPENING_UNDERLYING_IO,
    IO_STATE_SASL_HANDSHAKE,
    IO_STATE_OPEN,
    IO_STATE_CLOSING,
    IO_STATE_ERROR
} IO_STATE;

typedef enum SASL_HEADER_EXCHANGE_STATE_TAG
{
    SASL_HEADER_EXCHANGE_IDLE,
    SASL_HEADER_EXCHANGE_HEADER_SENT,
    SASL_HEADER_EXCHANGE_HEADER_RCVD,
    SASL_HEADER_EXCHANGE_HEADER_EXCH
} SASL_HEADER_EXCHANGE_STATE;

typedef enum SASL_CLIENT_NEGOTIATION_STATE_TAG
{
    SASL_CLIENT_NEGOTIATION_NOT_STARTED,
    SASL_CLIENT_NEGOTIATION_MECH_RCVD,
    SASL_CLIENT_NEGOTIATION_INIT_SENT,
    SASL_CLIENT_NEGOTIATION_CHALLENGE_RCVD,
    SASL_CLIENT_NEGOTIATION_RESPONSE_SENT,
    SASL_CLIENT_NEGOTIATION_OUTCOME_RCVD,
    SASL_CLIENT_NEGOTIATION_ERROR
} SASL_CLIENT_NEGOTIATION_STATE;

typedef struct SASL_CLIENT_IO_INSTANCE_TAG
{
    XIO_HANDLE underlying_io;
    ON_BYTES_RECEIVED on_bytes_received;
    ON_IO_OPEN_COMPLETE on_io_open_complete;
    ON_IO_CLOSE_COMPLETE on_io_close_complete;
    ON_IO_ERROR on_io_error;
    void* on_bytes_received_context;
    void* on_io_open_complete_context;
    void* on_io_close_complete_context;
    void* on_io_error_context;
    SASL_HEADER_EXCHANGE_STATE sasl_header_exchange_state;
    SASL_CLIENT_NEGOTIATION_STATE sasl_client_negotiation_state;
    size_t header_bytes_received;
    SASL_FRAME_CODEC_HANDLE sasl_frame_codec;
    FRAME_CODEC_HANDLE frame_codec;
    IO_STATE io_state;
    SASL_MECHANISM_HANDLE sasl_mechanism;
    unsigned int is_trace_on : 1;
} SASL_CLIENT_IO_INSTANCE;

/* Codes_SRS_SASLCLIENTIO_01_002: [The protocol header consists of the upper case ASCII letters "AMQP" followed by a protocol id of three, followed by three unsigned bytes representing the major, minor, and revision of the specification version (currently 1 (SASL-MAJOR), 0 (SASLMINOR), 0 (SASL-REVISION)).] */
/* Codes_SRS_SASLCLIENTIO_01_124: [SASL-MAJOR 1 major protocol version.] */
/* Codes_SRS_SASLCLIENTIO_01_125: [SASL-MINOR 0 minor protocol version.] */
/* Codes_SRS_SASLCLIENTIO_01_126: [SASL-REVISION 0 protocol revision.] */
const unsigned char sasl_header[] = { 'A', 'M', 'Q', 'P', 3, 1, 0, 0 };

static void indicate_error(SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance)
{
    if (sasl_client_io_instance->on_io_error != NULL)
    {
        sasl_client_io_instance->on_io_error(sasl_client_io_instance->on_io_error_context);
    }
}

static void indicate_open_complete(SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance, IO_OPEN_RESULT open_result)
{
    if (sasl_client_io_instance->on_io_open_complete != NULL)
    {
        sasl_client_io_instance->on_io_open_complete(sasl_client_io_instance->on_io_open_complete_context, open_result);
    }
}

static void indicate_close_complete(SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance)
{
    if (sasl_client_io_instance->on_io_close_complete != NULL)
    {
        sasl_client_io_instance->on_io_close_complete(sasl_client_io_instance->on_io_close_complete_context);
    }
}

static void on_underlying_io_close_complete(void* context)
{
    SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)context;

    switch (sasl_client_io_instance->io_state)
    {
    default:
        break;

    case IO_STATE_OPENING_UNDERLYING_IO:
    case IO_STATE_SASL_HANDSHAKE:
        sasl_client_io_instance->io_state = IO_STATE_NOT_OPEN;
        indicate_open_complete(sasl_client_io_instance, IO_OPEN_ERROR);
        break;

    case IO_STATE_CLOSING:
        sasl_client_io_instance->io_state = IO_STATE_NOT_OPEN;
        indicate_close_complete(sasl_client_io_instance);
        break;
    }
}

static void handle_error(SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance)
{
    switch (sasl_client_io_instance->io_state)
    {
    default:
    case IO_STATE_NOT_OPEN:
        break;

    case IO_STATE_OPENING_UNDERLYING_IO:
    case IO_STATE_SASL_HANDSHAKE:
        if (xio_close(sasl_client_io_instance->underlying_io, on_underlying_io_close_complete, sasl_client_io_instance) != 0)
        {
            sasl_client_io_instance->io_state = IO_STATE_NOT_OPEN;
            indicate_open_complete(sasl_client_io_instance, IO_OPEN_ERROR);
        }
        break;

    case IO_STATE_OPEN:
        sasl_client_io_instance->io_state = IO_STATE_ERROR;
        indicate_error(sasl_client_io_instance);
        break;
    }
}

static int send_sasl_header(SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance)
{
    int result;

    /* Codes_SRS_SASLCLIENTIO_01_078: [SASL client IO shall start the header exchange by sending the SASL header.] */
    /* Codes_SRS_SASLCLIENTIO_01_095: [Sending the header shall be done by using xio_send.] */
    if (xio_send(sasl_client_io_instance->underlying_io, sasl_header, sizeof(sasl_header), NULL, NULL) != 0)
    {
        result = __LINE__;
    }
    else
    {
        if (sasl_client_io_instance->is_trace_on == 1)
        {
            LOG(AZ_LOG_TRACE, LOG_LINE, "-> Header (AMQP 3.1.0.0)");
        }
        result = 0;
    }

    return result;
}

static void on_underlying_io_open_complete(void* context, IO_OPEN_RESULT open_result)
{
    SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)context;

    switch (sasl_client_io_instance->io_state)
    {
    default:
        break;

    case IO_STATE_OPENING_UNDERLYING_IO:
        if (open_result == IO_OPEN_OK)
        {
            sasl_client_io_instance->io_state = IO_STATE_SASL_HANDSHAKE;
            if (sasl_client_io_instance->sasl_header_exchange_state != SASL_HEADER_EXCHANGE_IDLE)
            {
                /* Codes_SRS_SASLCLIENTIO_01_116: [Any underlying IO state changes to state OPEN after the header exchange has been started shall trigger no action.] */
                handle_error(sasl_client_io_instance);
            }
            else
            {
                /* Codes_SRS_SASLCLIENTIO_01_105: [start header exchange] */
                /* Codes_SRS_SASLCLIENTIO_01_001: [To establish a SASL layer, each peer MUST start by sending a protocol header.] */
                if (send_sasl_header(sasl_client_io_instance) != 0)
                {
                    /* Codes_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
                    /* Codes_SRS_SASLCLIENTIO_01_077: [If sending the SASL header fails, the SASL client IO state shall be set to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                    handle_error(sasl_client_io_instance);
                }
                else
                {
                    sasl_client_io_instance->sasl_header_exchange_state = SASL_HEADER_EXCHANGE_HEADER_SENT;
                }
            }
        }
        else
        {
            handle_error(sasl_client_io_instance);
        }

        break;
    }
}

static void on_underlying_io_error(void* context)
{
    SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)context;

    switch (sasl_client_io_instance->io_state)
    {
    default:
        break;

    case IO_STATE_OPENING_UNDERLYING_IO:
    case IO_STATE_SASL_HANDSHAKE:
        sasl_client_io_instance->io_state = IO_STATE_NOT_OPEN;
        indicate_open_complete(sasl_client_io_instance, IO_OPEN_ERROR);
        break;

    case IO_STATE_OPEN:
        sasl_client_io_instance->io_state = IO_STATE_ERROR;
        indicate_error(sasl_client_io_instance);
        break;
    }
}

static const char* get_frame_type_as_string(AMQP_VALUE descriptor)
{
    const char* result;

    if (is_sasl_mechanisms_type_by_descriptor(descriptor))
    {
        result = "[SASL MECHANISMS]";
    }
    else if (is_sasl_init_type_by_descriptor(descriptor))
    {
        result = "[SASL INIT]";
    }
    else if (is_sasl_challenge_type_by_descriptor(descriptor))
    {
        result = "[SASL CHALLENGE]";
    }
    else if (is_sasl_response_type_by_descriptor(descriptor))
    {
        result = "[SASL RESPONSE]";
    }
    else if (is_sasl_outcome_type_by_descriptor(descriptor))
    {
        result = "[SASL OUTCOME]";
    }
    else
    {
        result = "[Unknown]";
    }

    return result;
}

static void log_incoming_frame(AMQP_VALUE performative)
{
#ifdef NO_LOGGING
    UNUSED(performative);
#else
    if (xlogging_get_log_function() != NULL)
    {
        AMQP_VALUE descriptor = amqpvalue_get_inplace_descriptor(performative);
        if (descriptor != NULL)
        {
            LOG(AZ_LOG_TRACE, 0, "<- ");
            LOG(AZ_LOG_TRACE, 0, (char*)get_frame_type_as_string(descriptor));
            char* performative_as_string = NULL;
            LOG(AZ_LOG_TRACE, LOG_LINE, (performative_as_string = amqpvalue_to_string(performative)));
            if (performative_as_string != NULL)
            {
                amqpalloc_free(performative_as_string);
            }
        }
    }
#endif
}

static void log_outgoing_frame(AMQP_VALUE performative)
{
#ifdef NO_LOGGING
    UNUSED(performative);
#else
    if (xlogging_get_log_function() != NULL)
    {
        AMQP_VALUE descriptor = amqpvalue_get_inplace_descriptor(performative);
        if (descriptor != NULL)
        {
            LOG(AZ_LOG_TRACE, 0, "-> ");
            LOG(AZ_LOG_TRACE, 0, (char*)get_frame_type_as_string(descriptor));
            char* performative_as_string = NULL;
            LOG(AZ_LOG_TRACE, LOG_LINE, (performative_as_string = amqpvalue_to_string(performative)));
            if (performative_as_string != NULL)
            {
                amqpalloc_free(performative_as_string);
            }
        }
    }
#endif
}

static int saslclientio_receive_byte(SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance, unsigned char b)
{
    int result;

    switch (sasl_client_io_instance->sasl_header_exchange_state)
    {
    default:
        result = __LINE__;
        break;

    case SASL_HEADER_EXCHANGE_HEADER_EXCH:
        switch (sasl_client_io_instance->sasl_client_negotiation_state)
        {
        case SASL_CLIENT_NEGOTIATION_ERROR:
            result = __LINE__;
            break;

        default:
            /* Codes_SRS_SASLCLIENTIO_01_068: [During the SASL frame exchange that constitutes the handshake the received bytes from the underlying IO shall be fed to the frame_codec instance created in saslclientio_create by calling frame_codec_receive_bytes.] */
            if (frame_codec_receive_bytes(sasl_client_io_instance->frame_codec, &b, 1) != 0)
            {
                /* Codes_SRS_SASLCLIENTIO_01_088: [If frame_codec_receive_bytes fails, the state of SASL client IO shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                result = __LINE__;
            }
            else
            {
                result = 0;
            }

            break;

        case SASL_CLIENT_NEGOTIATION_OUTCOME_RCVD:
            sasl_client_io_instance->on_bytes_received(sasl_client_io_instance->on_bytes_received_context, &b, 1);
            result = 0;
            break;
        }

        break;

    /* Codes_SRS_SASLCLIENTIO_01_003: [Other than using a protocol id of three, the exchange of SASL layer headers follows the same rules specified in the version negotiation section of the transport specification (See Part 2: section 2.2).] */
    case SASL_HEADER_EXCHANGE_IDLE:
    case SASL_HEADER_EXCHANGE_HEADER_SENT:
        if (b != sasl_header[sasl_client_io_instance->header_bytes_received])
        {
            result = __LINE__;
        }
        else
        {
            sasl_client_io_instance->header_bytes_received++;
            if (sasl_client_io_instance->header_bytes_received == sizeof(sasl_header))
            {
                if (sasl_client_io_instance->is_trace_on == 1)
                {
                    LOG(AZ_LOG_TRACE, LOG_LINE, "<- Header (AMQP 3.1.0.0)");
                }

                switch (sasl_client_io_instance->sasl_header_exchange_state)
                {
                default:
                    result = __LINE__;
                    break;
                
                case SASL_HEADER_EXCHANGE_HEADER_SENT:
                    /* from this point on we need to decode SASL frames */
                    sasl_client_io_instance->sasl_header_exchange_state = SASL_HEADER_EXCHANGE_HEADER_EXCH;
                    result = 0;
                    break;

                case SASL_HEADER_EXCHANGE_IDLE:
                    sasl_client_io_instance->sasl_header_exchange_state = SASL_HEADER_EXCHANGE_HEADER_RCVD;
                    if (send_sasl_header(sasl_client_io_instance) != 0)
                    {
                        /* Codes_SRS_SASLCLIENTIO_01_077: [If sending the SASL header fails, the SASL client IO state shall be set to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                        result = __LINE__;
                    }
                    else
                    {
                        result = 0;
                    }

                    break;
                }
            }
            else
            {
                result = 0;
            }
        }

        break;
    }

    return result;
}

static void on_underlying_io_bytes_received(void* context, const unsigned char* buffer, size_t size)
{
    SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)context;

    /* Codes_SRS_SASLCLIENTIO_01_028: [If buffer is NULL or size is zero, nothing should be indicated as received and the saslio state shall be switched to ERROR the on_state_changed callback shall be triggered.] */
    if ((buffer == NULL) ||
        (size == 0))
    {
        handle_error(sasl_client_io_instance);
    }
    else
    {
        switch (sasl_client_io_instance->io_state)
        {
        default:
            break;

        case IO_STATE_OPEN:
            /* Codes_SRS_SASLCLIENTIO_01_027: [When the on_bytes_received callback passed to the underlying IO is called and the SASL client IO state is IO_STATE_OPEN, the bytes shall be indicated to the user of SASL client IO by calling the on_bytes_received that was passed in saslclientio_open.] */
            /* Codes_SRS_SASLCLIENTIO_01_029: [The context argument shall be set to the callback_context passed in saslclientio_open.] */
            sasl_client_io_instance->on_bytes_received(sasl_client_io_instance->on_bytes_received_context, buffer, size);
            break;

        case IO_STATE_SASL_HANDSHAKE:
        {
            size_t i;

            for (i = 0; i < size; i++)
            {
                if (saslclientio_receive_byte(sasl_client_io_instance, buffer[i]) != 0)
                {
                    break;
                }
            }

            if (i < size)
            {
                /* Codes_SRS_SASLCLIENTIO_01_073: [If the handshake fails (i.e. the outcome is an error) the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.]  */
                handle_error(sasl_client_io_instance);
            }

            break;
        }

        case IO_STATE_ERROR:
            /* Codes_SRS_SASLCLIENTIO_01_031: [If bytes are received when the SASL client IO state is IO_STATE_ERROR, SASL client IO shall do nothing.]  */
            break;
        }
    }
}

static void on_bytes_encoded(void* context, const unsigned char* bytes, size_t length, bool encode_complete)
{
    (void)encode_complete;

    SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)context;

    /* Codes_SRS_SASLCLIENTIO_01_120: [When SASL client IO is notified by sasl_frame_codec of bytes that have been encoded via the on_bytes_encoded callback and SASL client IO is in the state OPENING, SASL client IO shall send these bytes by using xio_send.] */
    if (xio_send(sasl_client_io_instance->underlying_io, bytes, length, NULL, NULL) != 0)
    {
        /* Codes_SRS_SASLCLIENTIO_01_121: [If xio_send fails, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
        handle_error(sasl_client_io_instance);
    }
}

static int send_sasl_init(SASL_CLIENT_IO_INSTANCE* sasl_client_io, const char* sasl_mechanism_name)
{
    int result;

    SASL_INIT_HANDLE sasl_init;
    SASL_MECHANISM_BYTES init_bytes;

    /* Codes_SRS_SASLCLIENTIO_01_045: [The name of the SASL mechanism used for the SASL exchange.] */
    sasl_init = sasl_init_create(sasl_mechanism_name);
    if (sasl_init == NULL)
    {
        /* Codes_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
        result = __LINE__;
    }
    else
    {
        /* Codes_SRS_SASLCLIENTIO_01_048: [The contents of this data are defined by the SASL security mechanism.] */
        if (saslmechanism_get_init_bytes(sasl_client_io->sasl_mechanism, &init_bytes) != 0)
        {
            /* Codes_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
            result = __LINE__;
        }
        else
        {
            amqp_binary creds;
            creds.bytes = init_bytes.bytes;
            creds.length = init_bytes.length;
            if ((init_bytes.length > 0) &&
                /* Codes_SRS_SASLCLIENTIO_01_047: [A block of opaque data passed to the security mechanism.] */
                (sasl_init_set_initial_response(sasl_init, creds) != 0))
            {
                /* Codes_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                result = __LINE__;
            }
            else
            {
                AMQP_VALUE sasl_init_value = amqpvalue_create_sasl_init(sasl_init);
                if (sasl_init_value == NULL)
                {
                    /* Codes_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                    result = __LINE__;
                }
                else
                {
                    /* Codes_SRS_SASLCLIENTIO_01_070: [When a frame needs to be sent as part of the SASL handshake frame exchange, the send shall be done by calling sasl_frame_codec_encode_frame.] */
                    if (sasl_frame_codec_encode_frame(sasl_client_io->sasl_frame_codec, sasl_init_value, on_bytes_encoded, sasl_client_io) != 0)
                    {
                        /* Codes_SRS_SASLCLIENTIO_01_071: [If sasl_frame_codec_encode_frame fails, then the state of SASL client IO shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                        result = __LINE__;
                    }
                    else
                    {
                        if (sasl_client_io->is_trace_on == 1)
                        {
                            log_outgoing_frame(sasl_init_value);
                        }

                        result = 0;
                    }

                    amqpvalue_destroy(sasl_init_value);
                }
            }
        }

        sasl_init_destroy(sasl_init);
    }

    return result;
}

static int send_sasl_response(SASL_CLIENT_IO_INSTANCE* sasl_client_io, SASL_MECHANISM_BYTES sasl_response)
{
    int result;

    SASL_RESPONSE_HANDLE sasl_response_handle;
    amqp_binary response_binary_value;

    response_binary_value.bytes = sasl_response.bytes;
    response_binary_value.length = sasl_response.length;

    /* Codes_SRS_SASLCLIENTIO_01_055: [Send the SASL response data as defined by the SASL specification.] */
    /* Codes_SRS_SASLCLIENTIO_01_056: [A block of opaque data passed to the security mechanism.] */
    if ((sasl_response_handle = sasl_response_create(response_binary_value)) == NULL)
    {
        result = __LINE__;
    }
    else
    {
        AMQP_VALUE sasl_response_value = amqpvalue_create_sasl_response(sasl_response_handle);
        if (sasl_response_value == NULL)
        {
            result = __LINE__;
        }
        else
        {
            /* Codes_SRS_SASLCLIENTIO_01_070: [When a frame needs to be sent as part of the SASL handshake frame exchange, the send shall be done by calling sasl_frame_codec_encode_frame.] */
            if (sasl_frame_codec_encode_frame(sasl_client_io->sasl_frame_codec, sasl_response_value, on_bytes_encoded, sasl_client_io) != 0)
            {
                result = __LINE__;
            }
            else
            {
                if (sasl_client_io->is_trace_on == 1)
                {
                    log_outgoing_frame(sasl_response_value);
                }
                result = 0;
            }

            amqpvalue_destroy(sasl_response_value);
        }

        sasl_response_destroy(sasl_response_handle);
    }

    return result;
}

static void sasl_frame_received_callback(void* context, AMQP_VALUE sasl_frame)
{
    SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)context;

    /* Codes_SRS_SASLCLIENTIO_01_067: [The SASL frame exchange shall be started as soon as the SASL header handshake is done.] */
    switch (sasl_client_io_instance->io_state)
    {
    default:
        break;

    case IO_STATE_OPEN:
    case IO_STATE_OPENING_UNDERLYING_IO:
    case IO_STATE_CLOSING:
        /* Codes_SRS_SASLCLIENTIO_01_117: [If on_sasl_frame_received_callback is called when the state of the IO is OPEN then the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
        handle_error(sasl_client_io_instance);
        break;

    case IO_STATE_SASL_HANDSHAKE:
        if (sasl_client_io_instance->sasl_header_exchange_state != SASL_HEADER_EXCHANGE_HEADER_EXCH)
        {
            /* Codes_SRS_SASLCLIENTIO_01_118: [If on_sasl_frame_received_callback is called in the OPENING state but the header exchange has not yet been completed, then the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
            handle_error(sasl_client_io_instance);
        }
        else
        {
            AMQP_VALUE descriptor = amqpvalue_get_inplace_descriptor(sasl_frame);
            if (descriptor == NULL)
            {
                /* Codes_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                handle_error(sasl_client_io_instance);
            }
            else
            {
                if (sasl_client_io_instance->is_trace_on == 1)
                {
                    log_incoming_frame(sasl_frame);
                }

                /* Codes_SRS_SASLCLIENTIO_01_032: [The peer acting as the SASL server MUST announce supported authentication mechanisms using the sasl-mechanisms frame.] */
                /* Codes_SRS_SASLCLIENTIO_01_040: [The peer playing the role of the SASL client and the peer playing the role of the SASL server MUST correspond to the TCP client and server respectively.] */
                /* Codes_SRS_SASLCLIENTIO_01_034: [<-- SASL-MECHANISMS] */
                if (is_sasl_mechanisms_type_by_descriptor(descriptor))
                {
                    switch (sasl_client_io_instance->sasl_client_negotiation_state)
                    {
                    case SASL_CLIENT_NEGOTIATION_NOT_STARTED:
                    {
                        SASL_MECHANISMS_HANDLE sasl_mechanisms_handle;

                        if (amqpvalue_get_sasl_mechanisms(sasl_frame, &sasl_mechanisms_handle) != 0)
                        {
                            /* Codes_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                            handle_error(sasl_client_io_instance);
                        }
                        else
                        {
                            AMQP_VALUE sasl_server_mechanisms;
                            uint32_t mechanisms_count;

                            if ((sasl_mechanisms_get_sasl_server_mechanisms(sasl_mechanisms_handle, &sasl_server_mechanisms) != 0) ||
                                (amqpvalue_get_array_item_count(sasl_server_mechanisms, &mechanisms_count) != 0) ||
                                (mechanisms_count == 0))
                            {
                                /* Codes_SRS_SASLCLIENTIO_01_042: [It is invalid for this list to be null or empty.] */
                                handle_error(sasl_client_io_instance);
                            }
                            else
                            {
                                const char* sasl_mechanism_name = saslmechanism_get_mechanism_name(sasl_client_io_instance->sasl_mechanism);
                                if (sasl_mechanism_name == NULL)
                                {
                                    /* Codes_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                                    handle_error(sasl_client_io_instance);
                                }
                                else
                                {
                                    uint32_t i;

                                    for (i = 0; i < mechanisms_count; i++)
                                    {
                                        AMQP_VALUE sasl_server_mechanism;
                                        sasl_server_mechanism = amqpvalue_get_array_item(sasl_server_mechanisms, i);
                                        if (sasl_server_mechanism == NULL)
                                        {
                                            i = mechanisms_count;
                                        }
                                        else
                                        {
                                            const char* sasl_server_mechanism_name;
                                            if (amqpvalue_get_symbol(sasl_server_mechanism, &sasl_server_mechanism_name) != 0)
                                            {
                                                i = mechanisms_count;
                                            }
                                            else
                                            {
                                                if (strcmp(sasl_mechanism_name, sasl_server_mechanism_name) == 0)
                                                {
                                                    amqpvalue_destroy(sasl_server_mechanism);
                                                    break;
                                                }
                                            }

                                            amqpvalue_destroy(sasl_server_mechanism);
                                        }
                                    }

                                    if (i == mechanisms_count)
                                    {
                                        /* Codes_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                                        handle_error(sasl_client_io_instance);
                                    }
                                    else
                                    {
                                        sasl_client_io_instance->sasl_client_negotiation_state = SASL_CLIENT_NEGOTIATION_MECH_RCVD;

                                        /* Codes_SRS_SASLCLIENTIO_01_035: [SASL-INIT -->] */
                                        /* Codes_SRS_SASLCLIENTIO_01_033: [The partner MUST then choose one of the supported mechanisms and initiate a sasl exchange.] */
                                        /* Codes_SRS_SASLCLIENTIO_01_054: [Selects the sasl mechanism and provides the initial response if needed.] */
                                        if (send_sasl_init(sasl_client_io_instance, sasl_mechanism_name) != 0)
                                        {
                                            /* Codes_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                                            handle_error(sasl_client_io_instance);
                                        }
                                        else
                                        {
                                            sasl_client_io_instance->sasl_client_negotiation_state = SASL_CLIENT_NEGOTIATION_INIT_SENT;
                                        }
                                    }
                                }
                            }

                            sasl_mechanisms_destroy(sasl_mechanisms_handle);
                        }

                        break;
                    }
                    }
                }
                /* Codes_SRS_SASLCLIENTIO_01_052: [Send the SASL challenge data as defined by the SASL specification.] */
                /* Codes_SRS_SASLCLIENTIO_01_036: [<-- SASL-CHALLENGE *] */
                /* Codes_SRS_SASLCLIENTIO_01_039: [the SASL challenge/response step can occur zero or more times depending on the details of the SASL mechanism chosen.] */
                else if (is_sasl_challenge_type_by_descriptor(descriptor))
                {
                    /* Codes_SRS_SASLCLIENTIO_01_032: [The peer acting as the SASL server MUST announce supported authentication mechanisms using the sasl-mechanisms frame.] */
                    if ((sasl_client_io_instance->sasl_client_negotiation_state != SASL_CLIENT_NEGOTIATION_INIT_SENT) &&
                        (sasl_client_io_instance->sasl_client_negotiation_state != SASL_CLIENT_NEGOTIATION_RESPONSE_SENT))
                    {
                        handle_error(sasl_client_io_instance);
                    }
                    else
                    {
                        SASL_CHALLENGE_HANDLE sasl_challenge_handle;

                        if (amqpvalue_get_sasl_challenge(sasl_frame, &sasl_challenge_handle) != 0)
                        {
                            /* Codes_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                            handle_error(sasl_client_io_instance);
                        }
                        else
                        {
                            amqp_binary challenge_binary_value;
                            SASL_MECHANISM_BYTES response_bytes;

                            /* Codes_SRS_SASLCLIENTIO_01_053: [Challenge information, a block of opaque binary data passed to the security mechanism.] */
                            if (sasl_challenge_get_challenge(sasl_challenge_handle, &challenge_binary_value) != 0)
                            {
                                /* Codes_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                                handle_error(sasl_client_io_instance);
                            }
                            else
                            {
                                SASL_MECHANISM_BYTES challenge;

                                challenge.bytes = challenge_binary_value.bytes;
                                challenge.length = challenge_binary_value.length;

                                /* Codes_SRS_SASLCLIENTIO_01_057: [The contents of this data are defined by the SASL security mechanism.] */
                                /* Codes_SRS_SASLCLIENTIO_01_037: [SASL-RESPONSE -->] */
                                if ((saslmechanism_challenge(sasl_client_io_instance->sasl_mechanism, &challenge, &response_bytes) != 0) ||
                                    (send_sasl_response(sasl_client_io_instance, response_bytes) != 0))
                                {
                                    /* Codes_SRS_SASLCLIENTIO_01_119: [If any error is encountered when parsing the received frame, the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
                                    handle_error(sasl_client_io_instance);
                                }
                            }

                            sasl_challenge_destroy(sasl_challenge_handle);
                        }
                    }
                }
                /* Codes_SRS_SASLCLIENTIO_01_058: [This frame indicates the outcome of the SASL dialog.] */
                /* Codes_SRS_SASLCLIENTIO_01_038: [<-- SASL-OUTCOME] */
                else if (is_sasl_outcome_type_by_descriptor(descriptor))
                {
                    /* Codes_SRS_SASLCLIENTIO_01_032: [The peer acting as the SASL server MUST announce supported authentication mechanisms using the sasl-mechanisms frame.] */
                    if ((sasl_client_io_instance->sasl_client_negotiation_state != SASL_CLIENT_NEGOTIATION_INIT_SENT) &&
                        (sasl_client_io_instance->sasl_client_negotiation_state != SASL_CLIENT_NEGOTIATION_RESPONSE_SENT))
                    {
                        handle_error(sasl_client_io_instance);
                    }
                    else
                    {
                        SASL_OUTCOME_HANDLE sasl_outcome;

                        sasl_client_io_instance->sasl_client_negotiation_state = SASL_CLIENT_NEGOTIATION_OUTCOME_RCVD;

                        if (amqpvalue_get_sasl_outcome(sasl_frame, &sasl_outcome) != 0)
                        {
                            handle_error(sasl_client_io_instance);
                        }
                        else
                        {
                            sasl_code sasl_code;

                            /* Codes_SRS_SASLCLIENTIO_01_060: [A reply-code indicating the outcome of the SASL dialog.] */
                            if (sasl_outcome_get_code(sasl_outcome, &sasl_code) != 0)
                            {
                                handle_error(sasl_client_io_instance);
                            }
                            else
                            {
                                switch (sasl_code)
                                {
                                default:
                                case sasl_code_auth:
                                    /* Codes_SRS_SASLCLIENTIO_01_063: [1 Connection authentication failed due to an unspecified problem with the supplied credentials.] */
                                case sasl_code_sys:
                                    /* Codes_SRS_SASLCLIENTIO_01_064: [2 Connection authentication failed due to a system error.] */
                                case sasl_code_sys_perm:
                                    /* Codes_SRS_SASLCLIENTIO_01_065: [3 Connection authentication failed due to a system error that is unlikely to be corrected without intervention.] */
                                case sasl_code_sys_temp:
                                    /* Codes_SRS_SASLCLIENTIO_01_066: [4 Connection authentication failed due to a transient system error.] */
                                    handle_error(sasl_client_io_instance);
                                    break;

                                case sasl_code_ok:
                                    /* Codes_SRS_SASLCLIENTIO_01_059: [Upon successful completion of the SASL dialog the security layer has been established] */
                                    /* Codes_SRS_SASLCLIENTIO_01_062: [0 Connection authentication succeeded.]  */
                                    sasl_client_io_instance->io_state = IO_STATE_OPEN;
                                    indicate_open_complete(sasl_client_io_instance, IO_OPEN_OK);
                                    break;
                                }
                            }

                            sasl_outcome_destroy(sasl_outcome);
                        }
                    }
                }
                else
                {
                    LogError("Bad SASL frame");
                }
            }
        }
        break;
    }
}

static void on_frame_codec_error(void* context)
{
    SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)context;

    /* Codes_SRS_SASLCLIENTIO_01_122: [When on_frame_codec_error is called while in the OPENING or OPEN state the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
    /* Codes_SRS_SASLCLIENTIO_01_123: [When on_frame_codec_error is called in the ERROR state nothing shall be done.] */
    handle_error(sasl_client_io_instance);
}

static void on_sasl_frame_codec_error(void* context)
{
    SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)context;

    /* Codes_SRS_SASLCLIENTIO_01_124: [**When on_sasl_frame_codec_error is called while in the OPENING or OPEN state the SASL client IO state shall be switched to IO_STATE_ERROR and the on_state_changed callback shall be triggered.] */
    /* Codes_SRS_SASLCLIENTIO_01_125: [When on_sasl_frame_codec_error is called in the ERROR state nothing shall be done.] */
    handle_error(sasl_client_io_instance);
}

CONCRETE_IO_HANDLE saslclientio_create(void* io_create_parameters)
{
    SASLCLIENTIO_CONFIG* sasl_client_io_config = io_create_parameters;
    SASL_CLIENT_IO_INSTANCE* result;

    /* Codes_SRS_SASLCLIENTIO_01_005: [If xio_create_parameters is NULL, saslclientio_create shall fail and return NULL.] */
    if ((sasl_client_io_config == NULL) ||
        /* Codes_SRS_SASLCLIENTIO_01_092: [If any of the sasl_mechanism or underlying_io members of the configuration structure are NULL, saslclientio_create shall fail and return NULL.] */
        (sasl_client_io_config->underlying_io == NULL) ||
        (sasl_client_io_config->sasl_mechanism == NULL))
    {
        result = NULL;
    }
    else
    {
        result = amqpalloc_malloc(sizeof(SASL_CLIENT_IO_INSTANCE));
        /* Codes_SRS_SASLCLIENTIO_01_006: [If memory cannot be allocated for the new instance, saslclientio_create shall fail and return NULL.] */
        if (result != NULL)
        {
            result->underlying_io = sasl_client_io_config->underlying_io;
            if (result->underlying_io == NULL)
            {
                amqpalloc_free(result);
                result = NULL;
            }
            else
            {
                /* Codes_SRS_SASLCLIENTIO_01_089: [saslclientio_create shall create a frame_codec to be used for encoding/decoding frames bycalling frame_codec_create and passing the underlying_io as argument.] */
                result->frame_codec = frame_codec_create(on_frame_codec_error, result);
                if (result->frame_codec == NULL)
                {
                    /* Codes_SRS_SASLCLIENTIO_01_090: [If frame_codec_create fails, then saslclientio_create shall fail and return NULL.] */
                    amqpalloc_free(result);
                    result = NULL;
                }
                else
                {
                    /* Codes_SRS_SASLCLIENTIO_01_084: [saslclientio_create shall create a sasl_frame_codec to be used for SASL frame encoding/decoding by calling sasl_frame_codec_create and passing the just created frame_codec as argument.] */
                    result->sasl_frame_codec = sasl_frame_codec_create(result->frame_codec, sasl_frame_received_callback, on_sasl_frame_codec_error, result);
                    if (result->sasl_frame_codec == NULL)
                    {
                        frame_codec_destroy(result->frame_codec);
                        amqpalloc_free(result);
                        result = NULL;
                    }
                    else
                    {
                        /* Codes_SRS_SASLCLIENTIO_01_004: [saslclientio_create shall return on success a non-NULL handle to a new SASL client IO instance.] */
                        result->on_bytes_received = NULL;
                        result->on_io_open_complete = NULL;
                        result->on_io_error = NULL;
                        result->on_io_close_complete = NULL;
                        result->on_bytes_received_context = NULL;
                        result->on_io_open_complete_context = NULL;
                        result->on_io_close_complete_context = NULL;
                        result->on_io_error_context = NULL;
                        result->sasl_mechanism = sasl_client_io_config->sasl_mechanism;

                        result->io_state = IO_STATE_NOT_OPEN;
                    }
                }
            }
        }
    }

    return result;
}

void saslclientio_destroy(CONCRETE_IO_HANDLE sasl_client_io)
{
    if (sasl_client_io != NULL)
    {
        SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)sasl_client_io;

        /* Codes_SRS_SASLCLIENTIO_01_007: [saslclientio_destroy shall free all resources associated with the SASL client IO handle.] */
        /* Codes_SRS_SASLCLIENTIO_01_086: [saslclientio_destroy shall destroy the sasl_frame_codec created in saslclientio_create by calling sasl_frame_codec_destroy.] */
        sasl_frame_codec_destroy(sasl_client_io_instance->sasl_frame_codec);

        /* Codes_SRS_SASLCLIENTIO_01_091: [saslclientio_destroy shall destroy the frame_codec created in saslclientio_create by calling frame_codec_destroy.] */
        frame_codec_destroy(sasl_client_io_instance->frame_codec);
        amqpalloc_free(sasl_client_io);
    }
}

int saslclientio_open(CONCRETE_IO_HANDLE sasl_client_io, ON_IO_OPEN_COMPLETE on_io_open_complete, void* on_io_open_complete_context, ON_BYTES_RECEIVED on_bytes_received, void* on_bytes_received_context, ON_IO_ERROR on_io_error, void* on_io_error_context)
{
    int result = 0;

    /* Codes_SRS_SASLCLIENTIO_01_011: [If any of the sasl_client_io or on_bytes_received arguments is NULL, saslclientio_open shall fail and return a non-zero value.] */
    if ((sasl_client_io == NULL) ||
        (on_bytes_received == NULL))
    {
        result = __LINE__;
    }
    else
    {
        SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)sasl_client_io;

        if (sasl_client_io_instance->io_state != IO_STATE_NOT_OPEN)
        {
            result = __LINE__;
        }
        else
        {
            sasl_client_io_instance->on_bytes_received = on_bytes_received;
            sasl_client_io_instance->on_io_open_complete = on_io_open_complete;
            sasl_client_io_instance->on_io_error = on_io_error;
            sasl_client_io_instance->on_bytes_received_context = on_bytes_received_context;
            sasl_client_io_instance->on_io_open_complete_context = on_io_open_complete_context;
            sasl_client_io_instance->on_io_error_context = on_io_error_context;
            sasl_client_io_instance->sasl_header_exchange_state = SASL_HEADER_EXCHANGE_IDLE;
            sasl_client_io_instance->sasl_client_negotiation_state = SASL_CLIENT_NEGOTIATION_NOT_STARTED;
            sasl_client_io_instance->header_bytes_received = 0;
            sasl_client_io_instance->io_state = IO_STATE_OPENING_UNDERLYING_IO;
            sasl_client_io_instance->is_trace_on = 0;

            /* Codes_SRS_SASLCLIENTIO_01_009: [saslclientio_open shall call xio_open on the underlying_io passed to saslclientio_create.] */
            /* Codes_SRS_SASLCLIENTIO_01_013: [saslclientio_open shall pass to xio_open a callback for receiving bytes and a state changed callback for the underlying_io state changes.] */
            if (xio_open(sasl_client_io_instance->underlying_io, on_underlying_io_open_complete, sasl_client_io_instance, on_underlying_io_bytes_received, sasl_client_io_instance, on_underlying_io_error, sasl_client_io_instance) != 0)
            {
                /* Codes_SRS_SASLCLIENTIO_01_012: [If the open of the underlying_io fails, saslclientio_open shall fail and return non-zero value.] */
                result = __LINE__;
            }
            else
            {
                /* Codes_SRS_SASLCLIENTIO_01_010: [On success, saslclientio_open shall return 0.] */
                result = 0;
            }
        }
    }
    
    return result;
}

int saslclientio_close(CONCRETE_IO_HANDLE sasl_client_io, ON_IO_CLOSE_COMPLETE on_io_close_complete, void* on_io_close_complete_context)
{
    int result = 0;

    /* Codes_SRS_SASLCLIENTIO_01_017: [If sasl_client_io is NULL, saslclientio_close shall fail and return a non-zero value.] */
    if (sasl_client_io == NULL)
    {
        result = __LINE__;
    }
    else
    {
        SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)sasl_client_io;

        /* Codes_SRS_SASLCLIENTIO_01_098: [saslclientio_close shall only perform the close if the state is OPEN, OPENING or ERROR.] */
        if ((sasl_client_io_instance->io_state == IO_STATE_NOT_OPEN) ||
            (sasl_client_io_instance->io_state == IO_STATE_CLOSING))
        {
            result = __LINE__;
        }
        else
        {
            sasl_client_io_instance->io_state = IO_STATE_CLOSING;

            sasl_client_io_instance->on_io_close_complete = on_io_close_complete;
            sasl_client_io_instance->on_io_close_complete_context = on_io_close_complete_context;

            /* Codes_SRS_SASLCLIENTIO_01_015: [saslclientio_close shall close the underlying io handle passed in saslclientio_create by calling xio_close.] */
            if (xio_close(sasl_client_io_instance->underlying_io, on_underlying_io_close_complete, sasl_client_io_instance) != 0)
            {
                /* Codes_SRS_SASLCLIENTIO_01_018: [If xio_close fails, then saslclientio_close shall return a non-zero value.] */
                result = __LINE__;
            }
            else
            {
                /* Codes_SRS_SASLCLIENTIO_01_016: [On success, saslclientio_close shall return 0.] */
                result = 0;
            }
        }
    }

    return result;
}

int saslclientio_send(CONCRETE_IO_HANDLE sasl_client_io, const void* buffer, size_t size, ON_SEND_COMPLETE on_send_complete, void* callback_context)
{
    int result;

    /* Codes_SRS_SASLCLIENTIO_01_022: [If the saslio or buffer argument is NULL, saslclientio_send shall fail and return a non-zero value.] */
    if ((sasl_client_io == NULL) ||
        (buffer == NULL) ||
        /* Codes_SRS_SASLCLIENTIO_01_023: [If size is 0, saslclientio_send shall fail and return a non-zero value.] */
        (size == 0))
    {
        /* Invalid arguments */
        result = __LINE__;
    }
    else
    {
        SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)sasl_client_io;

        /* Codes_SRS_SASLCLIENTIO_01_019: [If saslclientio_send is called while the SASL client IO state is not IO_STATE_OPEN, saslclientio_send shall fail and return a non-zero value.] */
        if (sasl_client_io_instance->io_state != IO_STATE_OPEN)
        {
            result = __LINE__;
        }
        else
        {
            /* Codes_SRS_SASLCLIENTIO_01_020: [If the SASL client IO state is IO_STATE_OPEN, saslclientio_send shall call xio_send on the underlying_io passed to saslclientio_create, while passing as arguments the buffer, size, on_send_complete and callback_context.] */
            if (xio_send(sasl_client_io_instance->underlying_io, buffer, size, on_send_complete, callback_context) != 0)
            {
                /* Codes_SRS_SASLCLIENTIO_01_024: [If the call to xio_send fails, then saslclientio_send shall fail and return a non-zero value.] */
                result = __LINE__;
            }
            else
            {
                /* Codes_SRS_SASLCLIENTIO_01_021: [On success, saslclientio_send shall return 0.] */
                result = 0;
            }
        }
    }

    return result;
}

void saslclientio_dowork(CONCRETE_IO_HANDLE sasl_client_io)
{
    /* Codes_SRS_SASLCLIENTIO_01_026: [If the sasl_client_io argument is NULL, saslclientio_dowork shall do nothing.] */
    if (sasl_client_io != NULL)
    {
        SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)sasl_client_io;

        /* Codes_SRS_SASLCLIENTIO_01_025: [saslclientio_dowork shall call the xio_dowork on the underlying_io passed in saslclientio_create.] */
        if (sasl_client_io_instance->io_state != IO_STATE_NOT_OPEN)
        {
            /* Codes_SRS_SASLCLIENTIO_01_025: [saslclientio_dowork shall call the xio_dowork on the underlying_io passed in saslclientio_create.] */
            xio_dowork(sasl_client_io_instance->underlying_io);
        }
    }
}

/* Codes_SRS_SASLCLIENTIO_03_001: [saslclientio_setoption shall forward options to underlying io.]*/
int saslclientio_setoption(CONCRETE_IO_HANDLE sasl_client_io, const char* optionName, const void* value)
{
    int result;

    if (sasl_client_io == NULL)
    {
        result = __LINE__;
    }
    else
    {
        SASL_CLIENT_IO_INSTANCE* sasl_client_io_instance = (SASL_CLIENT_IO_INSTANCE*)sasl_client_io;

        if (sasl_client_io_instance->underlying_io == NULL)
        {
            result = __LINE__;
        }
        else if (strcmp("logtrace", optionName) == 0)
        {
            sasl_client_io_instance->is_trace_on = *((bool*)value) == true ? 1 : 0;
            result = 0;
        }
        else
        {
            result = xio_setoption(sasl_client_io_instance->underlying_io, optionName, value);
        }
    }
    return result;
}

/*this function will clone an option given by name and value*/
static void* saslclientio_CloneOption(const char* name, const void* value)
{
    (void)(name, value);
    return NULL;
}

/*this function destroys an option previously created*/
static void saslclientio_DestroyOption(const char* name, const void* value)
{
    (void)(name, value);
}

static OPTIONHANDLER_HANDLE saslclientio_retrieveoptions(CONCRETE_IO_HANDLE handle)
{
    OPTIONHANDLER_HANDLE result;
    (void)handle;
    result = OptionHandler_Create(saslclientio_CloneOption, saslclientio_DestroyOption, saslclientio_setoption);
    if (result == NULL)
    {
        LogError("unable to OptionHandler_Create");
        /*return as is*/
    }
    else
    {
        /*insert here work to add the options to "result" handle*/
    }
    return result;
}

static const IO_INTERFACE_DESCRIPTION sasl_client_io_interface_description =
{
    saslclientio_retrieveoptions,
    saslclientio_create,
    saslclientio_destroy,
    saslclientio_open,
    saslclientio_close,
    saslclientio_send,
    saslclientio_dowork,
    saslclientio_setoption
};

/* Codes_SRS_SASLCLIENTIO_01_087: [saslclientio_get_interface_description shall return a pointer to an IO_INTERFACE_DESCRIPTION structure that contains pointers to the functions: saslclientio_create, saslclientio_destroy, saslclientio_open, saslclientio_close, saslclientio_send and saslclientio_dowork.] */
const IO_INTERFACE_DESCRIPTION* saslclientio_get_interface_description(void)
{
    return &sasl_client_io_interface_description;
}
