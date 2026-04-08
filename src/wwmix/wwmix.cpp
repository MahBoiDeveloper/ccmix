#include "wwmix.hpp"

#include "archive_cli.hpp"
#include "mix_db_gmd.hpp"
#include "mix_header.hpp"
#include "mixid.hpp"

#include "cryptopp/blowfish.h"
#include "cryptopp/modes.h"

#include <array>
#include <cstring>
#include <fstream>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace WwMix
{
class GameCodec
{
  public:
    static std::optional<Game> Parse(const std::string_view token)
    {
        switch (Match(token))
        {
        case Match("td"):
            return GameTd;
        case Match("ra"):
            return GameRa;
        case Match("ts"):
            return GameTs;
        case Match("ra2"):
            return GameRa2;
        default:
            return std::nullopt;
        }
    }

    static std::string_view Name(const Game game)
    {
        switch (game)
        {
        case GameTd:
            return "td";
        case GameRa:
            return "ra";
        case GameTs:
            return "ts";
        case GameRa2:
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

class GmdCommand
{
  public:
    explicit GmdCommand(std::string_view displayName)
        : m_displayName(displayName)
    {
    }

    int Run(const std::span<char *> arguments)
    {
        if (!Parse(arguments))
        {
            return m_showedHelp ? 0 : 1;
        }

        MixGmd gmd;
        std::fstream inputFile(m_inputPath, std::ios_base::in | std::ios_base::binary);
        if (!inputFile.is_open())
        {
            std::println("Failed to open global mix database: {}", m_inputPath);
            return 1;
        }

        gmd.ReadDb(inputFile);
        inputFile.close();

        const std::optional<NamePairs> additions = ReadAdditions(m_additionsPath);
        if (!additions.has_value())
        {
            return 1;
        }

        for (const auto &entry : *additions)
        {
            std::println("{} - {}", entry.first, entry.second);
            if (!gmd.AddName(m_gameType, entry.first, entry.second))
            {
                std::println("Failed to add entry: {}", entry.first);
                return 1;
            }
        }

        std::fstream outputFile(m_outputPath, std::ios_base::out | std::ios_base::binary);
        if (!outputFile.is_open())
        {
            std::println("Failed to open output file: {}", m_outputPath);
            return 1;
        }

        gmd.WriteDb(outputFile);
        outputFile.close();

        std::println("Wrote {} entries into the {} section of {}.",
                     additions->size(), GameCodec::Name(m_gameType), m_outputPath);
        return 0;
    }

  private:
    using NamePairs = std::vector<std::pair<std::string, std::string>>;

    bool Parse(const std::span<char *> arguments)
    {
        if (arguments.empty())
        {
            HelpPrinter::ShowGmdUsage(m_displayName);
            m_showedHelp = true;
            return false;
        }

        if (ParseLegacy(arguments))
        {
            return true;
        }

        for (int index = 0; index < static_cast<int>(arguments.size()); ++index)
        {
            const std::string_view token(arguments[index]);
            if (token == "-?" || token == "--help")
            {
                HelpPrinter::ShowGmdHelp(m_displayName);
                m_showedHelp = true;
                return false;
            }
            if (token == "--input")
            {
                if (!TryReadNamedValue(arguments, index, token, "an input GMD path",
                                       m_inputPath))
                {
                    return false;
                }
                continue;
            }
            if (token == "--additions")
            {
                if (!TryReadNamedValue(arguments, index, token, "an additions CSV path",
                                       m_additionsPath))
                {
                    return false;
                }
                continue;
            }
            if (token == "--output")
            {
                if (!TryReadNamedValue(arguments, index, token, "an output GMD path",
                                       m_outputPath))
                {
                    return false;
                }
                continue;
            }
            if (token == "--game")
            {
                std::string gameName;
                if (!TryReadNamedValue(arguments, index, token, "a game name", gameName))
                {
                    return false;
                }
                const std::optional<Game> game = GameCodec::Parse(gameName);
                if (!game.has_value())
                {
                    std::println("--game is either td, ra, ts or ra2.");
                    return false;
                }
                m_gameType = *game;
                continue;
            }

            std::println("Invalid argument: {}", token);
            HelpPrinter::ShowGmdUsage(m_displayName);
            return false;
        }

        if (m_inputPath.empty() || m_additionsPath.empty() || m_outputPath.empty())
        {
            std::println("You must specify input, additions, and output paths.");
            HelpPrinter::ShowGmdUsage(m_displayName);
            return false;
        }

        return true;
    }

    bool ParseLegacy(const std::span<char *> arguments)
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

        m_inputPath = arguments[0];
        m_additionsPath = arguments[1];
        m_outputPath = arguments[2];
        if (arguments.size() == 4)
        {
            const std::optional<Game> game = GameCodec::Parse(arguments[3]);
            if (!game.has_value())
            {
                std::println("GAME must be td, ra, ts or ra2.");
                return false;
            }
            m_gameType = *game;
        }

        return true;
    }

    bool TryReadNamedValue(const std::span<char *> arguments, int &index,
                           const std::string_view optionName,
                           const std::string_view valueDescription,
                           std::string &value) const
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

    std::string m_displayName;
    std::string m_inputPath;
    std::string m_additionsPath;
    std::string m_outputPath;
    Game m_gameType = GameTd;
    bool m_showedHelp = false;
};

class KeyCommand
{
  public:
    explicit KeyCommand(std::string_view displayName)
        : m_displayName(displayName)
    {
    }

    int Run(const std::span<char *> arguments)
    {
        if (!Parse(arguments))
        {
            return m_showedHelp ? 0 : 1;
        }

        std::fstream file(m_mixPath, std::ios_base::in | std::ios_base::binary);
        if (!file.is_open())
        {
            std::println("Failed to open mix file: {}", m_mixPath);
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
        if (flags != MixEncryptedFlag && flags != MixEncryptedFlag + MixChecksumFlag)
        {
            std::println("{} does not contain an encrypted MIX header.", m_mixPath);
            return 1;
        }

        file.clear();
        file.seekg(0, std::ios::beg);

        MixHeader header(m_gameType);
        if (!header.ReadHeader(file))
        {
            std::println("Failed to decode the encrypted MIX header.");
            return 1;
        }

        std::array<char, 8> firstBlock = {0};
        file.clear();
        file.seekg(84, std::ios::beg);
        file.read(firstBlock.data(), static_cast<std::streamsize>(firstBlock.size()));
        if (file.gcount() != static_cast<std::streamsize>(firstBlock.size()))
        {
            std::println("Failed to read the first encrypted header block.");
            return 1;
        }

        CryptoPP::ECB_Mode<CryptoPP::Blowfish>::Decryption blowfish;
        blowfish.SetKey(reinterpret_cast<const uint8_t *>(header.GetKey()), 56);
        blowfish.ProcessString(reinterpret_cast<uint8_t *>(firstBlock.data()),
                               static_cast<unsigned int>(firstBlock.size()));

        std::println("Mix file: {}", m_mixPath);
        std::println("Game hint: {}", GameCodec::Name(m_gameType));
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

  private:
    bool Parse(const std::span<char *> arguments)
    {
        if (arguments.empty())
        {
            HelpPrinter::ShowKeyUsage(m_displayName);
            m_showedHelp = true;
            return false;
        }

        for (int index = 0; index < static_cast<int>(arguments.size()); ++index)
        {
            const std::string_view token(arguments[index]);
            if (token == "-?" || token == "--help")
            {
                HelpPrinter::ShowKeyHelp(m_displayName);
                m_showedHelp = true;
                return false;
            }
            if (token == "--mix")
            {
                if (!TryReadNamedValue(arguments, index, token, "a mix file", m_mixPath))
                {
                    return false;
                }
                continue;
            }
            if (token == "--game")
            {
                std::string gameName;
                if (!TryReadNamedValue(arguments, index, token, "a game name", gameName))
                {
                    return false;
                }
                const std::optional<Game> game = GameCodec::Parse(gameName);
                if (!game.has_value())
                {
                    std::println("--game is either td, ra, ts or ra2.");
                    return false;
                }
                m_gameType = *game;
                continue;
            }

            std::println("Invalid argument: {}", token);
            HelpPrinter::ShowKeyUsage(m_displayName);
            return false;
        }

        if (m_mixPath.empty())
        {
            std::println("You must specify --mix FILE.");
            HelpPrinter::ShowKeyUsage(m_displayName);
            return false;
        }

        return true;
    }

    bool TryReadNamedValue(const std::span<char *> arguments, int &index,
                           const std::string_view optionName,
                           const std::string_view valueDescription,
                           std::string &value) const
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

    std::string m_displayName;
    std::string m_mixPath;
    Game m_gameType = GameRa;
    bool m_showedHelp = false;
};

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

    if (firstToken == "help")
    {
        if (argc == 2)
        {
            HelpPrinter::ShowGeneral(programName);
            return 0;
        }

        const std::string topic = argv[2];
        if (topic == "mix")
        {
            ArchiveCli::ShowHelp(programName);
            return 0;
        }
        if (topic == "gmd" || topic == "gmdedit")
        {
            HelpPrinter::ShowGmdHelp(programName + " gmd");
            return 0;
        }
        if (topic == "key" || topic == "mixkey")
        {
            HelpPrinter::ShowKeyHelp(programName + " key");
            return 0;
        }

        std::println("Unknown help topic: {}", topic);
        HelpPrinter::ShowGeneral(programName);
        return 1;
    }

    if (ArchiveCli::IsSwitchToken(firstToken) || ArchiveCli::IsCommandToken(firstToken))
    {
        return ArchiveCli::Run(
            programPath,
            programName,
            std::span<char *>(argv + 1, static_cast<std::size_t>(argc - 1)));
    }

    if (firstToken == "mix")
    {
        if (argc == 2 ||
            (argc == 3 &&
             (std::string_view(argv[2]) == "-?" ||
              std::string_view(argv[2]) == "--help")))
        {
            ArchiveCli::ShowHelp(programName + " mix");
            return 0;
        }

        return ArchiveCli::Run(
            programPath,
            programName + " mix",
            std::span<char *>(argv + 2, static_cast<std::size_t>(argc - 2)));
    }

    if (firstToken == "gmd" || firstToken == "gmdedit")
    {
        GmdCommand command(programName + " gmd");
        return command.Run(
            std::span<char *>(argv + 2, static_cast<std::size_t>(argc - 2)));
    }

    if (firstToken == "key" || firstToken == "mixkey")
    {
        KeyCommand command(programName + " key");
        return command.Run(
            std::span<char *>(argv + 2, static_cast<std::size_t>(argc - 2)));
    }

    std::println("Unknown command: {}", firstToken);
    HelpPrinter::ShowGeneral(programName);
    return 1;
}
} // namespace WwMix
