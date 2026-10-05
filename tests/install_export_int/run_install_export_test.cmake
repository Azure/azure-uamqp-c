#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

# Installs the project that has just been built into a scratch prefix and then
# configures and compiles a standalone find_package(uamqp CONFIG REQUIRED)
# consumer against it. Run as `cmake -P`, so nothing here may assume a project.

foreach(required_var TOP_BINARY_DIR CONSUMER_SRC_DIR WORK_DIR)
    if(NOT DEFINED ${required_var})
        message(FATAL_ERROR "${required_var} was not passed to this script")
    endif()
endforeach()

set(prefix_dir "${WORK_DIR}/prefix")
set(consumer_build_dir "${WORK_DIR}/consumer")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${prefix_dir}")
file(MAKE_DIRECTORY "${consumer_build_dir}")

function(run_or_fail description)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${description} failed (${result}):\n${output}")
    endif()
    message(STATUS "${description} succeeded")
endfunction()

set(install_command "${CMAKE_COMMAND}" --install "${TOP_BINARY_DIR}" --prefix "${prefix_dir}")
if(BUILD_CONFIG)
    list(APPEND install_command --config "${BUILD_CONFIG}")
endif()
run_or_fail("install into ${prefix_dir}" ${install_command})

# find_package must locate the package through CMAKE_PREFIX_PATH alone, in the
# conventional <libdir>/cmake/<pkg> location.
file(GLOB_RECURSE found_configs "${prefix_dir}/*/uamqpConfig.cmake")
if(NOT found_configs)
    message(FATAL_ERROR "uamqpConfig.cmake was not installed under ${prefix_dir}")
endif()
foreach(config_file IN LISTS found_configs)
    get_filename_component(config_dir "${config_file}" DIRECTORY)
    file(RELATIVE_PATH relative_config_dir "${prefix_dir}" "${config_dir}")
    if(NOT relative_config_dir MATCHES "cmake/uamqp$")
        message(FATAL_ERROR
            "uamqpConfig.cmake was installed to '${relative_config_dir}'; expected <libdir>/cmake/uamqp")
    endif()
endforeach()

set(configure_command "${CMAKE_COMMAND}" -S "${CONSUMER_SRC_DIR}" -B "${consumer_build_dir}"
    "-DCMAKE_PREFIX_PATH=${prefix_dir}")
if(GENERATOR)
    list(APPEND configure_command -G "${GENERATOR}")
endif()
if(GENERATOR_PLATFORM)
    list(APPEND configure_command -A "${GENERATOR_PLATFORM}")
endif()
if(GENERATOR_TOOLSET)
    list(APPEND configure_command -T "${GENERATOR_TOOLSET}")
endif()
if(C_COMPILER)
    list(APPEND configure_command "-DCMAKE_C_COMPILER=${C_COMPILER}")
endif()
if(CXX_COMPILER)
    list(APPEND configure_command "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
endif()
if(BUILD_CONFIG)
    list(APPEND configure_command "-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}")
endif()
run_or_fail("configure the find_package(uamqp CONFIG REQUIRED) consumer" ${configure_command})

set(build_command "${CMAKE_COMMAND}" --build "${consumer_build_dir}")
if(BUILD_CONFIG)
    list(APPEND build_command --config "${BUILD_CONFIG}")
endif()
run_or_fail("compile the consumer against the installed uamqp package" ${build_command})
