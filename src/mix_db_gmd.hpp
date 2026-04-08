/* 
 * File:   mix_db_gmd.hpp
 * Author: fbsagr
 *
 * Created on May 30, 2014, 4:30 PM
 */

#pragma once

#include "mix_db_gamedb.hpp"

#include <vector>
#include <fstream>
#include <map>

// Callers job to ensure file handles are valid and in usable state.
class MixGmd
{
public:
    MixGmd();
    void ReadDb(std::fstream &fh);
    void WriteDb(std::fstream &fh);
    std::string GetName(Game game, int32_t id) const;
    bool AddName(Game game, const std::string& name, const std::string& desc);
    bool DeleteName(Game game, const std::string& name);
private:
    MixGameDb m_td_list;
    MixGameDb m_ra_list;
    MixGameDb m_ts_list;
    MixGameDb m_ra2_list;
    std::vector<MixGameDb*> m_db_array;
};


