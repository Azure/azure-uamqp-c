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
#include "azure_uamqp_c/uamqp.h"

static TEST_MUTEX_HANDLE g_testByTest;
static TEST_MUTEX_HANDLE g_dllByDll;

static const char* test_redirect_hostname = "blahblah";
static const char* test_redirect_network_host = "1.2.3.4";
static const char* test_redirect_address = "blahblah/hagauaga";
static uint16_t test_redirect_port = 4242;

#define TEST_TIMEOUT 30 // seconds

static int generate_port_number(void)
{
    int port_number;

    port_number = 5672 + (int)(5000 * (double)rand() / RAND_MAX); // pseudo random number.

    LogInfo("Generated port number: %d", port_number);

    return port_number;
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

    srand((unsigned int)clock());
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
    size_t link_count;
    LINK_HANDLE links[2];
    MESSAGE_RECEIVER_HANDLE message_receivers[2];
    size_t received_messages;
    XIO_HANDLE header_detect_io;
    XIO_HANDLE underlying_io;
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

static void on_message_send_cancelled(void* context, MESSAGE_SEND_RESULT send_result)
{
    size_t* cancelled_messages = (size_t*)context;
    if (send_result == MESSAGE_SEND_CANCELLED)
    {
        (*cancelled_messages)++;
    }
    else
    {
        ASSERT_FAIL("Unexpected message send result");
    }
}

static void on_message_receivers_state_changed(const void* context, MESSAGE_RECEIVER_STATE new_state, MESSAGE_RECEIVER_STATE previous_state)
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

    result = message_get_body_amqp_data_in_place(message, 0, &binary_data);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "message receiver open failed");

    ASSERT_ARE_EQUAL_WITH_MSG(size_t, sizeof(expected_payload), binary_data.length, "received message length mismatch");
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, memcmp(expected_payload, binary_data.bytes, sizeof(expected_payload)), "received message payload mismatch");

    return messaging_delivery_accepted();
}

static bool on_new_link_attached(void* context, LINK_ENDPOINT_HANDLE new_link_endpoint, const char* name, role role, AMQP_VALUE source, AMQP_VALUE target)
{
    SERVER_INSTANCE* server = (SERVER_INSTANCE*)context;
    int result;

    server->links[server->link_count] = link_create_from_endpoint(server->session, new_link_endpoint, name, role, source, target);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->links[server->link_count], "Could not create link");
    server->message_receivers[server->link_count] = messagereceiver_create(server->links[server->link_count], on_message_receivers_state_changed, server);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->message_receivers[server->link_count], "Could not create message receiver");
    result = messagereceiver_open(server->message_receivers[server->link_count], on_message_received, server);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "message receiver open failed");
    server->link_count++;

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

static void on_socket_accepted(void* context, const IO_INTERFACE_DESCRIPTION* interface_description, void* io_parameters)
{
    HEADER_DETECT_IO_CONFIG header_detect_io_config;
    HEADER_DETECT_ENTRY header_detect_entries[1];
    SERVER_INSTANCE* server = (SERVER_INSTANCE*)context;
    int result;
    AMQP_HEADER amqp_header;

    server->underlying_io = xio_create(interface_description, io_parameters);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->underlying_io, "Could not create underlying IO");

    amqp_header = header_detect_io_get_amqp_header();
    header_detect_entries[0].header.header_bytes = amqp_header.header_bytes;
    header_detect_entries[0].header.header_size = amqp_header.header_size;
    header_detect_entries[0].io_interface_description = NULL;

    header_detect_io_config.underlying_io = server->underlying_io;
    header_detect_io_config.header_detect_entry_count = 1;
    header_detect_io_config.header_detect_entries = header_detect_entries;

    server->header_detect_io = xio_create(header_detect_io_get_interface_description(), &header_detect_io_config);
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
    int port_number = generate_port_number();
    SERVER_INSTANCE server_instance;
    SOCKET_LISTENER_HANDLE socket_listener = socketlistener_create(port_number);
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
    SOCKETIO_CONFIG socketio_config = { "localhost", 0, NULL };
    unsigned char hello[] = { 'H', 'e', 'l', 'l', 'o' };
    BINARY_DATA binary_data;
    ASYNC_OPERATION_HANDLE send_async_operation;

    server_instance.connection = NULL;
    server_instance.session = NULL;
    server_instance.link_count = 0;
    server_instance.links[0] = NULL;
    server_instance.message_receivers[0] = NULL;
    server_instance.received_messages = 0;

    sent_messages = 0;

    result = socketlistener_start(socket_listener, on_socket_accepted, &server_instance);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "socketlistener_start failed");

    // start the client
    socketio_config.port = port_number;
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
    binary_data.bytes = hello;
    binary_data.length = sizeof(hello);
    result = message_add_body_amqp_data(client_send_message, binary_data);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot set message body");

    /* create a message sender */
    client_message_sender = messagesender_create(client_link, NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_message_sender, "Could not create message sender");
    result = messagesender_open(client_message_sender);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot open message sender");
    send_async_operation = messagesender_send_async(client_message_sender, client_send_message, on_message_send_complete, &sent_messages, 0);
    ASSERT_IS_NOT_NULL_WITH_MSG(send_async_operation, "cannot send message");
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

    messagereceiver_destroy(server_instance.message_receivers[0]);
    link_destroy(server_instance.links[0]);
    session_destroy(server_instance.session);
    connection_destroy(server_instance.connection);
    xio_destroy(server_instance.header_detect_io);
    xio_destroy(server_instance.underlying_io);
    socketlistener_destroy(socket_listener);
}

