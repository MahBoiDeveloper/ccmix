#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace Utf8Path
{
/// @brief Convert a UTF-8 path string into a filesystem path.
std::filesystem::path FromUtf8(std::string_view path);

/// @brief Convert a filesystem path into a UTF-8 string.
std::string ToUtf8(const std::filesystem::path &path);

/// @brief Join two UTF-8 path components.
std::string Join(std::string_view left, std::string_view right);

/// @brief Return the filename component of a UTF-8 path.
std::string FileName(std::string_view path);

/// @brief Return whether a UTF-8 path exists.
bool Exists(std::string_view path);

/// @brief Remove a file at a UTF-8 path.
bool Remove(std::string_view path);

/// @brief Rename a file between UTF-8 paths.
bool Rename(std::string_view from, std::string_view to);

template <typename Stream>
void Open(Stream &stream, std::string_view path, std::ios::openmode mode)
{
    stream.open(FromUtf8(path), mode);
}
} // namespace Utf8Path
