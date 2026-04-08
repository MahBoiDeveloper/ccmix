/* 
 * File:   MixGmd.hpp
 * Author: fbsagr
 *
 * Created on May 30, 2014, 4:30 PM
 */

#pragma once

#include "MixGameDb.hpp"

#include <vector>
#include <fstream>
#include <map>

// Callers job to ensure file handles are valid and in usable state.
class MixGmd
{
  public:
    /// @brief Construct the full global mix database container.
    MixGmd();

    /// @brief Load all game database sections from a stream.
    void ReadDb(std::fstream &fh);

    /// @brief Write all game database sections to a stream.
    void WriteDb(std::fstream &fh);

    /// @brief Resolve a file ID using the selected game's section.
    std::string GetName(Game game, int32_t id) const;

    /// @brief Add a name and description entry to a game's section.
    bool AddName(Game game, const std::string &name, const std::string &desc);

    /// @brief Remove a name entry from a game's section.
    bool DeleteName(Game game, const std::string &name);

  private:
    MixGameDb m_td_list;
    MixGameDb m_ra_list;
    MixGameDb m_ts_list;
    MixGameDb m_ra2_list;
    std::vector<MixGameDb *> m_db_array;
};
