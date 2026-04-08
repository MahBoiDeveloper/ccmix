#include "mix_game_db.hpp"
#include "mixid.hpp"
#include <cstring>
#include <print>

MixGameDb::MixGameDb(Game game)
    : m_size(0),
      m_entries(0),
      m_game_type(game)
{
}

void MixGameDb::ReadDb(const char *data, uint32_t offset)
{
    m_name_map.clear();
    m_size = 0;
    m_entries = 0;
    data += offset;
    //get count of entries
    std::memcpy(&m_entries, data, sizeof(m_entries));
    m_size += 4;
    data += 4;

    //retrieve each entry into the struct as a string then push to the map.
    //relies on string constructor reading to 0;
    IdData id_data;
    for (uint32_t i = 0; i != m_entries; i++)
    {
        std::pair<IdIterator, bool> rv;

        //data is incremented and read twice, once for filename, once for desc.
        id_data.name = data;
        data += id_data.name.length() + 1;
        m_size += id_data.name.length() + 1;
        id_data.description = data;
        data += id_data.description.length() + 1;
        m_size += id_data.description.length() + 1;
        //attempt to insert data and figure out if we had a collision.
        rv = m_name_map.insert(IdPair(MixId::IdGen(m_game_type,
                                                   id_data.name),
                                      id_data));
    }
}

void MixGameDb::WriteDb(std::fstream &fh)
{
    //first record how many entries we have for this db.

    if (!fh.is_open())
    {
        std::print("File not open to write DB\n");
    }
    fh.write(reinterpret_cast<char *>(&m_entries), sizeof(uint32_t));
    //filenames
    for (IdIterator it = m_name_map.begin(); it != m_name_map.end(); ++it)
    {
        fh.write(it->second.name.c_str(), it->second.name.size() + 1);
        fh.write(it->second.description.c_str(), it->second.description.size() + 1);
    }
}

std::string MixGameDb::GetName(int32_t id) const
{
    IdIterator rv = m_name_map.find(id);

    if (rv != m_name_map.end())
    {
        return rv->second.name;
    }

    return "[id]" + MixId::IdStr(id);
}

bool MixGameDb::AddName(const std::string &name, const std::string &description)
{
    IdData id_data;
    id_data.name = name;
    id_data.description = description;

    std::pair<IdIterator, bool> rv;
    rv = m_name_map.insert(IdPair(MixId::IdGen(m_game_type,
                                               name),
                                  id_data));
    if (rv.second)
    {
        m_size += name.length() + 1;
        m_size += description.length() + 1;
        m_entries++;
        return true;
    }
    else
    {
        std::println("{} generates an ID conflict with existing entry {}",
                     name, rv.first->second.name);
        return false;
    }
    return false;
}

bool MixGameDb::DeleteName(const std::string &name)
{
    std::println("{} DeleteName not implemented yet", name);
    return false;
}
