#pragma once

#include <cstdint>
#include <string>

enum class Game
{
    TD,
    RA,
    TS,
    D2,
    D2K,
    RA2
};

constexpr bool UsesClassicMixIds(const Game game)
{
    return game == Game::TD || game == Game::RA;
}

constexpr bool SupportsExtendedMixHeaders(const Game game)
{
    return game != Game::TD;
}

namespace MixId
{
/// @brief Generate a MIX file ID for a name in the selected game's format.
int32_t IdGen(Game game, const std::string &fname);

/// @brief Format a numeric MIX ID as eight hexadecimal digits.
std::string IdStr(int32_t id);

/// @brief Format a raw byte sequence as a hexadecimal string.
std::string IdStr(const char *id, uint32_t size);

/// @brief Parse a hexadecimal string into a numeric MIX ID.
int32_t StrId(const std::string &hex);

/// @brief Return whether a file name already uses the explicit [id] form.
bool IsIdName(const std::string &fname);
} // namespace MixId
