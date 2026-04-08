/*
 * File:   ccmix.cpp
 * Author: fbsagr
 *
 * Created on April 10, 2014, 1:43 PM
 */

#include "mix_file.hpp"

#include <array>
#include <charconv>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif

#ifdef _WIN32
constexpr char DirSeparator = '\\';
#else
constexpr char DirSeparator = '/';
#endif

enum class OptionId {
    Help,
    Extract,
    Create,
    Game,
    File,
    Directory,
    List,
    Mix,
    Id,
    LocalDb,
    Encrypt,
    Checksum,
    Info,
    Add,
    Remove
};

enum class MixMode {
    None,
    Extract,
    Create,
    Add,
    Remove,
    List,
    Info
};

enum class ArgumentRequirement {
    None,
    Required
};

struct OptionSpec {
    OptionId Id;
    std::string_view Name;
    ArgumentRequirement Requirement;
};

struct ParsedOption {
    OptionId Id;
    std::string_view Name;
    std::optional<std::string_view> Value;
};

struct CommandLineOptions {
    uint32_t FileId = 0;
    std::string FileName;
    std::string Directory;
    std::string InputMixFile;
    Game GameType = GameTd;
    MixMode Mode = MixMode::None;
    bool CreateLocalDb = false;
    bool EncryptHeader = false;
    bool AddChecksum = false;
    bool ShowedHelp = false;
};

namespace
{
    constexpr auto GameNames = std::to_array<std::string_view>({
        "td",
        "ra",
        "ts",
        "ra2"
    });

    constexpr auto OptionSpecs = std::to_array<OptionSpec>({
        { OptionId::Extract,   "--extract",   ArgumentRequirement::None     },
        { OptionId::Create,    "--create",    ArgumentRequirement::None     },
        { OptionId::Add,       "--add",       ArgumentRequirement::None     },
        { OptionId::Remove,    "--remove",    ArgumentRequirement::None     },
        { OptionId::List,      "--list",      ArgumentRequirement::None     },
        { OptionId::Info,      "--info",      ArgumentRequirement::None     },
        { OptionId::LocalDb,   "--lmd",       ArgumentRequirement::None     },
        { OptionId::Encrypt,   "--encrypt",   ArgumentRequirement::None     },
        { OptionId::Checksum,  "--checksum",  ArgumentRequirement::None     },
        { OptionId::File,      "--file",      ArgumentRequirement::Required },
        { OptionId::Id,        "--id",        ArgumentRequirement::Required },
        { OptionId::Directory, "--directory", ArgumentRequirement::Required },
        { OptionId::Mix,       "--mix",       ArgumentRequirement::Required },
        { OptionId::Game,      "--game",      ArgumentRequirement::Required },
        { OptionId::Help,      "-?",          ArgumentRequirement::None     },
        { OptionId::Help,      "--help",      ArgumentRequirement::None     }
    });

    [[nodiscard]] bool FileExists(const std::string& path)
    {
        std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
        return file.is_open();
    }

#ifdef _WIN32
    [[nodiscard]] std::optional<std::string> GetEnvironmentValue(const char* name)
    {
        char* value = NULL;
        std::size_t length = 0;
        const errno_t error = _dupenv_s(&value, &length, name);
        if (error != 0 || value == NULL || length == 0) {
            std::free(value);
            return std::nullopt;
        }

        std::string result(value);
        std::free(value);
        return result;
    }
#endif

    [[nodiscard]] bool IsOptionToken(const std::string_view token)
    {
        return token.size() > 1 && token.front() == '-';
    }

    [[nodiscard]] const OptionSpec* FindOptionSpec(const std::string_view token)
    {
        for (const OptionSpec& spec : OptionSpecs) {
            if (spec.Name == token) {
                return &spec;
            }
        }

        return NULL;
    }

    [[nodiscard]] std::optional<ParsedOption> ParseOptionToken(
            const std::string_view token)
    {
        const std::string_view::size_type separator = token.find('=');
        const std::string_view optionName =
                separator == std::string_view::npos ? token : token.substr(0, separator);
        const OptionSpec* spec = FindOptionSpec(optionName);

        if (spec == NULL) {
            return std::nullopt;
        }

        ParsedOption parsedOption = { spec->Id, spec->Name, std::nullopt };
        if (separator != std::string_view::npos) {
            parsedOption.Value = token.substr(separator + 1);
        }

        return parsedOption;
    }
}

/// @brief Get the directory that contains the program executable.
static std::string GetProgramDir(const std::string_view programLocation)
{
    const std::string_view::size_type separator = programLocation.find_last_of("\\/");
    if (separator == std::string_view::npos) {
        return ".";
    }

    return std::string(programLocation.substr(0, separator));
}

