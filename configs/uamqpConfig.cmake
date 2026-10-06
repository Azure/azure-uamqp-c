#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

include(CMakeFindDependencyMacro)

# uamqp's exported targets reference aziotsharedutil via INTERFACE_LINK_LIBRARIES,
# so consumers must be able to resolve it before the targets file is included.
find_dependency(azure_c_shared_utility)

include("${CMAKE_CURRENT_LIST_DIR}/uamqpTargets.cmake")

get_target_property(UAMQP_INCLUDES uamqp INTERFACE_INCLUDE_DIRECTORIES)

set(UAMQP_INCLUDES ${UAMQP_INCLUDES} CACHE INTERNAL "")