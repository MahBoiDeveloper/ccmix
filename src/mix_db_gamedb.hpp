/* 
 * File:   mix_db_gamedb.hpp
 * Author: fbsagr
 *
 * Created on June 3, 2014, 3:40 PM
 */

#ifndef MIX_DB_GAMEDB_H
#define	MIX_DB_GAMEDB_H

#include "mixid.hpp"
#include <string>
#include <fstream>
#include <map>

//GameDB databases represent internal databases in global mix database
//each handled game has its own DB internally
class MixGameDb
{
public:
    MixGameDb(Game game);
    void ReadDb(const char* data, uint32_t offset);
    void WriteDb(std::fstream &fh);
    std::string GetName(int32_t id) const;
    bool AddName(const std::string& name, const std::string& description);
    bool DeleteName(const std::string& name);
    Game GetGame() const { return m_game_type; }
    uint32_t GetSize() const { return m_size; }
    
private:
    struct IdData {
        std::string name;
        std::string description;
    };
    
    typedef std::map<int32_t, IdData> IdMap;
    typedef std::pair<int32_t, IdData> IdPair;
    typedef std::map<int32_t, IdData>::const_iterator IdIterator;
    
    IdMap m_name_map;
    uint32_t m_size;
    uint32_t m_entries;
    Game m_game_type;
};

#endif	/* MIX_DB_GAMEDB_H */