/// @brief Search a few locations for the global mix database.
/// TODO copy gmd if found but not in a home dir config.
std::string FindGmd(const std::string& programDir, const std::string& homeDir)
{
    const std::string gmdLocation = "global mix database.dat";
    std::vector<std::string> gmdDirectories(3);
    gmdDirectories[0] = homeDir;
    gmdDirectories[1] = "/usr/share/ccmix";
    gmdDirectories[2] = programDir;
    for (std::size_t i = 0; i < gmdDirectories.size(); ++i) {
        const std::string gmdTest = gmdDirectories[i] + DirSeparator + gmdLocation;
        if (FileExists(gmdTest)) {
            return gmdTest;
        }
    }
    return gmdLocation;
}

std::string FindKeySource(const std::string& programDir)
{
    const std::string keySourceLocation = "key.source";
    const std::string keySourcePath = programDir + DirSeparator + keySourceLocation;
    if (FileExists(keySourcePath)) {
        return keySourcePath;
    }

    return keySourceLocation;
}

inline void ShowUsage(const std::string_view programName)
{
    std::println("Usage: {} [--mode] (--file FILE) (--directory DIR) [--mix MIXFILE]",
            programName);
    std::println("Try `{}` -? or `{}` --help for more information.",
            programName, programName);
}

void ShowHelp(const std::string_view programName)
{
    std::println("\n***ccmix program usage***");
    std::println("Usage: {} [--mode] (--file FILE) (--directory DIR) (--game [td|ra|ts|ra2]) [--mix MIXFILE]",
            programName);
    std::print(
            "\nModes:\n\n"
            "--extract\n"
            "Extracts the contents of the specified mix file to the current directory.\n"
            "--file specifies a single file to extract.\n"
            "--directory specifies an alternative directory to extract to.\n"
            "--game specified the game the mix is from, td covers the\n"
            "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
            "expansions. ts covers Tiberian Sun and ra2 covers Red Alert 2/Yuri's Revenge.\n\n"
            "--create\n"
            "Creates a new mix file from the contents of the current folder.\n"
            "--file specifies a single file as the initial file to add to the\n"
            "new mix.\n"
            "--directory specifies an alternative directory to create mix from.\n"
            "--checksum specifies the mix should have a checksum.\n"
            "--encrypted specified the mix header should be encrypted.\n"
            "--game specified the game the mix is from, td covers the\n"
            "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
            "expansions. ts covers Tiberian Sun and ra2 covers Red Alert 2/Yuri's Revenge.\n\n"
            "--list\n"
            "Lists the contents of the specified mix file.\n"
            "--game specified the game the mix is from, td covers the\n"
            "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
            "expansions. ts covers Tiberian Sun and Red Alert 2/Yuri's Revenge.\n\n"
            "--add\n"
            "Adds the specified file or mix feature.\n"
            "--file specifies a single file to add.\n"
            "--checksum specifies the mix should have a checksum.\n"
            "--game specified the game the mix is from, td covers the\n"
            "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
            "expansions. ts covers Tiberian Sun and ra2 covers Red Alert 2/Yuri's Revenge.\n\n"
            "--remove\n"
            "Removes the specified file or mix feature.\n"
            "--file specifies a single file to remove.\n"
            "--checksum specifies the mix should not have a checksum.\n"
            "--game specified the game the mix is from, td covers the\n"
            "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
            "expansions. ts covers Tiberian Sun and ra2 covers Red Alert 2/Yuri's Revenge.\n\n");
}

inline void NoMultiMode(const std::string_view programName)
{
    std::println("You cannot specify more than one mode at once.");
    ShowUsage(programName);
}

uint32_t StringToId(const std::string_view inputString)
{
    if (inputString.empty() || inputString.size() > 8) {
        return 0;
    }

    uint32_t value = 0;
    const std::from_chars_result result = std::from_chars(
            inputString.data(),
            inputString.data() + inputString.size(),
            value,
            16);

    if (result.ec != std::errc() ||
        result.ptr != inputString.data() + inputString.size()) {
        return 0;
    }

    return value;
}

