// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Entry points that live inside libshadps4.so itself.
//
// Everything that merely measures the device lives in probe.cpp instead, which links none of
// shadPS4, so that a failure to load this library can still be reported. What belongs here is
// only what proves this library loaded and that code inside it runs.

#include <string>
#include <jni.h>

#include "common/path_util.h"
#include "common/scm_rev.h"
#include "core/cpu_backend.h"

extern "C" JNIEXPORT jstring JNICALL
Java_net_shadps4_android_NativeBridge_emulatorInfo(JNIEnv* env, jclass, jstring user_dir) {
    const char* dir = env->GetStringUTFChars(user_dir, nullptr);
    Common::FS::SetUserDirectory(dir);
    env->ReleaseStringUTFChars(user_dir, dir);

    std::string out;
    out += std::string("build:   ") + Common::g_scm_desc + "\n";
    out += std::string("branch:  ") + Common::g_scm_branch + "\n";
    // Reaching this means static initialisation completed and the user directory resolved.
    out += std::string("userdir: ") + Common::FS::GetUserPathString(Common::FS::PathType::UserDir) +
           "\n";
    out += std::string("cpu:     ") + std::string(Core::Cpu().Name()) + "\n";
    return env->NewStringUTF(out.c_str());
}
