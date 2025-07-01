#pragma once

#include <string>
#include <stdint.h>

enum class GameKind
{
    TD,
    RA,
    TS,
    D2,
    D2K,
    RA2
};

namespace MixID
{
    int32_t     idGen(GameKind game, std::string fname);
    std::string idStr(int32_t id);
    std::string idStr(char* id, uint32_t size);
    int32_t     strId(std::string hex);
    bool        isIdName(std::string fname);
}
