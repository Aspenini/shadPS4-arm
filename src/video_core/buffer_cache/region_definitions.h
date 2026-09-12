// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/bit_array.h"
#include "common/types.h"

namespace VideoCore {

#ifdef __ANDROID__
// Android ships 16 KB pages from API 35 onwards, and mprotect cannot act at a finer granularity
// than the host page size. Tracking at 4 KB on such a device silently rounds each protect up to
// 16 KB, so un-protecting after a fault also clears protection on neighbouring pages that still
// have watchers -- the symptom is missed GPU invalidations, not an obvious page-size error.
// The PS4 guest page size is 16 KB anyway, so this also lines guest, host and tracking
// granularity up 1:1. Coarser than necessary on a 4 KB device, but correct on both.
constexpr u64 TRACKER_PAGE_BITS = 14; // 16K pages
#else
constexpr u64 TRACKER_PAGE_BITS = 12; // 4K pages
#endif
constexpr u64 TRACKER_BYTES_PER_PAGE = 1ULL << TRACKER_PAGE_BITS;

constexpr u64 TRACKER_HIGHER_PAGE_BITS = 22; // each region is 4MB
constexpr u64 TRACKER_HIGHER_PAGE_SIZE = 1ULL << TRACKER_HIGHER_PAGE_BITS;
constexpr u64 TRACKER_HIGHER_PAGE_MASK = TRACKER_HIGHER_PAGE_SIZE - 1ULL;
constexpr u64 NUM_PAGES_PER_REGION = TRACKER_HIGHER_PAGE_SIZE / TRACKER_BYTES_PER_PAGE;

enum class Type {
    CPU,
    GPU,
};

using RegionBits = Common::BitArray<NUM_PAGES_PER_REGION>;

} // namespace VideoCore
