// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package net.shadps4.android

/** Entry points into libshadps4.so. Implemented in src/android/jni.cpp. */
object NativeBridge {
    /**
     * Loads the emulator library and reports what this device can do.
     *
     * Loading is the interesting half: libshadps4.so is the whole emulator, so if this throws,
     * the native port does not work on this device at all.
     */
    fun load(): Result<Unit> = runCatching { System.loadLibrary("shadps4") }

    /**
     * Measures the things the Android port depends on and cannot be determined off-device:
     * host page size, reservable address space, and Vulkan capability.
     *
     * @param userDir writable directory the emulator should treat as its user directory
     */
    @JvmStatic
    external fun deviceReport(userDir: String): String
}
