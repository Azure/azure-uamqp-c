#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

include_directories(.)

add_subdirectory(amqp_frame_codec_ut)
add_subdirectory(amqpvalue_ut)
add_subdirectory(connection_ut)
add_subdirectory(frame_codec_ut)
add_subdirectory(message_ut)
add_subdirectory(sasl_anonymous_ut)
add_subdirectory(sasl_frame_codec_ut)
add_subdirectory(sasl_mechanism_ut)
add_subdirectory(sasl_plain_ut)
add_subdirectory(session_ut)
add_subdirectory(saslclientio_ut)
