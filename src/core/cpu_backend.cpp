// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "common/arch.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/cpu_backend.h"
#include "core/libraries/kernel/threads/stack_asm.h"
#include "core/linker.h"

#ifdef ENABLE_FEXCORE
#include "core/cpu_backend_fex.h"
#endif

namespace Core {

CpuBackend::~CpuBackend() = default;

namespace {

using ModuleEntryFunc = PS4_SYSV_ABI s32 (*)(u64 args, const void* argp, void* param);

#ifdef ARCH_X86_64

/// Runs guest code directly on the host CPU.
///
/// Valid only because the guest and the host share an instruction set. The guest image is mapped
/// at its own virtual addresses and entered with a plain jump or call, so there is no translation,
/// no register state to marshal, and nothing to intercept.
class NativeCpuBackend final : public CpuBackend {
public:
    std::string_view Name() const override {
        return "native x86-64";
    }

    [[noreturn]] void EnterEntryPoint(EntryParams& params, VAddr exit_func) override {
        // The kernel hands control to a PS4 executable with a particular stack layout, which has
        // to be built by hand because there is no way to express it in C.
        asm volatile("andq $-16, %%rsp\n" // Align to 16 bytes
                     "subq $8, %%rsp\n"   // videoout_basic expects the stack to be misaligned

                     // Kernel also pushes some more things here during process init
                     // at least: environment, auxv, possibly other things

                     "pushq 8(%1)\n" // copy EntryParams to top of stack like the kernel does
                     "pushq 0(%1)\n" // OpenOrbis expects to find it there

                     "movq %1, %%rdi\n" // also pass params and exit func
                     "movq %2, %%rsi\n" // as before

                     "jmp *%0\n" // can't use call here, as that would mangle the prepared stack.
                                 // there's no coming back
                     :
                     : "r"(params.entry_addr), "r"(&params), "r"(exit_func)
                     : "rax", "rsi", "rdi");
        UNREACHABLE();
    }

    s32 CallModuleEntry(VAddr entry, u64 args, const void* argp, void* param) override {
        return reinterpret_cast<ModuleEntryFunc>(entry)(args, argp, param);
    }

    void* CallThreadEntry(VAddr start_routine, void* arg, void* stack) override {
        return _runOnAnotherStack(arg, reinterpret_cast<void*>(start_routine), stack);
    }
};

using ActiveCpuBackend = NativeCpuBackend;

#else

/// Stands in where guest code cannot run.
///
/// Guest code is x86-64, so on any other host it needs translating, and nothing here does that
/// yet. Everything else -- HLE, the GPU, audio, the loader -- is architecture independent and
/// works; this is the piece that is missing, and each method marks a point a real backend has to
/// implement.
class UnsupportedCpuBackend final : public CpuBackend {
public:
    std::string_view Name() const override {
        return "none (guest code cannot execute on this architecture)";
    }

    [[noreturn]] void EnterEntryPoint(EntryParams& params, VAddr exit_func) override {
        UNREACHABLE_MSG("No CPU backend: cannot enter guest code at {:#x}. Guest code is x86-64 "
                        "and this host is not, so it has to be translated.",
                        params.entry_addr);
    }

    s32 CallModuleEntry(VAddr entry, u64 args, const void* argp, void* param) override {
        UNREACHABLE_MSG("No CPU backend: cannot call guest module entry at {:#x}", entry);
    }

    void* CallThreadEntry(VAddr start_routine, void* arg, void* stack) override {
        UNREACHABLE_MSG("No CPU backend: cannot start guest thread at {:#x}", start_routine);
    }
};

using ActiveCpuBackend = UnsupportedCpuBackend;

#endif

} // Anonymous namespace

CpuBackend& Cpu() {
    static const std::unique_ptr<CpuBackend> backend = [] -> std::unique_ptr<CpuBackend> {
#ifdef ENABLE_FEXCORE
        return MakeFexCpuBackend();
#else
        return std::make_unique<ActiveCpuBackend>();
#endif
    }();
    LOG_INFO(Core_Linker, "CPU backend: {}", backend->Name());
    return *backend;
}

} // namespace Core
