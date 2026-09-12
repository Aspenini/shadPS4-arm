// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/arch.h"
#include "common/signal_context.h"

#ifdef _WIN32
#include <windows.h>
#elif defined(__FreeBSD__)
#include <machine/npx.h>
#include <sys/ucontext.h>
#else
#include <sys/ucontext.h>
#endif

#if !defined(_WIN32) && !defined(__APPLE__) && defined(ARCH_ARM64)
// Linux and Android expose the fault's ESR through a tagged record chain stored in the
// mcontext's reserved area, rather than as a named field.
#include <asm/sigcontext.h>
#endif

namespace Common {

#if !defined(_WIN32) && !defined(__APPLE__) && defined(ARCH_ARM64)
namespace {
/// Walks the _aarch64_ctx record chain in uc_mcontext.__reserved looking for the ESR record.
/// Returns false when the kernel did not provide one.
bool TryGetEsr(const ucontext_t* ctx, u64& esr) {
    // glibc types __reserved as unsigned char[4096], bionic as __uint128_t[256]. Walk it as bytes
    // so the stride is correct on both.
    const auto* base = reinterpret_cast<const unsigned char*>(ctx->uc_mcontext.__reserved);
    size_t offset = 0;
    while (offset + sizeof(_aarch64_ctx) <= sizeof(ctx->uc_mcontext.__reserved)) {
        const auto* head = reinterpret_cast<const _aarch64_ctx*>(base + offset);
        if (head->magic == 0 || head->size == 0) {
            return false;
        }
        if (head->magic == ESR_MAGIC) {
            esr = reinterpret_cast<const esr_context*>(head)->esr;
            return true;
        }
        offset += head->size;
    }
    return false;
}
} // Anonymous namespace
#endif

void* GetRip(void* ctx) {
#if defined(_WIN32) && defined(ARCH_X86_64)
    return (void*)((EXCEPTION_POINTERS*)ctx)->ContextRecord->Rip;
#elif defined(_WIN32) && defined(ARCH_ARM64)
    return (void*)((EXCEPTION_POINTERS*)ctx)->ContextRecord->Pc;
#elif defined(__APPLE__) && defined(ARCH_X86_64)
    return (void*)((ucontext_t*)ctx)->uc_mcontext->__ss.__rip;
#elif defined(__APPLE__) && defined(ARCH_ARM64)
    return (void*)((ucontext_t*)ctx)->uc_mcontext->__ss.__pc;
#elif defined(__FreeBSD__)
    return (void*)((ucontext_t*)ctx)->uc_mcontext.mc_rip;
#elif defined(ARCH_X86_64)
    return (void*)((ucontext_t*)ctx)->uc_mcontext.gregs[REG_RIP];
#elif defined(ARCH_ARM64)
    return (void*)((ucontext_t*)ctx)->uc_mcontext.pc;
#else
#error "Unsupported architecture"
#endif
}

bool IsWriteError(void* ctx) {
#if defined(_WIN32)
    return ((EXCEPTION_POINTERS*)ctx)->ExceptionRecord->ExceptionInformation[0] == 1;
#elif defined(__APPLE__) && defined(ARCH_X86_64)
    return ((ucontext_t*)ctx)->uc_mcontext->__es.__err & 0x2;
#elif defined(__APPLE__) && defined(ARCH_ARM64)
    return ((ucontext_t*)ctx)->uc_mcontext->__es.__esr & 0x40;
#elif defined(__FreeBSD__) && defined(ARCH_X86_64)
    return ((ucontext_t*)ctx)->uc_mcontext.mc_err & 0x2;
#elif defined(ARCH_X86_64)
    return ((ucontext_t*)ctx)->uc_mcontext.gregs[REG_ERR] & 0x2;
#elif defined(ARCH_ARM64)
    // ESR_ELx.WnR (bit 6) distinguishes a write from a read for data aborts.
    u64 esr = 0;
    return TryGetEsr((const ucontext_t*)ctx, esr) && (esr & 0x40) != 0;
#else
#error "Unsupported architecture"
#endif
}

} // namespace Common
