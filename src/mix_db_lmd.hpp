/* 
 * File:   mix_db_lmd.hpp
 * Author: fbsagr
 *
 * Created on May 21, 2014, 11:57 AM
 */

#pragma once

#include "mix_numeric.hpp"
#include "mixid.hpp"

#include <cstddef>
#include <fstream>
#include <map>

class MixLmd
{
  public:
    MixLmd(Game game);
    void ReadDb(std::fstream &fh, uint32_t offset, uint32_t size);
    void WriteDb(std::fstream &fh);
    std::string GetName(int32_t id) const;
    bool AddName(const std::string &name);
    bool DeleteName(const std::string &name);
    bool DeleteName(int32_t id);
    Game GetGame() const
    {
        return m_game_type;
    }
    uint32_t GetSize() const
    {
        return MixNumeric::ToUint32(m_size, "local database size");
    }
    std::string GetDbName() const
    {
        return "local mix database.dat";
    }

  private:
    typedef std::map<int32_t, std::string> IdMap;
    typedef std::pair<int32_t, std::string> IdPair;
    typedef std::map<int32_t, std::string>::const_iterator IdIterator;
    static const char m_xcc_id[32];
    IdMap m_name_map;
    std::size_t m_size;
    Game m_game_type;
    int32_t m_id;
};