TEST_FUNCTION(client_and_server_connect_and_send_one_message_unsettled)
{
    // arrange
    int port_number = generate_port_number();
    SERVER_INSTANCE server_instance;
    SOCKET_LISTENER_HANDLE socket_listener = socketlistener_create(port_number);
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
    SOCKETIO_CONFIG socketio_config = { "localhost", 0, NULL };
    unsigned char hello[] = { 'H', 'e', 'l', 'l', 'o' };
    BINARY_DATA binary_data;
    ASYNC_OPERATION_HANDLE send_async_operation;

    server_instance.connection = NULL;
    server_instance.session = NULL;
    server_instance.link_count = 0;
    server_instance.links[0] = NULL;
    server_instance.message_receivers[0] = NULL;
    server_instance.received_messages = 0;

    sent_messages = 0;

    result = socketlistener_start(socket_listener, on_socket_accepted, &server_instance);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "socketlistener_start failed");

    // start the client
    socketio_config.port = port_number;
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
    binary_data.bytes = hello;
    binary_data.length = sizeof(hello);
    result = message_add_body_amqp_data(client_send_message, binary_data);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot set message body");

    /* create a message sender */
    client_message_sender = messagesender_create(client_link, NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_message_sender, "Could not create message sender");
    result = messagesender_open(client_message_sender);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot open message sender");
    send_async_operation = messagesender_send_async(client_message_sender, client_send_message, on_message_send_complete, &sent_messages, 0);
    ASSERT_IS_NOT_NULL_WITH_MSG(send_async_operation, "cannot send message");
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

    messagereceiver_destroy(server_instance.message_receivers[0]);
    link_destroy(server_instance.links[0]);
    session_destroy(server_instance.session);
    connection_destroy(server_instance.connection);
    xio_destroy(server_instance.header_detect_io);
    xio_destroy(server_instance.underlying_io);
    socketlistener_destroy(socket_listener);
}

