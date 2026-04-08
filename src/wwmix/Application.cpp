#include "Application.hpp"

#include "ArchiveCli.hpp"
#include "MixFile.hpp"
#include "MixGmd.hpp"
#include "MixHeader.hpp"
#include "MixId.hpp"
#include "Utf8Path.hpp"

#include "cryptopp/blowfish.h"
#include "cryptopp/modes.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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

    std::string ProgramDirectory() const
    {
        const std::filesystem::path programPath = Utf8Path::FromUtf8(ProgramPath);
        const std::filesystem::path programDir =
            programPath.has_parent_path() ? programPath.parent_path() :
                                            std::filesystem::path(".");
        return Utf8Path::ToUtf8(programDir);
    }

    std::string DisplayName(const std::string_view commandName) const
    {
        if (commandName.empty())
        {
            return ProgramName;
        }

        return ProgramName + " " + std::string(commandName);
    }

    std::string GmdCachePath() const
    {
        return Utf8Path::Join(ProgramDirectory(), "gmd.json");
    }

    std::string GlobalDbPath() const
    {
        const std::filesystem::path localPath =
            Utf8Path::FromUtf8(ProgramDirectory()) / "global mix database.dat";
        if (std::filesystem::exists(localPath))
        {
            return Utf8Path::ToUtf8(localPath);
        }

        return "global mix database.dat";
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
        std::println("  {} guess --mix FILE [OPTIONS]", programName);
        std::println("  {} help [mix|gmd|key|guess]", programName);
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
        ShowEntry("guess", "Bruteforce candidate names for unknown MIX IDs.");
        ShowEntry("mix", "Compatibility wrapper for archive commands.");
        ShowEntry("help", "Show top-level or command-specific help.");
        std::println("");
        std::println("Archive Flags:");
        ShowEntry("-?, --help", "Show top-level or command-specific help.");
        ShowEntry("-g{game}, --game GAME", "Archive game hint: td, ra, ts, or ra2.");
        ShowEntry("-o{dir}, -out{dir}", "Archive extraction output directory.");
        ShowEntry("-d{dir}, -dir{dir}", "Archive create source directory.");
        ShowEntry("-f{file}, --file FILE", "Archive single-file operand form.");
        ShowEntry("-id{hex}, --id HEX", "Archive extraction by hexadecimal file id.");
        ShowEntry("-lmd[-], --lmd", "Archive local mix database toggle.");
        ShowEntry("-encrypt[-], --encrypt", "Archive encrypted-header toggle.");
        ShowEntry("-checksum[-], --checksum", "Archive checksum toggle.");
        ShowEntry("--extract / --create / --add", "Legacy archive mode switches.");
        ShowEntry("--remove / --list / --info", "More legacy archive mode switches.");
        ShowEntry("--mix FILE", "Legacy archive-path switch.");
        ShowEntry("--directory DIR, --dir DIR", "Legacy archive directory switch.");
        ShowEntry("--", "Stop archive switch parsing.");
        std::println("");
        std::println("GMD Flags:");
        ShowEntry("--input GMD", "Existing global mix database file.");
        ShowEntry("--additions CSV", "Text file containing name,description pairs.");
        ShowEntry("--output GMD", "Destination path for the updated database.");
        ShowEntry("--game GAME", "Game section to update: td, ra, ts, or ra2.");
        std::println("");
        std::println("Key Flags:");
        ShowEntry("--mix FILE", "Encrypted MIX file to inspect.");
        ShowEntry("--game GAME", "Header format hint for key decoding.");
        std::println("");
        std::println("Guess Flags:");
        ShowEntry("--mix FILE", "Archive whose unknown IDs should be targeted.");
        ShowEntry("--id HEX", "Target a specific ID; repeatable.");
        ShowEntry("--ext EXT[,EXT...]", "Required extension list for candidate names.");
        ShowEntry("--charset CHARS", "Characters used for the bruteforced stem.");
        ShowEntry("--min N", "Minimum bruteforced stem length.");
        ShowEntry("--max N", "Maximum bruteforced stem length.");
        ShowEntry("--prefix TEXT", "Fixed prefix before the bruteforced stem.");
        ShowEntry("--suffix TEXT", "Fixed suffix before the extension.");
        ShowEntry("--force", "Allow very large search spaces.");
        std::println("");
        std::println("Compatibility:");
        std::println("  {} mix l CONQUER.MIX", programName);
        std::println("  {} --list --mix CONQUER.MIX", programName);
        std::println("");
        std::println("More Help:");
        std::println("  {} help mix", programName);
        std::println("  {} help gmd", programName);
        std::println("  {} help key", programName);
        std::println("  {} help guess", programName);
    }

    static void ShowHelpUsage(const std::string_view programName)
    {
        std::println("Usage:");
        std::println("  {} help [mix|gmd|key|guess]", programName);
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
        std::println("Flags:");
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
        std::println("Flags:");
        ShowEntry("-?, --help", "Show this help message and exit.");
        ShowEntry("--mix FILE", "Encrypted MIX file to inspect.");
        ShowEntry("--game GAME", "Header format hint: ra, ts, or ra2. Default: ra.");
        std::println("");
        std::println("Output:");
        ShowEntry("blowfish key", "Recovered 56-byte key in hexadecimal form.");
        ShowEntry("key source", "Raw 80-byte key source block from the archive.");
        ShowEntry("first block", "First decrypted 8-byte encrypted-header block.");
    }

    static void ShowGuessUsage(const std::string_view programName)
    {
        std::println("Usage:");
        std::println("  {} --mix FILE [--game GAME] --ext EXT[,EXT...] [--min N] [--max N]",
                     programName);
        std::println("      [--charset CHARS] [--prefix TEXT] [--suffix TEXT] [--force]",
                     programName);
        std::println("  {} --id HEX [--id HEX ...] [--game GAME] --ext EXT[,EXT...] [--min N] [--max N]",
                     programName);
    }

    static void ShowGuessHelp(const std::string_view programName)
    {
        std::println("Unknown MIX filename bruteforcer");
        std::println("");
        ShowGuessUsage(programName);
        std::println("");
        std::println("Flags:");
        ShowEntry("-?, --help", "Show this help message and exit.");
        ShowEntry("--mix FILE", "Use unresolved IDs from an archive as targets.");
        ShowEntry("--id HEX", "Target a specific hexadecimal ID; repeatable.");
        ShowEntry("--game GAME", "Hashing mode: td, ra, ts, or ra2. Default: td.");
        ShowEntry("--ext EXT[,EXT...]", "Required extension list such as .shp,.ini.");
        ShowEntry("--charset CHARS", "Characters used for the bruteforced stem.");
        ShowEntry("--min N", "Minimum bruteforced stem length. Default: 1.");
        ShowEntry("--max N", "Maximum bruteforced stem length. Default: 3.");
        ShowEntry("--prefix TEXT", "Fixed text before the bruteforced stem.");
        ShowEntry("--suffix TEXT", "Fixed text after the bruteforced stem.");
        ShowEntry("--force", "Allow search spaces larger than the safety limit.");
        std::println("");
        std::println("Notes:");
        ShowEntry("target IDs", "Use --id to target specific IDs, or --mix to scan unknown archive entries.");
        ShowEntry("search space", "Candidate names are built as prefix + stem + suffix + extension.");
        std::println("");
        std::println("Examples:");
        std::println("  {} --mix CACHE.MIX --game ts --ext .shp --min 3 --max 4",
                     programName);
        std::println("  {} --id DEADBEEF --game ra --ext .ini --charset abcdef0123 --min 1 --max 5",
                     programName);
        std::println("  {} --mix TEM.MIX --game ts --ext .aud,.bag --prefix sfx_ --min 2 --max 3 --force",
                     programName);
    }

  private:
    static void ShowEntry(const std::string_view syntax,
                          const std::string_view description)
    {
        std::println("  {:<38} {}", syntax, description);
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
        if (!gmd.Load(state.InputPath, context.GmdCachePath(), false))
        {
            std::println("Failed to open global mix database: {}", state.InputPath);
            return 1;
        }

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

        std::fstream outputFile;
        Utf8Path::Open(
            outputFile, state.OutputPath,
            std::ios_base::out | std::ios_base::binary);
        if (!outputFile.is_open())
        {
            std::println("Failed to open output file: {}", state.OutputPath);
            return 1;
        }

        gmd.WriteDb(outputFile);
        outputFile.close();
        gmd.WriteCache(state.OutputPath, context.GmdCachePath());

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
        std::fstream file;
        Utf8Path::Open(file, path, std::ios_base::in);
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

        std::fstream file;
        Utf8Path::Open(
            file, state.MixPath,
            std::ios_base::in | std::ios_base::binary);
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

class GuessBruteforcer
{
  public:
    struct Options
    {
        Game GameType = Game::TD;
        std::vector<std::string> Extensions;
        std::string Charset = "abcdefghijklmnopqrstuvwxyz0123456789_";
        std::string Prefix;
        std::string Suffix;
        uint32_t MinLength = 1;
        uint32_t MaxLength = 3;
    };

    struct Match
    {
        int32_t Id = 0;
        std::string Name;
    };

    struct Result
    {
        uint64_t Tried = 0;
        std::vector<Match> Matches;
        std::vector<int32_t> UnresolvedIds;
    };

    static constexpr uint64_t SafeSearchLimit = 50000000;

    static uint64_t EstimateCandidateCount(const Options &options)
    {
        if (options.Extensions.empty())
        {
            return 0;
        }

        uint64_t total = 0;
        for (uint32_t length = options.MinLength; length <= options.MaxLength; ++length)
        {
            uint64_t combinations = 1;
            for (uint32_t index = 0; index < length; ++index)
            {
                combinations = SaturatingMultiply(
                    combinations,
                    static_cast<uint64_t>(options.Charset.size()));
            }

            combinations = SaturatingMultiply(
                combinations,
                static_cast<uint64_t>(options.Extensions.size()));
            total = SaturatingAdd(total, combinations);
        }

        return total;
    }

    static Result Run(const std::vector<int32_t> &targetIds,
                      const Options &options)
    {
        Result result;
        std::vector<int32_t> orderedTargets;
        std::unordered_set<int32_t> remainingIds;
        for (const int32_t id : targetIds)
        {
            if (remainingIds.insert(id).second)
            {
                orderedTargets.push_back(id);
            }
        }

        std::unordered_map<int32_t, std::string> matches;
        for (uint32_t length = options.MinLength;
             length <= options.MaxLength && !remainingIds.empty();
             ++length)
        {
            std::string stem(length, '\0');
            SearchStem(stem, 0, options, remainingIds, matches, result.Tried);
        }

        for (const int32_t id : orderedTargets)
        {
            const auto iterator = matches.find(id);
            if (iterator != matches.end())
            {
                result.Matches.push_back({id, iterator->second});
            }
            else
            {
                result.UnresolvedIds.push_back(id);
            }
        }

        return result;
    }

  private:
    static uint64_t SaturatingMultiply(const uint64_t left, const uint64_t right)
    {
        if (left == 0 || right == 0)
        {
            return 0;
        }
        if (left > std::numeric_limits<uint64_t>::max() / right)
        {
            return std::numeric_limits<uint64_t>::max();
        }

        return left * right;
    }

    static uint64_t SaturatingAdd(const uint64_t left, const uint64_t right)
    {
        if (left > std::numeric_limits<uint64_t>::max() - right)
        {
            return std::numeric_limits<uint64_t>::max();
        }

        return left + right;
    }

    static void SearchStem(std::string &stem, const std::size_t index,
                           const Options &options,
                           std::unordered_set<int32_t> &remainingIds,
                           std::unordered_map<int32_t, std::string> &matches,
                           uint64_t &tried)
    {
        if (remainingIds.empty())
        {
            return;
        }

        if (index == stem.size())
        {
            const std::string baseName = options.Prefix + stem + options.Suffix;
            for (const std::string &extension : options.Extensions)
            {
                const std::string candidateName = baseName + extension;
                ++tried;

                const int32_t candidateId =
                    MixId::IdGen(options.GameType, candidateName);
                if (remainingIds.erase(candidateId) > 0)
                {
                    matches.emplace(candidateId, candidateName);
                    std::println("Match: {} -> {}",
                                 MixId::IdStr(candidateId), candidateName);
                    if (remainingIds.empty())
                    {
                        return;
                    }
                }

                if (tried % 1000000 == 0 && !remainingIds.empty())
                {
                    std::println("Tried {} candidates, {} IDs remaining...",
                                 tried, remainingIds.size());
                }
            }

            return;
        }

        for (const char value : options.Charset)
        {
            stem[index] = value;
            SearchStem(stem, index + 1, options, remainingIds, matches, tried);
            if (remainingIds.empty())
            {
                return;
            }
        }
    }
};

class GuessCommand final : public Command
{
  public:
    std::string_view CanonicalName() const override
    {
        return "guess";
    }

    bool Matches(const std::string_view token) const override
    {
        return token == "guess" || token == "bruteforce";
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

        std::vector<int32_t> targetIds = state.TargetIds;
        if (targetIds.empty())
        {
            MixFile mixFile(
                context.GlobalDbPath(), state.Options.GameType,
                context.GmdCachePath());
            if (!mixFile.Open(state.MixPath))
            {
                std::println("Cannot open specified mix file");
                return 1;
            }

            targetIds = mixFile.CollectUnknownIds();
            if (targetIds.empty())
            {
                std::println("No unknown archive IDs were found in {}.",
                             state.MixPath);
                return 0;
            }
        }

        const uint64_t estimate =
            GuessBruteforcer::EstimateCandidateCount(state.Options);
        std::println("Target IDs: {}", targetIds.size());
        std::println("Search space: {} candidate names", estimate);
        if (estimate > GuessBruteforcer::SafeSearchLimit && !state.Force)
        {
            std::println("Refusing to search more than {} candidates without --force.",
                         GuessBruteforcer::SafeSearchLimit);
            return 1;
        }

        const GuessBruteforcer::Result result =
            GuessBruteforcer::Run(targetIds, state.Options);

        std::println("Tried {} candidate names.", result.Tried);
        if (!result.Matches.empty())
        {
            std::println("Resolved:");
            for (const GuessBruteforcer::Match &match : result.Matches)
            {
                std::println("  {} -> {}", MixId::IdStr(match.Id), match.Name);
            }
        }

        if (!result.UnresolvedIds.empty())
        {
            std::println("Still unresolved:");
            for (const int32_t id : result.UnresolvedIds)
            {
                std::println("  {}", MixId::IdStr(id));
            }
        }

        return result.UnresolvedIds.empty() ? 0 : 1;
    }

    void ShowHelp(const ApplicationContext &context) const override
    {
        HelpPrinter::ShowGuessHelp(context.DisplayName(CanonicalName()));
    }

  private:
    struct State
    {
        std::string DisplayName;
        std::string MixPath;
        std::vector<int32_t> TargetIds;
        GuessBruteforcer::Options Options;
        bool Force = false;
        bool ShowedHelp = false;
    };

    static bool Parse(const std::string &displayName,
                      const std::span<char *> arguments, State &state)
    {
        state.DisplayName = displayName;

        if (arguments.empty())
        {
            HelpPrinter::ShowGuessUsage(displayName);
            state.ShowedHelp = true;
            return false;
        }

        for (int index = 0; index < static_cast<int>(arguments.size()); ++index)
        {
            const std::string_view token(arguments[index]);
            if (token == "-?" || token == "--help")
            {
                HelpPrinter::ShowGuessHelp(displayName);
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
                state.Options.GameType = *game;
                continue;
            }
            if (token == "--id")
            {
                std::string idValue;
                if (!TryReadNamedValue(arguments, index, token, "a hexadecimal ID",
                                       idValue))
                {
                    return false;
                }

                int32_t parsedId = 0;
                if (!TryParseId(idValue, parsedId))
                {
                    std::println("Invalid hexadecimal ID: {}", idValue);
                    return false;
                }

                state.TargetIds.push_back(parsedId);
                continue;
            }
            if (token == "--ext")
            {
                std::string extensionValue;
                if (!TryReadNamedValue(arguments, index, token, "an extension list",
                                       extensionValue))
                {
                    return false;
                }

                const std::vector<std::string> parsedExtensions =
                    ParseExtensions(extensionValue);
                if (parsedExtensions.empty())
                {
                    std::println("--ext requires at least one extension.");
                    return false;
                }

                state.Options.Extensions.insert(
                    state.Options.Extensions.end(),
                    parsedExtensions.begin(),
                    parsedExtensions.end());
                continue;
            }
            if (token == "--charset")
            {
                if (!TryReadNamedValue(arguments, index, token, "a character set",
                                       state.Options.Charset))
                {
                    return false;
                }
                continue;
            }
            if (token == "--prefix")
            {
                if (!TryReadNamedValue(arguments, index, token, "a prefix",
                                       state.Options.Prefix))
                {
                    return false;
                }
                continue;
            }
            if (token == "--suffix")
            {
                if (!TryReadNamedValue(arguments, index, token, "a suffix",
                                       state.Options.Suffix))
                {
                    return false;
                }
                continue;
            }
            if (token == "--min")
            {
                std::string lengthValue;
                if (!TryReadNamedValue(arguments, index, token, "a minimum length",
                                       lengthValue))
                {
                    return false;
                }
                if (!TryParseLength(lengthValue, state.Options.MinLength))
                {
                    std::println("Invalid minimum length: {}", lengthValue);
                    return false;
                }
                continue;
            }
            if (token == "--max")
            {
                std::string lengthValue;
                if (!TryReadNamedValue(arguments, index, token, "a maximum length",
                                       lengthValue))
                {
                    return false;
                }
                if (!TryParseLength(lengthValue, state.Options.MaxLength))
                {
                    std::println("Invalid maximum length: {}", lengthValue);
                    return false;
                }
                continue;
            }
            if (token == "--force")
            {
                state.Force = true;
                continue;
            }

            std::println("Invalid argument: {}", token);
            HelpPrinter::ShowGuessUsage(displayName);
            return false;
        }

        state.Options.Charset = DeduplicateCharacters(state.Options.Charset);
        state.Options.Extensions = DeduplicateStrings(state.Options.Extensions);

        if (state.MixPath.empty() && state.TargetIds.empty())
        {
            std::println("You must specify --mix FILE or at least one --id HEX.");
            return false;
        }
        if (state.Options.Extensions.empty())
        {
            std::println("You must specify at least one --ext EXT.");
            return false;
        }
        if (state.Options.Charset.empty())
        {
            std::println("--charset must not be empty.");
            return false;
        }
        if (state.Options.MinLength > state.Options.MaxLength)
        {
            std::println("--min must not be greater than --max.");
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

    static bool TryParseId(const std::string_view text, int32_t &value)
    {
        uint32_t parsedValue = 0;
        const std::from_chars_result result = std::from_chars(
            text.data(), text.data() + text.size(), parsedValue, 16);
        if (result.ec != std::errc() ||
            result.ptr != text.data() + text.size())
        {
            return false;
        }

        value = static_cast<int32_t>(parsedValue);
        return true;
    }

    static bool TryParseLength(const std::string_view text, uint32_t &value)
    {
        const std::from_chars_result result = std::from_chars(
            text.data(), text.data() + text.size(), value, 10);
        return result.ec == std::errc() &&
               result.ptr == text.data() + text.size();
    }

    static std::vector<std::string> ParseExtensions(const std::string_view text)
    {
        std::vector<std::string> extensions;
        std::size_t start = 0;
        while (start <= text.size())
        {
            const std::size_t separator = text.find(',', start);
            const std::size_t end =
                separator == std::string_view::npos ? text.size() : separator;
            const std::string_view part = text.substr(start, end - start);
            if (!part.empty())
            {
                extensions.push_back(std::string(part));
            }

            if (separator == std::string_view::npos)
            {
                break;
            }

            start = separator + 1;
        }

        return extensions;
    }

    static std::string DeduplicateCharacters(const std::string &text)
    {
        std::string uniqueCharacters;
        for (const char value : text)
        {
            if (uniqueCharacters.find(value) == std::string::npos)
            {
                uniqueCharacters.push_back(value);
            }
        }

        return uniqueCharacters;
    }

    static std::vector<std::string> DeduplicateStrings(
        const std::vector<std::string> &values)
    {
        std::vector<std::string> uniqueValues;
        for (const std::string &value : values)
        {
            if (std::find(uniqueValues.begin(), uniqueValues.end(), value) ==
                uniqueValues.end())
            {
                uniqueValues.push_back(value);
            }
        }

        return uniqueValues;
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
    Add<GuessCommand>();
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
