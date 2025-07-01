#pragma once
#include "mixid.h"
#include <fstream>
#include <map>

class MixLMD
{
public:
    MixLMD(GameKind game);
    void readDB(std::fstream &fh, uint32_t offset, uint32_t size);
    void writeDB(std::fstream &fh);
    std::string getName(int32_t id);
    bool addName(std::string name);
    bool deleteName(std::string name);
    bool deleteName(int32_t id);
    GameKind getGame() { return m_game_type; }
    uint32_t getSize() { return m_size; }
    std::string getDBName() { return "local mix database.dat"; }
    
private:
    typedef std::map<int32_t, std::string> IdMap;
    typedef std::pair<int32_t, std::string> IdPair;
    typedef std::map<int32_t, std::string>::const_iterator IdIter;
    static const char m_xcc_id[32];
    IdMap m_name_map;
    uint32_t m_size;
    GameKind m_game_type;
    int32_t m_id;
};
