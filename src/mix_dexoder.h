#pragma once

#include <cstring>
#include <stdint.h>

void get_blowfish_key(const uint8_t* s, uint8_t* d);
void give_blowfish_key(const uint8_t* s, uint8_t* d);
