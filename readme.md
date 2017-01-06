This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

# uAMQP

uAMQP is a general purpose C library for AMQP.

The goal is to be as compliant with the standard as possible while optimizing for low RAM footprint and also being portable.

It is currently a client side implementation only. Although much of the standard is symmetrical, there are parts that are asymmetrical, like the SASL handshake.
Currently uAMQP does not provide the server side for these asymmetrical portions of the ISO.

## Dependencies

uAMQP uses azure-c-shared-utility, which is a C library provising common functionality for basic tasks (like string, list manipulation, IO, etc.).
azure-c-shared-utility is available here: https://github.com/Azure/azure-c-shared-utility and it is used as a submodule.

azure-c-shared-utility provides 3 tlsio implementations:
- tlsio_schannel - runs only on Windows
- tlsio_openssl - depends on OpenSSL being installed
- tlsio_wolfssl - depends on WolfSSL being installed 

For more information about configuring azure-c-shared-utility see https://github.com/Azure/azure-c-shared-utility.

uAMQP uses cmake for configuring build files.

For WebSockets support uAMQP depends on the support provided by azure-c-shared-utility (through libwebsockets).

## Setup

### Build

- Clone azure-uamqp-c by:

```
git clone --recursive https://github.com/Azure/azure-uamqp-c.git
```

- Create a folder cmake under azure-uamqp-c

- Switch to the cmake folder and run

```
cmake ..
```

- Build

```
cmake --build .
```

### Installation and Use
Optionally, you may choose to install azure-uamqp-c on your machine:

1. Switch to the *cmake* folder and run
    ```
    cmake -Duse_installed=ON ../
    ```
    ```
    cmake --build . --target install
    ```
    
    or install using the follow commands for each platform:

    On Linux:
    ```
    sudo make install
    ```

    On Windows:
    ```
    msbuild /m INSTALL.vcxproj
    ```

2. Use it in your project (if installed)
    ```
    find_package(uamqp REQUIRED CONFIG)
    target_link_library(yourlib uamqp)
    ```

_This requires that azure-c-shared-utility is installed (through CMake) on your machine._

_If running tests, this requires that umock-c, azure-ctest, and azure-c-testrunnerswitcher are installed (through CMake) on your machine._

### Building the tests

In order to build the tests use:

```
cmake .. -Drun_unittests:bool=ON
```

## Switching branches

After any switch of branches (git checkout for example), one should also update the submodule references by:

```
git submodule update --init --recursive
```

## Websocket support

- To enable websocket support, run: 

```
cmake .. -Duse_wsio:bool=ON
```

- Build

## Samples

Samples are available in the azure-uamqp-c/samples folder:
- Send messages to an Event Hub
- Receive messages from an Event Hub
- Send messages to an IoT Hub using CBS
- Send messages to an IoT Hub using AMQP over WebSockets
- Simple client/server sample using raw TCP