TEST_FUNCTION(cancelling_a_send_works)
{
    // arrange
    int port_number = generate_port_number();
    SERVER_INSTANCE server_instance;
    SOCKET_LISTENER_HANDLE socket_listener = socketlistener_create(port_number);
    int result;
    XIO_HANDLE socket_io;
    CONNECTION_HANDLE client_connection;
    SESSION_HANDLE client_session;
    LINK_HANDLE client_link;
    MESSAGE_HANDLE client_send_message;
    MESSAGE_SENDER_HANDLE client_message_sender;
    size_t cancelled_messages;
    AMQP_VALUE source;
    AMQP_VALUE target;
    time_t now_time;
    time_t start_time;
    SOCKETIO_CONFIG socketio_config = { "localhost", 0, NULL };
    unsigned char hello[] = { 'H', 'e', 'l', 'l', 'o' };
    BINARY_DATA binary_data;
    ASYNC_OPERATION_HANDLE send_async_operation;

    server_instance.connection = NULL;
    server_instance.session = NULL;
    server_instance.link_count = 0;
    server_instance.links[0] = NULL;
    server_instance.message_receivers[0] = NULL;
    server_instance.received_messages = 0;

    cancelled_messages = 0;

    result = socketlistener_start(socket_listener, on_socket_accepted, &server_instance);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "socketlistener_start failed");

    // start the client
    socketio_config.port = port_number;
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
    binary_data.bytes = hello;
    binary_data.length = sizeof(hello);
    result = message_add_body_amqp_data(client_send_message, binary_data);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot set message body");

    /* create a message sender */
    client_message_sender = messagesender_create(client_link, NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_message_sender, "Could not create message sender");
    result = messagesender_open(client_message_sender);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot open message sender");
    send_async_operation = messagesender_send_async(client_message_sender, client_send_message, on_message_send_cancelled, &cancelled_messages, 0);
    ASSERT_IS_NOT_NULL_WITH_MSG(send_async_operation, "cannot send message");
    result = async_operation_cancel(send_async_operation);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "async operation cancel failed");
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
        if (cancelled_messages == 1)
        {
            break;
        }

        ThreadAPI_Sleep(1);
    }

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(size_t, 1, cancelled_messages, "Bad cancelled messages count");
    ASSERT_ARE_EQUAL_WITH_MSG(size_t, 0, server_instance.received_messages, "Bad received messages count");

    // cleanup
    socketlistener_stop(socket_listener);
    messagesender_destroy(client_message_sender);
    link_destroy(client_link);
    session_destroy(client_session);
    connection_destroy(client_connection);
    xio_destroy(socket_io);

    messagereceiver_destroy(server_instance.message_receivers[0]);
    link_destroy(server_instance.links[0]);
    session_destroy(server_instance.session);
    connection_destroy(server_instance.connection);
    xio_destroy(server_instance.header_detect_io);
    xio_destroy(server_instance.underlying_io);
    socketlistener_destroy(socket_listener);
}

TEST_FUNCTION(destroying_one_out_of_2_senders_works)
{
    // arrange
    int port_number = generate_port_number();
    SERVER_INSTANCE server_instance;
    SOCKET_LISTENER_HANDLE socket_listener = socketlistener_create(port_number);
    int result;
    XIO_HANDLE socket_io;
    CONNECTION_HANDLE client_connection;
    SESSION_HANDLE client_session;
    LINK_HANDLE client_link_1;
    LINK_HANDLE client_link_2;
    MESSAGE_HANDLE client_send_message;
    MESSAGE_SENDER_HANDLE client_message_sender_1;
    MESSAGE_SENDER_HANDLE client_message_sender_2;
    size_t sent_messages;
    AMQP_VALUE source;
    AMQP_VALUE target;
    time_t now_time;
    time_t start_time;
    SOCKETIO_CONFIG socketio_config = { "localhost", 0, NULL };
    unsigned char hello[] = { 'H', 'e', 'l', 'l', 'o' };
    BINARY_DATA binary_data;
    ASYNC_OPERATION_HANDLE send_async_operation;

    server_instance.connection = NULL;
    server_instance.session = NULL;
    server_instance.link_count = 0;
    server_instance.links[0] = NULL;
    server_instance.links[1] = NULL;
    server_instance.message_receivers[0] = NULL;
    server_instance.message_receivers[1] = NULL;
    server_instance.received_messages = 0;

    sent_messages = 0;

    result = socketlistener_start(socket_listener, on_socket_accepted, &server_instance);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "socketlistener_start failed");

    // start the client
    socketio_config.port = port_number;
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

    // 1st sender link
    client_link_1 = link_create(client_session, "sender-link-1", role_sender, source, target);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_link_1, "Could not create client link 1");
    result = link_set_snd_settle_mode(client_link_1, sender_settle_mode_unsettled);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot set sender settle mode on link 1");

    // 2ndt sender link
    client_link_2 = link_create(client_session, "sender-link-2", role_sender, source, target);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_link_2, "Could not create client link 2");
    result = link_set_snd_settle_mode(client_link_2, sender_settle_mode_unsettled);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot set sender settle mode on link 2");

    amqpvalue_destroy(source);
    amqpvalue_destroy(target);

    client_send_message = message_create();
    ASSERT_IS_NOT_NULL_WITH_MSG(client_send_message, "Could not create message");
    binary_data.bytes = hello;
    binary_data.length = sizeof(hello);
    result = message_add_body_amqp_data(client_send_message, binary_data);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot set message body");

    /* create the 1st message sender */
    client_message_sender_1 = messagesender_create(client_link_1, NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_message_sender_1, "Could not create message sender 1");
    result = messagesender_open(client_message_sender_1);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot open message sender 1");

    /* create the 2nd message sender */
    client_message_sender_2 = messagesender_create(client_link_2, NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_message_sender_2, "Could not create message sender 2");
    result = messagesender_open(client_message_sender_2);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot open message sender 2");

    // send message
    send_async_operation = messagesender_send_async(client_message_sender_1, client_send_message, on_message_send_complete, &sent_messages, 0);
    ASSERT_IS_NOT_NULL_WITH_MSG(send_async_operation, "cannot send message");

    // wait for either time elapsed or message received
    start_time = time(NULL);
    while ((now_time = time(NULL)),
        (difftime(now_time, start_time) < TEST_TIMEOUT))
    {
        // schedule work for all components
        socketlistener_dowork(socket_listener);
        connection_dowork(client_connection);
        connection_dowork(server_instance.connection);

        // if we received the message, break
        if (sent_messages == 1)
        {
            break;
        }

        ThreadAPI_Sleep(1);
    }

    ASSERT_ARE_EQUAL_WITH_MSG(size_t, 1, sent_messages, "Could not send one message");

    // detach link
    messagesender_destroy(client_message_sender_2);
    link_destroy(client_link_2);

    // send 2nd message
    send_async_operation = messagesender_send_async(client_message_sender_1, client_send_message, on_message_send_complete, &sent_messages, 0);
    ASSERT_IS_NOT_NULL_WITH_MSG(send_async_operation, "cannot send message");
    message_destroy(client_send_message);

    // wait for 
    start_time = time(NULL);
    while ((now_time = time(NULL)),
        (difftime(now_time, start_time) < TEST_TIMEOUT))
    {
        // schedule work for all components
        socketlistener_dowork(socket_listener);
        connection_dowork(client_connection);
        connection_dowork(server_instance.connection);

        // if we received the message, break
        if (sent_messages == 2)
        {
            break;
        }

        ThreadAPI_Sleep(1);
    }

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(size_t, 2, sent_messages, "Bad sent messages count");
    ASSERT_ARE_EQUAL_WITH_MSG(size_t, 2, server_instance.received_messages, "Bad received messages count");

    // cleanup
    socketlistener_stop(socket_listener);
    messagesender_destroy(client_message_sender_1);
    link_destroy(client_link_1);
    session_destroy(client_session);
    connection_destroy(client_connection);
    xio_destroy(socket_io);

    messagereceiver_destroy(server_instance.message_receivers[0]);
    messagereceiver_destroy(server_instance.message_receivers[1]);
    link_destroy(server_instance.links[0]);
    link_destroy(server_instance.links[1]);
    session_destroy(server_instance.session);
    connection_destroy(server_instance.connection);
    xio_destroy(server_instance.header_detect_io);
    xio_destroy(server_instance.underlying_io);
    socketlistener_destroy(socket_listener);
}

