#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace MixNumeric
{
/// @brief Convert a value to uint32_t and reject anything outside the MIX format range.
template <typename T>
uint32_t ToUint32(T value, std::string_view context)
{
    if (!std::in_range<uint32_t>(value))
    {
        throw std::overflow_error(std::string(context) +
                                  " exceeds the 32-bit MIX format limit");
    }

    return static_cast<uint32_t>(value);
}
} // namespace MixNumeric
