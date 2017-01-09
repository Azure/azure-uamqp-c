#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
project(uamqp)

# Build with -fPIC always
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

#making a nice global variable to know if we are on linux or not.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX TRUE)
    #on Linux, enable valgrind
    #these commands (MEMORYCHECK...) need to apear BEFORE include(CTest) or they will not have any effect
    find_program(MEMORYCHECK_COMMAND valgrind)	
    set(MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full --error-exitcode=1 --track-origins=yes")
endif()

include (CTest)

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
elseif(LINUX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()

IF(WIN32)
    #windows needs this define
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    # Make warning as error
    add_definitions(/WX)
elseif(OSX)
ELSE()
    # Make warning as error
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
ENDIF(WIN32)

option(run_e2e_tests "set run_e2e_tests to ON to run e2e tests (default is OFF) [if possible, they are always built]" OFF)
option(run_longhaul_tests "set run_longhaul_tests to ON to run longhaul tests (default is OFF)[if possible, they are always built]" OFF)
option(run_unittests "set run_unittests to ON to run unittests (default is OFF)" OFF)
option(skip_samples "set skip_samples to ON to skip building samples (default is OFF)[if possible, they are always built]" OFF)
option(leak_detection "set leak_detection to ON (default is OFF)" OFF)
option(compileOption_C "passes a string to the command line of the C compiler" OFF)
option(compileOption_CXX "passes a string to the command line of the C++ compiler" OFF)
option(use_wsio "set use_wsio to ON to use libwebsockets for WebSocket support (default is OFF)" OFF)
option(use_installed_dependencies "set use_installed_dependencies to ON to use installed packages instead of building dependencies from submodules" OFF)

if(WIN32)
    option(use_schannel "set use_schannel to ON if schannel is to be used, set to OFF to not use schannel" ON)
    option(use_openssl "set use_openssl to ON if openssl is to be used, set to OFF to not use openssl" OFF)
    option(use_wolfssl "set use_wolfssl to ON if wolfssl is to be used, set to OFF to not use wolfssl" OFF)
else()
    option(use_schannel "set use_schannel to ON if schannel is to be used, set to OFF to not use schannel" OFF)
    option(use_openssl "set use_openssl to ON if openssl is to be used, set to OFF to not use openssl" ON)
    option(use_wolfssl "set use_wolfssl to ON if wolfssl is to be used, set to OFF to not use wolfssl" OFF)
endif()

include(CheckSymbolExists)
function(detect_architecture symbol arch) 
    if (NOT DEFINED ARCHITECTURE OR ARCHITECTURE STREQUAL "")
        set(CMAKE_REQUIRED_QUIET 1)
        check_symbol_exists("${symbol}" "" ARCHITECTURE_${arch}) 
        unset(CMAKE_REQUIRED_QUIET) 
  
        # The output variable needs to be unique across invocations otherwise 
        # CMake's crazy scope rules will keep it defined 
        if (ARCHITECTURE_${arch}) 
            set(ARCHITECTURE "${arch}" PARENT_SCOPE) 
            set(ARCHITECTURE_${arch} 1 PARENT_SCOPE) 
            add_definitions(-DARCHITECTURE_${arch}=1) 
        endif() 
    endif() 
endfunction()
if (MSVC) 
    detect_architecture("_M_AMD64" x86_64) 
    detect_architecture("_M_IX86" x86) 
    detect_architecture("_M_ARM" ARM) 
else() 
    detect_architecture("__x86_64__" x86_64) 
    detect_architecture("__i386__" x86) 
    detect_architecture("__arm__" ARM) 
endif() 
if (NOT DEFINED ARCHITECTURE OR ARCHITECTURE STREQUAL "")
    set(ARCHITECTURE "GENERIC") 
endif() 
message(STATUS "AMQP Target architecture: ${ARCHITECTURE}")

#Use solution folders. 
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_definitions(-DREFCOUNT_ATOMIC_DONTCARE)
add_definitions(-D__STDC_NO_ATOMICS__=1)

include("dependencies.cmake")

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

option(memory_trace "set memory_trace to ON if memory usage is to be used, set to OFF to not use it" ON)

include_directories(${CMAKE_CURRENT_LIST_DIR}/inc ${SHARED_UTIL_INC_FOLDER})

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
    ./inc/azure_uamqp_c/amqp_definitions.h
    ./inc/azure_uamqp_c/amqp_frame_codec.h
    ./inc/azure_uamqp_c/amqp_management.h
    ./inc/azure_uamqp_c/amqp_types.h
    ./inc/azure_uamqp_c/amqpalloc.h
    ./inc/azure_uamqp_c/amqpvalue.h
    ./inc/azure_uamqp_c/amqpvalue_to_string.h
    ./inc/azure_uamqp_c/cbs.h
    ./inc/azure_uamqp_c/connection.h
    ./inc/azure_uamqp_c/frame_codec.h
    ./inc/azure_uamqp_c/header_detect_io.h
    ./inc/azure_uamqp_c/link.h
    ./inc/azure_uamqp_c/message.h
    ./inc/azure_uamqp_c/message_receiver.h
    ./inc/azure_uamqp_c/message_sender.h
    ./inc/azure_uamqp_c/messaging.h
    ./inc/azure_uamqp_c/sasl_anonymous.h
    ./inc/azure_uamqp_c/sasl_frame_codec.h
    ./inc/azure_uamqp_c/sasl_mechanism.h
    ./inc/azure_uamqp_c/sasl_mssbcbs.h
    ./inc/azure_uamqp_c/sasl_plain.h
    ./inc/azure_uamqp_c/saslclientio.h
    ./inc/azure_uamqp_c/session.h
    ./inc/azure_uamqp_c/socket_listener.h
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
    ${socketlistener_c_files}
    )

target_link_libraries(uamqp aziotsharedutil)

if (NOT ${ARCHITECTURE} STREQUAL "ARM")
    if (NOT ${skip_samples})
        add_subdirectory(samples)
    endif()
endif()
    
if (${run_unittests})
    include("dependencies-test.cmake")
    add_subdirectory(tests)
endif()

# The following "set" statetement exports across the project a global variable
set(UAMQP_INC_FOLDER ${CMAKE_CURRENT_LIST_DIR}/inc CACHE INTERNAL "This is the include folder for UAMQP" FORCE)
set(UAMQP_SRC_FOLDER ${CMAKE_CURRENT_LIST_DIR}/src CACHE INTERNAL "This is the lib folder for UAMQP" FORCE)

if(${use_installed_dependencies})
    # Set CMAKE_INSTALL_LIBDIR if not defined
    include(GNUInstallDirs)

    # Install uamqp
    set(package_location "cmake")

    if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
        set(CMAKE_INSTALL_LIBDIR "lib")
    endif()

    install(TARGETS uamqp EXPORT uamqpTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/../bin
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/azureiot
    )
    install(FILES ${uamqp_h_files} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/azureiot/azure_uamqp_c)

    include(CMakePackageConfigHelpers)

    configure_file("configs/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake"
        COPYONLY
    )

    install(EXPORT uamqpTargets
        FILE
            "${PROJECT_NAME}Targets.cmake"
        DESTINATION
            ${package_location}
    )

    install(
        FILES
            "configs/${PROJECT_NAME}Config.cmake"
        DESTINATION
            ${package_location}
    )
endif()