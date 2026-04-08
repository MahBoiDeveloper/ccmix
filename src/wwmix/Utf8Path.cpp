#include "Utf8Path.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Utf8Path
{
namespace
{
#ifdef _WIN32
std::wstring Utf8ToWide(const std::string_view text)
{
    if (text.empty())
    {
        return std::wstring();
    }

    const int wideSize = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (wideSize <= 0)
    {
        return std::wstring();
    }

    std::wstring wideText(static_cast<std::size_t>(wideSize), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        wideText.data(),
        wideSize);
    return wideText;
}

std::string WideToUtf8(const std::wstring_view text)
{
    if (text.empty())
    {
        return std::string();
    }

    const int utf8Size = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (utf8Size <= 0)
    {
        return std::string();
    }

    std::string utf8Text(static_cast<std::size_t>(utf8Size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        utf8Text.data(),
        utf8Size,
        nullptr,
        nullptr);
    return utf8Text;
}
#endif
} // namespace

std::filesystem::path FromUtf8(const std::string_view path)
{
#ifdef _WIN32
    return std::filesystem::path(Utf8ToWide(path));
#else
    return std::filesystem::path(std::string(path));
#endif
}

std::string ToUtf8(const std::filesystem::path &path)
{
#ifdef _WIN32
    return WideToUtf8(path.native());
#else
    return path.string();
#endif
}

std::string Join(const std::string_view left, const std::string_view right)
{
    return ToUtf8(FromUtf8(left) / FromUtf8(right));
}

std::string FileName(const std::string_view path)
{
    return ToUtf8(FromUtf8(path).filename());
}

bool Exists(const std::string_view path)
{
    std::error_code error;
    return std::filesystem::exists(FromUtf8(path), error) && !error;
}

bool Remove(const std::string_view path)
{
    std::error_code error;
    const bool removed = std::filesystem::remove(FromUtf8(path), error);
    return removed && !error;
}

bool Rename(const std::string_view from, const std::string_view to)
{
    std::error_code error;
    std::filesystem::rename(FromUtf8(from), FromUtf8(to), error);
    return !error;
}
} // namespace Utf8Path
