#include "mix_db_gamedb.h"
#include "mixid.h"
#include <iostream>

using namespace std;

MixGameDB::MixGameDB(GameKind game) : m_size(0), m_game_type(game)
{
}

void MixGameDB::readDB(const char* data, uint32_t offset)
{
    data += offset;
    //get count of entries
    m_entries = *reinterpret_cast<const uint32_t*>(data);
    m_size += 4;
    data += 4;
    
    //retrieve each entry into the struct as a string then push to the map.
    //relies on string constructor reading to 0;
    IdData id_data;
    for(uint32_t i = 0; i != m_entries; i++){
        std::pair<IdIter,bool> rv;
        
        //data is incremented and read twice, once for filename, once for desc.
        id_data.name = data;
        data += id_data.name.length() + 1;
        m_size += id_data.name.length() + 1;
        id_data.description = data;
        data += id_data.description.length() + 1;
        m_size += id_data.description.length() + 1;
        //attempt to insert data and figure out if we had a collision.
        rv = m_name_map.insert(IdPair(MixID::GenerateID(m_game_type,
                        id_data.name), id_data));
    }
}

void MixGameDB::writeDB(std::fstream& fh)
{
    //first record how many entries we have for this db.
    
    if(!fh.is_open()){
        std::wcout << "File not Open to write DB\n";
    }
    fh.write(reinterpret_cast<char*>(&m_entries), sizeof(uint32_t));
    //filenames
    for(IdIter it = m_name_map.begin(); it != m_name_map.end(); ++it) {
        fh.write(it->second.name.c_str(), it->second.name.size() + 1);
        fh.write(it->second.description.c_str(), it->second.description.size() + 1);
    }
}

std::string MixGameDB::getName(int32_t id)
{
    IdIter rv = m_name_map.find(id);
    
    if(rv != m_name_map.end()){
        return rv->second.name;
    }
    
    return "[id]" + MixID::ToHexString(id);
}

bool MixGameDB::addName(std::string name, std::string description)
{
    IdData id_data;
    id_data.name = name;
    id_data.description = description;
    
    std::pair<IdIter,bool> rv;
    rv = m_name_map.insert(IdPair(MixID::GenerateID(m_game_type,
                    name), id_data));
    if(rv.second) {
        m_size += name.length() + 1;
        m_size += description.length() + 1;
        m_entries++;
        return true;
    } else {
        cout << name << " generates an ID conflict with existing entry " << 
                rv.first->second.name << endl;
        return false;
    }
    return false;
}

bool MixGameDB::deleteName(std::string name)
{
    cout << name << " DeleteName not implemented yet" << endl;
    return false;
}