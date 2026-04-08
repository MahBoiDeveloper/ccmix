#include "MixLmd.hpp"
#include "MixId.hpp"

#include <nlohmann/json.hpp>

#include <cstring>
#include <optional>
#include <print>
#include <string_view>
#include <vector>

const char MixLmd::m_xcc_id[32] = "XCC by Olaf van der Spek\x1a\x04\x17\x27\x10\x19\x80";

class MixLmdSupport
{
  public:
    static std::optional<Game> DecodeStoredGame(const uint32_t storedValue)
    {
        switch (storedValue)
        {
        case static_cast<uint32_t>(Game::TD):
            return Game::TD;
        case static_cast<uint32_t>(Game::RA):
            return Game::RA;
        case static_cast<uint32_t>(Game::TS):
            return Game::TS;
        case static_cast<uint32_t>(Game::D2):
            return Game::D2;
        case static_cast<uint32_t>(Game::D2K):
            return Game::D2K;
        case static_cast<uint32_t>(Game::RA2):
            return Game::RA2;
        default:
            return std::nullopt;
        }
    }

    static std::optional<Game> ParseGame(const std::string_view token)
    {
        if (token == "td")
        {
            return Game::TD;
        }
        if (token == "ra")
        {
            return Game::RA;
        }
        if (token == "ts")
        {
            return Game::TS;
        }
        if (token == "d2")
        {
            return Game::D2;
        }
        if (token == "d2k")
        {
            return Game::D2K;
        }
        if (token == "ra2")
        {
            return Game::RA2;
        }

        return std::nullopt;
    }

    static std::string_view GameName(const Game game)
    {
        switch (game)
        {
        case Game::TD:
            return "td";
        case Game::RA:
            return "ra";
        case Game::TS:
            return "ts";
        case Game::D2:
            return "d2";
        case Game::D2K:
            return "d2k";
        case Game::RA2:
            return "ra2";
        default:
            return "unknown";
        }
    }
};

MixLmd::MixLmd(Game game)
{
    Reset(game);
}

void MixLmd::Reset(Game game)
{
    m_game_type = game;
    m_name_map.clear();
    m_size = 52;
    m_id = MixId::IdGen(m_game_type, GetDbName());
    AddName(GetDbName());
}

bool MixLmd::ReadDb(std::fstream &fh, uint32_t offset, uint32_t size)
{
    if (size < 52)
    {
        Reset(m_game_type);
        return false;
    }

    std::vector<char> data(size);
    fh.seekg(offset, std::ios_base::beg);
    fh.read(data.data(), size);
    if (fh.gcount() != static_cast<std::streamsize>(size))
    {
        Reset(m_game_type);
        return false;
    }

    if (std::memcmp(data.data(), m_xcc_id, sizeof(m_xcc_id)) != 0)
    {
        Reset(m_game_type);
        return false;
    }

    uint32_t storedSize = 0;
    std::memcpy(&storedSize, data.data() + 32, sizeof(storedSize));
    if (storedSize < 52 || storedSize > size)
    {
        Reset(m_game_type);
        return false;
    }

    uint32_t storedGameValue = 0;
    std::memcpy(&storedGameValue, data.data() + 44, sizeof(storedGameValue));
    const std::optional<Game> storedGame =
        MixLmdSupport::DecodeStoredGame(storedGameValue);
    if (!storedGame.has_value())
    {
        Reset(m_game_type);
        return false;
    }
    Reset(*storedGame);

    //move pointer past most of header to entry count, total header is 52 bytes.
    const char *cursor = data.data() + 48;
    const char *limit = data.data() + storedSize;

    //get count of entries
    int32_t count = 0;
    std::memcpy(&count, cursor, sizeof(count));
    cursor += 4;
    if (count < 0)
    {
        Reset(m_game_type);
        return false;
    }

    //retrieve each entry into the struct as a string then push to the map.
    //relies on string constructor reading to \0;
    //local mix db doesn't have descriptions.
    std::string id_data;
    for (int32_t entryIndex = 0; entryIndex < count; ++entryIndex)
    {
        if (cursor >= limit)
        {
            Reset(m_game_type);
            return false;
        }

        const char *terminator = static_cast<const char *>(
            std::memchr(cursor, '\0', static_cast<std::size_t>(limit - cursor)));
        if (terminator == nullptr)
        {
            Reset(m_game_type);
            return false;
        }

        //get the id for this filename
        id_data.assign(cursor, terminator);
        int32_t id = MixId::IdGen(m_game_type, id_data);
        //check if its the LMD itself, if it is skip add logic
        if (id == m_id)
        {
            cursor = terminator + 1;
            continue;
        }

        std::pair<IdIterator, bool> rv;
        cursor = terminator + 1;
        rv = m_name_map.insert(IdPair(id, id_data));
        if (rv.second)
        {
            m_size += id_data.length() + 1;
        }
        else
        {
            std::println("{} generates an ID conflict with existing entry {}",
                         id_data, rv.first->second);
        }
    }

    return true;
}

