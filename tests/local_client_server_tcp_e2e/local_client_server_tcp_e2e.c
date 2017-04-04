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
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/socketio.h"
#include "azure_uamqp_c/message_sender.h"
#include "azure_uamqp_c/message_receiver.h"
#include "azure_uamqp_c/socket_listener.h"
#include "azure_uamqp_c/header_detect_io.h"
#include "azure_uamqp_c/connection.h"
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/link.h"
#include "azure_uamqp_c/messaging.h"

static TEST_MUTEX_HANDLE g_testByTest;
static TEST_MUTEX_HANDLE g_dllByDll;

#define TEST_TIMEOUT 30 // seconds

static int g_port_number;

extern void set_port_number(const char* seed)
{
    if (seed == NULL)
    {
        g_port_number = 5672;
    }
    else
    {
        int hash = 0;

        while (*seed != '\0')
        {
            hash += (int)(*seed - '0');
            seed++;
        }

        g_port_number = 5672 + hash;
    }

    LogInfo("Server port number set to %d", g_port_number);
}


BEGIN_TEST_SUITE(local_client_server_tcp_e2e)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;

    TEST_INITIALIZE_MEMORY_DEBUG(g_dllByDll);
    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    result = platform_init();
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "platform_init failed");

    srand((unsigned int)time(NULL));
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    platform_deinit();

    TEST_MUTEX_DESTROY(g_testByTest);
    TEST_DEINITIALIZE_MEMORY_DEBUG(g_dllByDll);
}

typedef struct SERVER_INSTANCE_TAG
{
    CONNECTION_HANDLE connection;
    SESSION_HANDLE session;
    LINK_HANDLE link;
    MESSAGE_RECEIVER_HANDLE message_receiver;
    size_t received_messages;
    XIO_HANDLE header_detect_io;
} SERVER_INSTANCE;

static void on_message_send_complete(void* context, MESSAGE_SEND_RESULT send_result)
{
    size_t* sent_messages = (size_t*)context;
    if (send_result == MESSAGE_SEND_OK)
    {
        (*sent_messages)++;
    }
    else
    {
        ASSERT_FAIL("Message send failed");
    }
}

static void on_message_receiver_state_changed(const void* context, MESSAGE_RECEIVER_STATE new_state, MESSAGE_RECEIVER_STATE previous_state)
{
    (void)context;
    (void)new_state;
    (void)previous_state;
}

static AMQP_VALUE on_message_received(const void* context, MESSAGE_HANDLE message)
{
    BINARY_DATA binary_data;
    int result;
    SERVER_INSTANCE* server;
    const unsigned char expected_payload[] = { 'H', 'e', 'l', 'l', 'o' };

    (void)message;

    server = (SERVER_INSTANCE*)context;
    server->received_messages++;

    result = message_get_body_amqp_data(message, 0, &binary_data);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "message receiver open failed");

    ASSERT_ARE_EQUAL_WITH_MSG(size_t, sizeof(expected_payload), binary_data.length, "received message length mismatch");
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, memcmp(expected_payload, binary_data.bytes, sizeof(expected_payload)), "received message payload mismatch");

    return messaging_delivery_accepted();
}

static bool on_new_link_attached(void* context, LINK_ENDPOINT_HANDLE new_link_endpoint, const char* name, role role, AMQP_VALUE source, AMQP_VALUE target)
{
    SERVER_INSTANCE* server = (SERVER_INSTANCE*)context;
    int result;

    server->link = link_create_from_endpoint(server->session, new_link_endpoint, name, role, source, target);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->link, "Could not create link");
    server->message_receiver = messagereceiver_create(server->link, on_message_receiver_state_changed, server);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->message_receiver, "Could not create message receiver");
    result = messagereceiver_open(server->message_receiver, on_message_received, server);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "message receiver open failed");

    return true;
}

static bool on_new_session_endpoint(void* context, ENDPOINT_HANDLE new_endpoint)
{
    SERVER_INSTANCE* server = (SERVER_INSTANCE*)context;
    int result;

    server->session = session_create_from_endpoint(server->connection, new_endpoint, on_new_link_attached, server);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->session, "Could not create server session");
    result = session_begin(server->session);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot begin server session");

    return true;
}

