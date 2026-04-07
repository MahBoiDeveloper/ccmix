#include "mix_db_lmd.hpp"
#include "mixid.hpp"
#include <cstring>
#include <iostream>
#include <vector>

const char MixLMD::m_xcc_id[32] = "XCC by Olaf van der Spek\x1a\x04\x17\x27\x10\x19\x80";

MixLMD::MixLMD(t_game game)
{
    m_game_type = game;
    m_size = 52;
    addName(getDBName());
    m_id = MixID::idGen(m_game_type, getDBName());
}

void MixLMD::readDB(std::fstream &fh, uint32_t offset, uint32_t size)
{
    m_name_map.clear();
    m_size = 52;
    addName(getDBName());

    std::vector<char> data(size);
    fh.seekg(offset, std::ios_base::beg);
    fh.read(&data.at(0), size);
    
    //move pointer past most of header to entry count, total header is 52 bytes.
    const char* cursor = &data.at(0) + 48;
    
    //get count of entries
    int32_t count = 0;
    std::memcpy(&count, cursor, sizeof(count));
    cursor += 4;
    
    //retrieve each entry into the struct as a string then push to the map.
    //relies on string constructor reading to \0;
    //local mix db doesn't have descriptions.
    std::string id_data;
    while (count--) {
        //get the id for this filename
        id_data = cursor;
        int32_t id = MixID::idGen(m_game_type, id_data);
        //check if its the LMD itself, if it is skip add logic
        if(id == m_id) {
            cursor += getDBName().length() + 1;
            continue;
        }
        
        std::pair<t_id_iter,bool> rv;
        cursor += id_data.length() + 1;
        rv = m_name_map.insert(t_id_pair(id, id_data));
        if(rv.second) {
            m_size += id_data.length() + 1;
        } else {
            std::cout << id_data << " generates an ID conflict with existing entry " << 
                    rv.first->second << std::endl;
        }
    }
}

void MixLMD::writeDB(std::fstream& fh)
{    
    // this is the rest of the header that follows xcc_id
    // two 0 constants are xcc type and xcc version according to xcc spec.
    uint32_t xcc_head[] = {m_size, 0, 0, m_game_type, 
                              static_cast<uint32_t>(m_name_map.size())};
    //xcc id
    fh.write(m_xcc_id, sizeof(m_xcc_id));
    //rest of header
    fh.write(reinterpret_cast<const char*> (xcc_head), sizeof(xcc_head));
    //filenames
    for(t_id_iter it = m_name_map.begin(); it != m_name_map.end(); ++it) {
        fh.write(it->second.c_str(), it->second.size() + 1);
    }
}

std::string MixLMD::getName(int32_t id) const
{
    t_id_iter rv = m_name_map.find(id);
    
    if(rv != m_name_map.end()){
        return rv->second;
    }
    
    return "[id]" + MixID::idStr(id);
}

bool MixLMD::addName(const std::string& name)
{
    std::pair<t_id_iter,bool> rv;
    rv = m_name_map.insert(t_id_pair(MixID::idGen(m_game_type, name), name));
    if(rv.second) {
        m_size += name.length() + 1;
        return true;
    } else {
        std::cout << name << " generates an ID conflict with existing entry " << 
                rv.first->second << std::endl;
        return false;
    }
    return false;
}

bool MixLMD::deleteName(const std::string& name)
{
    return deleteName(MixID::idGen(m_game_type, name));
}

bool MixLMD::deleteName(int32_t id)
{
    t_id_iter rv = m_name_map.find(id);
    if(rv == m_name_map.end()){
        std::cout << "Name not found in local DB." << std::endl;
        return false;
    }
    
    m_size -= rv->second.size() + 1;
    m_name_map.erase(id);
    return true;
}
