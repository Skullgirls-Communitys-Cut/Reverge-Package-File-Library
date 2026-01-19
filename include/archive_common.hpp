#pragma once
#include <bit>
#include <array>
#include <ranges>
#include <type_traits>
#include <algorithm>

namespace RPFL {

    template<std::integral T>
    constexpr T byteswap(T value) noexcept
    {
        static_assert(std::has_unique_object_representations_v<T>,
            "T may not have padding bits");
        auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
        std::ranges::reverse(value_representation);
        return std::bit_cast<T>(value_representation);
    }

    // Определение порядка байтов в файле
    enum class Endianness {
        Little,
        Big,
        Native
    };

    // Вспомогательные функции для записи чисел с учетом порядка байтов
    template<typename T>
        requires std::is_integral_v<T>
    void write_with_endianness(std::byte* dest, T value, Endianness endian) {
        T to_write = value;
        if (endian != Endianness::Native) {
            bool should_swap = (endian == Endianness::Big) != (std::endian::native == std::endian::big);
            if (should_swap) {
                to_write = byteswap(value);
            }
        }
        std::memcpy(dest, &to_write, sizeof(T));
    }

    template<typename T>
        requires std::is_integral_v<T>
    T read_with_endianness(const std::byte* src, Endianness endian) {
        T value;
        std::memcpy(&value, src, sizeof(T));
        if (endian != Endianness::Native) {
            bool should_swap = (endian == Endianness::Big) != (std::endian::native == std::endian::big);
            if (should_swap) {
                value = byteswap(value);
            }
        }
        return value;
    }

} // namespace RPFL