/* 
 * File:   mix_dexoder.hpp
 * Author: ivosh-l
 *
 * Created on 29. prosinec 2011, 11:31
 */

#ifndef MIX_DEXODER_H
#define	MIX_DEXODER_H

#include <cstdint>
#include <cstring>


void GetBlowfishKey(const uint8_t* s, uint8_t* d);
void GiveBlowfishKey(const uint8_t* s, uint8_t* d);


#endif	/* MIX_DEXODER_H */


