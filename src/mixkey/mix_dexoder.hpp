/* 
 * File:   mix_dexoder.hpp
 * Author: ivosh-l
 *
 * Created on 29. prosinec 2011, 11:31
 */

#pragma once

#include <cstdint>
#include <cstring>

/// @brief Decode a Blowfish key from an 80-byte MIX key source block.
void GetBlowfishKey(const uint8_t *s, uint8_t *d);

/// @brief Encode a Blowfish key back into an 80-byte MIX key source block.
void GiveBlowfishKey(const uint8_t *s, uint8_t *d);
