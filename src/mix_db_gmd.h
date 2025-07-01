#pragma once
#include "mix_db_gamedb.h"

#include <vector>
#include <fstream>
#include <map>

// Callers job to ensure file handles are valid and in usable state.
class GlobalMixDataBase
{
private: // Data
    MixGameDB TDList;
    MixGameDB RAList;
    MixGameDB TSList;
    MixGameDB RA2List;
    std::vector<MixGameDB*> vDataBase;

public: // Methods
    GlobalMixDataBase();
    void ReadDB(std::fstream &fh);
    void WriteDB(std::fstream &fh);
    std::string GetName(GameKind game, int32_t id);
    bool AddName(GameKind game, std::string name, std::string desc);
    bool DeleteName(GameKind game, std::string name);
};
