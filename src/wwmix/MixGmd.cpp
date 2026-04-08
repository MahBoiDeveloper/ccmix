#include "MixGmd.hpp"
#include "MixNumeric.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>

namespace
{
namespace fs = std::filesystem;

constexpr int GmdCacheVersion = 1;

struct GmdSourceInfo
{
    std::string path;
    std::uintmax_t size = 0;
    int64_t modified = 0;
};

std::string NormalizePath(const std::string &path)
{
    std::error_code error;
    const fs::path absolutePath = fs::absolute(fs::u8path(path), error);
    if (error)
    {
        const fs::path normalizedPath = fs::u8path(path).lexically_normal();
        const std::u8string normalizedPathText = normalizedPath.u8string();
        return std::string(
            normalizedPathText.begin(), normalizedPathText.end());
    }

    const std::u8string normalizedPathText =
        absolutePath.lexically_normal().u8string();
    return std::string(normalizedPathText.begin(), normalizedPathText.end());
}

bool TryGetSourceInfo(const std::string &sourcePath, GmdSourceInfo &info)
{
    const std::string normalizedPath = NormalizePath(sourcePath);
    std::error_code error;
    const fs::path path = fs::u8path(normalizedPath);
    const std::uintmax_t size = fs::file_size(path, error);
    if (error)
    {
        return false;
    }

    const fs::file_time_type modifiedTime = fs::last_write_time(path, error);
    if (error)
    {
        return false;
    }

    info.path = normalizedPath;
    info.size = size;
    info.modified = static_cast<int64_t>(
        modifiedTime.time_since_epoch().count());
    return true;
}
} // namespace

MixGmd::MixGmd()
    : m_td_list(Game::TD),
      m_ra_list(Game::RA),
      m_ts_list(Game::TS),
      m_ra2_list(Game::RA2)
{
    m_db_array.push_back(&m_td_list);
    m_db_array.push_back(&m_ra_list);
    m_db_array.push_back(&m_ts_list);
    m_db_array.push_back(&m_ra2_list);
}

void MixGmd::ReadDb(std::fstream &fh)
{
    uint32_t offset = 0;

    // get file size
    fh.seekg(0, std::ios::beg);
    const std::streamoff begin = fh.tellg();
    fh.seekg(0, std::ios::end);
    const std::streamoff end = fh.tellg();
    if (begin < 0 || end < begin)
    {
        return;
    }

    const uint32_t size = MixNumeric::ToUint32(end - begin, "global mix database size");
    if (size == 0)
    {
        return;
    }

    //read file into data buffer
    std::vector<char> data(size);

    fh.seekg(0, std::ios::beg);
    fh.read(data.data(), static_cast<std::streamsize>(size));
    if (fh.gcount() != static_cast<std::streamsize>(size))
    {
        return;
    }

    // read file from buffer into respective dbs
    for (std::size_t i = 0; i < m_db_array.size(); ++i)
    {
        m_db_array[i]->ReadDb(data.data(), offset);
        offset += m_db_array[i]->GetSize();
    }
}

void MixGmd::WriteDb(std::fstream &fh)
{
    for (std::size_t i = 0; i < m_db_array.size(); ++i)
    {
        m_db_array[i]->WriteDb(fh);
    }
}

bool MixGmd::ReadJson(const nlohmann::json &document)
{
    if (!document.is_object())
    {
        return false;
    }
    if (document.contains("type") &&
        (!document.at("type").is_string() ||
         document.at("type").get<std::string>() != "gmd"))
    {
        return false;
    }
    if (!document.contains("games") || !document.at("games").is_object())
    {
        return false;
    }

    const nlohmann::json &games = document.at("games");
    if (!games.contains("td") ||
        !games.contains("ra") ||
        !games.contains("ts") ||
        !games.contains("ra2"))
    {
        return false;
    }

    MixGameDb td(Game::TD);
    MixGameDb ra(Game::RA);
    MixGameDb ts(Game::TS);
    MixGameDb ra2(Game::RA2);

    if (!td.ReadJson(games.at("td")) ||
        !ra.ReadJson(games.at("ra")) ||
        !ts.ReadJson(games.at("ts")) ||
        !ra2.ReadJson(games.at("ra2")))
    {
        return false;
    }

    m_td_list = std::move(td);
    m_ra_list = std::move(ra);
    m_ts_list = std::move(ts);
    m_ra2_list = std::move(ra2);
    return true;
}