static void on_socket_accepted(void* context, XIO_HANDLE io)
{
    HEADERDETECTIO_CONFIG header_detect_io_config;
    SERVER_INSTANCE* server = (SERVER_INSTANCE*)context;
    int result;

    header_detect_io_config.underlying_io = io;
    server->header_detect_io = xio_create(headerdetectio_get_interface_description(), &header_detect_io_config);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->header_detect_io, "Could not create header detect IO");
    server->connection = connection_create(server->header_detect_io, NULL, "1", on_new_session_endpoint, server);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->connection, "Could not create server connection");
    (void)connection_set_trace(server->connection, true);
    result = connection_listen(server->connection);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot start listening");
}

TEST_FUNCTION(client_and_server_connect_and_send_one_message_settled)
{
    // arrange
    SERVER_INSTANCE server_instance;
    SOCKET_LISTENER_HANDLE socket_listener = socketlistener_create(g_port_number);
    int result;
    XIO_HANDLE socket_io;
    CONNECTION_HANDLE client_connection;
    SESSION_HANDLE client_session;
    LINK_HANDLE client_link;
    MESSAGE_HANDLE client_send_message;
    MESSAGE_SENDER_HANDLE client_message_sender;
    size_t sent_messages;
    AMQP_VALUE source;
    AMQP_VALUE target;
    time_t now_time;
    time_t start_time;

    server_instance.connection = NULL;
    server_instance.session = NULL;
    server_instance.link = NULL;
    server_instance.message_receiver = NULL;
    server_instance.received_messages = 0;

    sent_messages = 0;

    result = socketlistener_start(socket_listener, on_socket_accepted, &server_instance);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "socketlistener_start failed");

    // start the client
    SOCKETIO_CONFIG socketio_config = { "localhost", 0, NULL };
    socketio_config.port = g_port_number;
    socket_io = xio_create(socketio_get_interface_description(), &socketio_config);
    ASSERT_IS_NOT_NULL_WITH_MSG(socket_io, "Could not create socket IO");

    /* create the connection, session and link */
    client_connection = connection_create(socket_io, "localhost", "some", NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_connection, "Could not create client connection");

    (void)connection_set_trace(client_connection, true);
    client_session = session_create(client_connection, NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_session, "Could not create client session");

    source = messaging_create_source("ingress");
    ASSERT_IS_NOT_NULL_WITH_MSG(source, "Could not create source");
    target = messaging_create_target("localhost/ingress");
    ASSERT_IS_NOT_NULL_WITH_MSG(target, "Could not create target");
    client_link = link_create(client_session, "sender-link", role_sender, source, target);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_link, "Could not create client link");
    result = link_set_snd_settle_mode(client_link, sender_settle_mode_settled);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot set sender settle mode");

    amqpvalue_destroy(source);
    amqpvalue_destroy(target);

    client_send_message = message_create();
    ASSERT_IS_NOT_NULL_WITH_MSG(client_send_message, "Could not create message");
    unsigned char hello[] = { 'H', 'e', 'l', 'l', 'o' };
    BINARY_DATA binary_data;
    binary_data.bytes = hello;
    binary_data.length = sizeof(hello);
    result = message_add_body_amqp_data(client_send_message, binary_data);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot set message body");

    /* create a message sender */
    client_message_sender = messagesender_create(client_link, NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_message_sender, "Could not create message sender");
    result = messagesender_open(client_message_sender);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot open message sender");
    result = messagesender_send(client_message_sender, client_send_message, on_message_send_complete, &sent_messages);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot send message");
    message_destroy(client_send_message);

    start_time = time(NULL);
    while ((now_time = time(NULL)),
        (difftime(now_time, start_time) < TEST_TIMEOUT))
    {
        // schedule work for all components
        socketlistener_dowork(socket_listener);
        connection_dowork(client_connection);
        connection_dowork(server_instance.connection);

        // if we received the message, break
        if (server_instance.received_messages >= 1)
        {
            break;
        }

        ThreadAPI_Sleep(1);
    }

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(size_t, 1, sent_messages, "Bad sent messages count");
    ASSERT_ARE_EQUAL_WITH_MSG(size_t, 1, server_instance.received_messages, "Bad received messages count");

    // cleanup
    socketlistener_stop(socket_listener);
    messagesender_destroy(client_message_sender);
    link_destroy(client_link);
    session_destroy(client_session);
    connection_destroy(client_connection);
    xio_destroy(socket_io);

    messagereceiver_destroy(server_instance.message_receiver);
    link_destroy(server_instance.link);
    session_destroy(server_instance.session);
    connection_destroy(server_instance.connection);
    xio_destroy(server_instance.header_detect_io);
    socketlistener_destroy(socket_listener);
}

