#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
project(uamqp)

option(run_e2e_tests "set run_e2e_tests to ON to run e2e tests (default is OFF) [if possible, they are always build]" OFF)
option(run_longhaul_tests "set run_longhaul_tests to ON to run longhaul tests (default is OFF)[if possible, they are always build]" OFF)
option(skip_unittests "set skip_unittests to ON to skip unittests (default is OFF)[if possible, they are always build]" OFF)
option(leak_detection "set leak_detection to ON (default is OFF)" OFF)
option(compileOption_C "passes a string to the command line of the C compiler" OFF)
option(compileOption_CXX "passes a string to the command line of the C++ compiler" OFF)
option(use_wsio "set use_wsio to ON to use libwebsockets for WebSocket support (default is OFF)" OFF)

if(WIN32)
    option(use_schannel "set use_schannel to ON if schannel is to be used, set to OFF to not use schannel" ON)
    option(use_openssl "set use_openssl to ON if openssl is to be used, set to OFF to not use openssl" OFF)
    option(use_wolfssl "set use_wolfssl to ON if wolfssl is to be used, set to OFF to not use wolfssl" OFF)
else()
    option(use_schannel "set use_schannel to ON if schannel is to be used, set to OFF to not use schannel" OFF)
    option(use_openssl "set use_openssl to ON if openssl is to be used, set to OFF to not use openssl" ON)
    option(use_wolfssl "set use_wolfssl to ON if wolfssl is to be used, set to OFF to not use wolfssl" OFF)
endif()

#Use solution folders. 
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_definitions(-DREFCOUNT_ATOMIC_DONTCARE)
add_definitions(-D__STDC_NO_ATOMICS__=1)

if(${use_wsio})
    include_directories(libwebsockets/lib)
    include_directories(${PROJECT_BINARY_DIR}/libwebsockets)
    add_subdirectory(libwebsockets)
endif()

add_subdirectory(azure-c-shared-utility/c)

enable_testing()
#if any compiler has a command line switch called "OFF" then it will need special care
if(NOT "${compileOption_C}" STREQUAL "OFF")
    set(CMAKE_C_FLAGS "${compileOption_C} ${CMAKE_C_FLAGS}")
endif()

if(NOT "${compileOption_CXX}" STREQUAL "OFF")
    set(CMAKE_CXX_FLAGS "${compileOption_CXX} ${CMAKE_CXX_FLAGS}")
endif()

#this project uses several other projects that are build not by these CMakeFiles
#this project also targets several OSes

#this function takes care of three things:
#1. copying some shared libraries(.dll or .so) to the location of the output executable