bool Extraction(MixFile& inputFile, const std::string& fileName,
        const std::string& outputDirectory, const uint32_t id)
{
    const std::string extractionDirectory =
            outputDirectory.empty() ? "." : outputDirectory;
    const std::string destination = extractionDirectory + DirSeparator + fileName;

    if (fileName.empty() && id == 0) {
        if (!inputFile.ExtractAll(extractionDirectory)) {
            std::println("Extraction failed");
            return false;
        }
        return true;
    }

    if (!fileName.empty() && id == 0) {
        if (!inputFile.ExtractFile(fileName, destination)) {
            std::println("Extraction failed");
            return false;
        }
        return true;
    }

    if (id != 0) {
        if (fileName.empty()) {
            std::println("You must specify a filename to extract to when giving an ID");
            return false;
        }
        if (!inputFile.ExtractFile(id, destination)) {
            std::println("Extraction failed");
            return false;
        }
        return true;
    }

    std::println("You have used an unsupported combination of options.");
    return false;
}

std::string GetHomeDir()
{
    const char* tmp = NULL;
    std::string result;

#ifdef _WIN32
    const std::optional<std::string> homeDrive = GetEnvironmentValue("HOMEDRIVE");
    if (!homeDrive.has_value()) {
        return "";
    }
    result = *homeDrive;

    const std::optional<std::string> homePath = GetEnvironmentValue("HOMEPATH");
    if (!homePath.has_value()) {
        return "";
    }
    result += *homePath;
#else
    tmp = std::getenv("HOME");
    if (tmp == NULL) {
        result = std::string(getpwuid(getuid())->pw_dir);
    } else {
        result = std::string(tmp);
    }
#endif

    return result;
}

bool TrySetMode(MixMode& currentMode, const MixMode nextMode,
        const std::string_view programName)
{
    if (currentMode != MixMode::None) {
        NoMultiMode(programName);
        return false;
    }

    currentMode = nextMode;
    return true;
}

bool TryReadRequiredValue(const std::span<char*> arguments, int& index,
        const ParsedOption& option, const std::string_view valueDescription,
        std::string& value)
{
    if (option.Value.has_value()) {
        value = std::string(*option.Value);
        return true;
    }

    if (index + 1 >= static_cast<int>(arguments.size()) ||
        IsOptionToken(arguments[index + 1])) {
        std::println("{} option requires {}.", option.Name, valueDescription);
        return false;
    }

    value = arguments[++index];
    return true;
}

bool TryApplyOption(const std::span<char*> arguments, int& index,
        const ParsedOption& option, const std::string_view programName,
        CommandLineOptions& commandLineOptions)
{
    const OptionSpec* spec = FindOptionSpec(option.Name);
    if (spec == NULL) {
        return false;
    }

    if (spec->Requirement == ArgumentRequirement::None &&
        option.Value.has_value()) {
        std::println("Invalid argument: {}", option.Name);
        ShowUsage(programName);
        return false;
    }

    switch (option.Id) {
        case OptionId::Help:
            ShowHelp(programName);
            commandLineOptions.ShowedHelp = true;
            return false;
        case OptionId::File:
            return TryReadRequiredValue(arguments, index, option, "a filename",
                    commandLineOptions.FileName);
        case OptionId::Id:
        {
            std::string value;
            if (!TryReadRequiredValue(arguments, index, option, "a file id", value)) {
                return false;
            }
            commandLineOptions.FileId = StringToId(value);
            return true;
        }
        case OptionId::Mix:
            if (!TryReadRequiredValue(arguments, index, option, "a mix file",
                    commandLineOptions.InputMixFile)) {
                return false;
            }
            std::println("Operating on {}", commandLineOptions.InputMixFile);
            return true;
        case OptionId::Directory:
            return TryReadRequiredValue(arguments, index, option, "a directory name",
                    commandLineOptions.Directory);
        case OptionId::LocalDb:
            commandLineOptions.CreateLocalDb = true;
            return true;
        case OptionId::Encrypt:
            commandLineOptions.EncryptHeader = true;
            return true;
        case OptionId::Checksum:
            commandLineOptions.AddChecksum = true;
            return true;
        case OptionId::Game:
        {
            std::string gameName;
            if (!TryReadRequiredValue(arguments, index, option, "a game name",
                    gameName)) {
                return false;
            }

            if (gameName == GameNames[0]) {
                commandLineOptions.GameType = GameTd;
            } else if (gameName == GameNames[1]) {
                commandLineOptions.GameType = GameRa;
            } else if (gameName == GameNames[2]) {
                commandLineOptions.GameType = GameTs;
            } else if (gameName == GameNames[3]) {
                commandLineOptions.GameType = GameRa2;
            } else {
                std::println("--game is either td, ra, ts or ra2.");
                return false;
            }

            return true;
        }
        case OptionId::Create:
            return TrySetMode(commandLineOptions.Mode, MixMode::Create, programName);
        case OptionId::Extract:
            return TrySetMode(commandLineOptions.Mode, MixMode::Extract, programName);
        case OptionId::List:
            return TrySetMode(commandLineOptions.Mode, MixMode::List, programName);
        case OptionId::Info:
            return TrySetMode(commandLineOptions.Mode, MixMode::Info, programName);
        case OptionId::Add:
            return TrySetMode(commandLineOptions.Mode, MixMode::Add, programName);
        case OptionId::Remove:
            return TrySetMode(commandLineOptions.Mode, MixMode::Remove, programName);
    }

    return false;
}

