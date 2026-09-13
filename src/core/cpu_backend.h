// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"

namespace Core {

struct EntryParams;

/// Executes guest PS4 x86-64 code.
///
/// shadPS4 has historically had no CPU emulation at all: guest code is mapped into this process
/// and jumped into, so it only runs where the host is itself x86-64. Every place that hands
/// control to the guest goes through this interface, so that a backend which translates guest
/// instructions has one seam to implement rather than a scattering of inline assembly.
///
/// Guest code is entered rarely -- process entry, module initialisers, thread starts -- so the
/// virtual dispatch here costs nothing measurable. Calls *out* of the guest into HLE are a
/// different problem, handled at the HOST_CALL boundary in tls.h.
class CpuBackend {
public:
    virtual ~CpuBackend();

    /// Name of this backend, for logging.
    virtual std::string_view Name() const = 0;

    /// Transfers control to the guest program's entry point. Never returns; the guest leaves
    /// through the exit function or by terminating the process.
    [[noreturn]] virtual void EnterEntryPoint(EntryParams& params, VAddr exit_func) = 0;

    /// Calls a guest module's initialisation function and returns its result.
    virtual s32 CallModuleEntry(VAddr entry, u64 args, const void* argp, void* param) = 0;

    /// Calls a guest thread's start routine on `stack`, returning the value it produced. `stack`
    /// points at the top of the guest stack the thread was created with.
    virtual void* CallThreadEntry(VAddr start_routine, void* arg, void* stack) = 0;
};

/// The active backend. Chosen once during startup and stable thereafter.
CpuBackend& Cpu();

} // namespace Core
