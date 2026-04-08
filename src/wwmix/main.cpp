#include "Application.hpp"

#include <filesystem>

#include <string>
#include <vector>

int main(int argc, wchar_t **argv)
{
    std::vector<std::string> utf8Arguments;
    utf8Arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index)
    {
        const std::filesystem::path argument =
            argv[index] != nullptr ? std::filesystem::path(argv[index]) :
                                     std::filesystem::path(L"");
        const std::u8string utf8Argument = argument.u8string();
        utf8Arguments.emplace_back(utf8Argument.begin(), utf8Argument.end());
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