static void on_connection_redirect_received(void* context, ERROR_HANDLE error)
{
    bool* redirect_received = (bool*)context;
    fields info = NULL;
    AMQP_VALUE hostname_key = amqpvalue_create_string("hostname");
    AMQP_VALUE network_host_key = amqpvalue_create_string("network-host");
    AMQP_VALUE port_key = amqpvalue_create_string("port");
    AMQP_VALUE hostname_value;
    AMQP_VALUE network_host_value;
    AMQP_VALUE port_value;
    const char* condition_string;
    const char* hostname_string;
    const char* network_host_string;
    uint16_t port_number;

    ASSERT_IS_NOT_NULL_WITH_MSG(error, "NULL error information");
    (void)error_get_condition(error, &condition_string);
    ASSERT_ARE_EQUAL(char_ptr, "amqp:connection:redirect", condition_string);
    (void)error_get_info(error, &info);
    ASSERT_IS_NOT_NULL_WITH_MSG(info, "NULL info in error");

    hostname_value = amqpvalue_get_map_value(info, hostname_key);
    ASSERT_IS_NOT_NULL_WITH_MSG(hostname_value, "NULL hostname_value");
    network_host_value = amqpvalue_get_map_value(info, network_host_key);
    ASSERT_IS_NOT_NULL_WITH_MSG(network_host_value, "NULL network_host_value");
    port_value = amqpvalue_get_map_value(info, port_key);
    ASSERT_IS_NOT_NULL_WITH_MSG(port_value, "NULL port_value");

    (void)amqpvalue_get_string(hostname_value, &hostname_string);
    ASSERT_ARE_EQUAL(char_ptr, test_redirect_hostname, hostname_string);
    (void)amqpvalue_get_string(network_host_value, &network_host_string);
    ASSERT_ARE_EQUAL(char_ptr, test_redirect_network_host, network_host_string);
    (void)amqpvalue_get_ushort(port_value, &port_number);
    ASSERT_ARE_EQUAL(uint16_t, test_redirect_port, port_number);

    amqpvalue_destroy(hostname_key);
    amqpvalue_destroy(network_host_key);
    amqpvalue_destroy(port_key);
    amqpvalue_destroy(hostname_value);
    amqpvalue_destroy(network_host_value);
    amqpvalue_destroy(port_value);

    *redirect_received = true;
}