void MixLmd::WriteDb(std::fstream &fh)
{
    // this is the rest of the header that follows xcc_id
    // two 0 constants are xcc type and xcc version according to xcc spec.
    const uint32_t xcc_head[] = {
        GetSize(),
        0,
        0,
        MixNumeric::ToUint32(static_cast<int>(m_game_type), "game type"),
        MixNumeric::ToUint32(m_name_map.size(), "local database entry count")};
    //xcc id
    fh.write(m_xcc_id, sizeof(m_xcc_id));
    //rest of header
    fh.write(reinterpret_cast<const char *>(xcc_head), sizeof(xcc_head));
    //filenames
    for (IdIterator it = m_name_map.begin(); it != m_name_map.end(); ++it)
    {
        fh.write(it->second.c_str(), it->second.size() + 1);
    }
}

bool MixLmd::ReadJson(const nlohmann::json &document)
{
    if (!document.is_object())
    {
        return false;
    }
    if (document.contains("type") &&
        (!document.at("type").is_string() ||
         document.at("type").get<std::string>() != "lmd"))
    {
        return false;
    }

    Game game = m_game_type;
    if (document.contains("game"))
    {
        if (!document.at("game").is_string())
        {
            return false;
        }

        const std::optional<Game> parsedGame =
            MixLmdSupport::ParseGame(document.at("game").get<std::string>());
        if (!parsedGame.has_value())
        {
            return false;
        }
        game = *parsedGame;
    }

    if (!document.contains("entries") || !document.at("entries").is_array())
    {
        return false;
    }

    Reset(game);
    for (const nlohmann::json &entry : document.at("entries"))
    {
        if (!entry.is_object() ||
            !entry.contains("name") ||
            !entry.at("name").is_string())
        {
            return false;
        }

        const std::string name = entry.at("name").get<std::string>();
        if (name == GetDbName())
        {
            continue;
        }

        const int32_t computedId = MixId::IdGen(m_game_type, name);
        if (entry.contains("id") &&
            (!entry.at("id").is_number_integer() ||
             entry.at("id").get<int32_t>() != computedId))
        {
            return false;
        }

        if (!AddName(name))
        {
            return false;
        }
    }

    return true;
}

nlohmann::json MixLmd::WriteJson() const
{
    nlohmann::json entries = nlohmann::json::array();
    for (IdIterator it = m_name_map.begin(); it != m_name_map.end(); ++it)
    {
        if (it->first == m_id && it->second == GetDbName())
        {
            continue;
        }

        entries.push_back(
            {
                {"id", it->first},
                {"name", it->second}
            });
    }

    return {
        {"type", "lmd"},
        {"game", MixLmdSupport::GameName(m_game_type)},
        {"entries", entries}
    };
}

std::string MixLmd::GetName(int32_t id) const
{
    IdIterator rv = m_name_map.find(id);

    if (rv != m_name_map.end())
    {
        return rv->second;
    }

    return "[id]" + MixId::IdStr(id);
}

bool MixLmd::AddName(const std::string &name)
{
    std::pair<IdIterator, bool> rv;
    rv = m_name_map.insert(IdPair(MixId::IdGen(m_game_type, name), name));
    if (rv.second)
    {
        m_size += name.length() + 1;
        return true;
    }
    else
    {
        std::println("{} generates an ID conflict with existing entry {}",
                     name, rv.first->second);
        return false;
    }
    return false;
}

bool MixLmd::DeleteName(const std::string &name)
{
    return DeleteName(MixId::IdGen(m_game_type, name));
}

bool MixLmd::DeleteName(int32_t id)
{
    IdIterator rv = m_name_map.find(id);
    if (rv == m_name_map.end())
    {
        std::println("Name not found in local DB.");
        return false;
    }

    m_size -= rv->second.size() + 1;
    m_name_map.erase(id);
    return true;
}
