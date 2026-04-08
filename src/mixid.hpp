#pragma once

#include <cstdint>
#include <string>

typedef enum 
{ 
    GameTd,
    GameRa,
    GameTs,
    GameDune2,
    GameDune2000,
    GameRa2
} Game;

namespace MixId
{
    int32_t IdGen(Game game, const std::string& fname);
    std::string IdStr(int32_t id);
    std::string IdStr(const char* id, uint32_t size);
    int32_t StrId(const std::string& hex);
    bool IsIdName(const std::string& fname);
}



