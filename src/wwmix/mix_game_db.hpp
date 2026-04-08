/* 
 * File:   mix_game_db.hpp
 * Author: fbsagr
 *
 * Created on June 3, 2014, 3:40 PM
 */

#pragma once

#include "mix_numeric.hpp"
#include "mixid.hpp"

#include <cstddef>
#include <string>
#include <fstream>
#include <map>

//GameDB databases represent internal databases in global mix database
//each handled game has its own DB internally
class MixGameDb
{
  public:
    /// @brief Construct a global database section for a specific game.
    MixGameDb(Game game);

    /// @brief Load a game database section from a raw memory buffer.
    void ReadDb(const char *data, uint32_t offset);

    /// @brief Write the game database section to a binary stream.
    void WriteDb(std::fstream &fh);

    /// @brief Resolve a file ID to its stored file name.
    std::string GetName(int32_t id) const;

    /// @brief Add a file name and description entry to the section.
    bool AddName(const std::string &name, const std::string &description);

    /// @brief Remove a file name entry from the section.
    bool DeleteName(const std::string &name);

    /// @brief Return the game that owns this database section.
    Game GetGame() const
    {
        return m_game_type;
    }

    /// @brief Return the serialized size of this database section.
    uint32_t GetSize() const
    {
        return MixNumeric::ToUint32(m_size, "global database size");
    }

  private:
    struct IdData
    {
        std::string name;
        std::string description;
    };

    typedef std::map<int32_t, IdData> IdMap;
    typedef std::pair<int32_t, IdData> IdPair;
    typedef std::map<int32_t, IdData>::const_iterator IdIterator;

    IdMap m_name_map;
    std::size_t m_size;
    uint32_t m_entries;
    Game m_game_type;
};
