# SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Builds FEXCore, the x86-64 to AArch64 translation library from FEX, and exposes it as `fexcore`.
#
# FEXCore is the piece that could execute guest PS4 code on ARM. shadPS4 runs guest code natively
# today, which only works when the host is x86-64; Core::CpuBackend is the seam a translator plugs
# into, and this provides the translator to plug in.
#
# Built through ExternalProject rather than add_subdirectory for two reasons: FEX's top level
# CMakeLists also defines the FEXLoader frontend and its tooling, none of which are wanted here,
# and its build needs a patch applied to the submodule first.

find_program(BASH_PROGRAM bash)
if (NOT BASH_PROGRAM)
    message(FATAL_ERROR "Building FEXCore needs bash to prepare the FEX submodule.")
endif()

set(FEX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../fex")
if (NOT EXISTS "${FEX_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "The FEX submodule is not checked out. Run:\n"
        "  git submodule update --init externals/fex")
endif()

set(FEXCORE_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/fex-build")
set(FEXCORE_LIBRARY "${FEXCORE_BUILD_DIR}/FEXCore/Source/libFEXCore.a")

# Runs on every configure; the script is idempotent.
execute_process(
    COMMAND "${BASH_PROGRAM}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare-fex.sh"
            --fex-dir "${FEX_SOURCE_DIR}"
            --patch "${CMAKE_CURRENT_SOURCE_DIR}/fex-android-cross.patch"
    RESULT_VARIABLE PREPARE_RESULT
    OUTPUT_VARIABLE PREPARE_OUTPUT
    ERROR_VARIABLE PREPARE_OUTPUT
)
if (NOT PREPARE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to prepare the FEX submodule:\n${PREPARE_OUTPUT}")
endif()
message(STATUS "FEXCore: ${PREPARE_OUTPUT}")

include(ExternalProject)
ExternalProject_Add(fexcore-build
    SOURCE_DIR "${FEX_SOURCE_DIR}"
    BINARY_DIR "${FEXCORE_BUILD_DIR}"
    DOWNLOAD_COMMAND ""
    CMAKE_ARGS
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        "-DANDROID_ABI=${CMAKE_ANDROID_ARCH_ABI}"
        "-DANDROID_PLATFORM=${ANDROID_PLATFORM}"
        "-DCMAKE_BUILD_TYPE=Release"
        # Everything below trims the build down to the library. The frontend, its configuration
        # tool and the thunk generator are Linux desktop pieces that shadPS4 supplies itself.
        "-DBUILD_FEXCONFIG=False"
        "-DBUILD_THUNKS=False"
        "-DBUILD_FEX_LINUX_TESTS=False"
        "-DENABLE_LTO=False"
        "-DENABLE_CCACHE=False"
        # jemalloc's glibc allocator hooks have nothing to hook on bionic.
        "-DENABLE_JEMALLOC_GLIBC_ALLOC=False"
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target FEXCore --config Release
    BUILD_BYPRODUCTS "${FEXCORE_LIBRARY}"
    INSTALL_COMMAND ""
    USES_TERMINAL_BUILD 1
)

# NOTE, before anything actually links this: shadPS4 and FEX each vendor their own fmt, and at
# the time of writing they disagree -- 12.1.1 here against 12.2.1 there. Both mangle into the same
# fmt::v12 inline namespace, so linking both static libraries would leave the linker to pick one
# definition for symbols the other also defines. FEX does find_package(fmt QUIET) before falling
# back to its bundle, so the fix is either to align the two submodules or to hand FEX an installed
# fmt through fmt_DIR. Nothing references FEXCore yet, so this is latent rather than broken.

add_library(fexcore INTERFACE)
add_dependencies(fexcore fexcore-build)
target_include_directories(fexcore INTERFACE
    "${FEX_SOURCE_DIR}/FEXCore/include"
    "${FEX_SOURCE_DIR}/Source"
)
target_link_libraries(fexcore INTERFACE "${FEXCORE_LIBRARY}")