static bool on_new_session_endpoint_connection_redirect(void* context, ENDPOINT_HANDLE new_endpoint)
{
    SERVER_INSTANCE* server = (SERVER_INSTANCE*)context;
    AMQP_VALUE redirect_map = amqpvalue_create_map();
    AMQP_VALUE hostname_key = amqpvalue_create_string("hostname");
    AMQP_VALUE network_host_key = amqpvalue_create_string("network-host");
    AMQP_VALUE port_key = amqpvalue_create_string("port");
    AMQP_VALUE hostname_value = amqpvalue_create_string(test_redirect_hostname);
    AMQP_VALUE network_host_value = amqpvalue_create_string(test_redirect_network_host);
    AMQP_VALUE port_value = amqpvalue_create_ushort(test_redirect_port);

    (void)new_endpoint;

    (void)amqpvalue_set_map_value(redirect_map, hostname_key, hostname_value);
    (void)amqpvalue_set_map_value(redirect_map, network_host_key, network_host_value);
    (void)amqpvalue_set_map_value(redirect_map, port_key, port_value);

    amqpvalue_destroy(hostname_key);
    amqpvalue_destroy(hostname_value);
    amqpvalue_destroy(network_host_key);
    amqpvalue_destroy(network_host_value);
    amqpvalue_destroy(port_key);
    amqpvalue_destroy(port_value);

    (void)connection_close(server->connection, connection_error_redirect, "Redirect", redirect_map);
    amqpvalue_destroy(redirect_map);

    return false;
}

static void on_socket_accepted_connection_redirect(void* context, const IO_INTERFACE_DESCRIPTION* interface_description, void* io_parameters)
{
    HEADER_DETECT_IO_CONFIG header_detect_io_config;
    HEADER_DETECT_ENTRY header_detect_entries[1];
    SERVER_INSTANCE* server = (SERVER_INSTANCE*)context;
    int result;
    AMQP_HEADER amqp_header;

    server->underlying_io = xio_create(interface_description, io_parameters);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->underlying_io, "Could not create underlying IO");

    amqp_header = header_detect_io_get_amqp_header();
    header_detect_entries[0].header.header_bytes = amqp_header.header_bytes;
    header_detect_entries[0].header.header_size = amqp_header.header_size;
    header_detect_entries[0].io_interface_description = NULL;

    header_detect_io_config.underlying_io = server->underlying_io;
    header_detect_io_config.header_detect_entry_count = 1;
    header_detect_io_config.header_detect_entries = header_detect_entries;

    server->header_detect_io = xio_create(header_detect_io_get_interface_description(), &header_detect_io_config);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->header_detect_io, "Could not create header detect IO");
    server->connection = connection_create(server->header_detect_io, NULL, "1", on_new_session_endpoint_connection_redirect, server);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->connection, "Could not create server connection");
    (void)connection_set_trace(server->connection, true);
    result = connection_listen(server->connection);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot start listening");
}

