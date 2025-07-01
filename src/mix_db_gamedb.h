#pragma once

#include "mixid.h"
#include <string>
#include <fstream>
#include <map>

/// @brief GameDB databases represent internal databases in global mix database. Each handled game has its own DB internally
class MixGameDB
{
private:
    struct IdData 
    {
        std::string name;
        std::string description;
    };

    typedef std::map<int32_t, IdData> IdMap;
    typedef std::pair<int32_t, IdData> IdPair;
    typedef std::map<int32_t, IdData>::const_iterator IdIter;

    IdMap m_name_map;
    uint32_t m_size;
    uint32_t m_entries;
    GameKind m_game_type;

public:
    MixGameDB(GameKind game);
    void readDB(const char* data, uint32_t offset);
    void writeDB(std::fstream &fh);
    std::string getName(int32_t id);
    bool addName(std::string name, std::string description);
    bool deleteName(std::string name);
    GameKind getGame() { return m_game_type; }
    uint32_t getSize() { return m_size; }
};
