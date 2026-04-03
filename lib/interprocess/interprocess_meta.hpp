#pragma once

#include <cstdint>

namespace hft {

// Update kIpcVersion if any file in /lib/interprocess/... changes
constexpr uint32_t kIpcVersion = 19;
constexpr uint32_t kIpcMagic = 547892;

}  // namespace hft
