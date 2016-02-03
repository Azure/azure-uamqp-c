# uAMQP

uAMQP is a general purpose C library for AMQP.

The goal is to be as compliant with the standard as possible while optimizing for low RAM footprint and also being portable.

It is currently a client side implementation only. Although much of the standard is symmetrical, there are parts that are asymmetrical, like the SASL handshake.
Currently uAMQP does not provide the server side for these asymmetrical protions of the ISO.

## Dependencies

uAMQP uses azure-c-shared-utility, which is a C library provising common functionality for basic tasks (like string, list manipulation, IO, etc.).
azure-c-shared-utility is available here: https://github.com/Azure/azure-c-shared-utility and it is used as a submodule.

azure-c-shared-utility provides 3 tlsio implementations:
- tlsio_schannel - runs only on Windows
- tlsio_openssl - depends on OpenSSL being installed
- tlsio_wolfssl - depends on WolfSSL being installed 

For more information about configuring azure-c-shared-utility see https://github.com/Azure/azure-c-shared-utility.

uAMQP uses cmake for configuring build files.

For WebSockets support uAMQP depends on libwebsockets, which is available here: https://github.com/warmcat/libwebsockets and is also used as a submodule.

## Setup

- Clone azure-uamqp-c by:
```
git clone --recursive https://github.com/Azure/azure-uamqp-c.git
```
- Create a folder build under azure-uamqp-c
- Switch to the build folder and run
   cmake ..

## Switching branches

After any switch of branches (git checkout for example), one should also update the submodule references by:

```
git submodule update --init --recursive
```

## Websocket support

- run "cmake .. -Dwsio:bool=ON" to enable websocket support.

- build

## Samples

Samples are available in the azure-uamqp-c/samples folder:
- Send messages to an Event Hub
- Receive messages from an Event Hub
- Send messages to an IoT Hub using CBS
- Send messages to an IoT Hub using AMQP over WebSockets
- Simple client/server sample using raw TCP
