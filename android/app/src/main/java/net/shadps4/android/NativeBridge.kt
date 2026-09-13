// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package net.shadps4.android

/**
 * The two native libraries are kept separate on purpose.
 *
 * libshadps4_probe.so links none of the emulator, so it reports what the device can do even when
 * libshadps4.so cannot be loaded -- which is the case its answers are most needed for. Loading
 * the emulator runs every static initialiser in it, and an abort there takes the process with it.
 */
object NativeBridge {
    /** Loads the standalone probe. Should never fail; if it does, the app itself is broken. */
    fun loadProbe(): Result<Unit> = runCatching { System.loadLibrary("shadps4_probe") }

    /** Loads the emulator. Failing here is a real result, not an error to hide. */
    fun loadEmulator(): Result<Unit> = runCatching { System.loadLibrary("shadps4") }

    /** Host page size, reservable address space and Vulkan capability. From probe.cpp. */
    @JvmStatic
    external fun deviceReport(): String

    /**
     * Proves the emulator library loaded and that code inside it runs. From jni.cpp.
     *
     * @param userDir writable directory the emulator should treat as its user directory
     */
    @JvmStatic
    external fun emulatorInfo(userDir: String): String
}
