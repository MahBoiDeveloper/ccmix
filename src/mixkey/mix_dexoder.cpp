/* 
 * File:   mix_dexoder.cpp
 * Author: ivosh-l
 * 
 * Created on 29. prosinec 2011, 11:31
 */

#include "mix_dexoder.hpp"
#include "cryptopp/integer.h"
#include <print>

const char *pubkey_str = "AihRvNoIbTn85FZRYNZRcT+i6KpU+maCsEqr3Q5q+LDB5tH7Tz2qQ38V";
const char *prvkey_str = "AigKVje8mROcR8QixnxUEF5b29Curkq01DNDWCdOG99XBqH79OaCiTCB";

const static char char2num[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

typedef uint32_t bignum4[4];
typedef uint32_t bignum[64];
typedef uint32_t bignum130[130];

static struct
{
    bignum key1;
    bignum key2;
    uint32_t len;
} pubkey;
bignum glob1;
uint32_t glob1_bitlen, glob1_len_x2;
bignum130 glob2;
bignum4 glob1_hi, glob1_hi_inv;
uint32_t glob1_hi_bitlen;
uint32_t glob1_hi_inv_lo, glob1_hi_inv_hi;

void PrintHex(const char* bigint, int size)
{
    for(int i = size - 1; i >= 0; i--){
        std::print("{:02X}", static_cast<unsigned int>(
                static_cast<unsigned char>(bigint[i])));
    }
}

static void InitBignum(bignum n, uint32_t val, uint32_t len)
{
    memset((void *)n, 0, len * 4);
    n[0] = val;
}

static void MoveKeyToBig(bignum n, char *key, uint32_t klen, uint32_t blen)
{
    uint32_t sign;
    unsigned int i;

    if (key[0] & 0x80) sign = 0xff;
    else sign = 0;

    for (i = blen*4; i > klen; i--)
        ((char *)n)[i-1] = sign;
    for (; i > 0; i--)
        ((char *)n)[i-1] = key[klen-i];
}

static void KeyToBignum(bignum n, char *key, uint32_t len)
{
    uint32_t keylen;
    int i;

    if (key[0] != 2) return;
    key++;

    if (key[0] & 0x80)
    {
        keylen = 0;
        for (i = 0; i < (key[0] & 0x7f); i++) keylen = (keylen << 8) | key[i+1];
        key += (key[0] & 0x7f) + 1;
    }
    else
    {
        keylen = key[0];
        key++;
    }
    if (keylen <= len*4)
        MoveKeyToBig(n, key, keylen, len);
}

static uint32_t LenBignum(bignum n, uint32_t len)
{
  int i;
  i = len-1;
  while ((i >= 0) && (n[i] == 0)) i--;
  return i+1;
}

static uint32_t BitlenBignum(bignum n, uint32_t len)
{
  uint32_t ddlen, bitlen, mask;
  ddlen = LenBignum(n, len);
  if (ddlen == 0) return 0;
  bitlen = ddlen * 32;
  mask = 0x80000000;
  while ((mask & n[ddlen-1]) == 0) {
    mask >>= 1;
    bitlen--;
  }
  return bitlen;
}

static void InitPubkey()
{
    uint32_t i, i2, tmp;
    char keytmp[256];

    InitBignum(pubkey.key2, 0x10001, 64);

    i = 0;
    i2 = 0;
    while (i < strlen(pubkey_str))
    {
        tmp = char2num[(int)pubkey_str[i++]];
        tmp <<= 6; tmp |= char2num[(int)pubkey_str[i++]];
        tmp <<= 6; tmp |= char2num[(int)pubkey_str[i++]];
        tmp <<= 6; tmp |= char2num[(int)pubkey_str[i++]];
        keytmp[i2++] = (tmp >> 16) & 0xff;
        keytmp[i2++] = (tmp >> 8) & 0xff;
        keytmp[i2++] = tmp & 0xff;
    }
    KeyToBignum(pubkey.key1, keytmp, 64);
    pubkey.len = BitlenBignum(pubkey.key1, 64) - 1;
}

static void InitPrvkey()
{
    uint32_t i, i2, tmp;
    char keytmp[256];
    char keytmp2[256];
    
    i = 0;
    i2 = 0;
    while (i < strlen(prvkey_str))
    {
        tmp = char2num[(int)prvkey_str[i++]];
        tmp <<= 6; tmp |= char2num[(int)prvkey_str[i++]];
        tmp <<= 6; tmp |= char2num[(int)prvkey_str[i++]];
        tmp <<= 6; tmp |= char2num[(int)prvkey_str[i++]];
        keytmp2[i2++] = (tmp >> 16) & 0xff;
        keytmp2[i2++] = (tmp >> 8) & 0xff;
        keytmp2[i2++] = tmp & 0xff;
    }
    
    KeyToBignum(pubkey.key2, keytmp2, 64);

    i = 0;
    i2 = 0;
    while (i < strlen(pubkey_str))
    {
        tmp = char2num[(int)pubkey_str[i++]];
        tmp <<= 6; tmp |= char2num[(int)pubkey_str[i++]];
        tmp <<= 6; tmp |= char2num[(int)pubkey_str[i++]];
        tmp <<= 6; tmp |= char2num[(int)pubkey_str[i++]];
        keytmp[i2++] = (tmp >> 16) & 0xff;
        keytmp[i2++] = (tmp >> 8) & 0xff;
        keytmp[i2++] = tmp & 0xff;
    }
    KeyToBignum(pubkey.key1, keytmp, 64);
    pubkey.len = BitlenBignum(pubkey.key2, 64) - 1;
}

static uint32_t LenPredata()
{
    uint32_t a = (pubkey.len - 1) / 8;
    return (55 / a + 1) * (a + 1);
}

static uint32_t LenPredataGen()
{
    uint32_t a = (pubkey.len - 1) / 8;
    return (79 / a + 1) * (a + 1);
}

static long int CmpBignum(bignum n1, bignum n2, uint32_t len)
{
  n1 += len-1;
  n2 += len-1;
  while (len > 0) {
    if (*n1 < *n2) return -1;
    if (*n1 > *n2) return 1;
    n1--;
    n2--;
    len--;
  }
  return 0;
}

static void MovBignum(bignum dest, bignum src, uint32_t len)
{
  memmove(dest, src, len*4);
}

static void ShrBignum(bignum n, uint32_t bits, long int len)
{
  uint32_t i, i2;

  i2 = bits / 32;
  if (i2 > 0) {
    for (i = 0; i < (uint32_t)len - i2; i++) n[i] = n[i + i2];
    for (; i < (uint32_t)len; i++) n[i] = 0;
    bits = bits % 32;
  }
  if (bits == 0) return;
  for (i = 0; i < (uint32_t)len - 1; i++) n[i] = (n[i] >> bits) | (n[i + 1] << (32 -
bits));
  n[i] = n[i] >> bits;
}

static void ShlBignum(bignum n, uint32_t bits, uint32_t len)
{
  uint32_t i, i2;

  i2 = bits / 32;
  if (i2 > 0) {
    for (i = len - 1; i > i2; i--) n[i] = n[i - i2];
    for (; i > 0; i--) n[i] = 0;
    bits = bits % 32;
  }
  if (bits == 0) return;
  for (i = len - 1; i > 0; i--) n[i] = (n[i] << bits) | (n[i - 1] >> (32 -
bits));
  n[0] <<= bits;
}

static uint32_t SubBignum(bignum dest, bignum src1, bignum src2, uint32_t carry, uint32_t len)
{
  uint32_t i1, i2;

  len += len;
  while (--len != (uint32_t)-1) {
    i1 = *(uint16_t *)src1;
    i2 = *(uint16_t *)src2;
    *(uint16_t *)dest = i1 - i2 - carry;
    src1 = (uint32_t *)(((uint16_t *)src1) + 1);
    src2 = (uint32_t *)(((uint16_t *)src2) + 1);
    dest = (uint32_t *)(((uint16_t *)dest) + 1);
    if ((i1 - i2 - carry) & 0x10000) carry = 1; else carry = 0;
  }
  return carry;
}

static void InvBignum(bignum n1, bignum n2, uint32_t len)
{
  bignum n_tmp;
  uint32_t n2_uint8_tlen, bit;
  long int n2_bitlen;

  InitBignum(n_tmp, 0, len);
  InitBignum(n1, 0, len);
  n2_bitlen = BitlenBignum(n2, len);
  bit = ((uint32_t)1) << (n2_bitlen % 32);
  n1 += ((n2_bitlen + 32) / 32) - 1;
  n2_uint8_tlen = ((n2_bitlen - 1) / 32) * 4;
  n_tmp[n2_uint8_tlen / 4] |= ((uint32_t)1) << ((n2_bitlen - 1) & 0x1f);

  while (n2_bitlen > 0) {
    n2_bitlen--;
    ShlBignum(n_tmp, 1, len);
    if (CmpBignum(n_tmp, n2, len) != -1) {
      SubBignum(n_tmp, n_tmp, n2, 0, len);
      *n1 |= bit;
    }
    bit >>= 1;
    if (bit == 0) {
      n1--;
      bit = 0x80000000;
    }
  }
  InitBignum(n_tmp, 0, len);
}

static void IncBignum(bignum n, uint32_t len)
{
  while ((++*n == 0) && (--len > 0)) n++;
}

static void InitTwoDw(bignum n, uint32_t len)
{
    MovBignum(glob1, n, len);
    glob1_bitlen = BitlenBignum(glob1, len);
    glob1_len_x2 = (glob1_bitlen + 15) / 16;
    MovBignum(glob1_hi, glob1 + LenBignum(glob1, len) - 2, 2);
    glob1_hi_bitlen = BitlenBignum(glob1_hi, 2) - 32;
    ShrBignum(glob1_hi, glob1_hi_bitlen, 2);
    InvBignum(glob1_hi_inv, glob1_hi, 2);
    ShrBignum(glob1_hi_inv, 1, 2);
    glob1_hi_bitlen = (glob1_hi_bitlen + 15) % 16 + 1;
    IncBignum(glob1_hi_inv, 2);
    if (BitlenBignum(glob1_hi_inv, 2) > 32)
    {
        ShrBignum(glob1_hi_inv, 1, 2);
        glob1_hi_bitlen--;
    }
    //glob1_hi_inv_lo = *(uint16_t *)glob1_hi_inv;
    //glob1_hi_inv_hi = *(((uint16_t *)glob1_hi_inv) + 1);
    glob1_hi_inv_lo = (uint16_t)(glob1_hi_inv[0] & 0x0000FFFF);
    glob1_hi_inv_hi = (uint16_t)((glob1_hi_inv[0] & 0xFFFF0000) >> 16);

}

static void MulBignumUint16(bignum n1, bignum n2, uint32_t mul, uint32_t len)
{
  uint32_t i, tmp;

  tmp = 0;
  for (i = 0; i < len; i++) {
    tmp = mul * (*(uint16_t *)n2) + *(uint16_t *)n1 + tmp;
    *(uint16_t *)n1 = tmp;
    n1 = (uint32_t *)(((uint16_t *)n1) + 1);
    n2 = (uint32_t *)(((uint16_t *)n2) + 1);
    tmp >>= 16;
  }
  *(uint16_t *)n1 += tmp;
}

static void MulBignum(bignum dest, bignum src1, bignum src2, uint32_t len)
{
  uint32_t i;

  InitBignum(dest, 0, len*2);
  for (i = 0; i < len*2; i++) {
    MulBignumUint16(dest, src1, *(uint16_t *)src2, len*2);
    src2 = (uint32_t *)(((uint16_t *)src2) + 1);
    dest = (uint32_t *)(((uint16_t *)dest) + 1);
  }
}

static void NotBignum(bignum n, uint32_t len)
{
  uint32_t i;
  //for (i = 0; i < len; i++) *(n++) = ~(*n);
  for (i = 0; i < len; i++) n[i] = ~n[i];
}

static void NegBignum(bignum n, uint32_t len)
{
  NotBignum(n, len);
  IncBignum(n, len);
}

static uint32_t GetMulUint16(bignum n)
{
  uint32_t i;
  uint16_t *wn;

  wn = (uint16_t *)n;
  i = (((((((((*(wn-1) ^ 0xffff) & 0xffff) * glob1_hi_inv_lo + 0x10000) >> 1)
      + (((*(wn-2) ^ 0xffff) * glob1_hi_inv_hi + glob1_hi_inv_hi) >> 1) + 1)
      >> 16) + ((((*(wn-1) ^ 0xffff) & 0xffff) * glob1_hi_inv_hi) >> 1) +
      (((*wn ^ 0xffff) * glob1_hi_inv_lo) >> 1) + 1) >> 14) + glob1_hi_inv_hi
      * (*wn ^ 0xffff) * 2) >> glob1_hi_bitlen;
  if (i > 0xffff) i = 0xffff;
  return i & 0xffff;
}

static void DecBignum(bignum n, uint32_t len)
{
    while ((--*n == 0xffffffff) && (--len > 0))
        n++;
}

static void CalcABignum(bignum n1, bignum n2, bignum n3, uint32_t len)
{
    uint32_t g2_len_x2, len_diff;
    uint16_t *esi, *edi;
    uint16_t tmp;

    MulBignum(glob2, n2, n3, len);
    glob2[len*2] = 0;
    g2_len_x2 = LenBignum(glob2, len*2+1)*2;
    if (g2_len_x2 >= glob1_len_x2) {
        IncBignum(glob2, len*2+1);
        NegBignum(glob2, len*2+1);
        len_diff = g2_len_x2 + 1 - glob1_len_x2;
        esi = ((uint16_t *)glob2) + (1 + g2_len_x2 - glob1_len_x2);
        edi = ((uint16_t *)glob2) + (g2_len_x2 + 1);
        for (; len_diff != 0; len_diff--) {
            edi--;
            tmp = GetMulUint16((uint32_t *)edi);
            esi--;
            if (tmp > 0) {
                MulBignumUint16((uint32_t *)esi, glob1, tmp, 2*len);
                if ((*edi & 0x8000) == 0) {
                    if (SubBignum((uint32_t *)esi, (uint32_t *)esi, glob1, 0, len)) (*edi)--;
                }
            }
        }
        NegBignum(glob2, len);
        DecBignum(glob2, len);
    }
    MovBignum(n1, glob2, len);
}

static void ClearTmpVars(uint32_t len)
{
    InitBignum(glob1, 0, len);
    InitBignum(glob2, 0, len);
    InitBignum(glob1_hi_inv, 0, 4);
    InitBignum(glob1_hi, 0, 4);
    glob1_bitlen = 0;
    glob1_hi_bitlen = 0;
    glob1_len_x2 = 0;
    glob1_hi_inv_lo = 0;
    glob1_hi_inv_hi = 0;
}

static void CalcAKey(bignum n1, bignum n2, bignum key2, bignum key1, uint32_t len)
{
    bignum n_tmp;
    uint32_t key2_len, key1_len, key2_bitlen, bit_mask;
    
    InitBignum(n1, 1, len);
    key1_len = LenBignum(key1, len);
    InitTwoDw(key1, key1_len);
    key2_bitlen = BitlenBignum(key2, key1_len);
    key2_len = (key2_bitlen + 31) / 32;
    bit_mask = (((uint32_t)1) << ((key2_bitlen - 1) % 32)) >> 1;
    key2 += key2_len - 1;
    key2_bitlen--;
    MovBignum(n1, n2, key1_len);
    while (--key2_bitlen != (uint32_t)-1)
    {
        if (bit_mask == 0)
        {
            bit_mask = 0x80000000;
            key2--;
        }
        CalcABignum(n_tmp, n1, n1, key1_len);
        if (*key2 & bit_mask)
            CalcABignum(n1, n_tmp, n2, key1_len);
        else
            MovBignum(n1, n_tmp, key1_len);
        bit_mask >>= 1;
    }
    InitBignum(n_tmp, 0, key1_len);
    ClearTmpVars(len);
}

static void ProcessPredata(const uint8_t* pre, uint32_t pre_len, uint8_t *buf)
{
    bignum n2, n3;
    const uint32_t a = (pubkey.len - 1) / 8;
    while (a + 1 <= pre_len)
    {
        InitBignum(n2, 0, 64);
        memmove(n2, pre, a + 1);
        
        //CryptoPP::Integer blk((uint8_t*)n2, a + 1);
        //std::cout << "pre-enc block\n" << std::hex << blk << "\n";
        
        CalcAKey(n3, n2, pubkey.key2, pubkey.key1, 64);
        
        //CryptoPP::Integer blk2((uint8_t*)n3, a);
        //std::cout << "post-enc block\n" << std::hex << blk2 << "\n\n";
        
        memmove(buf, n3, a);

        pre_len -= a + 1;
        pre += a + 1;
        buf += a;
    }
}

void GetBlowfishKey(const uint8_t* s, uint8_t* d)
{
    static bool public_key_initialized = false;
    if (!public_key_initialized)
    {
        InitPubkey();
        public_key_initialized = true;
    }
    
    //CryptoPP::Integer pubmod((uint8_t*)pubkey.key1, 40), pubexp((uint8_t*)pubkey.key2, 40);
    //std::cout << std::hex << pubmod << "\n";
    //std::cout << std::hex << pubexp << "\n\n";
    
    uint8_t key[256];
    ProcessPredata(s, LenPredata(), key);
    memcpy(d, key, 56);
}

void GiveBlowfishKey(const uint8_t* s, uint8_t* d)
{
    static bool public_key_initialized = false;
    if (!public_key_initialized)
    {
        InitPrvkey();
        public_key_initialized = true;
    }
    
    uint8_t key[256];
    ProcessPredata(s, LenPredataGen(), key);
    memcpy(d, key, 80);
}

