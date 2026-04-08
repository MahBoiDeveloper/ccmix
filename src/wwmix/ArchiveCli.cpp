#include "ArchiveCli.hpp"

#include "MixFile.hpp"

#include <array>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace WwMix
{
enum class ArchiveCommand
{
    None,
    Extract,
    Create,
    Add,
    Remove,
    List,
    Info
};

enum class SwitchId
{
    Help,
    Game,
    Output,
    Directory,
    File,
    Id,
    LocalDb,
    Encrypt,
    Checksum,
    Mix,
    LegacyExtract,
    LegacyCreate,
    LegacyAdd,
    LegacyRemove,
    LegacyList,
    LegacyInfo
};

enum class SwitchArgument
{
    None,
    Required
};

struct SwitchForm
{
    SwitchId Id;
    std::string_view Key;
    SwitchArgument Argument;
    bool AllowMinusSuffix;
};

struct ParsedSwitch
{
    SwitchId Id;
    std::string_view Key;
    std::optional<std::string> Value;
    bool Enabled = true;
};

struct RawParseResult
{
    std::vector<ParsedSwitch> Switches;
    std::vector<std::string> NonSwitchStrings;
    std::optional<std::string> Error;
};

struct ArchiveCliOptions
{
    ArchiveCommand Command = ArchiveCommand::None;
    std::string ArchivePath;
    std::vector<std::string> Operands;
    std::string Directory;
    uint32_t FileId = 0;
    Game GameType = Game::TD;
    bool CreateLocalDb = false;
    bool EncryptHeader = false;
    bool AddChecksum = false;
    bool ShowedHelp = false;
};

class ArchiveStringUtil
{
  public:
    static char LowerAscii(const char value)
    {
        if (value >= 'A' && value <= 'Z')
        {
            return static_cast<char>(value - 'A' + 'a');
        }

        return value;
    }

    static bool EqualsIgnoreCase(const std::string_view left,
                                 const std::string_view right)
    {
        if (left.size() != right.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < left.size(); ++index)
        {
            if (LowerAscii(left[index]) != LowerAscii(right[index]))
            {
                return false;
            }
        }

        return true;
    }

    static bool StartsWithIgnoreCase(const std::string_view text,
                                     const std::string_view prefix)
    {
        if (prefix.size() > text.size())
        {
            return false;
        }

        return EqualsIgnoreCase(text.substr(0, prefix.size()), prefix);
    }
};

class ArchiveGameCodec
{
  public:
    static std::optional<Game> Parse(const std::string_view gameName)
    {
        if (ArchiveStringUtil::EqualsIgnoreCase(gameName, "td"))
        {
            return Game::TD;
        }
        if (ArchiveStringUtil::EqualsIgnoreCase(gameName, "ra"))
        {
            return Game::RA;
        }
        if (ArchiveStringUtil::EqualsIgnoreCase(gameName, "ts"))
        {
            return Game::TS;
        }
        if (ArchiveStringUtil::EqualsIgnoreCase(gameName, "ra2"))
        {
            return Game::RA2;
        }

        return std::nullopt;
    }
};

class ArchiveHelpPrinter
{
  public:
    static void ShowUsage(const std::string_view programName)
    {
        std::println("Usage:");
        std::println("  {} <command> <archive> [<items>...] [<switches>...]", programName);
        std::println("  {} --help", programName);
        std::println("Try '{} --help' for more information.", programName);
    }

    static void ShowHelp(const std::string_view programName)
    {
        std::println("{} - Westwood MIX archive tool", programName);
        std::println("");
        std::println("Usage:");
        std::println("  {} <command> <archive> [<items>...] [<switches>...]", programName);
        std::println("");
        std::println("Commands:");
        ShowEntry("x, e, extract", "Extract all files or one named file.");
        ShowEntry("c, create", "Create an archive from a directory.");
        ShowEntry("a, add", "Add file operands to an existing archive.");
        ShowEntry("d, delete", "Delete file operands from an archive.");
        ShowEntry("l, list", "List archive contents.");
        ShowEntry("i, info", "Show archive metadata.");
        std::println("");
        std::println("Flags:");
        ShowEntry("-?, --help", "Show this archive help and exit.");
        ShowEntry("-g{game}, --game GAME", "Select td, ra, ts, or ra2.");
        ShowEntry("-o{dir}, -out{dir}", "Extraction output directory.");
        ShowEntry("-d{dir}, -dir{dir}", "Create source directory.");
        ShowEntry("-f{file}, --file FILE", "Single file operand or legacy file switch.");
        ShowEntry("-id{hex}, --id HEX", "Extract by hexadecimal file id.");
        ShowEntry("-lmd[-], --lmd", "Enable or disable local mix database creation.");
        ShowEntry("-encrypt[-], --encrypt", "Enable or disable header encryption.");
        ShowEntry("-checksum[-], --checksum", "Enable or disable checksum handling.");
        ShowEntry("--mix FILE", "Legacy archive-path switch.");
        ShowEntry("--directory DIR, --dir DIR", "Legacy named directory switch.");
        ShowEntry("--", "Stop switch parsing and treat the rest as operands.");
        std::println("");
        std::println("Compatibility:");
        ShowEntry("--extract", "Legacy mode switch for extract.");
        ShowEntry("--create", "Legacy mode switch for create.");
        ShowEntry("--add", "Legacy mode switch for add.");
        ShowEntry("--remove", "Legacy mode switch for delete.");
        ShowEntry("--list", "Legacy mode switch for list.");
        ShowEntry("--info", "Legacy mode switch for info.");
        std::println("");
        std::println("Examples:");
        std::println("  {} l CONQUER.MIX", programName);
        std::println("  {} x CONQUER.MIX -oout", programName);
        std::println("  {} x CONQUER.MIX score.mix", programName);
        std::println("  {} c custom.mix data -gra2 -lmd -checksum", programName);
        std::println("  {} a custom.mix rules.ini sound.ini", programName);
        std::println("  {} d custom.mix rules.ini", programName);
    }

  private:
    static void ShowEntry(const std::string_view syntax,
                          const std::string_view description)
    {
        std::println("  {:<38} {}", syntax, description);
    }
};

class ArchiveEnvironment
{
  public:
    explicit ArchiveEnvironment(const std::string_view programPath)
        : m_programDir(GetProgramDir(programPath)),
          m_globalDbPath(FindGmd(m_programDir, GetHomeDir())),
          m_globalDbCachePath(m_programDir + DirSeparator + "gmd.json"),
          m_keySourcePath(FindKeySource(m_programDir))
    {
    }

    const std::string &GlobalDbPath() const
    {
        return m_globalDbPath;
    }

    const std::string &KeySourcePath() const
    {
        return m_keySourcePath;
    }

    const std::string &GlobalDbCachePath() const
    {
        return m_globalDbCachePath;
    }

  private:
#ifdef _WIN32
    static constexpr char DirSeparator = '\\';
#else
    static constexpr char DirSeparator = '/';
#endif

    static std::string GetProgramDir(const std::string_view programPath)
    {
        const std::string_view::size_type separator =
            programPath.find_last_of("/\\");
        if (separator == std::string_view::npos)
        {
            return ".";
        }
        if (separator == 0)
        {
            return std::string(programPath.substr(0, 1));
        }

        return std::string(programPath.substr(0, separator));
    }

    static bool FileExists(const std::string &path)
    {
        std::error_code error;
        return std::filesystem::exists(std::filesystem::u8path(path), error) &&
               !error;
    }

#ifdef _WIN32
    static std::optional<std::string> GetEnvironmentValue(const char *name)
    {
        char *value = nullptr;
        std::size_t length = 0;
        const errno_t error = _dupenv_s(&value, &length, name);
        if (error != 0 || value == nullptr || length == 0)
        {
            std::free(value);
            return std::nullopt;
        }

        std::string result(value);
        std::free(value);
        return result;
    }
#endif

    static std::string GetHomeDir()
    {
        const char *tmp = nullptr;
        std::string result;

#ifdef _WIN32
        const std::optional<std::string> homeDrive =
            GetEnvironmentValue("HOMEDRIVE");
        if (!homeDrive.has_value())
        {
            return "";
        }
        result = *homeDrive;

        const std::optional<std::string> homePath =
            GetEnvironmentValue("HOMEPATH");
        if (!homePath.has_value())
        {
            return "";
        }
        result += *homePath;
#else
        tmp = std::getenv("HOME");
        if (tmp == nullptr)
        {
            result = std::string(getpwuid(getuid())->pw_dir);
        }
        else
        {
            result = std::string(tmp);
        }
#endif

        return result;
    }

    static std::string FindGmd(const std::string &programDir,
                               const std::string &homeDir)
    {
        const std::string gmdLocation = "global mix database.dat";
        const std::array<std::string, 3> directories =
        {
            homeDir,
            "/usr/share/ccmix",
            programDir
        };

        for (const std::string &directory : directories)
        {
            const std::string gmdPath = directory + DirSeparator + gmdLocation;
            if (FileExists(gmdPath))
            {
                return gmdPath;
            }
        }

        return gmdLocation;
    }

    static std::string FindKeySource(const std::string &programDir)
    {
        const std::string keySourceLocation = "key.source";
        const std::string keySourcePath = programDir + DirSeparator + keySourceLocation;
        if (FileExists(keySourcePath))
        {
            return keySourcePath;
        }

        return keySourceLocation;
    }

    std::string m_programDir;
    std::string m_globalDbPath;
    std::string m_globalDbCachePath;
    std::string m_keySourcePath;
};

class ArchiveCommandParser
{
  public:
    bool Parse(const std::span<char *> arguments, ArchiveCliOptions &options) const
    {
        const RawParseResult rawResult = ParseRaw(arguments);
        return BuildOptions(rawResult, options);
    }

    static bool IsSwitchToken(const std::string_view token)
    {
        if (token.size() <= 1 || token == "-")
        {
            return false;
        }

        return token.starts_with('-') || token.starts_with('/');
    }

    static bool IsCommandToken(const std::string_view token)
    {
        return ParseCommandWord(token).has_value() ||
               ArchiveStringUtil::EqualsIgnoreCase(token, "help");
    }

  private:
    static constexpr auto SwitchForms = std::to_array<SwitchForm>(
        {
            {SwitchId::Help, "?", SwitchArgument::None, false},
            {SwitchId::Help, "help", SwitchArgument::None, false},
            {SwitchId::Game, "game", SwitchArgument::Required, false},
            {SwitchId::Game, "g", SwitchArgument::Required, false},
            {SwitchId::Output, "out", SwitchArgument::Required, false},
            {SwitchId::Output, "o", SwitchArgument::Required, false},
            {SwitchId::Directory, "directory", SwitchArgument::Required, false},
            {SwitchId::Directory, "dir", SwitchArgument::Required, false},
            {SwitchId::Directory, "d", SwitchArgument::Required, false},
            {SwitchId::File, "file", SwitchArgument::Required, false},
            {SwitchId::File, "f", SwitchArgument::Required, false},
            {SwitchId::Id, "id", SwitchArgument::Required, false},
            {SwitchId::LocalDb, "lmd", SwitchArgument::None, true},
            {SwitchId::Encrypt, "encrypt", SwitchArgument::None, true},
            {SwitchId::Checksum, "checksum", SwitchArgument::None, true},
            {SwitchId::Mix, "mix", SwitchArgument::Required, false},
            {SwitchId::LegacyExtract, "extract", SwitchArgument::None, false},
            {SwitchId::LegacyCreate, "create", SwitchArgument::None, false},
            {SwitchId::LegacyAdd, "add", SwitchArgument::None, false},
            {SwitchId::LegacyRemove, "remove", SwitchArgument::None, false},
            {SwitchId::LegacyList, "list", SwitchArgument::None, false},
            {SwitchId::LegacyInfo, "info", SwitchArgument::None, false}
        });

    RawParseResult ParseRaw(const std::span<char *> arguments) const
    {
        RawParseResult result;

        for (int index = 0; index < static_cast<int>(arguments.size()); ++index)
        {
            const std::string_view token(arguments[index]);
            if (token == "--")
            {
                for (int tailIndex = index + 1;
                     tailIndex < static_cast<int>(arguments.size());
                     ++tailIndex)
                {
                    result.NonSwitchStrings.push_back(arguments[tailIndex]);
                }
                return result;
            }

            if (!IsSwitchToken(token))
            {
                result.NonSwitchStrings.push_back(std::string(token));
                continue;
            }

            const std::string_view body =
                token[0] == '/' ? token.substr(1) :
                token.starts_with("--") ? token.substr(2) :
                                          token.substr(1);
            const SwitchForm *form = FindSwitchForm(body);
            if (form == nullptr)
            {
                result.Error = "Unknown switch: " + std::string(token);
                return result;
            }

            ParsedSwitch parsedSwitch = {form->Id, form->Key, std::nullopt, true};
            const std::string_view tail = body.substr(form->Key.size());

            if (form->Argument == SwitchArgument::Required)
            {
                if (tail.empty())
                {
                    if (index + 1 >= static_cast<int>(arguments.size()) ||
                        IsSwitchToken(arguments[index + 1]))
                    {
                        result.Error =
                            "Switch requires an argument: " + std::string(token);
                        return result;
                    }
                    parsedSwitch.Value = std::string(arguments[++index]);
                }
                else if (tail.front() == '=' || tail.front() == ':')
                {
                    parsedSwitch.Value = std::string(tail.substr(1));
                }
                else
                {
                    parsedSwitch.Value = std::string(tail);
                }
            }
            else if (form->AllowMinusSuffix && tail == "-")
            {
                parsedSwitch.Enabled = false;
            }
            else if (!tail.empty())
            {
                result.Error = "Switch does not take an argument: " + std::string(token);
                return result;
            }

            result.Switches.push_back(parsedSwitch);
        }

        return result;
    }

    bool BuildOptions(const RawParseResult &rawResult,
                      ArchiveCliOptions &options) const
    {
        if (rawResult.Error.has_value())
        {
            std::println("{}", *rawResult.Error);
            return false;
        }

        for (const ParsedSwitch &parsedSwitch : rawResult.Switches)
        {
            if (!ApplySwitchValue(options, parsedSwitch))
            {
                return false;
            }
        }

        std::size_t operandIndex = 0;
        if (!rawResult.NonSwitchStrings.empty())
        {
            const std::optional<ArchiveCommand> command =
                ParseCommandWord(rawResult.NonSwitchStrings[0]);
            if (command.has_value())
            {
                if (options.Command != ArchiveCommand::None &&
                    options.Command != *command)
                {
                    std::println("Conflicting command forms were specified.");
                    return false;
                }
                options.Command = *command;
                operandIndex = 1;
            }
            else if (ArchiveStringUtil::EqualsIgnoreCase(
                         rawResult.NonSwitchStrings[0], "help"))
            {
                options.ShowedHelp = true;
                return true;
            }
        }

        if (options.Command == ArchiveCommand::None)
        {
            if (options.ShowedHelp)
            {
                return true;
            }

            if (!rawResult.NonSwitchStrings.empty())
            {
                std::println("Unknown command: {}", rawResult.NonSwitchStrings[0]);
            }
            else
            {
                std::println("No command was specified.");
            }
            return false;
        }

        if (options.ArchivePath.empty())
        {
            if (operandIndex >= rawResult.NonSwitchStrings.size())
            {
                std::println("Archive path is required.");
                return false;
            }
            options.ArchivePath = rawResult.NonSwitchStrings[operandIndex++];
        }

        for (; operandIndex < rawResult.NonSwitchStrings.size(); ++operandIndex)
        {
            options.Operands.push_back(rawResult.NonSwitchStrings[operandIndex]);
        }

        return true;
    }

    bool ApplySwitchValue(ArchiveCliOptions &options,
                          const ParsedSwitch &parsedSwitch) const
    {
        switch (parsedSwitch.Id)
        {
        case SwitchId::Help:
            options.ShowedHelp = true;
            return true;
        case SwitchId::Game:
        {
            const std::optional<Game> game = ArchiveGameCodec::Parse(*parsedSwitch.Value);
            if (!game.has_value())
            {
                std::println("Game must be td, ra, ts or ra2.");
                return false;
            }
            options.GameType = *game;
            return true;
        }
        case SwitchId::Output:
        case SwitchId::Directory:
            options.Directory = *parsedSwitch.Value;
            return true;
        case SwitchId::File:
            options.Operands.push_back(*parsedSwitch.Value);
            return true;
        case SwitchId::Id:
            options.FileId = StringToId(*parsedSwitch.Value);
            if (options.FileId == 0)
            {
                std::println("Invalid file id: {}", *parsedSwitch.Value);
                return false;
            }
            return true;
        case SwitchId::LocalDb:
            options.CreateLocalDb = parsedSwitch.Enabled;
            return true;
        case SwitchId::Encrypt:
            options.EncryptHeader = parsedSwitch.Enabled;
            return true;
        case SwitchId::Checksum:
            options.AddChecksum = parsedSwitch.Enabled;
            return true;
        case SwitchId::Mix:
            options.ArchivePath = *parsedSwitch.Value;
            return true;
        case SwitchId::LegacyExtract:
            return TrySetLegacyCommand(options, ArchiveCommand::Extract,
                                       parsedSwitch.Key);
        case SwitchId::LegacyCreate:
            return TrySetLegacyCommand(options, ArchiveCommand::Create,
                                       parsedSwitch.Key);
        case SwitchId::LegacyAdd:
            return TrySetLegacyCommand(options, ArchiveCommand::Add,
                                       parsedSwitch.Key);
        case SwitchId::LegacyRemove:
            return TrySetLegacyCommand(options, ArchiveCommand::Remove,
                                       parsedSwitch.Key);
        case SwitchId::LegacyList:
            return TrySetLegacyCommand(options, ArchiveCommand::List,
                                       parsedSwitch.Key);
        case SwitchId::LegacyInfo:
            return TrySetLegacyCommand(options, ArchiveCommand::Info,
                                       parsedSwitch.Key);
        }

        return false;
    }

    static const SwitchForm *FindSwitchForm(const std::string_view body)
    {
        const SwitchForm *bestMatch = nullptr;
        std::size_t bestLength = 0;

        for (const SwitchForm &form : SwitchForms)
        {
            if (!ArchiveStringUtil::StartsWithIgnoreCase(body, form.Key))
            {
                continue;
            }

            const std::string_view tail = body.substr(form.Key.size());
            bool valid = false;
            if (form.Argument == SwitchArgument::Required)
            {
                valid = true;
            }
            else if (tail.empty())
            {
                valid = true;
            }
            else if (form.AllowMinusSuffix && tail == "-")
            {
                valid = true;
            }

            if (!valid)
            {
                continue;
            }

            if (form.Key.size() > bestLength)
            {
                bestMatch = &form;
                bestLength = form.Key.size();
            }
        }

        return bestMatch;
    }

    static bool TrySetLegacyCommand(ArchiveCliOptions &options,
                                    const ArchiveCommand command,
                                    const std::string_view switchName)
    {
        if (options.Command != ArchiveCommand::None &&
            options.Command != command)
        {
            std::println("Conflicting command switches: {}", switchName);
            return false;
        }

        options.Command = command;
        return true;
    }

    static std::optional<ArchiveCommand> ParseCommandWord(
        const std::string_view commandWord)
    {
        if (ArchiveStringUtil::EqualsIgnoreCase(commandWord, "x") ||
            ArchiveStringUtil::EqualsIgnoreCase(commandWord, "e") ||
            ArchiveStringUtil::EqualsIgnoreCase(commandWord, "extract"))
        {
            return ArchiveCommand::Extract;
        }
        if (ArchiveStringUtil::EqualsIgnoreCase(commandWord, "c") ||
            ArchiveStringUtil::EqualsIgnoreCase(commandWord, "create"))
        {
            return ArchiveCommand::Create;
        }
        if (ArchiveStringUtil::EqualsIgnoreCase(commandWord, "a") ||
            ArchiveStringUtil::EqualsIgnoreCase(commandWord, "add"))
        {
            return ArchiveCommand::Add;
        }
        if (ArchiveStringUtil::EqualsIgnoreCase(commandWord, "d") ||
            ArchiveStringUtil::EqualsIgnoreCase(commandWord, "del") ||
            ArchiveStringUtil::EqualsIgnoreCase(commandWord, "delete") ||
            ArchiveStringUtil::EqualsIgnoreCase(commandWord, "remove"))
        {
            return ArchiveCommand::Remove;
        }
        if (ArchiveStringUtil::EqualsIgnoreCase(commandWord, "l") ||
            ArchiveStringUtil::EqualsIgnoreCase(commandWord, "list"))
        {
            return ArchiveCommand::List;
        }
        if (ArchiveStringUtil::EqualsIgnoreCase(commandWord, "i") ||
            ArchiveStringUtil::EqualsIgnoreCase(commandWord, "info"))
        {
            return ArchiveCommand::Info;
        }

        return std::nullopt;
    }

    static uint32_t StringToId(const std::string_view inputString)
    {
        if (inputString.empty() || inputString.size() > 8)
        {
            return 0;
        }

        uint32_t value = 0;
        const std::from_chars_result result = std::from_chars(
            inputString.data(),
            inputString.data() + inputString.size(),
            value,
            16);

        if (result.ec != std::errc() ||
            result.ptr != inputString.data() + inputString.size())
        {
            return 0;
        }

        return value;
    }
};

class ArchiveCommandRunner
{
  public:
    ArchiveCommandRunner(std::string globalDbPath,
                         std::string globalDbCachePath,
                         std::string keySourcePath)
        : m_globalDbPath(std::move(globalDbPath)),
          m_globalDbCachePath(std::move(globalDbCachePath)),
          m_keySourcePath(std::move(keySourcePath))
    {
    }

    int Run(const ArchiveCliOptions &options) const
    {
        switch (options.Command)
        {
        case ArchiveCommand::Extract:
            return RunExtract(options);
        case ArchiveCommand::Create:
            return RunCreate(options);
        case ArchiveCommand::Add:
            return RunAdd(options);
        case ArchiveCommand::Remove:
            return RunRemove(options);
        case ArchiveCommand::List:
            return RunList(options);
        case ArchiveCommand::Info:
            return RunInfo(options);
        case ArchiveCommand::None:
            break;
        }

        return 1;
    }

  private:
#ifdef _WIN32
    static constexpr char DirSeparator = '\\';
#else
    static constexpr char DirSeparator = '/';
#endif

    static bool Extract(MixFile &inputFile, const std::string &fileName,
                        const std::string &outputDirectory, const uint32_t id)
    {
        const std::string extractionDirectory =
            outputDirectory.empty() ? "." : outputDirectory;
        const std::string destination =
            extractionDirectory + DirSeparator + fileName;

        if (fileName.empty() && id == 0)
        {
            return inputFile.ExtractAll(extractionDirectory);
        }

        if (!fileName.empty() && id == 0)
        {
            return inputFile.ExtractFile(fileName, destination);
        }

        if (id != 0)
        {
            if (fileName.empty())
            {
                std::println("You must specify a filename when extracting by file id.");
                return false;
            }
            return inputFile.ExtractFile(id, destination);
        }

        return false;
    }

    int RunExtract(const ArchiveCliOptions &options) const
    {
        if (options.Operands.size() > 1)
        {
            std::println("Extract accepts at most one file operand.");
            return 1;
        }

        MixFile inputFile(m_globalDbPath, options.GameType, m_globalDbCachePath);
        if (!inputFile.Open(options.ArchivePath))
        {
            std::println("Cannot open specified mix file");
            return 1;
        }

        const std::string fileName =
            options.Operands.empty() ? "" : options.Operands.front();
        if (!Extract(inputFile, fileName, options.Directory, options.FileId))
        {
            std::println("Extraction failed");
            return 1;
        }
        return 0;
    }

    int RunCreate(const ArchiveCliOptions &options) const
    {
        if (options.FileId != 0)
        {
            std::println("Create does not support file ids.");
            return 1;
        }
        if (options.Operands.size() > 1)
        {
            std::println("Create accepts at most one source directory operand.");
            return 1;
        }

        const std::string sourceDirectory =
            !options.Operands.empty() ? options.Operands.front() :
            !options.Directory.empty() ? options.Directory :
                                         ".";

        MixFile outputFile(m_globalDbPath, options.GameType, m_globalDbCachePath);
        if (!outputFile.CreateMix(options.ArchivePath, sourceDirectory,
                                  options.CreateLocalDb, options.EncryptHeader,
                                  options.AddChecksum, m_keySourcePath))
        {
            std::println("Failed to create new mix file");
            return 1;
        }

        return 0;
    }

    int RunAdd(const ArchiveCliOptions &options) const
    {
        MixFile inputFile(m_globalDbPath, options.GameType, m_globalDbCachePath);
        if (!inputFile.Open(options.ArchivePath, true))
        {
            std::println("Cannot open specified mix file");
            return 1;
        }

        if (options.Operands.empty() && !options.AddChecksum)
        {
            std::println("Add requires at least one file operand or -checksum.");
            return 1;
        }

        for (const std::string &operand : options.Operands)
        {
            if (!inputFile.AddFile(operand))
            {
                return 1;
            }
        }

        if (options.AddChecksum && !inputFile.AddChecksum())
        {
            return 1;
        }

        return 0;
    }

    int RunRemove(const ArchiveCliOptions &options) const
    {
        MixFile inputFile(m_globalDbPath, options.GameType, m_globalDbCachePath);
        if (!inputFile.Open(options.ArchivePath, true))
        {
            std::println("Cannot open specified mix file");
            return 1;
        }

        if (options.Operands.empty() && !options.AddChecksum)
        {
            std::println("Delete requires at least one file operand or -checksum.");
            return 1;
        }

        for (const std::string &operand : options.Operands)
        {
            if (!inputFile.RemoveFile(operand))
            {
                return 1;
            }
        }

        if (options.AddChecksum && !inputFile.RemoveChecksum())
        {
            return 1;
        }

        return 0;
    }

    int RunList(const ArchiveCliOptions &options) const
    {
        if (!options.Operands.empty())
        {
            std::println("List does not accept file operands.");
            return 1;
        }

        MixFile inputFile(m_globalDbPath, options.GameType, m_globalDbCachePath);
        if (!inputFile.Open(options.ArchivePath))
        {
            std::println("Cannot open specified mix file");
            return 1;
        }

        inputFile.PrintFileList();
        return 0;
    }

    int RunInfo(const ArchiveCliOptions &options) const
    {
        if (!options.Operands.empty())
        {
            std::println("Info does not accept file operands.");
            return 1;
        }

        MixFile inputFile(m_globalDbPath, options.GameType, m_globalDbCachePath);
        if (!inputFile.Open(options.ArchivePath))
        {
            std::println("Cannot open specified mix file");
            return 1;
        }

        inputFile.PrintInfo();
        return 0;
    }

    std::string m_globalDbPath;
    std::string m_globalDbCachePath;
    std::string m_keySourcePath;
};

bool ArchiveCli::IsSwitchToken(const std::string_view token)
{
    return ArchiveCommandParser::IsSwitchToken(token);
}

bool ArchiveCli::IsCommandToken(const std::string_view token)
{
    return ArchiveCommandParser::IsCommandToken(token);
}

void ArchiveCli::ShowUsage(const std::string_view programName)
{
    ArchiveHelpPrinter::ShowUsage(programName);
}

void ArchiveCli::ShowHelp(const std::string_view programName)
{
    ArchiveHelpPrinter::ShowHelp(programName);
}

int ArchiveCli::Run(const std::string_view programPath,
                    const std::string_view displayName,
                    const std::span<char *> arguments)
{
    if (arguments.empty())
    {
        ArchiveHelpPrinter::ShowUsage(displayName);
        return 0;
    }

    ArchiveCommandParser parser;
    ArchiveCliOptions options;
    if (!parser.Parse(arguments, options))
    {
        ArchiveHelpPrinter::ShowUsage(displayName);
        return 1;
    }

    if (options.ShowedHelp)
    {
        ArchiveHelpPrinter::ShowHelp(displayName);
        return 0;
    }

    const ArchiveEnvironment environment(programPath);
    ArchiveCommandRunner runner(environment.GlobalDbPath(),
                                environment.GlobalDbCachePath(),
                                environment.KeySourcePath());
    return runner.Run(options);
}
} // namespace WwMix
