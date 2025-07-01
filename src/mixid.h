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
    int32_t     GenerateID(GameKind game, std::string fname);
    std::string ToHexString(int32_t id);
    std::string ToHexString(const char* id, uint32_t size);
    int32_t     FromHexString(std::string hex);
    bool        IsIDExists(std::string fname);
}