bool ParseCommandLine(const std::span<char*> arguments,
        const std::string_view programName, CommandLineOptions& commandLineOptions)
{
    for (int index = 1; index < static_cast<int>(arguments.size()); ++index) {
        const std::string_view token(arguments[index]);
        const std::optional<ParsedOption> parsedOption = ParseOptionToken(token);

        if (!parsedOption.has_value()) {
            std::println("Invalid argument: {}", token);
            ShowUsage(programName);
            return false;
        }

        if (!TryApplyOption(arguments, index, *parsedOption, programName,
                commandLineOptions)) {
            return false;
        }
    }

    return true;
}

int main(int argc, char** argv)
{
    if (argc <= 1) {
        ShowUsage(argv[0]);
        return 0;
    }

    const std::span<char*> arguments(argv, static_cast<std::size_t>(argc));
    const std::string_view programPath(argv[0]);
    const std::string programDir = GetProgramDir(programPath);
    const std::string userHomeDir = GetHomeDir();
    const std::string globalDbPath = FindGmd(programDir, userHomeDir);
    const std::string keySourcePath = FindKeySource(programDir);
    CommandLineOptions commandLineOptions;

    std::srand(static_cast<unsigned int>(std::time(NULL)));

    if (!ParseCommandLine(arguments, programPath, commandLineOptions)) {
        if (commandLineOptions.ShowedHelp) {
            return 0;
        }
        return 1;
    }

    if (commandLineOptions.InputMixFile.empty()) {
        std::println("You must specify --mix MIXFILE to operate on.");
        return 1;
    }

    switch (commandLineOptions.Mode) {
        case MixMode::Extract:
        {
            MixFile inputFile(globalDbPath, commandLineOptions.GameType);

            if (!inputFile.Open(commandLineOptions.InputMixFile)) {
                std::println("Cannot open specified mix file");
                return 1;
            }

            if (!Extraction(inputFile, commandLineOptions.FileName,
                    commandLineOptions.Directory, commandLineOptions.FileId)) {
                return 1;
            }
            return 0;
        }
        case MixMode::Create:
        {
            MixFile outputFile(globalDbPath, commandLineOptions.GameType);

            if (!outputFile.CreateMix(commandLineOptions.InputMixFile,
                    commandLineOptions.Directory, commandLineOptions.CreateLocalDb,
                    commandLineOptions.EncryptHeader, commandLineOptions.AddChecksum,
                    keySourcePath)) {
                std::println("Failed to create new mix file");
                return 1;
            }

            return 0;
        }
        case MixMode::Add:
        {
            MixFile inputFile(globalDbPath, commandLineOptions.GameType);

            if (!inputFile.Open(commandLineOptions.InputMixFile)) {
                std::println("Cannot open specified mix file");
                return 1;
            }

            if (commandLineOptions.FileName.empty()) {
                if (commandLineOptions.AddChecksum) {
                    inputFile.AddChecksum();
                }
            } else {
                inputFile.AddFile(commandLineOptions.FileName);
            }

            return 0;
        }
        case MixMode::Remove:
        {
            MixFile inputFile(globalDbPath, commandLineOptions.GameType);

            if (!inputFile.Open(commandLineOptions.InputMixFile)) {
                std::println("Cannot open specified mix file");
                return 1;
            }

            if (commandLineOptions.FileName.empty()) {
                if (commandLineOptions.AddChecksum) {
                    inputFile.RemoveChecksum();
                }
            } else {
                inputFile.RemoveFile(commandLineOptions.FileName);
            }

            return 0;
        }
        case MixMode::List:
        {
            MixFile inputFile(globalDbPath, commandLineOptions.GameType);

            if (!inputFile.Open(commandLineOptions.InputMixFile)) {
                std::println("Cannot open specified mix file");
                return 1;
            }

            inputFile.PrintFileList();
            return 0;
        }
        case MixMode::Info:
        {
            MixFile inputFile(globalDbPath, commandLineOptions.GameType);

            if (!inputFile.Open(commandLineOptions.InputMixFile)) {
                std::println("Cannot open specified mix file");
                return 1;
            }

            inputFile.PrintInfo();
            return 0;
        }
        case MixMode::None:
            std::println("command switch default, this shouldn't happen!!");
            return 1;
    }

    return 0;
}
