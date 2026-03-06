#pragma once

#include <cstdint>

namespace hft {

// Update version if any file in /lib/interprocess/... changes
constexpr uint32_t IPC_VERSION = 4;
constexpr uint32_t IPC_MAGIC = 547892;

}  // namespace hft
