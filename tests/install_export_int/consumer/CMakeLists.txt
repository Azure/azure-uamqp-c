#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 3.10)

# CXX is required because azure_c_shared_utilityConfig.cmake includes
# azure_iot_build_rules.cmake, which runs CHECK_CXX_COMPILER_FLAG.
project(uamqp_install_export_consumer C CXX)

find_package(uamqp CONFIG REQUIRED)

get_target_property(uamqp_includes uamqp INTERFACE_INCLUDE_DIRECTORIES)
foreach(dir IN LISTS uamqp_includes)
    string(REGEX REPLACE "^\\$<BUILD_INTERFACE:.*>$" "" dir "${dir}")
    if(dir AND NOT IS_DIRECTORY "${dir}")
        message(FATAL_ERROR "uamqp exports include directory '${dir}', which the install did not create")
    endif()
endforeach()

# Compiling only: this asserts the exported include directories resolve the
# public headers. Linking is deliberately not attempted so the test stays
# usable when cross compiling.
add_library(uamqp_install_export_consumer OBJECT main.c)
target_link_libraries(uamqp_install_export_consumer uamqp)
