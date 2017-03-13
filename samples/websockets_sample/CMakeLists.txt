#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

compileAsC99()

add_executable(websockets_sample
    main.c
    iothub_certs.h
    iothub_certs.c)
    
include_directories(.)

if(WIN32)
    #windows needs this define
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DGB_MEASURE_MEMORY_FOR_THIS -DGB_DEBUG_ALLOC)

    target_link_libraries(websockets_sample
        uamqp
        ws2_32
        aziotsharedutil)
else()
    target_link_libraries(websockets_sample	uamqp aziotsharedutil ssl crypto)
endif()
