# SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Provides the `ffmpeg` target for Android, standing in for externals/ffmpeg-core.
#
# ext-ffmpeg-core downloads prebuilt archives and selects them by OS (WIN32 / APPLE / UNIX).
# Android matches UNIX, so it picks the glibc Linux build and the link fails on bionic with
# undefined __errno_location, __isoc99_sscanf, __xpg_strerror_r, __fprintf_chk and fcntl64.
# No Android archive is published, so build one from source with the NDK.
#
# Point SHADPS4_FFMPEG_ANDROID_PREFIX at an existing install to skip the build entirely; CI is
# expected to do that rather than rebuilding FFmpeg for every job.

# Must match the headers ext-ffmpeg-core ships, so that both platforms expose the same API.
set(FFMPEG_ANDROID_VERSION "7.1" CACHE STRING "FFmpeg version to build for Android")

set(FFMPEG_LIBS avfilter avformat avcodec swscale swresample avutil)

if (SHADPS4_FFMPEG_ANDROID_PREFIX)
    set(FFMPEG_PREFIX "${SHADPS4_FFMPEG_ANDROID_PREFIX}")
    add_custom_target(ffmpeg-android-build)
else()
    find_program(BASH_PROGRAM bash)
    if (NOT BASH_PROGRAM)
        message(FATAL_ERROR
            "Building FFmpeg for Android needs bash to run its configure script. Install one (Git "
            "for Windows or MSYS2 provide it), or set SHADPS4_FFMPEG_ANDROID_PREFIX to an existing "
            "FFmpeg install built for Android.")
    endif()

    set(FFMPEG_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install")

    # The NDK's toolchain file hardcodes CMAKE_SYSTEM_VERSION to 1, so the API level has to come
    # from the Android specific variables instead.
    if (DEFINED ANDROID_PLATFORM_LEVEL)
        set(FFMPEG_ANDROID_API "${ANDROID_PLATFORM_LEVEL}")
    elseif (DEFINED ANDROID_NATIVE_API_LEVEL)
        set(FFMPEG_ANDROID_API "${ANDROID_NATIVE_API_LEVEL}")
    elseif (DEFINED ANDROID_PLATFORM)
        string(REGEX REPLACE "^android-" "" FFMPEG_ANDROID_API "${ANDROID_PLATFORM}")
    else()
        message(FATAL_ERROR "Could not determine the Android API level to build FFmpeg against")
    endif()
    if (NOT FFMPEG_ANDROID_API MATCHES "^[0-9]+$")
        message(FATAL_ERROR "Unexpected Android API level '${FFMPEG_ANDROID_API}'")
    endif()

    set(FFMPEG_BYPRODUCTS "")
    foreach (lib IN LISTS FFMPEG_LIBS)
        list(APPEND FFMPEG_BYPRODUCTS "${FFMPEG_PREFIX}/lib/lib${lib}.a")
    endforeach()

    include(ExternalProject)
    ExternalProject_Add(ffmpeg-android-build
        DOWNLOAD_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            "${BASH_PROGRAM}" "${CMAKE_CURRENT_SOURCE_DIR}/build-ffmpeg.sh"
            --ndk "${CMAKE_ANDROID_NDK}"
            --abi "${CMAKE_ANDROID_ARCH_ABI}"
            --api "${FFMPEG_ANDROID_API}"
            --version "${FFMPEG_ANDROID_VERSION}"
            --prefix "${FFMPEG_PREFIX}"
        BUILD_IN_SOURCE 1
        BUILD_BYPRODUCTS ${FFMPEG_BYPRODUCTS}
        INSTALL_COMMAND ""
        USES_TERMINAL_BUILD 1
    )
endif()

add_library(ffmpeg INTERFACE)
add_dependencies(ffmpeg ffmpeg-android-build)

# The install tree's headers are guaranteed to match the libraries next to them, which the ones
# vendored in ext-ffmpeg-core would only be by convention.
target_include_directories(ffmpeg INTERFACE "${FFMPEG_PREFIX}/include")

foreach (lib IN LISTS FFMPEG_LIBS)
    target_link_libraries(ffmpeg INTERFACE "${FFMPEG_PREFIX}/lib/lib${lib}.a")
endforeach()
