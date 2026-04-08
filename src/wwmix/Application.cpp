#include "Application.hpp"

#include "ArchiveCli.hpp"
#include "MixGmd.hpp"
#include "MixHeader.hpp"
#include "MixId.hpp"

#include "cryptopp/blowfish.h"
#include "cryptopp/modes.h"

#include <array>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace WwMix
{
class CommandCatalog;

struct ApplicationContext
{
    std::string ProgramPath;
    std::string ProgramName;
    const CommandCatalog *Commands = nullptr;

    std::string DisplayName(const std::string_view commandName) const
    {
        if (commandName.empty())
        {
            return ProgramName;
        }

        return ProgramName + " " + std::string(commandName);
    }
};

class Command
{
  public:
    virtual ~Command() = default;
    virtual std::string_view CanonicalName() const = 0;
    virtual bool Matches(std::string_view token) const = 0;
    virtual int Run(const ApplicationContext &context,
                    std::span<char *> arguments) const = 0;
    virtual void ShowHelp(const ApplicationContext &context) const = 0;
};

class CommandCatalog
{
  public:
    CommandCatalog();
    const Command *Find(std::string_view token) const;

  private:
    template <typename T, typename... Args>
    void Add(Args &&...args)
    {
        m_commands.push_back(
            std::make_unique<T>(std::forward<Args>(args)...));
    }

    std::vector<std::unique_ptr<Command>> m_commands;
};

class GameCodec
{
  public:
    static std::optional<Game> Parse(const std::string_view token)
    {
        switch (Match(token))
        {
        case Match("td"):
            return Game::TD;
        case Match("ra"):
            return Game::RA;
        case Match("ts"):
            return Game::TS;
        case Match("ra2"):
            return Game::RA2;
        default:
            return std::nullopt;
        }
    }

    static std::string_view Name(const Game game)
    {
        switch (game)
        {
        case Game::TD:
            return "td";
        case Game::RA:
            return "ra";
        case Game::TS:
            return "ts";
        case Game::RA2:
            return "ra2";
        default:
            return "unknown";
        }
    }

  private:
    static constexpr uint32_t Match(const std::string_view value)
    {
        uint32_t key = static_cast<uint32_t>(value.size()) << 24;
        for (std::size_t index = 0; index < value.size() && index < 3; ++index)
        {
            key |= static_cast<uint32_t>(
                       static_cast<unsigned char>(value[index]))
                   << (16 - index * 8);
        }
        return key;
    }
};

class HelpPrinter
{
  public:
    static void ShowGeneral(const std::string_view programName)
    {
        std::println("wwmix - unified Westwood MIX tool");
        std::println("");
        std::println("Usage:");
        std::println("  {} <archive-command> <archive> [<items>...] [<switches>...]",
                     programName);
        std::println("  {} gmd --input GMD --additions CSV --output GMD [--game GAME]",
                     programName);
        std::println("  {} key --mix FILE [--game GAME]", programName);
        std::println("  {} help [mix|gmd|key]", programName);
        std::println("");
        std::println("Archive commands:");
        ShowEntry("x, e, extract", "Extract archive contents.");
        ShowEntry("c, create", "Create a new archive from a directory.");
        ShowEntry("a, add", "Add files to an existing archive.");
        ShowEntry("d, delete", "Delete files from an archive.");
        ShowEntry("l, list", "List archive contents.");
        ShowEntry("i, info", "Show archive metadata.");
        std::println("");
        std::println("Other commands:");
        ShowEntry("gmd", "Database editing workflow from gmdedit.");
        ShowEntry("key", "Encrypted-header key inspection from mixkey.");
        ShowEntry("mix", "Compatibility wrapper for archive commands.");
        ShowEntry("help", "Show top-level or command-specific help.");
        std::println("");
        std::println("Compatibility:");
        std::println("  {} mix l CONQUER.MIX", programName);
        std::println("  {} --list --mix CONQUER.MIX", programName);
    }

    static void ShowHelpUsage(const std::string_view programName)
    {
        std::println("Usage:");
        std::println("  {} help [mix|gmd|key]", programName);
    }

    static void ShowGmdUsage(const std::string_view programName)
    {
        std::println("Usage:");
        std::println("  {} --input GMD --additions CSV --output GMD [--game GAME]",
                     programName);
        std::println("  {} INPUT_GMD ADDITIONS_CSV OUTPUT_GMD [GAME]", programName);
    }

    static void ShowGmdHelp(const std::string_view programName)
    {
        std::println("Global mix database editor");
        std::println("");
        ShowGmdUsage(programName);
        std::println("");
        std::println("Options:");
        ShowEntry("-?, --help", "Show this help message and exit.");
        ShowEntry("--input GMD", "Existing global mix database file.");
        ShowEntry("--additions CSV", "Text file containing name,description pairs.");
        ShowEntry("--output GMD", "Destination path for the updated database.");
        ShowEntry("--game GAME", "Game section to update: td, ra, ts, or ra2. Default: td.");
    }

    static void ShowKeyUsage(const std::string_view programName)
    {
        std::println("Usage:");
        std::println("  {} --mix FILE [--game GAME]", programName);
    }

    static void ShowKeyHelp(const std::string_view programName)
    {
        std::println("Encrypted MIX key inspection");
        std::println("");
        ShowKeyUsage(programName);
        std::println("");
        std::println("Options:");
        ShowEntry("-?, --help", "Show this help message and exit.");
        ShowEntry("--mix FILE", "Encrypted MIX file to inspect.");
        ShowEntry("--game GAME", "Header format hint: ra, ts, or ra2. Default: ra.");
        std::println("");
        std::println("Output:");
        ShowEntry("blowfish key", "Recovered 56-byte key in hexadecimal form.");
        ShowEntry("key source", "Raw 80-byte key source block from the archive.");
        ShowEntry("first block", "First decrypted 8-byte encrypted-header block.");
    }

  private:
    static void ShowEntry(const std::string_view syntax,
                          const std::string_view description)
    {
        std::println("  {:<28} {}", syntax, description);
    }
};

class GmdCommand final : public Command
{
  public:
    std::string_view CanonicalName() const override
    {
        return "gmd";
    }

    bool Matches(const std::string_view token) const override
    {
        return token == "gmd" || token == "gmdedit";
    }

    int Run(const ApplicationContext &context,
            const std::span<char *> arguments) const override
    {
        State state;
        const std::span<char *> commandArguments =
            arguments.size() > 1 ? arguments.subspan(1) : std::span<char *>();
        if (!Parse(context.DisplayName(CanonicalName()), commandArguments, state))
        {
            return state.ShowedHelp ? 0 : 1;
        }

        MixGmd gmd;
        std::fstream inputFile(
            state.InputPath, std::ios_base::in | std::ios_base::binary);
        if (!inputFile.is_open())
        {
            std::println("Failed to open global mix database: {}", state.InputPath);
            return 1;
        }

        gmd.ReadDb(inputFile);
        inputFile.close();

        const std::optional<NamePairs> additions = ReadAdditions(state.AdditionsPath);
        if (!additions.has_value())
        {
            return 1;
        }

        for (const auto &entry : *additions)
        {
            std::println("{} - {}", entry.first, entry.second);
            if (!gmd.AddName(state.GameType, entry.first, entry.second))
            {
                std::println("Failed to add entry: {}", entry.first);
                return 1;
            }
        }

        std::fstream outputFile(
            state.OutputPath, std::ios_base::out | std::ios_base::binary);
        if (!outputFile.is_open())
        {
            std::println("Failed to open output file: {}", state.OutputPath);
            return 1;
        }

        gmd.WriteDb(outputFile);
        outputFile.close();

        std::println("Wrote {} entries into the {} section of {}.",
                     additions->size(), GameCodec::Name(state.GameType),
                     state.OutputPath);
        return 0;
    }

    void ShowHelp(const ApplicationContext &context) const override
    {
        HelpPrinter::ShowGmdHelp(context.DisplayName(CanonicalName()));
    }

  private:
    using NamePairs = std::vector<std::pair<std::string, std::string>>;

    struct State
    {
        std::string DisplayName;
        std::string InputPath;
        std::string AdditionsPath;
        std::string OutputPath;
        Game GameType = Game::TD;
        bool ShowedHelp = false;
    };

    static bool Parse(const std::string &displayName,
                      const std::span<char *> arguments, State &state)
    {
        state.DisplayName = displayName;

        if (arguments.empty())
        {
            HelpPrinter::ShowGmdUsage(displayName);
            state.ShowedHelp = true;
            return false;
        }

        if (ParseLegacy(arguments, state))
        {
            return true;
        }

        for (int index = 0; index < static_cast<int>(arguments.size()); ++index)
        {
            const std::string_view token(arguments[index]);
            if (token == "-?" || token == "--help")
            {
                HelpPrinter::ShowGmdHelp(displayName);
                state.ShowedHelp = true;
                return false;
            }
            if (token == "--input")
            {
                if (!TryReadNamedValue(arguments, index, token, "an input GMD path",
                                       state.InputPath))
                {
                    return false;
                }
                continue;
            }
            if (token == "--additions")
            {
                if (!TryReadNamedValue(arguments, index, token,
                                       "an additions CSV path",
                                       state.AdditionsPath))
                {
                    return false;
                }
                continue;
            }
            if (token == "--output")
            {
                if (!TryReadNamedValue(arguments, index, token, "an output GMD path",
                                       state.OutputPath))
                {
                    return false;
                }
                continue;
            }
            if (token == "--game")
            {
                std::string gameName;
                if (!TryReadNamedValue(arguments, index, token, "a game name",
                                       gameName))
                {
                    return false;
                }
                const std::optional<Game> game = GameCodec::Parse(gameName);
                if (!game.has_value())
                {
                    std::println("--game is either td, ra, ts or ra2.");
                    return false;
                }
                state.GameType = *game;
                continue;
            }

            std::println("Invalid argument: {}", token);
            HelpPrinter::ShowGmdUsage(displayName);
            return false;
        }

        if (state.InputPath.empty() || state.AdditionsPath.empty() ||
            state.OutputPath.empty())
        {
            std::println("You must specify input, additions, and output paths.");
            HelpPrinter::ShowGmdUsage(displayName);
            return false;
        }

        return true;
    }

    static bool ParseLegacy(const std::span<char *> arguments, State &state)
    {
        if (arguments.size() < 3 || arguments.size() > 4)
        {
            return false;
        }

        for (const char *argument : arguments)
        {
            if (ArchiveCli::IsSwitchToken(argument))
            {
                return false;
            }
        }

        state.InputPath = arguments[0];
        state.AdditionsPath = arguments[1];
        state.OutputPath = arguments[2];
        if (arguments.size() == 4)
        {
            const std::optional<Game> game = GameCodec::Parse(arguments[3]);
            if (!game.has_value())
            {
                std::println("GAME must be td, ra, ts or ra2.");
                return false;
            }
            state.GameType = *game;
        }

        return true;
    }

    static bool TryReadNamedValue(const std::span<char *> arguments, int &index,
                                  const std::string_view optionName,
                                  const std::string_view valueDescription,
                                  std::string &value)
    {
        if (index + 1 >= static_cast<int>(arguments.size()) ||
            ArchiveCli::IsSwitchToken(arguments[index + 1]))
        {
            std::println("{} requires {}.", optionName, valueDescription);
            return false;
        }

        value = arguments[++index];
        return true;
    }

    static std::optional<NamePairs> ReadAdditions(const std::string &path)
    {
        std::fstream file(path, std::ios_base::in);
        if (!file.is_open())
        {
            std::println("Failed to open additions file: {}", path);
            return std::nullopt;
        }

        NamePairs names;
        for (std::string line; std::getline(file, line);)
        {
            if (line.empty())
            {
                continue;
            }

            const std::size_t separator = line.find(',');
            std::pair<std::string, std::string> entry;
            if (separator == std::string::npos)
            {
                entry.first = line;
            }
            else
            {
                entry.first = line.substr(0, separator);
                entry.second = line.substr(separator + 1);
            }

            if (!entry.first.empty())
            {
                names.push_back(entry);
            }
        }

        return names;
    }
};

class KeyCommand final : public Command
{
  public:
    std::string_view CanonicalName() const override
    {
        return "key";
    }

    bool Matches(const std::string_view token) const override
    {
        return token == "key" || token == "mixkey";
    }

    int Run(const ApplicationContext &context,
            const std::span<char *> arguments) const override
    {
        State state;
        const std::span<char *> commandArguments =
            arguments.size() > 1 ? arguments.subspan(1) : std::span<char *>();
        if (!Parse(context.DisplayName(CanonicalName()), commandArguments, state))
        {
            return state.ShowedHelp ? 0 : 1;
        }

        std::fstream file(
            state.MixPath, std::ios_base::in | std::ios_base::binary);
        if (!file.is_open())
        {
            std::println("Failed to open mix file: {}", state.MixPath);
            return 1;
        }

        char flagBuffer[4] = {0};
        file.read(flagBuffer, 4);
        if (file.gcount() != 4)
        {
            std::println("Failed to read the MIX header flags.");
            return 1;
        }

        constexpr int32_t MixChecksumFlag = 0x00010000;
        constexpr int32_t MixEncryptedFlag = 0x00020000;
        const int32_t flags = BufferValue<int32_t>(flagBuffer);
        if (flags != MixEncryptedFlag &&
            flags != MixEncryptedFlag + MixChecksumFlag)
        {
            std::println("{} does not contain an encrypted MIX header.",
                         state.MixPath);
            return 1;
        }

        file.clear();
        file.seekg(0, std::ios::beg);

        MixHeader header(state.GameType);
        if (!header.ReadHeader(file))
        {
            std::println("Failed to decode the encrypted MIX header.");
            return 1;
        }

        std::array<char, 8> firstBlock = {0};
        file.clear();
        file.seekg(84, std::ios::beg);
        file.read(firstBlock.data(),
                  static_cast<std::streamsize>(firstBlock.size()));
        if (file.gcount() != static_cast<std::streamsize>(firstBlock.size()))
        {
            std::println("Failed to read the first encrypted header block.");
            return 1;
        }

        CryptoPP::ECB_Mode<CryptoPP::Blowfish>::Decryption blowfish;
        blowfish.SetKey(reinterpret_cast<const uint8_t *>(header.GetKey()), 56);
        blowfish.ProcessString(reinterpret_cast<uint8_t *>(firstBlock.data()),
                               static_cast<unsigned int>(firstBlock.size()));

        std::println("Mix file: {}", state.MixPath);
        std::println("Game hint: {}", GameCodec::Name(state.GameType));
        std::println("Encrypted: {}", header.GetIsEncrypted() ? "yes" : "no");
        std::println("Checksum: {}", header.GetHasChecksum() ? "yes" : "no");
        std::println("Files: {}", header.GetFileCount());
        std::println("Body size: {}", header.GetBodySize());
        std::println("Blowfish key: {}", MixId::IdStr(header.GetKey(), 56));
        std::println("Key source: {}", MixId::IdStr(header.GetKeySource(), 80));
        std::println("First decrypted block: {}",
                     MixId::IdStr(firstBlock.data(),
                                  static_cast<uint32_t>(firstBlock.size())));
        return 0;
    }

    void ShowHelp(const ApplicationContext &context) const override
    {
        HelpPrinter::ShowKeyHelp(context.DisplayName(CanonicalName()));
    }

  private:
    struct State
    {
        std::string DisplayName;
        std::string MixPath;
        Game GameType = Game::RA;
        bool ShowedHelp = false;
    };

    static bool Parse(const std::string &displayName,
                      const std::span<char *> arguments, State &state)
    {
        state.DisplayName = displayName;

        if (arguments.empty())
        {
            HelpPrinter::ShowKeyUsage(displayName);
            state.ShowedHelp = true;
            return false;
        }

        for (int index = 0; index < static_cast<int>(arguments.size()); ++index)
        {
            const std::string_view token(arguments[index]);
            if (token == "-?" || token == "--help")
            {
                HelpPrinter::ShowKeyHelp(displayName);
                state.ShowedHelp = true;
                return false;
            }
            if (token == "--mix")
            {
                if (!TryReadNamedValue(arguments, index, token, "a mix file",
                                       state.MixPath))
                {
                    return false;
                }
                continue;
            }
            if (token == "--game")
            {
                std::string gameName;
                if (!TryReadNamedValue(arguments, index, token, "a game name",
                                       gameName))
                {
                    return false;
                }
                const std::optional<Game> game = GameCodec::Parse(gameName);
                if (!game.has_value())
                {
                    std::println("--game is either td, ra, ts or ra2.");
                    return false;
                }
                state.GameType = *game;
                continue;
            }

            std::println("Invalid argument: {}", token);
            HelpPrinter::ShowKeyUsage(displayName);
            return false;
        }

        if (state.MixPath.empty())
        {
            std::println("You must specify --mix FILE.");
            HelpPrinter::ShowKeyUsage(displayName);
            return false;
        }

        return true;
    }

    static bool TryReadNamedValue(const std::span<char *> arguments, int &index,
                                  const std::string_view optionName,
                                  const std::string_view valueDescription,
                                  std::string &value)
    {
        if (index + 1 >= static_cast<int>(arguments.size()) ||
            ArchiveCli::IsSwitchToken(arguments[index + 1]))
        {
            std::println("{} requires {}.", optionName, valueDescription);
            return false;
        }

        value = arguments[++index];
        return true;
    }

    template <typename T>
    static T BufferValue(const char *buffer)
    {
        T value = T();
        std::memcpy(&value, buffer, sizeof(T));
        return value;
    }
};

class MixCommand final : public Command
{
  public:
    std::string_view CanonicalName() const override
    {
        return "mix";
    }

    bool Matches(const std::string_view token) const override
    {
        return token == CanonicalName();
    }

    int Run(const ApplicationContext &context,
            const std::span<char *> arguments) const override
    {
        if (arguments.size() == 1 ||
            (arguments.size() == 2 &&
             (std::string_view(arguments[1]) == "-?" ||
              std::string_view(arguments[1]) == "--help")))
        {
            ShowHelp(context);
            return 0;
        }

        return ArchiveCli::Run(
            context.ProgramPath,
            context.DisplayName(CanonicalName()),
            arguments.subspan(1));
    }

    void ShowHelp(const ApplicationContext &context) const override
    {
        ArchiveCli::ShowHelp(context.DisplayName(CanonicalName()));
    }
};

class ArchiveRootCommand final : public Command
{
  public:
    std::string_view CanonicalName() const override
    {
        return "mix";
    }

    bool Matches(const std::string_view token) const override
    {
        return ArchiveCli::IsSwitchToken(token) || ArchiveCli::IsCommandToken(token);
    }

    int Run(const ApplicationContext &context,
            const std::span<char *> arguments) const override
    {
        return ArchiveCli::Run(context.ProgramPath, context.ProgramName, arguments);
    }

    void ShowHelp(const ApplicationContext &context) const override
    {
        ArchiveCli::ShowHelp(context.ProgramName);
    }
};

class HelpCommand final : public Command
{
  public:
    std::string_view CanonicalName() const override
    {
        return "help";
    }

    bool Matches(const std::string_view token) const override
    {
        return token == CanonicalName();
    }

    int Run(const ApplicationContext &context,
            const std::span<char *> arguments) const override
    {
        if (arguments.size() <= 1)
        {
            HelpPrinter::ShowGeneral(context.ProgramName);
            return 0;
        }

        const Command *command =
            context.Commands != nullptr ? context.Commands->Find(arguments[1]) : nullptr;
        if (command != nullptr)
        {
            command->ShowHelp(context);
            return 0;
        }

        std::println("Unknown help topic: {}", arguments[1]);
        HelpPrinter::ShowGeneral(context.ProgramName);
        return 1;
    }

    void ShowHelp(const ApplicationContext &context) const override
    {
        HelpPrinter::ShowHelpUsage(context.ProgramName);
    }
};

CommandCatalog::CommandCatalog()
{
    Add<HelpCommand>();
    Add<MixCommand>();
    Add<GmdCommand>();
    Add<KeyCommand>();
    Add<ArchiveRootCommand>();
}

const Command *CommandCatalog::Find(const std::string_view token) const
{
    for (const auto &command : m_commands)
    {
        if (command->Matches(token))
        {
            return command.get();
        }
    }

    return nullptr;
}

std::string GetExecutableName(const std::string_view programPath)
{
    const std::string_view::size_type separator = programPath.find_last_of("/\\");
    if (separator == std::string_view::npos)
    {
        return std::string(programPath);
    }

    return std::string(programPath.substr(separator + 1));
}

int Application::Run(int argc, char **argv)
{
    const std::string programPath = argc > 0 ? argv[0] : "wwmix";
    const std::string programName = GetExecutableName(programPath);
    const CommandCatalog commands;
    const ApplicationContext context = {programPath, programName, &commands};

    if (argc <= 1)
    {
        HelpPrinter::ShowGeneral(programName);
        return 0;
    }

    const std::string_view firstToken(argv[1]);
    if (firstToken == "-?" || firstToken == "--help")
    {
        HelpPrinter::ShowGeneral(programName);
        return 0;
    }

    const Command *command = commands.Find(firstToken);
    if (command == nullptr)
    {
        std::println("Unknown command: {}", firstToken);
        HelpPrinter::ShowGeneral(programName);
        return 1;
    }

    return command->Run(
        context,
        std::span<char *>(argv + 1, static_cast<std::size_t>(argc - 1)));
}
} // namespace WwMix
