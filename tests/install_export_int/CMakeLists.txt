#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

# tests/ is added before the top level include(GNUInstallDirs), so pull it in
# here to get the same CMAKE_INSTALL_LIBDIR the install rules will use.
include(GNUInstallDirs)

# add_test arguments are a CMake list, so encode the list-valued prefix path.
string(REPLACE ";" "|" install_export_prefix_path "${CMAKE_PREFIX_PATH}")

add_test(NAME install_export_int
    COMMAND ${CMAKE_COMMAND}
        "-DTOP_BINARY_DIR=${CMAKE_BINARY_DIR}"
        "-DCONSUMER_SRC_DIR=${CMAKE_CURRENT_LIST_DIR}/consumer"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/work"
        "-DEXPECTED_CONFIG_DIR=${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
        "-DGENERATOR=${CMAKE_GENERATOR}"
        "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        "-DTOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        "-DOUTER_PREFIX_PATH=${install_export_prefix_path}"
        "-DC_COMPILER=${CMAKE_C_COMPILER}"
        "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DBUILD_CONFIG=$<CONFIG>"
        -P "${CMAKE_CURRENT_LIST_DIR}/run_install_export_test.cmake"
)
