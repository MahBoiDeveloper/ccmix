#include "Application.hpp"

#ifdef _WIN32
#include <windows.h>

#include <string>
#include <string_view>
#include <vector>

namespace
{
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

void InitializeUtf8Console()
{
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
}
} // namespace

int wmain(int argc, wchar_t **argv)
{
    InitializeUtf8Console();

    std::vector<std::string> utf8Arguments;
    utf8Arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index)
    {
        const std::wstring_view argument =
            argv[index] != nullptr ? std::wstring_view(argv[index]) :
                                     std::wstring_view();
        utf8Arguments.push_back(WideToUtf8(argument));
    }

    std::vector<char *> argumentPointers;
    argumentPointers.reserve(utf8Arguments.size());
    for (std::string &argument : utf8Arguments)
    {
        argumentPointers.push_back(argument.data());
    }

    WwMix::Application application;
    return application.Run(
        static_cast<int>(argumentPointers.size()),
        argumentPointers.data());
}
#else
int main(int argc, char **argv)
{
    WwMix::Application application;
    return application.Run(argc, argv);
}
#endif