TEST_FUNCTION(connection_redirect_notifies_the_user_of_the_event)
{
    // arrange
    int port_number = generate_port_number();
    SERVER_INSTANCE server_instance;
    SOCKET_LISTENER_HANDLE socket_listener = socketlistener_create(port_number);
    int result;
    XIO_HANDLE socket_io;
    CONNECTION_HANDLE client_connection;
    SESSION_HANDLE client_session;
    LINK_HANDLE client_link;
    MESSAGE_SENDER_HANDLE client_message_sender;
    bool redirect_received;
    AMQP_VALUE source;
    AMQP_VALUE target;
    time_t now_time;
    time_t start_time;
    SOCKETIO_CONFIG socketio_config = { "localhost", 0, NULL };

    server_instance.connection = NULL;
    server_instance.session = NULL;
    server_instance.link_count = 0;
    server_instance.links[0] = NULL;
    server_instance.links[1] = NULL;
    server_instance.message_receivers[0] = NULL;
    server_instance.message_receivers[1] = NULL;
    server_instance.received_messages = 0;

    redirect_received = false;

    result = socketlistener_start(socket_listener, on_socket_accepted_connection_redirect, &server_instance);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "socketlistener_start failed");

    // start the client
    socketio_config.port = port_number;
    socket_io = xio_create(socketio_get_interface_description(), &socketio_config);
    ASSERT_IS_NOT_NULL_WITH_MSG(socket_io, "Could not create socket IO");

    /* create the connection, session and link */
    client_connection = connection_create(socket_io, "localhost", "some", NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_connection, "Could not create client connection");

    (void)connection_set_trace(client_connection, true);
    client_session = session_create(client_connection, NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_session, "Could not create client session");

    (void)connection_subscribe_on_connection_close_received(client_connection, on_connection_redirect_received, &redirect_received);

    source = messaging_create_source("ingress");
    ASSERT_IS_NOT_NULL_WITH_MSG(source, "Could not create source");
    target = messaging_create_target("localhost/ingress");
    ASSERT_IS_NOT_NULL_WITH_MSG(target, "Could not create target");

    // link
    client_link = link_create(client_session, "sender-link-1", role_sender, source, target);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_link, "Could not create client link");
    result = link_set_snd_settle_mode(client_link, sender_settle_mode_unsettled);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot set sender settle mode on link");

    amqpvalue_destroy(source);
    amqpvalue_destroy(target);

    /* create the message sender */
    client_message_sender = messagesender_create(client_link, NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_message_sender, "Could not create message sender");
    result = messagesender_open(client_message_sender);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot open message sender");

    // wait for either time elapsed or message received
    start_time = time(NULL);
    while ((now_time = time(NULL)),
        (difftime(now_time, start_time) < TEST_TIMEOUT))
    {
        // schedule work for all components
        socketlistener_dowork(socket_listener);
        connection_dowork(client_connection);
        connection_dowork(server_instance.connection);

        // if we received the message, break
        if (redirect_received)
        {
            break;
        }

        ThreadAPI_Sleep(1);
    }

    // assert
    ASSERT_IS_TRUE_WITH_MSG(redirect_received, "Redirect information not received");

    // cleanup
    socketlistener_stop(socket_listener);
    messagesender_destroy(client_message_sender);
    link_destroy(client_link);
    session_destroy(client_session);
    connection_destroy(client_connection);
    xio_destroy(socket_io);

    messagereceiver_destroy(server_instance.message_receivers[0]);
    messagereceiver_destroy(server_instance.message_receivers[1]);
    link_destroy(server_instance.links[0]);
    link_destroy(server_instance.links[1]);
    session_destroy(server_instance.session);
    connection_destroy(server_instance.connection);
    xio_destroy(server_instance.header_detect_io);
    xio_destroy(server_instance.underlying_io);
    socketlistener_destroy(socket_listener);
}

static void on_link_redirect_received(void* context, ERROR_HANDLE error)
{
    bool* redirect_received = (bool*)context;
    fields info = NULL;
    AMQP_VALUE hostname_key = amqpvalue_create_string("hostname");
    AMQP_VALUE network_host_key = amqpvalue_create_string("network-host");
    AMQP_VALUE port_key = amqpvalue_create_string("port");
    AMQP_VALUE address_key = amqpvalue_create_string("address");
    AMQP_VALUE hostname_value;
    AMQP_VALUE network_host_value;
    AMQP_VALUE port_value;
    AMQP_VALUE address_value;
    const char* condition_string;
    const char* hostname_string;
    const char* network_host_string;
    const char* address_string;
    uint16_t port_number;

    ASSERT_IS_NOT_NULL_WITH_MSG(error, "NULL error information");
    (void)error_get_condition(error, &condition_string);
    ASSERT_ARE_EQUAL(char_ptr, "amqp:link:redirect", condition_string);
    (void)error_get_info(error, &info);
    ASSERT_IS_NOT_NULL_WITH_MSG(info, "NULL info in error");

    hostname_value = amqpvalue_get_map_value(info, hostname_key);
    ASSERT_IS_NOT_NULL_WITH_MSG(hostname_value, "NULL hostname_value");
    network_host_value = amqpvalue_get_map_value(info, network_host_key);
    ASSERT_IS_NOT_NULL_WITH_MSG(network_host_value, "NULL network_host_value");
    port_value = amqpvalue_get_map_value(info, port_key);
    ASSERT_IS_NOT_NULL_WITH_MSG(port_value, "NULL port_value");
    address_value = amqpvalue_get_map_value(info, address_key);
    ASSERT_IS_NOT_NULL_WITH_MSG(address_value, "NULL address_value");

    (void)amqpvalue_get_string(hostname_value, &hostname_string);
    ASSERT_ARE_EQUAL(char_ptr, test_redirect_hostname, hostname_string);
    (void)amqpvalue_get_string(network_host_value, &network_host_string);
    ASSERT_ARE_EQUAL(char_ptr, test_redirect_network_host, network_host_string);
    (void)amqpvalue_get_ushort(port_value, &port_number);
    ASSERT_ARE_EQUAL(uint16_t, test_redirect_port, port_number);
    (void)amqpvalue_get_string(address_value, &address_string);
    ASSERT_ARE_EQUAL(char_ptr, test_redirect_address, address_string);

    amqpvalue_destroy(hostname_key);
    amqpvalue_destroy(network_host_key);
    amqpvalue_destroy(port_key);
    amqpvalue_destroy(address_key);
    amqpvalue_destroy(hostname_value);
    amqpvalue_destroy(network_host_value);
    amqpvalue_destroy(port_value);
    amqpvalue_destroy(address_value);

    *redirect_received = true;
}

