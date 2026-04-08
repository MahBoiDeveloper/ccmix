#include "mix_db_lmd.hpp"
#include "mixid.hpp"
#include <cstring>
#include <print>
#include <vector>

const char MixLmd::m_xcc_id[32] = "XCC by Olaf van der Spek\x1a\x04\x17\x27\x10\x19\x80";

MixLmd::MixLmd(Game game)
{
    m_game_type = game;
    m_size = 52;
    AddName(GetDbName());
    m_id = MixId::IdGen(m_game_type, GetDbName());
}

void MixLmd::ReadDb(std::fstream &fh, uint32_t offset, uint32_t size)
{
    m_name_map.clear();
    m_size = 52;
    AddName(GetDbName());

    std::vector<char> data(size);
    fh.seekg(offset, std::ios_base::beg);
    fh.read(&data.at(0), size);

    //move pointer past most of header to entry count, total header is 52 bytes.
    const char *cursor = &data.at(0) + 48;

    //get count of entries
    int32_t count = 0;
    std::memcpy(&count, cursor, sizeof(count));
    cursor += 4;

    //retrieve each entry into the struct as a string then push to the map.
    //relies on string constructor reading to \0;
    //local mix db doesn't have descriptions.
    std::string id_data;
    while (count--)
    {
        //get the id for this filename
        id_data = cursor;
        int32_t id = MixId::IdGen(m_game_type, id_data);
        //check if its the LMD itself, if it is skip add logic
        if (id == m_id)
        {
            cursor += GetDbName().length() + 1;
            continue;
        }

        std::pair<IdIterator, bool> rv;
        cursor += id_data.length() + 1;
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
