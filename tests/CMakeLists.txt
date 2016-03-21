#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

include_directories(.)

add_subdirectory(amqp_frame_codec_unittests)
add_subdirectory(amqpvalue_limits_unittests)
add_subdirectory(amqpvalue_unittests)
add_subdirectory(connection_unittests)
add_subdirectory(frame_codec_unittests)
add_subdirectory(message_unittests)
add_subdirectory(sasl_anonymous_unittests)
add_subdirectory(sasl_frame_codec_unittests)
add_subdirectory(sasl_mechanism_unittests)
add_subdirectory(sasl_plain_unittests)
add_subdirectory(session_unittests)
add_subdirectory(saslclientio_unittests)

if(use_wsio)
    add_subdirectory(wsio_unittests)
endif()