static bool on_new_link_attached_link_redirect(void* context, LINK_ENDPOINT_HANDLE new_link_endpoint, const char* name, role role, AMQP_VALUE source, AMQP_VALUE target)
{
    SERVER_INSTANCE* server = (SERVER_INSTANCE*)context;
    int result;
    AMQP_VALUE redirect_map = amqpvalue_create_map();
    AMQP_VALUE hostname_key = amqpvalue_create_string("hostname");
    AMQP_VALUE network_host_key = amqpvalue_create_string("network-host");
    AMQP_VALUE port_key = amqpvalue_create_string("port");
    AMQP_VALUE address_key = amqpvalue_create_string("address");
    AMQP_VALUE hostname_value = amqpvalue_create_string(test_redirect_hostname);
    AMQP_VALUE network_host_value = amqpvalue_create_string(test_redirect_network_host);
    AMQP_VALUE port_value = amqpvalue_create_ushort(test_redirect_port);
    AMQP_VALUE address_value = amqpvalue_create_string(test_redirect_address);

    (void)amqpvalue_set_map_value(redirect_map, hostname_key, hostname_value);
    (void)amqpvalue_set_map_value(redirect_map, network_host_key, network_host_value);
    (void)amqpvalue_set_map_value(redirect_map, port_key, port_value);
    (void)amqpvalue_set_map_value(redirect_map, address_key, address_value);

    amqpvalue_destroy(hostname_key);
    amqpvalue_destroy(hostname_value);
    amqpvalue_destroy(network_host_key);
    amqpvalue_destroy(network_host_value);
    amqpvalue_destroy(port_key);
    amqpvalue_destroy(port_value);
    amqpvalue_destroy(address_key);
    amqpvalue_destroy(address_value);

    server->links[server->link_count] = link_create_from_endpoint(server->session, new_link_endpoint, name, role, source, target);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->links[server->link_count], "Could not create link");
    server->message_receivers[server->link_count] = messagereceiver_create(server->links[server->link_count], on_message_receivers_state_changed, server);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->message_receivers[server->link_count], "Could not create message receiver");
    result = messagereceiver_open(server->message_receivers[server->link_count], on_message_received, server);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "message receiver open failed");
    (void)link_detach(server->links[server->link_count], true, "amqp:link:redirect", "Redirect", redirect_map);
    amqpvalue_destroy(redirect_map);
    server->link_count++;

    return true;
}

static bool on_new_session_endpoint_link_redirect(void* context, ENDPOINT_HANDLE new_endpoint)
{
    SERVER_INSTANCE* server = (SERVER_INSTANCE*)context;
    int result;

    server->session = session_create_from_endpoint(server->connection, new_endpoint, on_new_link_attached_link_redirect, server);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->session, "Could not create server session");
    result = session_begin(server->session);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot begin server session");

    return true;
}

