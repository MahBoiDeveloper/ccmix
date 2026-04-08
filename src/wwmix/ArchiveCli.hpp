#pragma once

#include <span>
#include <string_view>

namespace WwMix
{
/// @brief Parse, describe, and execute archive commands.
class ArchiveCli
{
  public:
    /// @brief Return whether a token looks like an archive switch.
    static bool IsSwitchToken(std::string_view token);

    /// @brief Return whether a token names an archive command.
    static bool IsCommandToken(std::string_view token);

    /// @brief Print the short 7-Zip-style archive usage summary.
    static void ShowUsage(std::string_view programName);

    /// @brief Print the full archive help text.
    static void ShowHelp(std::string_view programName);

    /// @brief Parse and execute archive commands from a 7-Zip-style argument list.
    static int Run(std::string_view programPath, std::string_view displayName,
                   std::span<char *> arguments);
};
} // namespace WwMix
