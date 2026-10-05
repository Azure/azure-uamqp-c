#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

add_test(NAME install_export_int
    COMMAND ${CMAKE_COMMAND}
        "-DTOP_BINARY_DIR=${CMAKE_BINARY_DIR}"
        "-DCONSUMER_SRC_DIR=${CMAKE_CURRENT_LIST_DIR}/consumer"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/work"
        "-DGENERATOR=${CMAKE_GENERATOR}"
        "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        "-DC_COMPILER=${CMAKE_C_COMPILER}"
        "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DBUILD_CONFIG=$<CONFIG>"
        -P "${CMAKE_CURRENT_LIST_DIR}/run_install_export_test.cmake"
)