static void on_socket_accepted_link_redirect(void* context, const IO_INTERFACE_DESCRIPTION* interface_description, void* io_parameters)
{
    HEADER_DETECT_IO_CONFIG header_detect_io_config;
    HEADER_DETECT_ENTRY header_detect_entries[1];
    SERVER_INSTANCE* server = (SERVER_INSTANCE*)context;
    int result;
    AMQP_HEADER amqp_header;

    server->underlying_io = xio_create(interface_description, io_parameters);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->underlying_io, "Could not create underlying IO");

    amqp_header = header_detect_io_get_amqp_header();
    header_detect_entries[0].header.header_bytes = amqp_header.header_bytes;
    header_detect_entries[0].header.header_size = amqp_header.header_size;
    header_detect_entries[0].io_interface_description = NULL;

    header_detect_io_config.underlying_io = server->underlying_io;
    header_detect_io_config.header_detect_entry_count = 1;
    header_detect_io_config.header_detect_entries = header_detect_entries;

    server->header_detect_io = xio_create(header_detect_io_get_interface_description(), &header_detect_io_config);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->header_detect_io, "Could not create header detect IO");
    server->connection = connection_create(server->header_detect_io, NULL, "1", on_new_session_endpoint_link_redirect, server);
    ASSERT_IS_NOT_NULL_WITH_MSG(server->connection, "Could not create server connection");
    (void)connection_set_trace(server->connection, true);
    result = connection_listen(server->connection);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot start listening");
}

TEST_FUNCTION(link_redirect_notifies_the_user_of_the_event)
{
    // arrange
    int port_number = generate_port_number();
    SERVER_INSTANCE server_instance;
    SOCKET_LISTENER_HANDLE socket_listener = socketlistener_create(port_number);
    int result;
    XIO_HANDLE socket_io;
    CONNECTION_HANDLE client_connection;
    SESSION_HANDLE client_session;
    LINK_HANDLE client_link;
    MESSAGE_SENDER_HANDLE client_message_sender;
    bool redirect_received;
    AMQP_VALUE source;
    AMQP_VALUE target;
    time_t now_time;
    time_t start_time;
    SOCKETIO_CONFIG socketio_config = { "localhost", 0, NULL };

    server_instance.connection = NULL;
    server_instance.session = NULL;
    server_instance.link_count = 0;
    server_instance.links[0] = NULL;
    server_instance.links[1] = NULL;
    server_instance.message_receivers[0] = NULL;
    server_instance.message_receivers[1] = NULL;
    server_instance.received_messages = 0;

    redirect_received = false;

    result = socketlistener_start(socket_listener, on_socket_accepted_link_redirect, &server_instance);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "socketlistener_start failed");

    // start the client
    socketio_config.port = port_number;
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

    // link
    client_link = link_create(client_session, "sender-link-1", role_sender, source, target);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_link, "Could not create client link");
    result = link_set_snd_settle_mode(client_link, sender_settle_mode_unsettled);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot set sender settle mode on link");

    (void)link_subscribe_on_link_detach_received(client_link, on_link_redirect_received, &redirect_received);

    amqpvalue_destroy(source);
    amqpvalue_destroy(target);

    /* create the message sender */
    client_message_sender = messagesender_create(client_link, NULL, NULL);
    ASSERT_IS_NOT_NULL_WITH_MSG(client_message_sender, "Could not create message sender");
    result = messagesender_open(client_message_sender);
    ASSERT_ARE_EQUAL_WITH_MSG(int, 0, result, "cannot open message sender");

    // wait for either time elapsed or message received
    start_time = time(NULL);
    while ((now_time = time(NULL)),
        (difftime(now_time, start_time) < TEST_TIMEOUT))
    {
        // schedule work for all components
        socketlistener_dowork(socket_listener);
        connection_dowork(client_connection);
        connection_dowork(server_instance.connection);

        // if we received the message, break
        if (redirect_received)
        {
            break;
        }

        ThreadAPI_Sleep(1);
    }

    // assert
    ASSERT_IS_TRUE_WITH_MSG(redirect_received, "Redirect information not received");

    // cleanup
    socketlistener_stop(socket_listener);
    messagesender_destroy(client_message_sender);
    link_destroy(client_link);
    session_destroy(client_session);
    connection_destroy(client_connection);
    xio_destroy(socket_io);

    messagereceiver_destroy(server_instance.message_receivers[0]);
    messagereceiver_destroy(server_instance.message_receivers[1]);
    link_destroy(server_instance.links[0]);
    link_destroy(server_instance.links[1]);
    session_destroy(server_instance.session);
    connection_destroy(server_instance.connection);
    xio_destroy(server_instance.header_detect_io);
    xio_destroy(server_instance.underlying_io);
    socketlistener_destroy(socket_listener);
}

END_TEST_SUITE(local_client_server_tcp_e2e)
