// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Skeleton of a Core::CpuBackend backed by FEXCore, FEX's x86-64 to AArch64 translation library.
//
// This is NOT a working backend. It exists so that FEXCore is linked, built against and kept
// compiling as shadPS4 changes, and so that what remains is written down against the real API
// rather than guessed at. Every entry point still reports what is missing rather than running
// anything.
//
// What a working implementation needs, in rough order of difficulty:
//
//   1. Host feature detection. FEXCore::Context::Context::CreateNewContext takes a
//      FEXCore::HostFeatures describing the CPU it is generating code for, and gets that wrong
//      silently -- a default constructed one describes a CPU with no features at all. FEX detects
//      this in FetchHostFeatures() in Source/Common/HostFeatures.cpp, which is part of the
//      FEXLoader frontend rather than FEXCore, and which pulls in FEX's own configuration system.
//      Either that file gets built alongside FEXCore or the detection is reimplemented here.
//
//   2. FEXCore::HLE::SyscallHandler. Guest PS4 code does not issue Linux syscalls -- it calls into
//      HLE -- but the interface is mandatory, and QueryGuestExecutableRange and
//      LookupExecutableFileSection have to answer honestly about shadPS4's guest mappings for
//      code invalidation to work.
//
//   3. FEXCore::SignalDelegator. shadPS4 already owns SIGSEGV for GPU write tracking
//      (video_core/page_manager.cpp) and for the SRT walker. Both want the same signal, so the
//      two dispatchers have to be reconciled rather than both installing handlers.
//
//   4. FEXCore::ThunkHandler -- the hard one. Guest code reaches HLE through addresses written
//      into its PLT/GOT, which today are host function pointers. Under translation those have to
//      become addresses the JIT recognises and turns back into a native call. The saving grace is
//      that every one of the 5280 exports passes through HOST_CALL in core/tls.h with its full
//      C++ signature intact, so the marshalling can be generated from types rather than written
//      by hand the way FEX's own thunks are.
//
//   5. Memory. shadPS4 maps guest memory itself, at fixed guest addresses
//      (core/address_space.cpp), and FEXCore expects to be told about guest code ranges. The two
//      views have to agree before any of the above matters.

#include <FEXCore/Core/Context.h>
#include <FEXCore/Core/CoreState.h>
#include <FEXCore/Core/HostFeatures.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/cpu_backend.h"
#include "core/cpu_backend_fex.h"
#include "core/linker.h"

namespace Core {

namespace {

class FexCpuBackend final : public CpuBackend {
public:
    std::string_view Name() const override {
        return "FEXCore (skeleton, cannot execute guest code yet)";
    }

    [[noreturn]] void EnterEntryPoint(EntryParams& params, VAddr exit_func) override {
        Unimplemented("enter the guest entry point at {:#x}", params.entry_addr);
    }

    s32 CallModuleEntry(VAddr entry, u64 args, const void* argp, void* param) override {
        Unimplemented("call the guest module entry at {:#x}", entry);
    }

    void* CallThreadEntry(VAddr start_routine, void* arg, void* stack) override {
        Unimplemented("start a guest thread at {:#x}", start_routine);
    }

private:
    template <typename... Args>
    [[noreturn]] static void Unimplemented(fmt::format_string<Args...> what, Args&&... args) {
        UNREACHABLE_MSG("FEXCore backend cannot {} yet: it has no host feature detection, syscall "
                        "handler, signal delegator or thunk handler. See the comment at the top of "
                        "core/cpu_backend_fex.cpp.",
                        fmt::format(what, std::forward<Args>(args)...));
    }
};

} // Anonymous namespace

// Exists so that the link actually pulls libFEXCore.a in, and problems there -- duplicate symbols
// between the two vendored copies of fmt, say -- surface now rather than when the backend is
// finished. It must have external linkage and must not be constexpr: anything the optimiser can
// fold away takes the undefined reference with it, and then the archive member is never needed.
//
// Deliberately never called. A context built from default constructed HostFeatures would generate
// code for a CPU with no features at all.
extern "C" void* shadps4_fexcore_linkage_probe() {
    return reinterpret_cast<void*>(&FEXCore::Context::Context::CreateNewContext);
}

std::unique_ptr<CpuBackend> MakeFexCpuBackend() {
    LOG_WARNING(Core_Linker, "FEXCore backend selected, but it is a skeleton and cannot run "
                             "guest code. Anything that enters the guest will abort.");
    return std::make_unique<FexCpuBackend>();
}

} // namespace Core
