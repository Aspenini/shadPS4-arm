# SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Nested project that builds protoc for the machine running the build. Needed when the outer build
# is cross compiling: CMake deliberately refuses to substitute an executable target name into a
# custom command in that case, because the binary it produced cannot run on the build machine.
# Configured without the cross toolchain file on purpose.

cmake_minimum_required(VERSION 3.24)
project(shadps4_host_protoc C CXX)

if (NOT SHADPS4_EXTERNALS_DIR)
    message(FATAL_ERROR "SHADPS4_EXTERNALS_DIR must point at the externals directory")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)

# Keep these in step with externals/CMakeLists.txt.
set(ABSL_PROPAGATE_CXX_STD ON CACHE INTERNAL "")
add_subdirectory("${SHADPS4_EXTERNALS_DIR}/abseil-cpp" abseil-cpp EXCLUDE_FROM_ALL)

set(protobuf_BUILD_TESTS              OFF CACHE INTERNAL "")
set(protobuf_BUILD_EXAMPLES           OFF CACHE INTERNAL "")
set(protobuf_BUILD_PROTOC_BINARIES    ON  CACHE INTERNAL "")
set(protobuf_INSTALL                  OFF CACHE INTERNAL "")
set(protobuf_WITH_ZLIB                OFF CACHE INTERNAL "")
set(protobuf_BUILD_SHARED_LIBS        OFF CACHE INTERNAL "")
set(protobuf_LOCAL_DEPENDENCIES_ONLY  ON  CACHE INTERNAL "")
set(protobuf_FORCE_FETCH_DEPENDENCIES OFF CACHE INTERNAL "")
if (MSVC)
    set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory("${SHADPS4_EXTERNALS_DIR}/protobuf" protobuf EXCLUDE_FROM_ALL)
