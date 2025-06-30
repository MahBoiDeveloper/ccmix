#pragma once

#include <string>

#ifdef _MSC_VER
    #include "win32/stdint.h"
#else
    #include <stdint.h>
#endif

typedef enum 
{
    TD,
    RA,
    TS,
    D2,
    D2K,
    RA2
} GameKind;

namespace MixID
{
    int32_t idGen(GameKind game, std::string fname);
    std::string idStr(int32_t id);
    std::string idStr(char* id, uint32_t size);
    int32_t strId(std::string hex);
    bool isIdName(std::string fname);
}
