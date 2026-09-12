# SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

project(ImGui_Resources)

set(FONT_EMBED_SOURCE ${CMAKE_SOURCE_DIR}/externals/imgui/misc/fonts/binary_to_compressed_c.cpp)

# Dear_ImGui_FontEmbed turns .ttf files into C arrays at build time, so it runs on the machine
# doing the build. When cross compiling it therefore cannot be an ordinary target: that would
# produce a binary for the target architecture, which the build machine cannot execute.
if (CMAKE_CROSSCOMPILING)
    if (IMGUI_FONT_EMBED_EXECUTABLE)
        # Caller supplied a host binary; just use it.
        add_custom_target(Dear_ImGui_FontEmbed)
        set(FONT_EMBED_COMMAND "${IMGUI_FONT_EMBED_EXECUTABLE}")
    else()
        # Build one in a nested project that deliberately does not inherit the cross toolchain.
        include(ExternalProject)
        if (CMAKE_HOST_WIN32)
            set(FONT_EMBED_SUFFIX ".exe")
        else()
            set(FONT_EMBED_SUFFIX "")
        endif()
        set(FONT_EMBED_HOST_DIR "${CMAKE_CURRENT_BINARY_DIR}/font_embed_host")
        set(FONT_EMBED_COMMAND "${FONT_EMBED_HOST_DIR}/bin/Dear_ImGui_FontEmbed${FONT_EMBED_SUFFIX}")
        ExternalProject_Add(Dear_ImGui_FontEmbed
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/font_embed_host"
            BINARY_DIR "${FONT_EMBED_HOST_DIR}"
            CMAKE_ARGS
                "-DCMAKE_BUILD_TYPE=Release"
                "-DFONT_EMBED_SOURCE=${FONT_EMBED_SOURCE}"
                "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${FONT_EMBED_HOST_DIR}/bin"
                "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${FONT_EMBED_HOST_DIR}/bin"
            BUILD_BYPRODUCTS "${FONT_EMBED_COMMAND}"
            INSTALL_COMMAND ""
        )
    endif()
else()
    add_executable(Dear_ImGui_FontEmbed ${FONT_EMBED_SOURCE})
    set(FONT_EMBED_COMMAND "$<TARGET_FILE:Dear_ImGui_FontEmbed>")
endif()

set(FONT_LIST
    NotoSansArabic-Regular.ttf
    NotoSansCJK-Regular.ttc
    NotoSans-Regular.ttf
    NotoSansSymbols2-Regular.ttf
    NotoSansThai-Regular.ttf
    ProggyVector-Regular.ttf
)

set(OutputList "")
FOREACH (FONT_FILE ${FONT_LIST})
    string(REGEX REPLACE "-" "_" fontname ${FONT_FILE})
    string(TOLOWER ${fontname} fontname)
    string(REGEX REPLACE "\\.(ttf|otf|ttc)$" "" fontname_cpp ${fontname})
    set(fontname_cpp "imgui_font_${fontname_cpp}")

    MESSAGE(STATUS "Embedding font ${FONT_FILE}")
    set(OUTPUT "generated_fonts/imgui_fonts/${fontname}")
    add_custom_command(
        OUTPUT "${OUTPUT}.g.cpp"
        COMMAND ${CMAKE_COMMAND} -E make_directory "generated_fonts/imgui_fonts"
        COMMAND "${FONT_EMBED_COMMAND}" -nostatic "${CMAKE_CURRENT_SOURCE_DIR}/fonts/${FONT_FILE}" ${fontname_cpp} > "${OUTPUT}.g.cpp"
        DEPENDS Dear_ImGui_FontEmbed "fonts/${FONT_FILE}"
        USES_TERMINAL
    )
    list(APPEND OutputList "${OUTPUT}.g.cpp")
ENDFOREACH ()

add_library(ImGui_Resources STATIC ${OutputList})
set(IMGUI_RESOURCES_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/generated_fonts PARENT_SCOPE)
