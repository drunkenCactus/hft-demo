#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace hft {

enum class TraderId : std::uint8_t {
    kBtcUsdt = 0,
    kEthUsdt = 1,
};

inline constexpr auto kTraderIds = std::array<TraderId, 2>{
    TraderId::kBtcUsdt,
    TraderId::kEthUsdt,
};

template<typename T>
class ArrayByTraderId {
public:
    constexpr T& operator[](TraderId id) noexcept {
        return elems_[StorageIndex(id)];
    }

    constexpr const T& operator[](TraderId id) const noexcept {
        return elems_[StorageIndex(id)];
    }

private:
    static constexpr std::size_t StorageIndex(TraderId id) noexcept {
        return static_cast<std::size_t>(std::to_underlying(id));
    }

private:
    std::array<T, kTraderIds.size()> elems_{};

    static_assert(
        std::is_object_v<T>,
        "T must be a complete object type"
    );
    static_assert(
        std::is_same_v<T, std::remove_cv_t<T>>,
        "T must not be const- or volatile-qualified"
    );
    static_assert(
        std::is_default_constructible_v<T>,
        "T must be default constructible"
    );
};

}  // namespace hft