TEST_FUNCTION(client_and_server_connect_and_send_one_message_unsettled)
{
    // arrange
    SERVER_INSTANCE server_instance;
    SOCKET_LISTENER_HANDLE socket_listener = socketlistener_create(g_port_number);
    int result;
    XIO_HANDLE socket_io;
    CONNECTION_HANDLE client_connection;
    SESSION_HANDLE client_session;
    LINK_HANDLE client_link;
    MESSAGE_HANDLE client_send_message;
    MESSAGE_SENDER_HANDLE client_message_sender;
    size_t sent_messages;
    AMQP_VALUE source;
    AMQP_VALUE target;
    time_t now_time;
    time_t start_time;

    server_instance.connection = NULL;
    server_instance.session = NULL;
    server_instance.link = NULL;
    server_instance.message_receiver = NULL;
    server_instance.received_messages = 0;

    sent_messages = 0;

    result = socketlistener_start(socket_listener, on_socket_accepted, &server_instance);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "socketlistener_start failed");

    // start the client
    SOCKETIO_CONFIG socketio_config = { "localhost", 0, NULL };
    socketio_config.port = g_port_number;
    socket_io = xio_create(socketio_get_interface_description(), &socketio_config);
    ASSERT_IS_NOT_NULL_WITH_MSG(socket_io, "Could not create socket IO");

    /* create the connection, session and link */
    client_connection = connection_create(socket_io, "localhost", "some", NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_connection, "Could not create client connection");

    (void)connection_set_trace(client_connection, true);
    client_session = session_create(client_connection, NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_session, "Could not create client session");

    source = messaging_create_source("ingress");
    ASSERT_IS_NOT_NULL_WITH_MSG(source, "Could not create source");
    target = messaging_create_target("localhost/ingress");
    ASSERT_IS_NOT_NULL_WITH_MSG(target, "Could not create target");
    client_link = link_create(client_session, "sender-link", role_sender, source, target);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_link, "Could not create client link");
    result = link_set_snd_settle_mode(client_link, sender_settle_mode_unsettled);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot set sender settle mode");

    amqpvalue_destroy(source);
    amqpvalue_destroy(target);

    client_send_message = message_create();
    ASSERT_IS_NOT_NULL_WITH_MSG(client_send_message, "Could not create message");
    unsigned char hello[] = { 'H', 'e', 'l', 'l', 'o' };
    BINARY_DATA binary_data;
    binary_data.bytes = hello;
    binary_data.length = sizeof(hello);
    result = message_add_body_amqp_data(client_send_message, binary_data);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot set message body");

    /* create a message sender */
    client_message_sender = messagesender_create(client_link, NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_message_sender, "Could not create message sender");
    result = messagesender_open(client_message_sender);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot open message sender");
    result = messagesender_send(client_message_sender, client_send_message, on_message_send_complete, &sent_messages);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot send message");
    message_destroy(client_send_message);

    start_time = time(NULL);
    while ((now_time = time(NULL)),
        (difftime(now_time, start_time) < TEST_TIMEOUT))
    {
        // schedule work for all components
        socketlistener_dowork(socket_listener);
        connection_dowork(client_connection);
        connection_dowork(server_instance.connection);

        // if we received the message, break
        if ((server_instance.received_messages >= 1) &&
            (sent_messages >= 1))
        {
            break;
        }

        ThreadAPI_Sleep(1);
    }

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(size_t, 1, sent_messages, "Bad sent messages count");
    ASSERT_ARE_EQUAL_WITH_MSG(size_t, 1, server_instance.received_messages, "Bad received messages count");

    // cleanup
    socketlistener_stop(socket_listener);
    messagesender_destroy(client_message_sender);
    link_destroy(client_link);
    session_destroy(client_session);
    connection_destroy(client_connection);
    xio_destroy(socket_io);

    messagereceiver_destroy(server_instance.message_receiver);
    link_destroy(server_instance.link);
    session_destroy(server_instance.session);
    connection_destroy(server_instance.connection);
    xio_destroy(server_instance.header_detect_io);
    socketlistener_destroy(socket_listener);
}

END_TEST_SUITE(local_client_server_tcp_e2e)
