#include "mix_db_gmd.h"
#include <iostream>

MixGMD::MixGMD() :
m_td_list(TD),
m_ra_list(RA),
m_ts_list(TS),
m_ra2_list(RA2)
{
    m_db_array.push_back(&m_td_list);
    m_db_array.push_back(&m_ra_list);
    m_db_array.push_back(&m_ts_list);
    m_db_array.push_back(&m_ra2_list);
}

void MixGMD::readDB(std::fstream &fh)
{
    uint32_t begin, end, size, offset;
    
    // get file size
    fh.seekg(0, std::ios::beg);
    begin = fh.tellg();
    fh.seekg(0, std::ios::end);
    end = fh.tellg();
    size = end - begin;
    offset = 0;
    
    //read file into data buffer
    //char data[size];
	std::vector<char> data(size);

    fh.seekg(0, std::ios::beg);
    fh.read(&data.at(0), size);
    
    // read file from buffer into respective dbs
    for (uint32_t i = 0; i < m_db_array.size(); i++){
        m_db_array[i]->readDB(&data.at(0), offset);
        offset += m_db_array[i]->getSize();
    }
}

void MixGMD::writeDB(std::fstream& fh)
{
    for (unsigned int i = 0; i < m_db_array.size(); i++){
        if (!m_db_array[i]) continue;
        m_db_array[i]->writeDB(fh);
    }
}

std::string MixGMD::getName(GameKind game, int32_t id)
{
    switch(game){
        case TD:
            return m_td_list.getName(id);
        case RA:
            return m_ra_list.getName(id);
        case TS:
            return m_ts_list.getName(id);
        case RA2:
            return m_ra2_list.getName(id);
        default:
            return "";
    }
}

bool MixGMD::addName(GameKind game, std::string name, std::string desc = "")
{
    switch(game){
        case TD:
            return m_td_list.addName(name, desc);
        case RA:
            return m_ra_list.addName(name, desc);
        case TS:
            return m_ts_list.addName(name, desc);
        case RA2:
            return m_ra2_list.addName(name, desc);
        default:
            return false;
    }
}

bool MixGMD::deleteName(GameKind game, std::string name)
{
    switch(game){
        case TD:
            return m_td_list.deleteName(name);
        case RA:
            return m_ra_list.deleteName(name);
        case TS:
            return m_ts_list.deleteName(name);
        case RA2:
            return m_ra2_list.deleteName(name);
        default:
            return false;
    }
}
