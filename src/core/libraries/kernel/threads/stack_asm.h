// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/arch.h"
#include "common/types.h"

// Switches to `stackb` and calls `func(arg)` on it, restoring the previous stack afterwards.
// Implemented in assembly per architecture in stack_asm.cpp.
//
// This is a host-side stack switch. Where `func` is guest code the call belongs behind
// Core::CpuBackend instead, which decides what entering guest code means for the target.
#if defined(ARCH_X86_64) || defined(__arm64__) || defined(__aarch64__)
extern "C" void* PS4_SYSV_ABI _runOnAnotherStack(void* arg, void* func,
                                                 void* stackb) asm("_runOnAnotherStack");
#else
inline void* PS4_SYSV_ABI _runOnAnotherStack(void* arg, void* func, void* stackb) {
    UNREACHABLE_MSG("_runOnAnotherStack not implemented on target architecture.");
}
#endif
