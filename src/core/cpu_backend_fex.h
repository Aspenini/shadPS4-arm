// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

namespace Core {

class CpuBackend;

/// Creates the FEXCore backed backend. Only available when built with ENABLE_FEXCORE; see
/// cpu_backend_fex.cpp for what it does and does not do.
std::unique_ptr<CpuBackend> MakeFexCpuBackend();

} // namespace Core
