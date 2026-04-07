#ifndef MIXID_H
#define	MIXID_H

#include <string>

#ifdef _MSC_VER
#include "win32/stdint.h"
#else
#include <stdint.h>
#endif

typedef enum 
{ 
    game_td,
    game_ra,
    game_ts,
    game_dune2,
    game_dune2000,
    game_ra2
} t_game;

namespace MixID
{
    int32_t idGen(t_game game, const std::string& fname);
    std::string idStr(int32_t id);
    std::string idStr(const char* id, uint32_t size);
    int32_t strId(const std::string& hex);
    bool isIdName(const std::string& fname);
}

#endif	/* MIXID_H */