macro(compileAsC99)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "--std=c99 ${CMAKE_C_FLAGS}")
      set (CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")
    endif()
  else()
    set (CMAKE_C_STANDARD 99)
    set (CMAKE_CXX_STANDARD 11)
  endif()
endmacro(compileAsC99)

macro(compileAsC11)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "--std=c11 ${CMAKE_C_FLAGS}")
      set (CMAKE_C_FLAGS "-D_POSIX_C_SOURCE=200112L ${CMAKE_C_FLAGS}")
      set (CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")
    endif()
  else()
    set (CMAKE_C_STANDARD 11)
    set (CMAKE_CXX_STANDARD 11)
  endif()
endmacro(compileAsC11)

function(windows_unittests_add_dll whatIsBuilding)	
    link_directories(${whatIsBuilding}_dll ${MICROMOCK_UNITTEST_LIB_DIR} ${CTEST_LIB_DIR} ${SHARED_UTIL_LIB_DIR} $ENV{VCInstallDir}UnitTest/lib)
    
    add_library(${whatIsBuilding}_dll SHARED 
        ${${whatIsBuilding}_cpp_files} 
        ${${whatIsBuilding}_h_files} 
        ${${whatIsBuilding}_c_files}
    )
    
    #Add UnitTests to their own folder
    set_target_properties(${whatIsBuilding}_dll
               PROPERTIES
               FOLDER "UnitTests") 
    
    target_include_directories(${whatIsBuilding}_dll PUBLIC ${sharedutil_include_directories} $ENV{VCInstallDir}UnitTest/include)
    target_compile_definitions(${whatIsBuilding}_dll PUBLIC -DCPP_UNITTEST)
    target_link_libraries(${whatIsBuilding}_dll micromock_cpp_unittest ctest ${ARGN})
endfunction()

function(windows_unittests_add_exe whatIsBuilding)
    link_directories(${whatIsBuilding}_exe ${MICROMOCK_UNITTEST_LIB_DIR} ${CTEST_LIB_DIR} ${SHARED_UTIL_LIB_DIR} $ENV{VCInstallDir}UnitTest/lib)

    add_executable(${whatIsBuilding}_exe
        ${${whatIsBuilding}_cpp_files} 
        ${${whatIsBuilding}_h_files} 
        ${${whatIsBuilding}_c_files}
        ${CMAKE_CURRENT_LIST_DIR}/main.c
    )
    
    #Add UnitTests to their own folder
    set_target_properties(${whatIsBuilding}_exe
               PROPERTIES
               FOLDER "UnitTests")
    
    target_compile_definitions(${whatIsBuilding}_exe PUBLIC -DUSE_CTEST)
    target_include_directories(${whatIsBuilding}_exe PUBLIC ${sharedutil_include_directories})
    target_link_libraries(${whatIsBuilding}_exe micromock_ctest ctest ${ARGN})
    add_test(NAME ${whatIsBuilding} COMMAND ${whatIsBuilding}_exe)
endfunction()

function(windows_unittests_add_lib whatIsBuilding)
    link_directories(${whatIsBuilding}_lib ${MICROMOCK_UNITTEST_LIB_DIR} ${CTEST_LIB_DIR} ${SHARED_UTIL_LIB_DIR} $ENV{VCInstallDir}UnitTest/lib)
    
    add_library(${whatIsBuilding}_lib STATIC 
        ${${whatIsBuilding}_cpp_files} 
        ${${whatIsBuilding}_h_files} 
        ${${whatIsBuilding}_c_files}
    )

    #Add UnitTests to their own folder
    set_target_properties(${whatIsBuilding}_lib
               PROPERTIES
               FOLDER "UnitTests")
    
    target_include_directories(${whatIsBuilding}_lib PUBLIC ${sharedutil_include_directories})
    target_compile_definitions(${whatIsBuilding}_lib PUBLIC -DUSE_CTEST)
    target_link_libraries(${whatIsBuilding}_lib micromock_cpp_unittest ctest ${ARGN})
endfunction()

function(build_test_artifacts whatIsBuilding use_gballoc)
    
    #the first argument is what is building
    #the second argument is whether the tests should be build with gballoc #defines or not
    #the following arguments are a list of libraries to link with
    
    if(${use_gballoc})
        add_definitions(-DGB_MEASURE_MEMORY_FOR_THIS -DGB_DEBUG_ALLOC)
    else()	
    endif()
    
    #setting #defines
    if(WIN32)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    else()
    endif()
    
    #setting includes
    set(sharedutil_include_directories ${MICROMOCK_INC_FOLDER} ${TESTRUNNERSWITCHER_INC_FOLDER} ${CTEST_INC_FOLDER} ${SAL_INC_FOLDER} ${SHARED_UTIL_INC_FOLDER} ${SHARED_UTIL_SRC_FOLDER} ../)
    if(WIN32)
    else()
        include_directories(${sharedutil_include_directories})
    endif()

    #setting output type
    if(WIN32)
        #to disable running e2e or longhaul or unittests tests for windows, we build the the same thing as "static library" so it is not picked up by visual studio
        if(
            (("${whatIsBuilding}" MATCHES ".*e2e.*") AND NOT ${run_e2e_tests}) OR
            (("${whatIsBuilding}" MATCHES ".*longhaul.*") AND NOT ${run_longhaul_tests}) OR
            (("${whatIsBuilding}" MATCHES ".*unittests.*") AND ${skip_unittests})
        )
            windows_unittests_add_lib(${whatIsBuilding} ${ARGN})
        else()
            windows_unittests_add_exe(${whatIsBuilding} ${ARGN})
            windows_unittests_add_dll(${whatIsBuilding} ${ARGN})
        endif()
    else()
        if(
            (("${whatIsBuilding}" MATCHES ".*e2e.*") AND NOT ${run_e2e_tests}) OR
            (("${whatIsBuilding}" MATCHES ".*longhaul.*") AND NOT ${run_longhaul_tests}) OR
            (("${whatIsBuilding}" MATCHES ".*unittests.*") AND ${skip_unittests})
        )
            windows_unittests_add_lib(${whatIsBuilding} ${ARGN})
        else()
            windows_unittests_add_exe(${whatIsBuilding} ${ARGN})
        endif()
    endif()
endfunction(build_test_artifacts)

option(memory_trace "set memory_trace to ON if memory usage is to be used, set to OFF to not use it" ON)

include_directories(inc ${SHARED_UTIL_INC_FOLDER})

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

if(${leak_detection})
    add_definitions(-D_CRTDBG_MAP_ALLOC)
endif()

if(${memory_trace})
else()
    add_definitions(-DDISABLE_MEMORY_TRACE)
endif()

if(WIN32)
    include_directories($ENV{OpenSSLDir}/include)
endif()

set(uamqp_h_files
    ./inc/amqp_definitions.h
    ./inc/amqp_frame_codec.h
    ./inc/amqp_management.h
    ./inc/amqp_types.h
    ./inc/amqpalloc.h
    ./inc/amqpvalue.h
    ./inc/amqpvalue_to_string.h
    ./inc/cbs.h
    ./inc/connection.h
    ./inc/consolelogger.h
    ./inc/frame_codec.h
    ./inc/header_detect_io.h
    ./inc/link.h
    ./inc/logger.h
    ./inc/message.h
    ./inc/message_receiver.h
    ./inc/message_sender.h
    ./inc/messaging.h
    ./inc/sasl_anonymous.h
    ./inc/sasl_frame_codec.h
    ./inc/sasl_mechanism.h
    ./inc/sasl_mssbcbs.h
    ./inc/sasl_plain.h
    ./inc/saslclientio.h
    ./inc/session.h
    ./inc/socket_listener.h
)

set(uamqp_c_files
    ./src/amqp_definitions.c
    ./src/amqp_frame_codec.c
    ./src/amqp_management.c
    ./src/amqpalloc.c
    ./src/amqpvalue.c
    ./src/amqpvalue_to_string.c
    ./src/cbs.c
    ./src/connection.c
    ./src/consolelogger.c
    ./src/frame_codec.c
    ./src/header_detect_io.c
    ./src/link.c
    ./src/message.c
    ./src/message_receiver.c
    ./src/message_sender.c
    ./src/messaging.c
    ./src/sasl_anonymous.c
    ./src/sasl_frame_codec.c
    ./src/sasl_mechanism.c
    ./src/sasl_mssbcbs.c
    ./src/sasl_plain.c
    ./src/saslclientio.c
    ./src/session.c
)

if(${use_wsio})
    set(wsio_h_files
        ./inc/wsio.h
    )
    set(wsio_c_files
        ./src/wsio.c
    )
else()
    set(wsio_h_files)
    set(wsio_c_files)
endif()

if(WIN32)
    set(socketlistener_c_files
        ./src/socket_listener_win32.c
    )
else()
    set(socketlistener_c_files)
endif()

add_library(uamqp
    ${uamqp_c_files}
    ${uamqp_h_files}
    ${wsio_c_files}
    ${wsio_h_files}
    ${socketlistener_c_files}
    )

add_subdirectory(samples)
add_subdirectory(tests)

# The following "set" statetement exports across the project a global variable
set(UAMQP_INC_FOLDER ${CMAKE_CURRENT_LIST_DIR}/inc CACHE INTERNAL "This is the include folder for UAMQP" FORCE)
set(UAMQP_SRC_FOLDER ${CMAKE_CURRENT_LIST_DIR}/src CACHE INTERNAL "This is the lib folder for UAMQP" FORCE)