# SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Nested project that builds the font embedding tool for the machine running the build, used when
# the outer build is cross compiling and therefore cannot execute what it produces. It is
# configured without the cross toolchain file on purpose.

cmake_minimum_required(VERSION 3.24)
project(Dear_ImGui_FontEmbed_Host CXX)

if (NOT FONT_EMBED_SOURCE)
    message(FATAL_ERROR "FONT_EMBED_SOURCE must point at binary_to_compressed_c.cpp")
endif()

add_executable(Dear_ImGui_FontEmbed "${FONT_EMBED_SOURCE}")