nlohmann::json MixGmd::WriteJson() const
{
    return {
        {"type", "gmd"},
        {"games",
         {
             {"td", m_td_list.WriteJson()},
             {"ra", m_ra_list.WriteJson()},
             {"ts", m_ts_list.WriteJson()},
             {"ra2", m_ra2_list.WriteJson()}
         }}
    };
}

bool MixGmd::Load(const std::string &sourcePath, const std::string &cachePath,
                  const bool allowStaleCache)
{
    if (ReadCache(sourcePath, cachePath, false))
    {
        return true;
    }

    std::fstream sourceFile;
    sourceFile.open(
        fs::u8path(sourcePath), std::ios::in | std::ios::binary);
    if (sourceFile.is_open())
    {
        ReadDb(sourceFile);
        sourceFile.close();
        WriteCache(sourcePath, cachePath);
        return true;
    }

    if (allowStaleCache)
    {
        return ReadCache(sourcePath, cachePath, true);
    }

    return false;
}

bool MixGmd::WriteCache(const std::string &sourcePath,
                        const std::string &cachePath) const
{
    GmdSourceInfo sourceInfo;
    if (!TryGetSourceInfo(sourcePath, sourceInfo))
    {
        return false;
    }

    const nlohmann::json exportDocument = WriteJson();
    nlohmann::json cacheDocument =
    {
        {"version", GmdCacheVersion},
        {"source",
         {
             {"path", sourceInfo.path},
             {"size", static_cast<uint64_t>(sourceInfo.size)},
             {"modified", sourceInfo.modified}
         }},
        {"games", exportDocument.at("games")}
    };

    std::ofstream cacheFile;
    cacheFile.open(fs::u8path(cachePath), std::ios::out | std::ios::trunc);
    if (!cacheFile.is_open())
    {
        return false;
    }

    cacheFile << cacheDocument.dump(2);
    return cacheFile.good();
}

bool MixGmd::ReadCache(const std::string &sourcePath,
                       const std::string &cachePath,
                       const bool allowStaleCache)
{
    std::ifstream cacheFile;
    cacheFile.open(fs::u8path(cachePath), std::ios::in);
    if (!cacheFile.is_open())
    {
        return false;
    }

    nlohmann::json cacheDocument;
    try
    {
        cacheFile >> cacheDocument;
    }
    catch (const nlohmann::json::exception &)
    {
        return false;
    }

    if (!cacheDocument.is_object() ||
        cacheDocument.value("version", 0) != GmdCacheVersion ||
        !cacheDocument.contains("games") ||
        !cacheDocument.at("games").is_object())
    {
        return false;
    }

    if (!allowStaleCache)
    {
        GmdSourceInfo sourceInfo;
        if (!TryGetSourceInfo(sourcePath, sourceInfo))
        {
            return false;
        }

        if (!cacheDocument.contains("source") ||
            !cacheDocument.at("source").is_object())
        {
            return false;
        }

        const nlohmann::json &cachedSource = cacheDocument.at("source");
        if (cachedSource.value("path", std::string()) != sourceInfo.path ||
            cachedSource.value("size", uint64_t()) !=
                static_cast<uint64_t>(sourceInfo.size) ||
            cachedSource.value("modified", int64_t()) != sourceInfo.modified)
        {
            return false;
        }
    }

    return ReadJson(cacheDocument);
}

std::string MixGmd::GetName(Game game, int32_t id) const
{
    switch (game)
    {
    case Game::TD:
        return m_td_list.GetName(id);
    case Game::RA:
        return m_ra_list.GetName(id);
    case Game::TS:
        return m_ts_list.GetName(id);
    case Game::RA2:
        return m_ra2_list.GetName(id);
    default:
        return "";
    }
}

bool MixGmd::AddName(Game game, const std::string &name, const std::string &desc)
{
    switch (game)
    {
    case Game::TD:
        return m_td_list.AddName(name, desc);
    case Game::RA:
        return m_ra_list.AddName(name, desc);
    case Game::TS:
        return m_ts_list.AddName(name, desc);
    case Game::RA2:
        return m_ra2_list.AddName(name, desc);
    default:
        return false;
    }
}

bool MixGmd::DeleteName(Game game, const std::string &name)
{
    switch (game)
    {
    case Game::TD:
        return m_td_list.DeleteName(name);
    case Game::RA:
        return m_ra_list.DeleteName(name);
    case Game::TS:
        return m_ts_list.DeleteName(name);
    case Game::RA2:
        return m_ra2_list.DeleteName(name);
    default:
        return false;
    }
}
