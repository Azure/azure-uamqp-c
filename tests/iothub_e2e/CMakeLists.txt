#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

compileAsC99()
set(theseTestsName iothub_e2e)
set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
)

set(${theseTestsName}_h_files
)

build_c_test_artifacts(${theseTestsName} ON "tests/uamqp_tests")

target_link_libraries(${theseTestsName}_exe uamqp)
if(WIN32)
target_link_libraries(${theseTestsName}_dll uamqp)
endif()
