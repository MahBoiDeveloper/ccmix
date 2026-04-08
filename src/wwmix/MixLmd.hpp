/* 
 * File:   MixLmd.hpp
 * Author: fbsagr
 *
 * Created on May 21, 2014, 11:57 AM
 */

#pragma once

#include "MixNumeric.hpp"
#include "MixId.hpp"

#include <cstddef>
#include <fstream>
#include <map>

class MixLmd
{
  public:
    /// @brief Construct a local mix database for the selected game.
    MixLmd(Game game);

    /// @brief Load the local mix database from an archive region.
    void ReadDb(std::fstream &fh, uint32_t offset, uint32_t size);

    /// @brief Write the local mix database to a stream.
    void WriteDb(std::fstream &fh);

    /// @brief Resolve a file ID to a stored local name.
    std::string GetName(int32_t id) const;

    /// @brief Add a file name to the local database.
    bool AddName(const std::string &name);

    /// @brief Remove a file name from the local database by name.
    bool DeleteName(const std::string &name);

    /// @brief Remove a file name from the local database by ID.
    bool DeleteName(int32_t id);

    /// @brief Return the game associated with this local database.
    Game GetGame() const
    {
        return m_game_type;
    }

    /// @brief Return the serialized size of the local database.
    uint32_t GetSize() const
    {
        return MixNumeric::ToUint32(m_size, "local database size");
    }

    /// @brief Return the reserved file name used for the local database entry.
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
