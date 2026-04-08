#include "mix_db_gmd.hpp"
#include "mix_numeric.hpp"

MixGmd::MixGmd()
    : m_td_list(GameTd),
      m_ra_list(GameRa),
      m_ts_list(GameTs),
      m_ra2_list(GameRa2)
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

std::string MixGmd::GetName(Game game, int32_t id) const
{
    switch (game)
    {
    case GameTd:
        return m_td_list.GetName(id);
    case GameRa:
        return m_ra_list.GetName(id);
    case GameTs:
        return m_ts_list.GetName(id);
    case GameRa2:
        return m_ra2_list.GetName(id);
    default:
        return "";
    }
}

bool MixGmd::AddName(Game game, const std::string &name, const std::string &desc)
{
    switch (game)
    {
    case GameTd:
        return m_td_list.AddName(name, desc);
    case GameRa:
        return m_ra_list.AddName(name, desc);
    case GameTs:
        return m_ts_list.AddName(name, desc);
    case GameRa2:
        return m_ra2_list.AddName(name, desc);
    default:
        return false;
    }
}

bool MixGmd::DeleteName(Game game, const std::string &name)
{
    switch (game)
    {
    case GameTd:
        return m_td_list.DeleteName(name);
    case GameRa:
        return m_ra_list.DeleteName(name);
    case GameTs:
        return m_ts_list.DeleteName(name);
    case GameRa2:
        return m_ra2_list.DeleteName(name);
    default:
        return false;
    }
}
